/* Copyright 2012-2013 Hallowyn and others.
 * This file is part of qron, see <http://qron.hallowyn.com/>.
 * Qron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Qron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with qron. If not, see <http://www.gnu.org/licenses/>.
 */
#include "scheduler.h"
#include <QtDebug>
#include <QCoreApplication>
#include <QEvent>
#include "pf/pfparser.h"
#include "pf/pfdomhandler.h"
#include "data/host.h"
#include "data/cluster.h"
#include "util/timerwitharguments.h"
#include <QMetaObject>
#include "log/log.h"
#include "log/filelogger.h"
#include <QFile>
#include <stdio.h>

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler(QObject *parent) : QObject(parent),
  _alerter(new Alerter) {
  //qRegisterMetaType<CronTrigger>("CronTrigger");
  qRegisterMetaType<TaskRequest>("TaskRequest");
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<QWeakPointer<Executor> >("QWeakPointer<Executor>");
}

Scheduler::~Scheduler() {
  Log::clearLoggers();
  _alerter->deleteLater();
}

bool Scheduler::loadConfiguration(QIODevice *source,
                                  QString &errorString,
                                  bool appendToCurrentConfig) {
  Q_UNUSED(appendToCurrentConfig) // LATER implement appendToCurrentConfig
  if (!source->isOpen())
    if (!source->open(QIODevice::ReadOnly)) {
      errorString = source->errorString();
      Log::warning() << "cannot read configuration: " << errorString;
      return false;
    }
  PfDomHandler pdh;
  PfParser pp(&pdh);
  pp.parse(source);
  int n = pdh.roots().size();
  if (n < 1) {
    errorString = "empty configuration";
    Log::warning() << errorString;
    return false;
  }
  foreach (PfNode root, pdh.roots()) {
    if (root.name() == "qrontab") {
      if (!loadConfiguration(root, errorString))
        return false;
    } else {
      Log::warning() << "ignoring node '" << root.name()
                     << "' at configuration file top level";
    }
  }
  return true;
}

bool Scheduler::loadConfiguration(PfNode root, QString &errorString) {
  Q_UNUSED(errorString) // currently no fatal error, only warnings
  _resources.clear();
  QList<PfNode> children;
  children += root.childrenByName("param");
  children += root.childrenByName("log");
  children += root.childrenByName("taskgroup");
  children += root.childrenByName("task");
  children += root.childrenByName("host");
  children += root.childrenByName("cluster");
  children += root.childrenByName("maxtotaltaskinstances");
  children += root.childrenByName("alerts");
  QList<Logger*> loggers;
  foreach (PfNode node, children) {
    if (node.name() == "host") {
      Host host(node);
      _hosts.insert(host.id(), host);
      Log::debug() << "configured host '" << host.id() << "' with hostname '"
                   << host.hostname() << "'";
      _resources.insert(host.id(), host.resources());
    } else if (node.name() == "cluster") {
      Cluster cluster(node);
      foreach (PfNode child, node.childrenByName("host")) {
        Host host = _hosts.value(child.contentAsString());
        if (!host.isNull())
          cluster.appendHost(host);
        else
          Log::warning() << "host '" << child.contentAsString()
                         << "' not found, won't add it to cluster '"
                         << cluster.id() << "'";
      }
      _clusters.insert(cluster.id(), cluster);
      Log::debug() << "configured cluster '" << cluster.id() << "' with "
                   << cluster.hosts().size() << " hosts";
    } else if (node.name() == "task") {
      Task task(node);
      TaskGroup taskGroup = _tasksGroups.value(node.attribute("taskgroup"));
      if (taskGroup.isNull()) {
        Log::warning() << "ignoring task '" << task.id()
                       << "' without taskgroup";
      } else {
        task.setTaskGroup(taskGroup);
        _tasks.insert(task.fqtn(), task);
        foreach (CronTrigger trigger, task.cronTriggers()) {
          trigger.setTask(task);
          _cronTriggers.append(trigger);
        }
        Log::debug() << "configured task '" << task.fqtn() << "'";
      }
    } else if (node.name() == "taskgroup") {
      TaskGroup taskGroup(node, _globalParams);
      _tasksGroups.insert(taskGroup.id(), taskGroup);
      Log::debug() << "configured taskgroup '" << taskGroup.id() << "'";
    } else if (node.name() == "param") {
      QString key = node.attribute("key");
      QString value = node.attribute("value");
      if (key.isNull() || value.isNull()) {
        Log::warning() << "invalid global param " << node.toPf();
      } else {
        Log::debug() << "configured global param " << key << "=" << value;
        _globalParams.setValue(key, value);
      }
    } else if (node.name() == "maxtotaltaskinstances") {
      bool ok = true;
      int n = node.contentAsString().toInt(&ok);
      if (ok && n > 0) {
        if (!_executors.isEmpty()) {
          Log::warning() << "overriding maxtotaltaskinstances "
                         << _executors.size() << " with " << n;
          foreach (Executor *e, _executors)
            e->deleteLater();
          _executors.clear();
        }
        Log::debug() << "configured " << n << " task executors";
        while (n--) {
          Executor *e = new Executor;
          connect(e, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
                  this, SLOT(taskFinishing(TaskRequest,QWeakPointer<Executor>)));
          _executors.append(e);
        }
      } else {
        Log::warning() << "ignoring maxtotaltaskinstances with incorrect "
                          "value: " << node.contentAsString();
      }
    } else if (node.name() == "log") {
      QString level = node.attribute("level");
      QString filename = node.attribute("file");
      if (level.isEmpty()) {
        Log::warning() << "invalid log level in configuration: "
                       << node.toPf();
      } else if (filename.isEmpty()) {
        Log::warning() << "invalid log filename in configuration: "
                       << node.toPf();
      } else {
        filename = ParamSet().evaluate(filename);
        Log::info() << "adding logger " << node.toPf(); // FIXME
        loggers.append(new FileLogger(filename,
                                      Log::severityFromString(level)));
      }
    } else if (node.name() == "alerts") {
      if (!_alerter->loadConfiguration(node, errorString))
        return false;
      Log::debug() << "configured alerter";
    }
  }
  Log::info() << "replacing loggers " << loggers.size(); // FIXME
  Log::replaceLoggers(loggers);
  if (_executors.isEmpty()) {
    Log::debug() << "configured 16 task executors (default "
                    "maxtotaltaskinstances value)";
    for (int n = 16; n; --n) {
      Executor *e = new Executor;
      connect(e, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
              this, SLOT(taskFinishing(TaskRequest,QWeakPointer<Executor>)));
      _executors.append(e);
    }
  }
  foreach (CronTrigger trigger, _cronTriggers) {
    setTimerForCronTrigger(trigger);
    // LATER fire cron triggers if they were missed since last task exec
  }
  emit tasksConfigurationReset(_tasksGroups, _tasks);
  emit targetsConfigurationReset(_clusters, _hosts);
  emit hostResourceConfigurationChanged(_resources);
  emit globalParamsChanged(_globalParams);
  return true;
}

bool Scheduler::requestTask(const QString fqtn, ParamSet params, bool force) {
  Task task = _tasks.value(fqtn);
  if (task.isNull()) {
    Log::error() << "requested task not found: " << fqtn << params << force;
    return false;
  }
  if (!params.parent().isNull()) {
    Log::warning() << "task requested with non null params' parent (parent will"
                      " be ignored)";
  }
  params.setParent(task.params());
  TaskRequest request(task, params);
  if (force) {
    task.fetchAndAddInstancesCount(1);
    startTaskNowAnyway(request);
  } else {
    if (task.fetchAndAddInstancesCount(1) >= task.maxInstances()) {
      task.fetchAndAddInstancesCount(-1);
      Log::warning() << "requested task '" << fqtn << "' cannot be queued "
                        "because maxinstances is already reached ("
                     << task.maxInstances() << ")";
      return false;
    }
    // note: a request must always be queued even if the task can be started
    // immediately, to avoid the new tasks being started before queued ones
    Log::debug(task.fqtn(), request.id())
        << "queuing task '" << task.fqtn() << "' with params " << params;
    _queuedRequests.append(request);
    emit taskQueued(request);
    reevaluateQueuedRequests();
  }
  return true;
}

void Scheduler::triggerEvent(QString event) {
  foreach (Task task, _tasks.values()) {
    if (task.eventTriggers().contains(event)) {
      Log::debug() << "event '" << event << "' triggered task '" << task.fqtn()
                   << "'";
      // LATER support params at trigger level
      requestTask(task.fqtn());
    }
  }
}

void Scheduler::triggerTrigger(QVariant trigger) {
  if (trigger.canConvert<CronTrigger>()) {
    CronTrigger ct = qvariant_cast<CronTrigger>(trigger);
    Log::debug() << "trigger '" << ct.cronExpression() << "' triggered task '"
                 << ct.task().fqtn() << "'";
    // LATER support params at trigger level
    requestTask(ct.task().fqtn());
    // LATER this is theorically not accurate since it could miss a trigger
    setTimerForCronTrigger(ct);
  } else
    Log::warning() << "invalid internal object when triggering trigger";
}

void Scheduler::setFlag(QString flag) {
  Log::debug() << "setting flag '" << flag << "'"
               << (_setFlags.contains(flag) ? " which was already set"
                                            : "");
  _setFlags.insert(flag);
  reevaluateQueuedRequests();
}

void Scheduler::clearFlag(QString flag) {
  Log::debug() << "clearing flag '" << flag << "'"
               << (_setFlags.contains(flag) ? ""
                                            : " which was already cleared");
  _setFlags.remove(flag);
  reevaluateQueuedRequests();
}

bool Scheduler::tryStartTaskNow(TaskRequest request) {
  if (_executors.isEmpty()) {
    Log::info(request.task().fqtn(), request.id())
        << "cannot execute task '" << request.task().fqtn()
        << "' now because there are already too many tasks running "
           "(maxtotaltaskinstances reached)";
    _alerter->raiseAlert("maxtotaltaskinstances.reached");
    return false;
  }
  _alerter->cancelAlert("maxtotaltaskinstances.reached");
  // LATER check flags
  // TODO check maxtaskinstance
  QList<Host> hosts;
  Host host = _hosts.value(request.task().target());
  if (host.isNull())
    hosts.append(_clusters.value(request.task().target()).hosts());
  else
    hosts.append(host);
  if (hosts.isEmpty()) {
    Log::error(request.task().fqtn(), request.id())
        << "cannot execute task '" << request.task().fqtn()
        << "' because its target '" << request.task().target()
        << "' is not defined";
    _alerter->raiseAlert("task.failure."+request.task().fqtn());
    return false;
  }
  // LATER implement other cluster balancing methods than "first"
  QMap<QString,qint64> taskResources = request.task().resources();
  foreach (Host h, hosts) {
    QMap<QString,qint64> hostResources = _resources.value(h.id());
    foreach (QString kind, taskResources.keys()) {
      if (hostResources.value(kind) < taskResources.value(kind)) {
        Log::info(request.task().fqtn(), request.id())
            << "lacks resource '" << kind << "' on host '" << h.id()
            << "' for task '" << request.task().id() << "' (need "
            << taskResources.value(kind) << ", have "
            << hostResources.value(kind) << ")";
        goto nexthost;
      }
      Log::debug(request.task().fqtn(), request.id())
          << "resource '" << kind << "' ok on host '" << h.id()
          << "' for task '" << request.task().id();
    }
    foreach (QString kind, taskResources.keys())
      hostResources.insert(kind, hostResources.value(kind)
                            -taskResources.value(kind));
    _resources.insert(h.id(), hostResources);
    emit hostResourceAllocationChanged(h.id(), hostResources);
    _alerter->cancelAlert("resource.exhausted."+request.task().target());
    request.setTarget(h);
    request.setStartDatetime();
    request.task().setLastExecution(QDateTime::currentDateTime());
    _executors.takeFirst()->execute(request);
    emit taskStarted(request);
    reevaluateQueuedRequests();
    return true;
nexthost:;
  }
  Log::warning(request.task().fqtn(), request.id())
      << "cannot execute task '" << request.task().fqtn()
      << "' now because there is not enough resources on target '"
      << request.task().target() << "'";
  // LATER suffix alert with resources kind (one alert per exhausted kind)
  _alerter->raiseAlert("resource.exhausted."+request.task().target());
  return false;
}

void Scheduler::startTaskNowAnyway(TaskRequest request) {
  Executor *e;
  if (_executors.isEmpty()) {
    e = new Executor;
    e->setTemporary();
    connect(e, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
            this, SLOT(taskFinishing(TaskRequest,QWeakPointer<Executor>)));
  } else {
    e = _executors.takeFirst();
  }
  Host target = _hosts.value(request.task().target());
  if (target.isNull()) {
    QList<Host> hosts = _clusters.value(request.task().target()).hosts();
    if (!hosts.isEmpty())
      target = hosts.first(); // LATER implement other method than "first"
  }
  if (target.isNull()) {
    Log::error(request.task().fqtn(), request.id())
        << "cannot execute task '" << request.task().fqtn()
        << "' now because its target '" << request.task().target()
        << "' is not defined";
    return;
  }
  QMap<QString,qint64> taskResources = request.task().resources();
  QMap<QString,qint64> hostResources = _resources.value(target.id());
  foreach (QString kind, taskResources.keys())
    hostResources.insert(kind, hostResources.value(kind)
                          -taskResources.value(kind));
  _resources.insert(target.id(), hostResources);
  emit hostResourceAllocationChanged(target.id(), hostResources);
  request.setTarget(target);
  request.setStartDatetime();
  request.task().setLastExecution(QDateTime::currentDateTime());
  e->execute(request);
  emit taskStarted(request);
  reevaluateQueuedRequests();
}

void Scheduler::taskFinishing(TaskRequest request,
                              QWeakPointer<Executor> executor) {
  request.task().fetchAndAddInstancesCount(-1);
  Executor *e = executor.data();
  if (e) {
    if (e->isTemporary())
      e->deleteLater();
    else
      _executors.append(e);
  }
  QMap<QString,qint64> taskResources = request.task().resources();
  QMap<QString,qint64> hostResources = _resources.value(request.target().id());
  foreach (QString kind, taskResources.keys())
    hostResources.insert(kind, hostResources.value(kind)
                          +taskResources.value(kind));
  _resources.insert(request.target().id(), hostResources);
  if (request.success())
    _alerter->cancelAlert("task.failure."+request.task().fqtn());
  else
    _alerter->raiseAlert("task.failure."+request.task().fqtn());
  emit hostResourceAllocationChanged(request.target().id(), hostResources);
  // LATER try resubmit if the host was not reachable (this can be usefull with clusters or when host become reachable again)
  reevaluateQueuedRequests();
  emit taskFinished(request, executor);
}

void Scheduler::reevaluateQueuedRequests() {
  QCoreApplication::postEvent(this,
                              new QEvent(REEVALUATE_QUEUED_REQUEST_EVENT));
}

void Scheduler::customEvent(QEvent *event) {
  if (event->type() == REEVALUATE_QUEUED_REQUEST_EVENT) {
    QCoreApplication::removePostedEvents(this, REEVALUATE_QUEUED_REQUEST_EVENT);
    startQueuedTasksIfPossible();
  } else {
    QObject::customEvent(event);
  }
}

void Scheduler::startQueuedTasksIfPossible() {
  for (int i = 0; i < _queuedRequests.size(); ) {
    TaskRequest r = _queuedRequests[i];
    if (tryStartTaskNow(r)) {
      _queuedRequests.removeAt(i);
      // LATER remove other queued instances of same task if needed
    } else
      ++i;
  }
}

void Scheduler::setTimerForCronTrigger(CronTrigger trigger,
                                       QDateTime previous) {
  int ms = trigger.nextTriggerMsecs(previous);
  if (ms >= 0) {
    Log::debug() << "setting cron timer " << ms << " ms for task '"
                 << trigger.task().fqtn() << "' from trigger '"
                 << trigger.cronExpression() << "' at "
                 << QDateTime::currentDateTime() << " (previous exec was at "
                 << previous << " and cron expression was parsed as '"
                 << trigger.canonicalCronExpression() << "'";
    QVariant v;
    v.setValue(trigger);
    TimerWithArguments::singleShot(ms, this, "triggerTrigger", v);
    trigger.task().setNextScheduledExecution(QDateTime::currentDateTime()
                                             .addMSecs(ms));
  }
  // LATER handle cases were trigger is far away in the future
}
