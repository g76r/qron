/* Copyright 2012 Hallowyn and others.
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
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "scheduler.h"
#include <QtDebug>
#include <QCoreApplication>
#include <QEvent>
#include "pfparser.h"
#include "pfdomhandler.h"
#include "data/host.h"
#include "data/hostgroup.h"
#include "util/timerwithargument.h"
#include <QMetaObject>
#include "log/log.h"
#include "log/filelogger.h"
#include <QFile>
#include <stdio.h>

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler(QObject *parent) : QObject(parent) {
  //qRegisterMetaType<CronTrigger>("CronTrigger");
  qRegisterMetaType<TaskRequest>("TaskRequest");
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<QWeakPointer<Executor> >("QWeakPointer<Executor>");
  // TODO clearLoggers() and create loggers depending on config file
  Log::addLogger(new FileLogger("/tmp/log0", Log::Debug, this));
  Log::addLogger(new FileLogger("/tmp/info", Log::Info, this));
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
  QList<PfNode> children;
  children += root.childrenByName("log");
  children += root.childrenByName("taskgroup");
  children += root.childrenByName("task");
  children += root.childrenByName("host");
  children += root.childrenByName("hostgroup");
  children += root.childrenByName("param");
  children += root.childrenByName("maxtotaltasks");
  foreach (PfNode node, children) {
    if (node.name() == "host") {
      Host host(node);
      _hosts.insert(host.id(), host);
      Log::debug() << "configured host '" << host.id() << "' with hostname '"
                   << host.hostname() << "'";
    } else if (node.name() == "hostgroup") {
      HostGroup group(node);
      foreach (PfNode child, node.childrenByName("host")) {
        Host host = _hosts.value(child.contentAsString());
        if (!host.isNull())
          group.appendHost(host);
        else
          Log::warning() << "host '" << child.contentAsString()
                         << "' not found, won't add it to hostgroup '"
                         << group.id() << "'";
      }
      Log::debug() << "configured hostgroup '" << group.id() << "' with "
                   << group.hosts().size() << " hosts";
    } else if (node.name() == "task") {
      Task task(node);
      TaskGroup taskGroup = _taskGroups.value(node.attribute("taskgroup"));
      if (taskGroup.isNull()) {
        Log::warning() << "ignoring task '" << task.id()
                       << "' without taskgroup";
      } else {
        task.setTaskGroup(taskGroup);
        _tasks.insert(task.fqtn(), task);
        // TODO Task::cronTriggers() is a very bad pattern, replace it
        foreach (CronTrigger trigger, task.cronTriggers()) {
          trigger.setTask(task);
          _cronTriggers.append(trigger);
        }
        Log::debug() << "configured task '" << task.fqtn() << "'";
      }
    } else if (node.name() == "taskgroup") {
      TaskGroup taskGroup(node);
      _taskGroups.insert(taskGroup.id(), taskGroup);
      Log::debug() << "configured taskgroup '" << taskGroup.id() << "'";
    } else if (node.name() == "param") {
      QString key = node.attribute("key");
      QString value = node.attribute("value");
      if (key.isNull() || value.isNull()) {
        // LATER warn
      } else {
        Log::debug() << "configured global param " << key << "=" << value;
        _globalParams.setValue(key, value);
      }
    } else if (node.name() == "log") {
      // LATER log config
    } else if (node.name() == "maxtotaltasks") {
      bool ok = true;
      int n = node.contentAsString().toInt(&ok);
      if (ok && n > 0) {
        if (!_executors.isEmpty()) {
          Log::warning() << "overriding maxtotaltasks " << _executors.size()
                         << " with " << n;
          foreach (Executor *e, _executors)
            e->deleteLater();
          _executors.clear();
        }
        Log::debug() << "configured " << n << " task executors";
        while (n--) {
          Executor *e = new Executor(this);
          connect(e, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
                  this, SLOT(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)));
          _executors.append(e);
        }
      } else {
        Log::warning() << "ignoring maxtotaltasks with incorrect value: "
                       << node.contentAsString();
      }
    }
  }
  if (_executors.isEmpty()) {
    Log::debug() << "configured 16 task executors (default maxtotaltasks "
                    "value)";
    for (int n = 16; n; --n) {
      Executor *e = new Executor(this);
      connect(e, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
              this, SLOT(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)));
      _executors.append(e);
    }
  }
  foreach (CronTrigger trigger, _cronTriggers) {
    setTimerForCronTrigger(trigger);
    // LATER fire cron triggers if they were missed since last task exec
  }
  return true;
}

void Scheduler::requestTask(const QString fqtn, ParamSet params, bool force) {
  Task task = _tasks.value(fqtn);
  if (task.isNull()) {
    Log::warning() << "requested task not found: " << fqtn << params << force;
    return;
  }
  if (!params.parent().isNull()) {
    Log::warning() << "task requested with non null params' parent (parent will"
                      " be ignored)";
  }
  params.setParent(task.params());
  TaskRequest request(task, params);
  if (force) {
    startTaskNowAnyway(request);
  } else {
    // note: a request must always be queued even if the task can be started
    // immediately, to avoid the new tasks being started before queued ones
    Log::debug() << "queuing task '" << task.fqtn() << "' with params "
                 << params;
    _queuedRequests.append(request);
    reevaluateQueuedRequests();
  }
}

void Scheduler::triggerEvent(QString event) {
  foreach (Task task, _tasks.values()) {
    if (task.eventTriggers().contains(event)) {
      Log::debug() << "event '" << event << "' triggered task '" << task.fqtn()
                   << "'";
      requestTask(task.fqtn());
    }
  }
}

void Scheduler::triggerTrigger(QVariant trigger) {
  if (trigger.canConvert<CronTrigger>()) {
    CronTrigger ct = qvariant_cast<CronTrigger>(trigger);
    Log::debug() << "trigger '" << ct.cronExpression() << "' triggered task '"
                 << ct.task().fqtn() << "'";
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
  bool runnable = true;
  // check maxtotaltasks
  if (_executors.isEmpty()) {
    Log::debug() << "cannot execute task '" << request.task().fqtn()
                 << "' now because there are already too many tasks running "
                    "(maxtotaltasks reached)";
    runnable = false;
  }
  // LATER check flags
  // TODO check resources, choose target and consume resources
  // TODO start task if possible
  if (runnable) {
    Executor *exe = _executors.takeFirst();
    exe->execute(request, Host());
    reevaluateQueuedRequests();
  }
  return runnable;
}

void Scheduler::startTaskNowAnyway(TaskRequest request) {
  Executor *e;
  if (_executors.isEmpty()) {
    e = new Executor(this);
    e->setTemporary();
    connect(e, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
            this, SLOT(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)));
  } else {
    e = _executors.takeFirst();
  }
  Host target;
  // TODO consume resources, even setting them < 0
  // TODO choose target
  e->execute(request, target);
  reevaluateQueuedRequests();
}

void Scheduler::taskFinished(TaskRequest request, Host target, bool success,
                             int returnCode, QWeakPointer<Executor> executor) {
  Q_UNUSED(request)
  Q_UNUSED(target)
  Q_UNUSED(success)
  Q_UNUSED(returnCode)
  Executor *e = executor.data();
  if (e) {
    if (e->isTemporary())
      e->deleteLater();
    else
      _executors.append(e);
  }
  // TODO give resources back
  // LATER try resubmit if the host was not reachable (this can be usefull with hostgroups or when host become reachable again)
  reevaluateQueuedRequests();
}

void Scheduler::reevaluateQueuedRequests() {
  QCoreApplication::postEvent(this,
                              new QEvent(REEVALUATE_QUEUED_REQUEST_EVENT));
}

void Scheduler::customEvent(QEvent *event) {
  switch (event->type()) {
  case REEVALUATE_QUEUED_REQUEST_EVENT:
    QCoreApplication::removePostedEvents(this, REEVALUATE_QUEUED_REQUEST_EVENT);
    startQueuedTasksIfPossible();
    break;
  default:
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
                 << trigger.parsedCronExpression() << "'";
    QVariant v;
    v.setValue(trigger);
    TimerWithArgument::singleShot(ms, this, "triggerTrigger", v);
  }
  // LATER handle cases were trigger is far away in the future
}
