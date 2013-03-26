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
#include "config/configutils.h"

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler() : QObject(0), _thread(new QThread()),
  _configMutex(QMutex::Recursive),
  _alerter(new Alerter), _firstConfigurationLoad(true) {
  _thread->setObjectName("SchedulerThread");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  qRegisterMetaType<Task>("Task");
  qRegisterMetaType<TaskRequest>("TaskRequest");
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<QWeakPointer<Executor> >("QWeakPointer<Executor>");
  qRegisterMetaType<QList<Event> >("QList<Event>");
  qRegisterMetaType<QMap<QString,Task> >("QMap<QString,Task>");
  qRegisterMetaType<QMap<QString,TaskGroup> >("QMap<QString,TaskGroup>");
  qRegisterMetaType<QMap<QString,QMap<QString,qint64> > >("QMap<QString,QMap<QString,qint64> >");
  qRegisterMetaType<QMap<QString,Cluster> >("QMap<QString,Cluster>");
  qRegisterMetaType<QMap<QString,Host> >("QMap<QString,Host>");
  qRegisterMetaType<QMap<QString,qint64> >("QMap<QString,qint64>");
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodicChecks()));
  timer->start(60000);
  moveToThread(_thread);
}

Scheduler::~Scheduler() {
  Log::clearLoggers();
  _alerter->deleteLater();
}

bool Scheduler::reloadConfiguration(QIODevice *source, QString &errorString) {
  if (!source->isOpen())
    if (!source->open(QIODevice::ReadOnly)) {
      errorString = source->errorString();
      Log::error() << "cannot read configuration: " << errorString;
      return false;
    }
  PfDomHandler pdh;
  PfParser pp(&pdh);
  pp.parse(source);
  int n = pdh.roots().size();
  if (n < 1) {
    errorString = pdh.errorString()+" at line "+QString::number(pdh.errorLine())
        +" column "+QString::number(pdh.errorColumn());
    Log::error() << "empty or invalid configuration: " << errorString;
    return false;
  }
  foreach (PfNode root, pdh.roots()) {
    if (root.name() == "qrontab") {
      // LATER warn if several top level nodes
      return reloadConfiguration(root, errorString);
    } else {
      Log::warning() << "ignoring node '" << root.name()
                     << "' at configuration file top level";
    }
  }
  return true;
}

bool Scheduler::reloadConfiguration(PfNode root, QString &errorString) {
  // should not need a config (biglock) mutex if loadConfiguration is always executed by Scheduler's thread
  // however using only Scheduler's thread need to cope with QString&
  QMutexLocker ml(&_configMutex);
  // LATER decide if it is a good thing never returning false and not using errorString (which is currently the implementation)
  // alternative: (i) actually use bool + errorString, which implies not clearing member data in case the method fails
  // or (ii) remove bool and errorString from signature and rely on event loop instead of mutexes
  _resources.clear();
  _unsetenv.clear();
  _globalParams.clear();
  _setenv.clear();
  _setenv.setValue("TASKREQUESTID", "%!taskrequestid");
  _setenv.setValue("FQTN", "%!fqtn");
  _setenv.setValue("TASKGROUPID", "%!taskgroupid");
  _setenv.setValue("TASKID", "%!taskid");
  QList<Logger*> loggers;
  foreach (PfNode node, root.childrenByName("log")) {
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
  }
  //Log::debug() << "replacing loggers " << loggers.size();
  Log::replaceLoggers(loggers);
  if (!ConfigUtils::loadParamSet(root, _globalParams, errorString))
    return false;
  if (!ConfigUtils::loadSetenv(root, _setenv, errorString))
    return false;
  if (!ConfigUtils::loadUnsetenv(root, _unsetenv, errorString))
    return false;
  _tasksGroups.clear();
  foreach (PfNode node, root.childrenByName("taskgroup")) {
    QString id = node.attribute("id");
    if (id.isEmpty()) {
      Log::error() << "ignoring taskgroup with invalid id: " << node.toPf();
    } else {
      if (_tasksGroups.contains(id))
        Log::warning() << "duplicate taskgroup " << id << " in configuration";
      TaskGroup taskGroup(node, _globalParams, _setenv, _unsetenv, this);
      _tasksGroups.insert(taskGroup.id(), taskGroup);
      //Log::debug() << "configured taskgroup '" << taskGroup.id() << "'";
    }
  }
  QMap<QString,Task> oldTasks = _tasks;
  _tasks.clear();
  foreach (PfNode node, root.childrenByName("task")) {
    QString taskGroupId = node.attribute("taskgroup");
    TaskGroup taskGroup = _tasksGroups.value(taskGroupId);
    if (taskGroupId.isEmpty() || taskGroup.isNull()) {
      Log::error() << "ignoring task with invalid taskgroup: " << node.toPf();
    } else {
      QString id = node.attribute("id");
      if (id.isEmpty() || id.contains(QRegExp("[^a-zA-Z0-9_\\-]"))) {
        Log::error() << "ignoring task with invalid id: " << node.toPf();
      } else {
        QString fqtn = taskGroup.id()+"."+id;
        if (_tasks.contains(fqtn))
          Log::warning() << "duplicate task " << fqtn << " in configuration";
        Log::debug() << "loading task " << fqtn << " " << oldTasks.value(fqtn).fqtn();
        Task task(node, this, oldTasks.value(fqtn));
        task.completeConfiguration(taskGroup);
        _tasks.insert(task.fqtn(), task);
        //Log::debug() << "configured task '" << task.fqtn() << "'";
      }
    }
  }
  _hosts.clear();
  foreach (PfNode node, root.childrenByName("host")) {
    Host host(node);
    _hosts.insert(host.id(), host);
    //Log::debug() << "configured host '" << host.id() << "' with hostname '"
    //             << host.hostname() << "'";
    _resources.insert(host.id(), host.resources());
  }
  _clusters.clear();
  foreach (PfNode node, root.childrenByName("cluster")) {
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
    //Log::debug() << "configured cluster '" << cluster.id() << "' with "
    //             << cluster.hosts().size() << " hosts";
  }
  // FIXME
  foreach (PfNode node, root.childrenByName("maxtotaltaskinstances")) {
    int n = node.contentAsString().toInt(0, 0);
    if (n > 0) {
      if (!_executors.isEmpty()) {
        Log::warning() << "overriding maxtotaltaskinstances "
                       << _executors.size() << " with " << n;
        foreach (Executor *e, _executors)
          e->deleteLater();
        _executors.clear();
      }
      Log::debug() << "configuring " << n << " task executors";
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
  }
  foreach (PfNode node, root.childrenByName("alerts")) {
    // LATER warn or fail if duplicated
    if (!_alerter->loadConfiguration(node, errorString))
      return false;
    //Log::debug() << "configured alerter";
  }
  _onstart.clear();
  foreach (PfNode node, root.childrenByName("onstart"))
    if (!loadEventListConfiguration(node, _onstart, errorString))
      return false;
  _onsuccess.clear();
  foreach (PfNode node, root.childrenByName("onsuccess"))
    if (!loadEventListConfiguration(node, _onsuccess, errorString))
      return false;
  _onfailure.clear();
  foreach (PfNode node, root.childrenByName("onfailure"))
    if (!loadEventListConfiguration(node, _onfailure, errorString))
      return false;
  foreach (PfNode node, root.childrenByName("onfinish"))
    if (!loadEventListConfiguration(node, _onsuccess, errorString)
        || !loadEventListConfiguration(node, _onfailure, errorString))
      return false;
  _onlog.clear();
  foreach (PfNode node, root.childrenByName("onlog"))
    if (!loadEventListConfiguration(node, _onlog, errorString))
      return false;
  _onnotice.clear();
  foreach (PfNode node, root.childrenByName("onnotice"))
    if (!loadEventListConfiguration(node, _onnotice, errorString))
      return false;
  _onschedulerstart.clear();
  foreach (PfNode node, root.childrenByName("onschedulerstart"))
    if (!loadEventListConfiguration(node, _onschedulerstart, errorString))
      return false;
  // LATER onschedulerreload onschedulershutdown
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
  QMetaObject::invokeMethod(this, "checkTriggersForAllTasks",
                            Qt::QueuedConnection);
  emit tasksConfigurationReset(_tasksGroups, _tasks);
  emit targetsConfigurationReset(_clusters, _hosts);
  emit hostResourceConfigurationChanged(_resources);
  emit globalParamsChanged(_globalParams);
  emit eventsConfigurationReset(_onstart, _onsuccess, _onfailure, _onlog,
                                _onnotice, _onschedulerstart);
  // inspect queued requests to replace Task objects or remove request
  for (int i = 0; i < _queuedRequests.size(); ++i) {
    TaskRequest &r = _queuedRequests[i];
    QString fqtn = r.task().fqtn();
    Task t = _tasks.value(fqtn);
    if (t.isNull()) {
      Log::warning(fqtn, r.id())
          << "canceling queued task while reloading configuration because this "
             "task no longer exists: '" << fqtn << "'";
      _queuedRequests.removeAt(i--);
    } else {
      Log::info(fqtn, r.id())
          << "replacing task definition in queued request while reloading "
             "configuration";
      r.setTask(t);
    }
  }
  ml.unlock();
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

quint64 Scheduler::syncRequestTask(const QString fqtn, ParamSet params,
                               bool force) {
  if (this->thread() == QThread::currentThread())
    return doRequestTask(fqtn, params, force);
  quint64 id = -1;
  QMetaObject::invokeMethod(this, "doRequestTask", Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(quint64, id), Q_ARG(QString, fqtn),
                            Q_ARG(ParamSet, params), Q_ARG(bool, force));
  return id;
}

void Scheduler::asyncRequestTask(const QString fqtn, ParamSet params,
                                 bool force) {
  QMetaObject::invokeMethod(this, "doRequestTask", Qt::QueuedConnection,
                            Q_ARG(QString, fqtn), Q_ARG(ParamSet, params),
                            Q_ARG(bool, force));
}

quint64 Scheduler::doRequestTask(const QString fqtn, ParamSet params, bool force) {
  QMutexLocker ml(&_configMutex);
  Task task = _tasks.value(fqtn);
  ml.unlock();
  if (task.isNull()) {
    Log::warning() << "requested task not found: " << fqtn << params << force;
    return -1;
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
      return -1;
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
  return request.id();
}

void Scheduler::checkTriggersForTask(QVariant fqtn) {
  //Log::debug() << "Scheduler::checkTriggersForTask " << fqtn;
  QMutexLocker ml(&_configMutex);
  Task task = _tasks.value(fqtn.toString());
  foreach (const CronTrigger trigger, task.cronTriggers())
    checkTrigger(trigger, task, fqtn.toString());
}

void Scheduler::checkTriggersForAllTasks() {
  //Log::debug() << "Scheduler::checkTriggersForAllTasks ";
  QMutexLocker ml(&_configMutex);
  foreach (Task task, _tasks.values()) {
    QString fqtn = task.fqtn();
    foreach (const CronTrigger trigger, task.cronTriggers())
      checkTrigger(trigger, task, fqtn);
  }
}

bool Scheduler::checkTrigger(CronTrigger trigger, Task task, QString fqtn) {
  //Log::debug() << "Scheduler::checkTrigger " << trigger.cronExpression()
  //             << " " << fqtn;
  QDateTime now(QDateTime::currentDateTime());
  QDateTime next = trigger.nextTriggering();
  bool fired = false;
  if (next <= now) {
    // requestTask if trigger reached
    quint64 id = syncRequestTask(fqtn);
    if (id > 0)
      Log::debug(fqtn, id) << "cron trigger '" << trigger.cronExpression()
                           << "' triggered task '" << fqtn << "'";
    else
      Log::debug(fqtn) << "cron trigger '" << trigger.cronExpression()
                       << "' failed to trigger task '" << fqtn << "'";
    trigger.setLastTriggered(now);
    next = trigger.nextTriggering();
    fired = true;
  } else {
    QDateTime taskNext = task.nextScheduledExecution();
    if (taskNext.isValid() && taskNext <= next) {
      //Log::debug() << "Scheduler::checkTrigger don't trigger or plan new "
      //                "check for task " << fqtn << " "
      //             << now.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
      //             << next.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
      //             << taskNext.toString("yyyy-MM-dd hh:mm:ss,zzz");
      return false; // don't plan new check if already planned
    }
  }
  // plan new check
  qint64 ms = now.msecsTo(next);
  //Log::debug() << "Scheduler::checkTrigger planning new check for task "
  //             << fqtn << " "
  //             << now.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
  //             << next.toString("yyyy-MM-dd hh:mm:ss,zzz") << " " << ms;
  TimerWithArguments::singleShot(ms, this, "checkTriggersForTask", fqtn);
  task.setNextScheduledExecution(now.addMSecs(ms));
  return fired;
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
      asyncRequestTask(task.fqtn());
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
  QMutexLocker ml(&_configMutex);
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
    triggerEvents(_onstart, &request); // FIXME accessing _onstart needs lock but triggering events may need unlock...
    ml.unlock();
    request.task().triggerStartEvents(&request);
    _executors.takeFirst()->execute(request); // FIXME eaccessing _executors should not need lock but does...
    emit taskStarted(request);
    emit taskChanged(request.task());
    reevaluateQueuedRequests();
    return true;
nexthost:;
  }
  ml.unlock();
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
  Task requestedTask(request.task());
  QString fqtn(requestedTask.fqtn());
  QMutexLocker ml(&_configMutex);
  // configured and requested tasks are different if config reloaded meanwhile
  Task configuredTask(_tasks.value(fqtn));
  configuredTask.fetchAndAddInstancesCount(-1);
  if (executor) {
    Executor *e = executor.data();
    if (e->isTemporary())
      e->deleteLater();
    else
      _executors.append(e);
  }
  _runningRequests.removeAll(request);
  QMap<QString,qint64> taskResources = requestedTask.resources();
  QMap<QString,qint64> hostResources = _resources.value(request.target().id());
  foreach (QString kind, taskResources.keys())
    hostResources.insert(kind, hostResources.value(kind)
                          +taskResources.value(kind));
  _resources.insert(request.target().id(), hostResources);
  ml.unlock();
  if (request.success())
    _alerter->cancelAlert("task.failure."+fqtn);
  else
    _alerter->raiseAlert("task.failure."+fqtn);
  emit hostResourceAllocationChanged(request.target().id(), hostResources);
  // LATER try resubmit if the host was not reachable (this can be usefull with clusters or when host become reachable again)
  if (!request.endDatetime().isNull())
    configuredTask.setLastSuccessful(request.success());
  emit taskFinished(request, executor);
  emit taskChanged(configuredTask);
  if (request.success()) {
    triggerEvents(_onsuccess, &request);
    configuredTask.triggerSuccessEvents(&request);
  } else {
    triggerEvents(_onfailure, &request);
    configuredTask.triggerFailureEvents(&request);
  }
  if (configuredTask.maxExpectedDuration() < LLONG_MAX) {
    if (configuredTask.maxExpectedDuration() < request.totalMillis())
      _alerter->raiseAlert("task.toolong."+fqtn);
    else
      _alerter->cancelAlert("task.toolong."+fqtn);
  }
  if (configuredTask.minExpectedDuration() > 0) {
    if (configuredTask.minExpectedDuration() > request.runningMillis())
      _alerter->raiseAlert("task.tooshort."+fqtn);
    else
      _alerter->cancelAlert("task.tooshort."+fqtn);
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
      _runningRequests.append(_queuedRequests.takeAt(i));
      // LATER remove other queued instances of same task if needed
    } else
      ++i;
  }
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

void Scheduler::periodicChecks() {
  // detect queued or running tasks that exceeded their max expected duration
  QList<TaskRequest> currentRequests(_queuedRequests);
  currentRequests.append(_runningRequests);
  foreach (const TaskRequest r, currentRequests) {
    const Task t(r.task());
    if (t.maxExpectedDuration() < r.liveTotalMillis())
      _alerter->raiseAlert("task.toolong."+t.fqtn());
  }
  // restart timer for triggers if any was lost, this is never usefull apart
  // if current system time goes back (which btw should never occur on well
  // managed production servers, however it with naive sysops)
  checkTriggersForAllTasks();
}
