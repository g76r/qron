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
#include "config/host.h"
#include "config/cluster.h"
#include "util/timerwitharguments.h"
#include <QMetaObject>
#include "log/log.h"
#include "log/filelogger.h"
#include <QFile>
#include <stdio.h>
#include "event/postnoticeevent.h"
#include "event/logevent.h"
#include "event/udpevent.h"
#include "event/httpevent.h"
#include "event/raisealertevent.h"
#include "event/cancelalertevent.h"
#include "event/emitalertevent.h"
#include "event/setflagevent.h"
#include "event/clearflagevent.h"
#include "event/requesttaskevent.h"
#include <QThread>

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler(QObject *parent) : QObject(parent),
  _alerter(new Alerter), _firstConfigurationLoad(true) {
  //qRegisterMetaType<CronTrigger>("CronTrigger");
  qRegisterMetaType<TaskRequest>("TaskRequest");
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<QWeakPointer<Executor> >("QWeakPointer<Executor>");
  qRegisterMetaType<QList<Event> >("QList<Event>");
  qRegisterMetaType<QMap<QString,Task> >("QMap<QString,Task>");
  qRegisterMetaType<QMap<QString,TaskGroup> >("QMap<QString,TaskGroup>");
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
  children += root.childrenByName("onstart");
  children += root.childrenByName("onsuccess");
  children += root.childrenByName("onfailure");
  children += root.childrenByName("onfinish");
  children += root.childrenByName("onlog");
  children += root.childrenByName("onnotice");
  children += root.childrenByName("onschedulerstart");
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
          Log::error() << "host '" << child.contentAsString()
                       << "' not found, won't add it to cluster '"
                       << cluster.id() << "'";
      }
      _clusters.insert(cluster.id(), cluster);
      Log::debug() << "configured cluster '" << cluster.id() << "' with "
                   << cluster.hosts().size() << " hosts";
    } else if (node.name() == "task") {
      Task task(node, this);
      TaskGroup taskGroup = _tasksGroups.value(node.attribute("taskgroup"));
      if (taskGroup.isNull()) {
        Log::warning() << "ignoring task '" << task.id()
                       << "' without taskgroup";
      } else {
        task.completeConfiguration(taskGroup);
        _tasks.insert(task.fqtn(), task);
        foreach (CronTrigger trigger, task.cronTriggers()) {
          trigger.setTask(task);
          _cronTriggers.append(trigger);
        }
        Log::debug() << "configured task '" << task.fqtn() << "'";
      }
    } else if (node.name() == "taskgroup") {
      TaskGroup taskGroup(node, _globalParams, this);
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
        Log::debug() << "adding logger " << node.toPf();
        loggers.append(new FileLogger(filename,
                                      Log::severityFromString(level)));
      }
    } else if (node.name() == "alerts") {
      if (!_alerter->loadConfiguration(node, errorString))
        return false;
      Log::debug() << "configured alerter";
    } else if (node.name() == "onstart") {
      if (!loadEventListConfiguration(node, _onstart, errorString))
        return false;
    } else if (node.name() == "onsuccess") {
      if (!loadEventListConfiguration(node, _onsuccess, errorString))
        return false;
    } else if (node.name() == "onfailure") {
      if (!loadEventListConfiguration(node, _onfailure, errorString))
        return false;
    } else if (node.name() == "onfinish") {
      if (!loadEventListConfiguration(node, _onsuccess, errorString)
          || !loadEventListConfiguration(node, _onfailure, errorString))
        return false;
    } else if (node.name() == "onlog") {
      // LATER implement onlog events for real
      if (!loadEventListConfiguration(node, _onlog, errorString))
        return false;
    } else if (node.name() == "onnotice") {
      if (!loadEventListConfiguration(node, _onnotice, errorString))
        return false;
    } else if (node.name() == "onschedulerstart") {
      if (!loadEventListConfiguration(node, _onschedulerstart, errorString))
        return false;
    }
  }
  Log::debug() << "replacing loggers " << loggers.size();
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
  emit eventsConfigurationReset(_onstart, _onsuccess, _onfailure, _onlog,
                                _onnotice, _onschedulerstart);
  if (_firstConfigurationLoad) {
    _firstConfigurationLoad = false;
    Log::info() << "starting scheduler";
    _alerter->emitAlert("scheduler.start");
    triggerEvents(_onschedulerstart, 0);
  }
  return true;
}

bool Scheduler::loadEventListConfiguration(PfNode listnode, QList<Event> &list,
                                           QString &errorString) {
  Q_UNUSED(errorString)
  foreach (PfNode node, listnode.children()) {
    if (node.name() == "postnotice") {
      list.append(PostNoticeEvent(this, node.contentAsString()));
    } else if (node.name() == "setflag") {
      list.append(SetFlagEvent(this, node.contentAsString()));
    } else if (node.name() == "clearflag") {
      list.append(ClearFlagEvent(this, node.contentAsString()));
    } else if (node.name() == "raisealert") {
      list.append(RaiseAlertEvent(this, node.contentAsString()));
    } else if (node.name() == "cancelalert") {
      list.append(CancelAlertEvent(this, node.contentAsString()));
    } else if (node.name() == "emitalert") {
      list.append(EmitAlertEvent(this, node.contentAsString()));
    } else if (node.name() == "requesttask") {
      ParamSet params;
      // FIXME loadparams
      list.append(RequestTaskEvent(this, node.contentAsString(), params,
                                   node.hasChild("force")));
    } else if (node.name() == "udp") {
      list.append(UdpEvent(node.attribute("address"),
                           node.attribute("message")));
    } else if (node.name() == "log") {
      list.append(LogEvent(
                    Log::severityFromString(node.attribute("severity", "info")),
                    node.attribute("message")));
    } else {
      Log::warning() << "unknown event type: " << node.name();
    }
  }
  return true;
}

void Scheduler::triggerEvents(const QList<Event> list,
                              const ParamsProvider *context) {
  foreach (const Event e, list)
    e.trigger(context);
}

bool Scheduler::requestTask(const QString fqtn, ParamSet params, bool force) {
  if (this->thread() == QThread::currentThread())
    return doRequestTask(fqtn, params, force);
  bool success = false;
  QMetaObject::invokeMethod(this, "doRequestTask", Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(bool, success), Q_ARG(QString, fqtn),
                            Q_ARG(ParamSet, params), Q_ARG(bool, force));
  return success;
}

bool Scheduler::doRequestTask(const QString fqtn, ParamSet params, bool force) {
  QMutexLocker ml(&_configMutex);
  Task task = _tasks.value(fqtn);
  ml.unlock();
  if (task.isNull()) {
    Log::warning() << "requested task not found: " << fqtn << params << force;
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
    emit taskChanged(request.task());
    reevaluateQueuedRequests();
  }
  return true;
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

void Scheduler::setFlag(const QString flag) {
  QMutexLocker ml(&_flagsMutex);
  Log::debug() << "setting flag '" << flag << "'"
               << (_setFlags.contains(flag) ? " which was already set"
                                            : "");
  _setFlags.insert(flag);
  ml.unlock();
  emit flagSet(flag);
  reevaluateQueuedRequests();
}

void Scheduler::clearFlag(const QString flag) {
  QMutexLocker ml(&_flagsMutex);
  Log::debug() << "clearing flag '" << flag << "'"
               << (_setFlags.contains(flag) ? ""
                                            : " which was already cleared");
  _setFlags.remove(flag);
  ml.unlock();
  emit flagCleared(flag);
  reevaluateQueuedRequests();
}

bool Scheduler::isFlagSet(const QString flag) const {
  QMutexLocker ml(&_flagsMutex);
  return _setFlags.contains(flag);
}

class NoticeContext : public ParamsProvider {
public:
  QString _notice;
  NoticeContext(const QString notice) : _notice(notice) { }
  QString paramValue(const QString key, const QString defaultValue) const {
    if (key == "!notice")
      return _notice;
    return defaultValue;
  }
};

void Scheduler::postNotice(const QString notice) {
  QMutexLocker ml(&_configMutex);
  QMap<QString,Task> tasks = _tasks;
  ml.unlock();
  Log::debug() << "posting notice '" << notice << "'";
  foreach (Task task, tasks.values()) {
    if (task.noticeTriggers().contains(notice)) {
      Log::debug() << "notice '" << notice << "' triggered task '"
                   << task.fqtn() << "'";
      // LATER support params at trigger level
      requestTask(task.fqtn());
    }
  }
  emit noticePosted(notice);
  // TODO onnotice events are useless without a notice filter
  NoticeContext context(notice);
  triggerEvents(_onnotice, &context);
}

bool Scheduler::tryStartTaskNow(TaskRequest request) {
  if (!request.task().enabled())
    return false; // do not start disabled tasks
  if (_executors.isEmpty()) {
    Log::info(request.task().fqtn(), request.id())
        << "cannot execute task '" << request.task().fqtn()
        << "' now because there are already too many tasks running "
           "(maxtotaltaskinstances reached)";
    _alerter->raiseAlert("scheduler.maxtotaltaskinstances.reached");
    return false;
  }
  _alerter->cancelAlert("scheduler.maxtotaltaskinstances.reached");
  // LATER check flags
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
    request.setReturnCode(-1);
    request.setSuccess(false);
    request.setEndDatetime();
    request.task().fetchAndAddInstancesCount(-1);
    emit taskFinished(request, QWeakPointer<Executor>());
    emit taskChanged(request.task());
    return true;
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
    triggerEvents(_onstart, &request);
    request.task().triggerStartEvents(&request);
    _executors.takeFirst()->execute(request);
    emit taskStarted(request);
    emit taskChanged(request.task());
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
  QMutexLocker ml(&_configMutex);
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
  ml.unlock();
  emit hostResourceAllocationChanged(target.id(), hostResources);
  request.setTarget(target);
  request.setStartDatetime();
  request.task().setLastExecution(QDateTime::currentDateTime());
  e->execute(request);
  emit taskStarted(request);
  emit taskChanged(request.task());
  triggerEvents(_onstart, &request);
  request.task().triggerStartEvents(&request);
  reevaluateQueuedRequests();
}

void Scheduler::taskFinishing(TaskRequest request,
                              QWeakPointer<Executor> executor) {
  request.task().fetchAndAddInstancesCount(-1);
  if (executor) {
    Executor *e = executor.data();
    if (e->isTemporary())
      e->deleteLater();
    else
      _executors.append(e);
  }
  QMutexLocker ml(&_configMutex);
  QMap<QString,qint64> taskResources = request.task().resources();
  QMap<QString,qint64> hostResources = _resources.value(request.target().id());
  foreach (QString kind, taskResources.keys())
    hostResources.insert(kind, hostResources.value(kind)
                          +taskResources.value(kind));
  _resources.insert(request.target().id(), hostResources);
  ml.unlock();
  if (request.success())
    _alerter->cancelAlert("task.failure."+request.task().fqtn());
  else
    _alerter->raiseAlert("task.failure."+request.task().fqtn());
  emit hostResourceAllocationChanged(request.target().id(), hostResources);
  // LATER try resubmit if the host was not reachable (this can be usefull with clusters or when host become reachable again)
  emit taskFinished(request, executor);
  emit taskChanged(request.task());
  if (request.success()) {
    triggerEvents(_onsuccess, &request);
    request.task().triggerSuccessEvents(&request);
  } else {
    triggerEvents(_onfailure, &request);
    request.task().triggerFailureEvents(&request);
  }
  reevaluateQueuedRequests();
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

bool Scheduler::enableTask(const QString fqtn, bool enable) {
  QMutexLocker ml(&_configMutex);
  Task t = _tasks.value(fqtn);
  if (t.isNull())
    return false;
  t.setEnabled(enable);
  ml.unlock();
  if (enable)
    reevaluateQueuedRequests();
  emit taskChanged(t);
  return true;
}
