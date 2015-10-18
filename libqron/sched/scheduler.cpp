/* Copyright 2012-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#include "action/requesttaskaction.h"
#include <QThread>
#include "config/configutils.h"
#include "config/requestformfield.h"
#include "config/step.h"
#include "trigger/crontrigger.h"
#include "trigger/noticetrigger.h"

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

static SharedUiItem nullItem;

Scheduler::Scheduler() : QronConfigDocumentManager(0), _thread(new QThread()),
  _alerter(new Alerter), _authenticator(new InMemoryAuthenticator(this)),
  _usersDatabase(new InMemoryUsersDatabase(this)),
  _firstConfigurationLoad(true),
  _startdate(QDateTime::currentDateTime().toMSecsSinceEpoch()),
  _configdate(LLONG_MIN), _execCount(0), _runningTasksHwm(0),
  _queuedTasksHwm(0),
  _accessControlFilesWatcher(new QFileSystemWatcher(this)) {
  _thread->setObjectName("SchedulerThread");
  connect(this, &Scheduler::destroyed, _thread, &QThread::quit);
  connect(_thread, &QThread::finished, _thread, &QThread::deleteLater);
  _thread->start();
  qRegisterMetaType<TaskInstance>("TaskInstance");
  qRegisterMetaType<QList<TaskInstance> >("QList<TaskInstance>");
  // TODO not sure this is needed:
  qRegisterMetaType<QHash<QString,QHash<QString,qint64>>>("QHash<QString,QHash<QString,qint64>>");
  QTimer *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &Scheduler::periodicChecks);
  timer->start(60000);
  connect(_accessControlFilesWatcher, &QFileSystemWatcher::fileChanged,
          this, &Scheduler::reloadAccessControlConfig);
  moveToThread(_thread);
}

Scheduler::~Scheduler() {
  //Log::removeLoggers();
  //_alerter->deleteLater(); // TODO delete alerter only when last executor is deleted
}

void Scheduler::activateConfig(SchedulerConfig newConfig) {
  SchedulerConfig oldConfig = QronConfigDocumentManager::config();
  emit logConfigurationChanged(newConfig.logfiles());
  int executorsToAdd = newConfig.maxtotaltaskinstances()
      - oldConfig.maxtotaltaskinstances();
  if (executorsToAdd < 0) {
    if (-executorsToAdd > _availableExecutors.size()) {
      Log::warning() << "cannot set maxtotaltaskinstances down to "
                     << newConfig.maxtotaltaskinstances()
                     << " because there are too "
                        "currently many busy executors, setting it to "
                     << newConfig.maxtotaltaskinstances()
                        - (executorsToAdd - _availableExecutors.size())
                     << " instead";
      // TODO mark some executors as temporary to make them disappear later
      //maxtotaltaskinstances -= executorsToAdd - _availableExecutors.size();
      executorsToAdd = -_availableExecutors.size();
    }
    Log::debug() << "removing " << -executorsToAdd << " executors to reach "
                    "maxtotaltaskinstances of "
                 << newConfig.maxtotaltaskinstances();
    for (int i = 0; i < -executorsToAdd; ++i)
      _availableExecutors.takeFirst()->deleteLater();
  } else if (executorsToAdd > 0) {
    Log::debug() << "adding " << executorsToAdd << " executors to reach "
                    "maxtotaltaskinstances of "
                 << newConfig.maxtotaltaskinstances();
    for (int i = 0; i < executorsToAdd; ++i) {
      Executor *executor = new Executor(_alerter);
      connect(executor, &Executor::taskInstanceFinished,
              this, &Scheduler::taskInstanceFinishing);
      connect(executor, &Executor::taskInstanceStarted,
              this, &Scheduler::propagateTaskInstanceChange);
      connect(this, &Scheduler::noticePosted,
              executor, &Executor::noticePosted);
      _availableExecutors.append(executor);
    }
  } else {
    Log::debug() << "keep maxtotaltaskinstances of "
                 << newConfig.maxtotaltaskinstances();
  }
  newConfig.copyLiveAttributesFromOldTasks(oldConfig.tasks());
  setConfig(newConfig);
  _alerter->setConfig(newConfig.alerterConfig());
  reloadAccessControlConfig();
  QMetaObject::invokeMethod(this, "checkTriggersForAllTasks",
                            Qt::QueuedConnection);
  foreach (const QString &host, _consumedResources.keys()) {
    const QHash<QString,qint64> &hostConsumedResources
        = _consumedResources[host];
    QHash<QString,qint64> hostAvailableResources
        = newConfig.hosts().value(host).resources();
    foreach (const QString &kind, hostConsumedResources.keys())
      hostAvailableResources.insert(kind, hostAvailableResources.value(kind)
                                    - hostConsumedResources.value(kind));
    emit hostsResourcesAvailabilityChanged(host, hostAvailableResources);
  }
  reevaluateQueuedRequests();
  // inspect queued requests to replace Task objects or remove request
  for (int i = 0; i < _queuedRequests.size(); ++i) {
    TaskInstance &r = _queuedRequests[i];
    QString taskId = r.task().id();
    Task t = newConfig.tasks().value(taskId);
    if (t.isNull()) {
      Log::warning(taskId, r.idAsLong())
          << "canceling queued task while reloading configuration because this "
             "task no longer exists: '" << taskId << "'";
      r.setReturnCode(-1);
      r.setSuccess(false);
      r.setEndDatetime();
      // LATER maybe these signals should be emited asynchronously
      emit itemChanged(r, r, QStringLiteral("taskinstance"));
      _queuedRequests.removeAt(i--);
    } else {
      Log::info(taskId, r.idAsLong())
          << "replacing task definition in queued request while reloading "
             "configuration";
      r.setTask(t);
    }
  }
  _configdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  if (_firstConfigurationLoad) {
    _firstConfigurationLoad = false;
    Log::info() << "starting scheduler";
    foreach(EventSubscription sub, newConfig.onschedulerstart())
      sub.triggerActions();
  }
  foreach(EventSubscription sub, newConfig.onconfigload())
    sub.triggerActions();
}

void Scheduler::reloadAccessControlConfig() {
  config().accessControlConfig().applyConfiguration(
        _authenticator, _usersDatabase, _accessControlFilesWatcher);
}

QList<TaskInstance> Scheduler::syncRequestTask(
    QString taskId, ParamSet paramsOverriding, bool force,
    TaskInstance callerTask) {
  if (this->thread() == QThread::currentThread())
    return doRequestTask(taskId, paramsOverriding, force, callerTask);
  QList<TaskInstance> requests;
  QMetaObject::invokeMethod(this, "doRequestTask",
                            Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(QList<TaskInstance>, requests),
                            Q_ARG(QString, taskId),
                            Q_ARG(ParamSet, paramsOverriding),
                            Q_ARG(bool, force),
                            Q_ARG(TaskInstance, callerTask));
  return requests;
}

void Scheduler::asyncRequestTask(const QString taskId,
                                 ParamSet paramsOverriding,
                                 bool force, TaskInstance callerTask) {
  QMetaObject::invokeMethod(this, "doRequestTask", Qt::QueuedConnection,
                            Q_ARG(QString, taskId),
                            Q_ARG(ParamSet, paramsOverriding),
                            Q_ARG(bool, force),
                            Q_ARG(TaskInstance, callerTask));
}

QList<TaskInstance> Scheduler::doRequestTask(
    QString taskId, ParamSet overridingParams, bool force,
    TaskInstance callerTask) {
  Task task = config().tasks().value(taskId);
  Cluster cluster = config().clusters().value(task.target());
  if (task.isNull()) {
    Log::error() << "requested task not found: " << taskId << overridingParams
                 << force;
    return QList<TaskInstance>();
  }
  if (!task.enabled()) {
    Log::info(taskId) << "ignoring request since task is disabled: " << taskId;
    return QList<TaskInstance>();
  }
  bool fieldsValidated(true);
  foreach (RequestFormField field, task.requestFormFields()) {
    QString name(field.id());
    if (overridingParams.contains(name)) {
      QString value(overridingParams.value(name));
      if (!field.validate(value)) {
        Log::error() << "task " << taskId << " requested with an invalid "
                        "parameter override: '" << name << "'' set to '"
                     << value << "' whereas format is '" << field.format()
                     << "'";
        fieldsValidated = false;
      }
    }
  }
  if (!fieldsValidated)
    return QList<TaskInstance>();
  QList<TaskInstance> requests;
  TaskInstance workflowTaskInstance
      = callerTask.task().mean() == Task::Workflow ? callerTask
                                                   : TaskInstance();
  if (cluster.balancing() == Cluster::Each) {
    qint64 groupId = 0;
    foreach (Host host, cluster.hosts()) {
      TaskInstance request(task, groupId, force, workflowTaskInstance,
                           overridingParams);
      if (!groupId)
        groupId = request.groupId();
      request.setTarget(host);
      request = enqueueRequest(request, overridingParams);
      if (!request.isNull())
        requests.append(request);
    }
  } else {
    TaskInstance request(task, force, workflowTaskInstance, overridingParams);
    request = enqueueRequest(request, overridingParams);
    if (!request.isNull())
      requests.append(request);
  }
  if (!requests.isEmpty()) {
    reevaluateQueuedRequests();
  }
  return requests;
}

TaskInstance Scheduler::enqueueRequest(
    TaskInstance request, ParamSet paramsOverriding) {
  Task task(request.task());
  QString taskId(task.id());
  foreach (RequestFormField field, task.requestFormFields()) {
    QString name(field.id());
    if (paramsOverriding.contains(name)) {
      QString value(paramsOverriding.value(name));
      field.apply(value, &request);
    }
  }
  if (!request.force() && (!task.enabled()
      || task.discardAliasesOnStart() != Task::DiscardNone)) {
    // avoid stacking disabled task requests by canceling older ones
    for (int i = 0; i < _queuedRequests.size(); ++i) {
      const TaskInstance &r2 = _queuedRequests[i];
      if (taskId == r2.task().id() && request.groupId() != r2.groupId()) {
        Log::info(taskId, r2.idAsLong())
            << "canceling task because another instance of the same task "
               "is queued"
            << (!task.enabled() ? " and the task is disabled" : "") << ": "
            << taskId << "/" << request.id();
        r2.setReturnCode(-1);
        r2.setSuccess(false);
        r2.setEndDatetime();
        emit itemChanged(r2, r2, QStringLiteral("taskinstance"));
        _queuedRequests.removeAt(i--);
      }
    }
  }
  if (_queuedRequests.size() >= config().maxqueuedrequests()) {
    Log::error(taskId, request.idAsLong())
        << "cannot queue task because maxqueuedrequests is already reached ("
        << config().maxqueuedrequests() << ")";
    _alerter->raiseAlert("scheduler.maxqueuedrequests.reached");
    return TaskInstance();
  }
  _alerter->cancelAlert("scheduler.maxqueuedrequests.reached");
  Log::debug(taskId, request.idAsLong())
      << "queuing task " << taskId << "/" << request.id() << " "
      << paramsOverriding << " with request group id " << request.groupId();
  // note: a request must always be queued even if the task can be started
  // immediately, to avoid the new tasks being started before queued ones
  _queuedRequests.append(request);
  if (_queuedRequests.size() > _queuedTasksHwm)
    _queuedTasksHwm = _queuedRequests.size();
  emit itemChanged(request, request, QStringLiteral("taskinstance"));
  return request;
}

TaskInstance Scheduler::cancelRequest(quint64 id) {
  if (this->thread() == QThread::currentThread())
    return doCancelRequest(id);
  TaskInstance request;
  QMetaObject::invokeMethod(this, "doCancelRequest",
                            Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(TaskInstance, request),
                            Q_ARG(quint64, id));
  return request;
}

TaskInstance Scheduler::doCancelRequest(quint64 id) {
  for (int i = 0; i < _queuedRequests.size(); ++i) {
    TaskInstance r2 = _queuedRequests[i];
    if (id == r2.idAsLong()) {
      QString taskId(r2.task().id());
      Log::info(taskId, id) << "canceling task as requested";
      r2.setReturnCode(-1);
      r2.setSuccess(false);
      r2.setEndDatetime();
      emit itemChanged(r2, r2, QStringLiteral("taskinstance"));
      _queuedRequests.removeAt(i);
      if (r2.task().instancesCount() < r2.task().maxInstances())
        _alerter->cancelAlert("task.maxinstancesreached."+taskId);
      return r2;
    }
  }
  Log::warning() << "cannot cancel task request because it is not (or no "
                    "longer) in requests queue";
  return TaskInstance();
}

TaskInstance Scheduler::abortTask(quint64 id) {
  if (this->thread() == QThread::currentThread())
    return doAbortTask(id);
  TaskInstance taskInstance;
  QMetaObject::invokeMethod(this, "doAbortTask",
                            Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(TaskInstance, taskInstance),
                            Q_ARG(quint64, id));
  return taskInstance;
}

TaskInstance Scheduler::doAbortTask(quint64 id) {
  QList<TaskInstance> tasks = _runningTasks.keys();
  for (int i = 0; i < tasks.size(); ++i) {
    TaskInstance r2 = tasks[i];
    if (id == r2.idAsLong()) {
      QString taskId(r2.task().id());
      Executor *executor = _runningTasks.value(r2);
      if (executor) {
        Log::warning(taskId, id) << "aborting task as requested";
        // TODO should return TaskInstance() if executor cannot actually abort
        executor->abort();
        return r2;
      }
    }
  }
  Log::warning() << "cannot abort task because it is not in running tasks list";
  return TaskInstance();
}

void Scheduler::checkTriggersForTask(QVariant taskId) {
  //Log::debug() << "Scheduler::checkTriggersForTask " << taskId;
  Task task = config().tasks().value(taskId.toString());
  foreach (const CronTrigger trigger, task.cronTriggers())
    checkTrigger(trigger, task, taskId.toString());
}

void Scheduler::checkTriggersForAllTasks() {
  //Log::debug() << "Scheduler::checkTriggersForAllTasks ";
  QList<Task> tasksWithoutTimeTrigger;
  foreach (Task task, config().tasks().values()) {
    QString taskId = task.id();
    foreach (const CronTrigger trigger, task.cronTriggers())
      checkTrigger(trigger, task, taskId);
    if (task.cronTriggers().isEmpty()) {
      task.setNextScheduledExecution(QDateTime());
      tasksWithoutTimeTrigger.append(task);
    }
  }
  // LATER if this is usefull only to remove next exec time when reloading config w/o time trigger, this should be called in reloadConfig
  foreach (const Task task, tasksWithoutTimeTrigger)
    emit itemChanged(task, task, QStringLiteral("task"));
}

bool Scheduler::checkTrigger(CronTrigger trigger, Task task, QString taskId) {
  //Log::debug() << "Scheduler::checkTrigger " << trigger.cronExpression()
  //             << " " << taskId;
  QDateTime now(QDateTime::currentDateTime());
  QDateTime next = trigger.nextTriggering();
  bool fired = false;
  if (next <= now) {
    // requestTask if trigger reached
    ParamSet overridingParams;
    foreach (QString key, trigger.overridingParams().keys())
      overridingParams
          .setValue(key, config().globalParams()
                    .value(trigger.overridingParams().rawValue(key)));
    QList<TaskInstance> requests = syncRequestTask(taskId, overridingParams);
    if (!requests.isEmpty())
      foreach (TaskInstance request, requests)
        Log::info(taskId, request.idAsLong())
            << "cron trigger " << trigger.humanReadableExpression()
            << " triggered task " << taskId;
    else
      Log::debug(taskId) << "cron trigger " << trigger.humanReadableExpression()
                       << " failed to trigger task " << taskId;
    trigger.setLastTriggered(now);
    next = trigger.nextTriggering();
    fired = true;
  } else {
    QDateTime taskNext = task.nextScheduledExecution();
    if (taskNext.isValid() && taskNext <= next && taskNext > now) {
      //Log::debug() << "Scheduler::checkTrigger don't trigger or plan new "
      //                "check for task " << taskId << " "
      //             << now.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
      //             << next.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
      //             << taskNext.toString("yyyy-MM-dd hh:mm:ss,zzz");
      return false; // don't plan new check if already planned
    }
  }
  if (next.isValid()) {
    // plan new check
    qint64 ms = now.msecsTo(next);
    //Log::debug() << "Scheduler::checkTrigger planning new check for task "
    //             << taskId << " "
    //             << now.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
    //             << next.toString("yyyy-MM-dd hh:mm:ss,zzz") << " " << ms;
    // LATER one timer per trigger, not a new timer each time
    TimerWithArguments::singleShot(ms < INT_MAX ? ms : INT_MAX,
                                   this, "checkTriggersForTask", taskId);
    task.setNextScheduledExecution(now.addMSecs(ms));
  } else {
    task.setNextScheduledExecution(QDateTime());
  }
  emit itemChanged(task, task, QStringLiteral("task"));
  return fired;
}

void Scheduler::postNotice(QString notice, ParamSet params) {
  if (notice.isNull()) {
    Log::warning() << "cannot post a null/empty notice";
    return;
  }
  QHash<QString,Task> tasks = config().tasks();
  if (params.parent().isNull())
    params.setParent(config().globalParams());
  Log::debug() << "posting notice ^" << notice << " with params " << params;
  foreach (Task task, tasks.values()) {
    foreach (NoticeTrigger trigger, task.noticeTriggers()) {
      // LATER implement regexp patterns for notice triggers
      if (trigger.expression() == notice) {
        Log::info() << "notice " << trigger.humanReadableExpression()
                    << " triggered task " << task.id();
        ParamSet overridingParams;
        foreach (QString key, trigger.overridingParams().keys())
          overridingParams
              .setValue(key, params
                        .value(trigger.overridingParams().rawValue(key)));
        QList<TaskInstance> requests
            = syncRequestTask(task.id(), overridingParams);
        if (!requests.isEmpty())
          foreach (TaskInstance request, requests)
            Log::info(task.id(), request.idAsLong())
                << "notice " << trigger.humanReadableExpression()
                << " triggered task " << task.id();
        else
          Log::debug(task.id())
              << "notice " << trigger.humanReadableExpression()
              << " failed to trigger task " << task.id();
      }
    }
  }
  emit noticePosted(notice, params);
  params.setValue("!notice", notice);
  foreach (EventSubscription sub, config().onnotice())
    sub.triggerActions(params);
}

void Scheduler::reevaluateQueuedRequests() {
  QCoreApplication::postEvent(this,
                              new QEvent(REEVALUATE_QUEUED_REQUEST_EVENT));
}

void Scheduler::customEvent(QEvent *event) {
  if (event->type() == REEVALUATE_QUEUED_REQUEST_EVENT) {
    QCoreApplication::removePostedEvents(this, REEVALUATE_QUEUED_REQUEST_EVENT);
    startQueuedTasks();
  } else {
    QObject::customEvent(event);
  }
}

void Scheduler::startQueuedTasks() {
  for (int i = 0; i < _queuedRequests.size(); ) {
    TaskInstance r = _queuedRequests[i];
    if (startQueuedTask(r)) {
      _queuedRequests.removeAt(i);
      if (r.task().discardAliasesOnStart() != Task::DiscardNone) {
        // remove other requests of same task
        QString taskId= r.task().id();
        for (int j = 0; j < _queuedRequests.size(); ++j ) {
          TaskInstance r2 = _queuedRequests[j];
          if (taskId == r2.task().id() && r.groupId() != r2.groupId()) {
            Log::info(taskId, r2.idAsLong())
                << "canceling task because another instance of the same task "
                   "is starting: " << taskId << "/" << r.id();
            r2.setReturnCode(-1);
            r2.setSuccess(false);
            r2.setEndDatetime();
            emit itemChanged(r2, r2, QStringLiteral("taskinstance"));
            if (j < i)
              --i;
            _queuedRequests.removeAt(j--);
          }
        }
        emit itemChanged(r.task(), r.task(), QStringLiteral("task"));
      }
    } else
      ++i;
  }
}

bool Scheduler::startQueuedTask(TaskInstance instance) {
  Task task(instance.task());
  QString taskId = task.id();
  Executor *executor = 0;
  if (!task.enabled())
    return false; // do not start disabled tasks
  if (_availableExecutors.isEmpty() && !instance.force()) {
    QString s;
    QDateTime now = QDateTime::currentDateTime();
    foreach (const TaskInstance &ti, _runningTasks.keys())
      s.append(ti.id()).append(' ')
          .append(ti.task().id()).append(" since ")
          .append(QString::number(ti.startDatetime().msecsTo(now)))
          .append(" ms; ");
    if (s.size() >= 2)
      s.chop(2);
    Log::info(taskId, instance.idAsLong()) << "cannot execute task '" << taskId
        << "' now because there are already too many tasks running "
           "(maxtotaltaskinstances reached) currently running tasks: " << s;
    _alerter->raiseAlert("scheduler.maxtotaltaskinstances.reached");
    return false;
  }
  _alerter->cancelAlert("scheduler.maxtotaltaskinstances.reached");
  if (instance.force())
    task.fetchAndAddInstancesCount(1);
  else if (task.fetchAndAddInstancesCount(1) >= task.maxInstances()) {
    task.fetchAndAddInstancesCount(-1);
    Log::warning() << "requested task '" << taskId << "' cannot be executed "
                      "because maxinstances is already reached ("
                   << task.maxInstances() << ")";
    _alerter->raiseAlert("task.maxinstancesreached."+taskId);
    return false;
  }
  _alerter->cancelAlert("task.maxinstancesreached."+taskId);
  QString target = instance.target().id();
  if (target.isEmpty()) {
    // use task target if not overiden at intance level
    target = task.target();
    if (target.isEmpty()) {
      // inherit target from workflow task if any
      Task workflowTask = config().tasks().value(task.workflowTaskId());
      target = workflowTask.target();
      // silently use "localhost" as target for means not needing a real target
      if (target.isEmpty()) {
        Task::Mean mean = task.mean();
        if (mean == Task::DoNothing || mean == Task::Workflow)
          target = "localhost";
      }
    }
  }
  QList<Host> hosts;
  Host host = config().hosts().value(target);
  if (host.isNull())
    hosts.append(config().clusters().value(target).hosts());
  else
    hosts.append(host);
  if (hosts.isEmpty()) {
    Log::error(taskId, instance.idAsLong()) << "cannot execute task '" << taskId
        << "' because its target '" << target << "' is invalid";
    _alerter->raiseAlert("task.failure."+taskId);
    instance.setReturnCode(-1);
    instance.setSuccess(false);
    instance.setEndDatetime();
    task.fetchAndAddInstancesCount(-1);
    task.setLastExecution(QDateTime::currentDateTime());
    task.setLastSuccessful(false);
    task.setLastReturnCode(-1);
    task.setLastTotalMillis(0);
    task.setLastTaskInstanceId(instance.idAsLong());
    emit itemChanged(instance, instance, QStringLiteral("taskinstance"));
    emit itemChanged(task, task, QStringLiteral("task"));
    return true;
  }
  // LATER implement other cluster balancing methods than "first"
  // LATER implement best effort resource check for forced requests
  QHash<QString,qint64> taskResources = task.resources();
  foreach (Host h, hosts) {
    if (!taskResources.isEmpty()) {
      QHash<QString,qint64> hostConsumedResources
          = _consumedResources.value(h.id());
      QHash<QString,qint64> hostAvailableResources
          = config().hosts().value(h.id()).resources();
      if (!instance.force()) {
        foreach (QString kind, taskResources.keys()) {
          qint64 alreadyConsumed = hostConsumedResources.value(kind);
          qint64 stillAvailable = hostAvailableResources.value(kind)-alreadyConsumed;
          qint64 needed = taskResources.value(kind);
          if (stillAvailable < needed ) {
            Log::info(taskId, instance.idAsLong())
                << "lacks resource '" << kind << "' on host '" << h.id()
                << "' for task '" << task.id() << "' (need " << needed
                << ", have " << stillAvailable << ")";
            goto nexthost;
          }
          hostConsumedResources.insert(kind, alreadyConsumed+needed);
          hostAvailableResources.insert(kind, stillAvailable-needed);
          Log::debug(taskId, instance.idAsLong())
              << "resource '" << kind << "' ok on host '" << h.id()
              << "' for task '" << taskId << "'";
        }
      }
      // a host with enough resources was found
      _consumedResources.insert(h.id(), hostConsumedResources);
      emit hostsResourcesAvailabilityChanged(h.id(), hostAvailableResources);
    }
    _alerter->cancelAlert("resource.exhausted."+taskId);
    instance.setTarget(h);
    instance.setStartDatetime();
    foreach (EventSubscription sub, config().onstart())
      sub.triggerActions(instance);
    task.triggerStartEvents(instance);
    executor = _availableExecutors.takeFirst();
    if (!executor) {
      // this should only happen with force == true
      executor = new Executor(_alerter);
      executor->setTemporary();
      connect(executor, &Executor::taskInstanceFinished,
              this, &Scheduler::taskInstanceFinishing);
      connect(executor, &Executor::taskInstanceStarted,
              this, &Scheduler::propagateTaskInstanceChange);
      connect(this, &Scheduler::noticePosted,
              executor, &Executor::noticePosted);
    }
    executor->execute(instance);
    ++_execCount;
    reevaluateQueuedRequests();
    _runningTasks.insert(instance, executor);
    if (_runningTasks.size() > _runningTasksHwm)
      _runningTasksHwm = _runningTasks.size();
    return true;
nexthost:;
  }
  // no host has enough resources to execute the task
  task.fetchAndAddInstancesCount(-1);
  Log::warning(taskId, instance.idAsLong())
      << "cannot execute task '" << taskId
      << "' now because there is not enough resources on target '"
      << target << "'";
  // LATER suffix alert with resources kind (one alert per exhausted kind)
  _alerter->raiseAlert("resource.exhausted."+taskId);
  return false;
}

void Scheduler::taskInstanceFinishing(TaskInstance instance,
                                      Executor *executor) {
  Task requestedTask = instance.task();
  QString taskId = requestedTask.id();
  // configured and requested tasks are different if config was reloaded
  // meanwhile
  Task configuredTask = config().tasks().value(taskId);
  configuredTask.fetchAndAddInstancesCount(-1);
  if (executor) {
    // deleteLater() because it lives in its own thread
    if (executor->isTemporary())
      executor->deleteLater();
    else
      _availableExecutors.append(executor);
  }
  _runningTasks.remove(instance);
  QHash<QString,qint64> taskResources = requestedTask.resources();
  QHash<QString,qint64> hostConsumedResources =
      _consumedResources.value(instance.target().id());
  QHash<QString,qint64> hostAvailableResources =
      config().hosts().value(instance.target().id()).resources();
  foreach (QString kind, taskResources.keys()) {
    qint64 qty = hostConsumedResources.value(kind)-taskResources.value(kind);
    hostConsumedResources.insert(kind, qty);
    hostAvailableResources.insert(kind, hostAvailableResources.value(kind)-qty);
  }
  _consumedResources.insert(instance.target().id(), hostConsumedResources);
  if (instance.success())
    _alerter->cancelAlert("task.failure."+taskId);
  else
    _alerter->raiseAlert("task.failure."+taskId);
  emit hostsResourcesAvailabilityChanged(instance.target().id(),
                                         hostAvailableResources);
  // LATER try resubmit if the host was not reachable (this can be usefull with clusters or when host become reachable again)
  if (!instance.startDatetime().isNull() && !instance.endDatetime().isNull()) {
    configuredTask.setLastExecution(instance.startDatetime());
    configuredTask.setLastSuccessful(instance.success());
    configuredTask.setLastReturnCode(instance.returnCode());
    configuredTask.setLastTotalMillis(instance.totalMillis());
    configuredTask.setLastTaskInstanceId(instance.idAsLong());
  }
  emit itemChanged(instance, instance, QStringLiteral("taskinstance"));
  emit itemChanged(configuredTask, configuredTask, QStringLiteral("task"));
  if (instance.success()) {
    foreach (EventSubscription sub, config().onsuccess())
      sub.triggerActions(instance);
    configuredTask.triggerSuccessEvents(instance);
  } else {
    foreach (EventSubscription sub, config().onfailure())
      sub.triggerActions(instance);
    configuredTask.triggerFailureEvents(instance);
  }
  // LATER implement onstatus events
  if (configuredTask.maxExpectedDuration() < LLONG_MAX) {
    if (configuredTask.maxExpectedDuration() < instance.totalMillis())
      _alerter->raiseAlert("task.toolong."+taskId);
    else
      _alerter->cancelAlert("task.toolong."+taskId);
  }
  if (configuredTask.minExpectedDuration() > 0) {
    if (configuredTask.minExpectedDuration() > instance.runningMillis())
      _alerter->raiseAlert("task.tooshort."+taskId);
    else
      _alerter->cancelAlert("task.tooshort."+taskId);
  }
  reevaluateQueuedRequests();
}

bool Scheduler::enableTask(QString taskId, bool enable) {
  Task t = config().tasks().value(taskId);
  //Log::fatal() << "enableTask " << taskId << " " << enable << " " << t.id();
  if (t.isNull())
    return false;
  t.setEnabled(enable);
  if (enable)
    reevaluateQueuedRequests();
  emit itemChanged(t, t, QStringLiteral("task"));
  return true;
}

void Scheduler::enableAllTasks(bool enable) {
  QList<Task> tasks(config().tasks().values());
  foreach (Task t, tasks) {
    t.setEnabled(enable);
    emit itemChanged(t, t, QStringLiteral("task"));
  }
  if (enable)
    reevaluateQueuedRequests();
}

void Scheduler::periodicChecks() {
  // detect queued or running tasks that exceeded their max expected duration
  QList<TaskInstance> currentInstances;
  currentInstances.append(_queuedRequests);
  currentInstances.append(_runningTasks.keys());
  foreach (const TaskInstance r, currentInstances) {
    const Task t(r.task());
    if (t.maxExpectedDuration() < r.liveTotalMillis())
      _alerter->raiseAlert("task.toolong."+t.id());
  }
  // restart timer for triggers if any was lost, this is never usefull apart
  // if current system time goes back (which btw should never occur on well
  // managed production servers, however it with naive sysops)
  checkTriggersForAllTasks();
}

void Scheduler::activateWorkflowTransition(TaskInstance workflowTaskInstance,
                                           WorkflowTransition transition,
                                           ParamSet eventContext) {
  QMetaObject::invokeMethod(this, "doActivateWorkflowTransition",
                            Qt::QueuedConnection,
                            Q_ARG(TaskInstance, workflowTaskInstance),
                            Q_ARG(WorkflowTransition, transition),
                            Q_ARG(ParamSet, eventContext));
}

void Scheduler::doActivateWorkflowTransition(TaskInstance workflowTaskInstance,
                                             WorkflowTransition transition,
                                             ParamSet eventContext) {
  Executor *executor = _runningTasks.value(workflowTaskInstance);
  if (executor)
    executor->activateWorkflowTransition(transition, eventContext);
  else
    Log::error() << "cannot activate workflow transition on non-running "
                    "workflow " << workflowTaskInstance.task().id()
                 << "/" << workflowTaskInstance.id() << ": "
                 << transition.id();
}

void Scheduler::propagateTaskInstanceChange(TaskInstance instance) {
  emit itemChanged(instance, nullItem, QStringLiteral("taskinstance"));
}
