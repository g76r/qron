/* Copyright 2012-2014 Hallowyn and others.
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
#include "action/requesttaskaction.h"
#include <QThread>
#include "config/configutils.h"
#include "config/requestformfield.h"
#include "config/step.h"
#include "trigger/crontrigger.h"
#include "trigger/noticetrigger.h"

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler() : QObject(0), _thread(new QThread()),
  _alerter(new Alerter), _authenticator(new InMemoryAuthenticator(this)),
  _usersDatabase(new InMemoryUsersDatabase(this)),
  _firstConfigurationLoad(true),
  _startdate(QDateTime::currentDateTime().toMSecsSinceEpoch()),
  _configdate(LLONG_MIN), _execCount(0), _accessControlFilesWatcher(0) {
  _thread->setObjectName("SchedulerThread");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  qRegisterMetaType<Task>("Task");
  qRegisterMetaType<TaskInstance>("TaskInstance");
  qRegisterMetaType<QList<TaskInstance> >("QList<TaskInstance>");
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<LogFile>("LogFile");
  qRegisterMetaType<QPointer<Executor> >("QPointer<Executor>");
  qRegisterMetaType<QList<EventSubscription> >("QList<EventSubscription>");
  qRegisterMetaType<QHash<QString,Task> >("QHash<QString,Task>");
  qRegisterMetaType<QHash<QString,TaskGroup> >("QHash<QString,TaskGroup>");
  qRegisterMetaType<QHash<QString,QHash<QString,qint64> > >("QHash<QString,QHash<QString,qint64> >");
  qRegisterMetaType<QHash<QString,Cluster> >("QHash<QString,Cluster>");
  qRegisterMetaType<QHash<QString,Host> >("QHash<QString,Host>");
  qRegisterMetaType<QHash<QString,qint64> >("QHash<QString,qint64>");
  qRegisterMetaType<QList<LogFile> >("QList<LogFile>");
  qRegisterMetaType<QHash<QString,Calendar> >("QHash<QString,Calendar>");
  qRegisterMetaType<SchedulerConfig>("SchedulerConfig");
  qRegisterMetaType<AlerterConfig>("AlerterConfig");
  qRegisterMetaType<AccessControlConfig>("AccessControlConfig");
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodicChecks()));
  timer->start(60000);
  moveToThread(_thread);
}

Scheduler::~Scheduler() {
  Log::clearLoggers();
  //_alerter->deleteLater(); // FIXME delete alerter only when last executor is deleted
}

bool Scheduler::loadConfig(QIODevice *source) {
  if (!source->isOpen())
    if (!source->open(QIODevice::ReadOnly)) {
      QString errorString = source->errorString();
      Log::error() << "cannot read configuration: " << errorString;
      return false;
    }
  PfDomHandler pdh;
  PfParser pp(&pdh);
  pp.parse(source);
  if (pdh.errorOccured()) {
    QString errorString = pdh.errorString()+" at line "
        +QString::number(pdh.errorLine())
        +" column "+QString::number(pdh.errorColumn());
    Log::error() << "empty or invalid configuration: " << errorString;
    return false;
  }
  QList<PfNode> roots = pdh.roots();
  if (roots.size() == 0) {
    Log::error() << "configuration lacking root node";
  } else if (roots.size() == 1) {
    PfNode &root(roots.first());
    if (root.name() == "qrontab") {
      bool ok = false;
      if (QThread::currentThread() == thread())
        ok = loadConfig(root);
      else
        QMetaObject::invokeMethod(this, "loadConfig",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, ok),
                                  Q_ARG(PfNode, root));
      return ok;
    } else {
      Log::error() << "configuration root node is not \"qrontab\"";
    }
  } else {
    Log::error() << "configuration with more than one root node";
  }
  return false;
}

bool Scheduler::loadConfig(PfNode root) {
  SchedulerConfig config(root, this, true);
  emit logConfigurationChanged(config.logfiles());
  int executorsToAdd = config.maxtotaltaskinstances()
      - _config.maxtotaltaskinstances();
  if (executorsToAdd < 0) {
    if (-executorsToAdd > _availableExecutors.size()) {
      Log::warning() << "cannot set maxtotaltaskinstances down to "
                     << config.maxtotaltaskinstances()
                     << " because there are too "
                        "currently many busy executors, setting it to "
                     << config.maxtotaltaskinstances()
                        - (executorsToAdd - _availableExecutors.size())
                     << " instead";
      // TODO mark some executors as temporary to make them disapear later
      //maxtotaltaskinstances -= executorsToAdd - _availableExecutors.size();
      executorsToAdd = -_availableExecutors.size();
    }
    Log::debug() << "removing " << -executorsToAdd << " executors to reach "
                    "maxtotaltaskinstances of "
                 << config.maxtotaltaskinstances();
    for (int i = 0; i < -executorsToAdd; ++i)
      _availableExecutors.takeFirst()->deleteLater();
  } else if (executorsToAdd > 0) {
    Log::debug() << "adding " << executorsToAdd << " executors to reach "
                    "maxtotaltaskinstances of "
                 << config.maxtotaltaskinstances();
    for (int i = 0; i < executorsToAdd; ++i) {
      Executor *e = new Executor(_alerter);
      connect(e, SIGNAL(taskFinished(TaskInstance,QPointer<Executor>)),
              this, SLOT(taskFinishing(TaskInstance,QPointer<Executor>)));
      connect(e, SIGNAL(taskStarted(TaskInstance)),
              this, SIGNAL(taskStarted(TaskInstance)));
      connect(this, SIGNAL(noticePosted(QString,ParamSet)),
              e, SLOT(noticePosted(QString,ParamSet)));
      _availableExecutors.append(e);
    }
  } else {
    Log::debug() << "keep maxtotaltaskinstances of "
                 << config.maxtotaltaskinstances();
  }
  QList<PfNode> alertsChildren = root.childrenByName("alerts");
  if (alertsChildren.size() == 0)
    _alerter->loadConfig(PfNode());
  else {
    if (alertsChildren.size() > 1)
      Log::error() << "multiple 'alerts' configuration: ignoring all but first "
                      "one";
    _alerter->loadConfig(alertsChildren.first());
  }
  // TODO use AccessControlConfig instead
  _authenticator->clearUsers();
  _usersDatabase->clearUsers();
  bool accessControlEnabled = false;
  if (_accessControlFilesWatcher)
    _accessControlFilesWatcher->deleteLater();
  _accessControlFilesWatcher = new QFileSystemWatcher(this);
  connect(_accessControlFilesWatcher, SIGNAL(fileChanged(QString)),
          this, SLOT(reloadAccessControlConfig()));
  foreach (PfNode node, root.childrenByName("access-control")) {
    if (accessControlEnabled) {
      Log::error() << "ignoring multiple 'access-control' in configuration";
      break;
    }
    accessControlEnabled = true;
    _accessControlNode = node;
    reloadAccessControlConfig();
  }
  QMetaObject::invokeMethod(this, "checkTriggersForAllTasks",
                            Qt::QueuedConnection);
  _config = config;
  emit globalParamsChanged(_config.globalParams());
  emit globalSetenvChanged(_config.setenv());
  emit globalUnsetenvChanged(_config.unsetenv());
  emit accessControlConfigurationChanged(accessControlEnabled);
  emit configChanged(_config); // must be last signal
  reevaluateQueuedRequests();
  // inspect queued requests to replace Task objects or remove request
  for (int i = 0; i < _queuedRequests.size(); ++i) {
    TaskInstance &r = _queuedRequests[i];
    QString fqtn = r.task().fqtn();
    Task t = _config.tasks().value(fqtn);
    if (t.isNull()) {
      Log::warning(fqtn, r.id())
          << "canceling queued task while reloading configuration because this "
             "task no longer exists: '" << fqtn << "'";
      r.setReturnCode(-1);
      r.setSuccess(false);
      r.setEndDatetime();
      // LATER maybe these signals should be emited asynchronously
      emit taskFinished(r);
      emit taskChanged(r.task());
      _queuedRequests.removeAt(i--);
    } else {
      Log::info(fqtn, r.id())
          << "replacing task definition in queued request while reloading "
             "configuration";
      r.setTask(t);
    }
  }
  _configdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  if (_firstConfigurationLoad) {
    _firstConfigurationLoad = false;
    Log::info() << "starting scheduler";
    foreach(EventSubscription sub, _config.onschedulerstart())
      sub.triggerActions();
  }
  foreach(EventSubscription sub, _config.onconfigload())
    sub.triggerActions();
  return true;
}

void Scheduler::reloadAccessControlConfig() {
  //qDebug() << "reloadAccessControlConfig";
  foreach (PfNode node, _accessControlNode.childrenByName("user-file")) {
    QString path = node.contentAsString().trimmed();
    QString cipher = node.attribute("cipher", "password").trimmed().toLower();
    // implicitly, format is: login:crypted_password:role1,role2,rolen
    // later, other formats may be supported
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      Log::error() << "cannot open access control user file '" << path
                   << "': " << file.errorString();
      continue;
    }
    _accessControlFilesWatcher->addPath(path);
    QByteArray row;
    while (row = file.readLine(65535), !row.isNull()) {
      QString line = QString::fromUtf8(row).trimmed();
      if (line.size() == 0 || line.startsWith('#'))
        continue; // ignore empty lines and support # as a comment mark
      QStringList fields = line.split(':');
      if (fields.size() < 3) {
        Log::error() << "access control user file '" << path
                     << "' contains invalid line: " << line;
        continue;
      }
      QString id = fields[0].trimmed();
      QString password = fields[1].trimmed();
      QSet<QString> roles;
      foreach (const QString role,
               fields[2].trimmed().split(',', QString::SkipEmptyParts))
        roles.insert(role.trimmed());
      if (id.isEmpty() || password.isEmpty() || roles.isEmpty()) {
        Log::error() << "access control user file '" << path
                     << "' contains a line with empty mandatory fields: "
                     << line;
        continue;
      }
      InMemoryAuthenticator::Encoding encoding;
      if (cipher == "password" || cipher == "plain") {
        encoding = InMemoryAuthenticator::Plain;
      } else if (cipher == "md5hex") {
        encoding = InMemoryAuthenticator::Md5Hex;
      } else if (cipher == "md5" || cipher == "md5b64") {
        encoding = InMemoryAuthenticator::Md5Base64;
      } else if (cipher == "sha1" || cipher == "sha1hex") {
        encoding = InMemoryAuthenticator::Sha1Hex;
      } else if (cipher == "sha1b64") {
        encoding = InMemoryAuthenticator::Sha1Base64;
      } else if (cipher == "ldap") {
        encoding = InMemoryAuthenticator::OpenLdapStyle;
      } else {
        Log::error() << "access control user file '" << path
                     << "' with unsupported cipher type: '" << cipher << "'";
        break;
      }
      _authenticator->insertUser(id, password, encoding);
      _usersDatabase->insertUser(id, roles);
    }
    if (file.error() != QFileDevice::NoError)
      Log::error() << "error reading access control user file '" << path
                   << "': " << file.errorString();
  }
  foreach (PfNode node, _accessControlNode.childrenByName("user")) {
    QString id = node.attribute("id");
    QString password = node.attribute("password");
    QString md5 = node.attribute("md5");
    QString sha1 = node.attribute("sha1");
    QString ldap = node.attribute("ldap");
    int passwordCount = (password.isEmpty() ? 0 : 1)
        + (md5.isEmpty() ? 0 : 1)
        + (sha1.isEmpty() ? 0 : 1)
        + (ldap.isEmpty() ? 0 : 1);
    QStringList roles = node.stringListAttribute("roles");
    if (id.isEmpty())
      Log::error() << "found user with no id";
    if (passwordCount == 0)
      Log::error() << "user '" << id << "' with no password specification";
    if (passwordCount > 1)
      Log::error() << "user '" << id
                   << "' with severalpassword specifications";
    else {
      if (!password.isEmpty())
        _authenticator
            ->insertUser(id, password, InMemoryAuthenticator::Plain);
      else if (!md5.isEmpty())
        _authenticator->insertUser(id, md5, InMemoryAuthenticator::Md5Hex);
      else if (!sha1.isEmpty())
        _authenticator->insertUser(id, sha1, InMemoryAuthenticator::Sha1Hex);
      else if (!ldap.isEmpty())
        _authenticator
            ->insertUser(id, ldap, InMemoryAuthenticator::OpenLdapStyle);
      _usersDatabase->insertUser(id, roles.toSet());
      //Log::fatal() << "user: " << id << " : " << roles.toSet();
    }
  }
}

QList<TaskInstance> Scheduler::syncRequestTask(
    QString fqtn, ParamSet paramsOverriding, bool force,
    TaskInstance callerTask) {
  if (this->thread() == QThread::currentThread())
    return doRequestTask(fqtn, paramsOverriding, force, callerTask);
  QList<TaskInstance> requests;
  QMetaObject::invokeMethod(this, "doRequestTask",
                            Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(QList<TaskInstance>, requests),
                            Q_ARG(QString, fqtn),
                            Q_ARG(ParamSet, paramsOverriding),
                            Q_ARG(bool, force),
                            Q_ARG(TaskInstance, callerTask));
  return requests;
}

void Scheduler::asyncRequestTask(const QString fqtn, ParamSet paramsOverriding,
                                 bool force, TaskInstance callerTask) {
  QMetaObject::invokeMethod(this, "doRequestTask", Qt::QueuedConnection,
                            Q_ARG(QString, fqtn),
                            Q_ARG(ParamSet, paramsOverriding),
                            Q_ARG(bool, force),
                            Q_ARG(TaskInstance, callerTask));
}

QList<TaskInstance> Scheduler::doRequestTask(
    QString fqtn, ParamSet overridingParams, bool force,
    TaskInstance callerTask) {
  Task task = _config.tasks().value(fqtn);
  Cluster cluster = _config.clusters().value(task.target());
  if (task.isNull()) {
    Log::error() << "requested task not found: " << fqtn << overridingParams
                 << force;
    return QList<TaskInstance>();
  }
  if (!task.enabled()) {
    Log::info(fqtn) << "ignoring request since task is disabled: " << fqtn;
    return QList<TaskInstance>();
  }
  bool fieldsValidated(true);
  foreach (RequestFormField field, task.requestFormFields()) {
    QString name(field.param());
    if (overridingParams.contains(name)) {
      QString value(overridingParams.value(name));
      if (!field.validate(value)) {
        Log::error() << "task " << fqtn << " requested with an invalid "
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
  if (cluster.balancing() == "each") {
    qint64 groupId = 0;
    foreach (Host host, cluster.hosts()) {
      TaskInstance request(task, groupId, force, callerTask, overridingParams);
      if (!groupId)
        groupId = request.groupId();
      request.setTarget(host);
      request = enqueueRequest(request, overridingParams);
      if (!request.isNull())
        requests.append(request);
    }
  } else {
    TaskInstance request;
    request = enqueueRequest(
          TaskInstance(task, force, callerTask, overridingParams),
          overridingParams);
    if (!request.isNull())
      requests.append(request);
  }
  if (!requests.isEmpty()) {
    reevaluateQueuedRequests();
    emit taskChanged(task);
  }
  return requests;
}

TaskInstance Scheduler::enqueueRequest(
    TaskInstance request, ParamSet paramsOverriding) {
  Task task(request.task());
  QString fqtn(task.fqtn());
  foreach (RequestFormField field, task.requestFormFields()) {
    QString name(field.param());
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
      if (fqtn == r2.task().fqtn() && request.groupId() != r2.groupId()) {
        Log::info(fqtn, r2.id())
            << "canceling task because another instance of the same task "
               "is queued"
            << (!task.enabled() ? " and the task is disabled" : "") << ": "
            << fqtn << "/" << request.id();
        r2.setReturnCode(-1);
        r2.setSuccess(false);
        r2.setEndDatetime();
        emit taskFinished(r2);
        _queuedRequests.removeAt(i--);
      }
    }
  }
  if (_queuedRequests.size() >= _config.maxqueuedrequests()) {
    Log::error(fqtn, request.id())
        << "cannot queue task because maxqueuedrequests is already reached ("
        << _config.maxqueuedrequests() << ")";
    _alerter->raiseAlert("scheduler.maxqueuedrequests.reached");
    return TaskInstance();
  }
  _alerter->cancelAlert("scheduler.maxqueuedrequests.reached");
  Log::debug(fqtn, request.id())
      << "queuing task " << fqtn << "/" << request.id() << " "
      << paramsOverriding << " with request group id " << request.groupId();
  // note: a request must always be queued even if the task can be started
  // immediately, to avoid the new tasks being started before queued ones
  _queuedRequests.append(request);
  emit taskQueued(request);
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
    if (id == r2.id()) {
      QString fqtn(r2.task().fqtn());
      Log::info(fqtn, id) << "canceling task as requested";
      r2.setReturnCode(-1);
      r2.setSuccess(false);
      r2.setEndDatetime();
      emit taskFinished(r2);
      _queuedRequests.removeAt(i);
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
  TaskInstance request;
  QMetaObject::invokeMethod(this, "doAbortTask",
                            Qt::BlockingQueuedConnection,
                            Q_RETURN_ARG(TaskInstance, request),
                            Q_ARG(quint64, id));
  return request;
}

TaskInstance Scheduler::doAbortTask(quint64 id) {
  QList<TaskInstance> tasks = _runningTasks.keys();
  for (int i = 0; i < tasks.size(); ++i) {
    TaskInstance r2 = tasks[i];
    if (id == r2.id()) {
      QString fqtn(r2.task().fqtn());
      Executor *executor = _runningTasks.value(r2);
      if (executor) {
        Log::warning(fqtn, id) << "aborting task as requested";
        // TODO should return TaskRequest() if executor cannot actually abort
        executor->abort();
        return r2;
      }
    }
  }
  Log::warning() << "cannot abort task because it is not in running tasks list";
  return TaskInstance();
}

void Scheduler::checkTriggersForTask(QVariant fqtn) {
  //Log::debug() << "Scheduler::checkTriggersForTask " << fqtn;
  Task task = _config.tasks().value(fqtn.toString());
  foreach (const CronTrigger trigger, task.cronTriggers())
    checkTrigger(trigger, task, fqtn.toString());
}

void Scheduler::checkTriggersForAllTasks() {
  //Log::debug() << "Scheduler::checkTriggersForAllTasks ";
  QList<Task> tasksWithoutTimeTrigger;
  foreach (Task task, _config.tasks().values()) {
    QString fqtn = task.fqtn();
    foreach (const CronTrigger trigger, task.cronTriggers())
      checkTrigger(trigger, task, fqtn);
    if (task.cronTriggers().isEmpty()) {
      task.setNextScheduledExecution(QDateTime());
      tasksWithoutTimeTrigger.append(task);
    }
  }
  // LATER if this is usefull only to remove next exec time when reloading config w/o time trigger, this should be called in reloadConfig
  foreach (const Task task, tasksWithoutTimeTrigger)
    emit taskChanged(task);
}

bool Scheduler::checkTrigger(CronTrigger trigger, Task task, QString fqtn) {
  //Log::debug() << "Scheduler::checkTrigger " << trigger.cronExpression()
  //             << " " << fqtn;
  QDateTime now(QDateTime::currentDateTime());
  QDateTime next = trigger.nextTriggering();
  bool fired = false;
  if (next <= now) {
    // requestTask if trigger reached
    ParamSet overridingParams;
    foreach (QString key, trigger.overridingParams().keys())
      overridingParams
          .setValue(key, _config.globalParams()
                    .value(trigger.overridingParams().rawValue(key)));
    QList<TaskInstance> requests = syncRequestTask(fqtn, overridingParams);
    if (!requests.isEmpty())
      foreach (TaskInstance request, requests)
        Log::debug(fqtn, request.id())
            << "cron trigger " << trigger.humanReadableExpression()
            << " triggered task " << fqtn;
    else
      Log::debug(fqtn) << "cron trigger " << trigger.humanReadableExpression()
                       << " failed to trigger task " << fqtn;
    trigger.setLastTriggered(now);
    next = trigger.nextTriggering();
    fired = true;
  } else {
    QDateTime taskNext = task.nextScheduledExecution();
    if (taskNext.isValid() && taskNext <= next && taskNext > now) {
      //Log::debug() << "Scheduler::checkTrigger don't trigger or plan new "
      //                "check for task " << fqtn << " "
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
    //             << fqtn << " "
    //             << now.toString("yyyy-MM-dd hh:mm:ss,zzz") << " "
    //             << next.toString("yyyy-MM-dd hh:mm:ss,zzz") << " " << ms;
    // LATER one timer per trigger, not a new timer each time
    TimerWithArguments::singleShot(ms < INT_MAX ? ms : INT_MAX,
                                   this, "checkTriggersForTask", fqtn);
    task.setNextScheduledExecution(now.addMSecs(ms));
  } else {
    task.setNextScheduledExecution(QDateTime());
  }
  emit taskChanged(task);
  return fired;
}

void Scheduler::postNotice(QString notice, ParamSet params) {
  if (notice.isNull()) {
    Log::warning() << "cannot post a null/empty notice";
    return;
  }
  QHash<QString,Task> tasks = _config.tasks();
  if (params.parent().isNull())
    params.setParent(_config.globalParams());
  Log::debug() << "posting notice ^" << notice << " with params " << params;
  foreach (Task task, tasks.values()) {
    foreach (NoticeTrigger trigger, task.noticeTriggers()) {
      // LATER implement regexp patterns for notice triggers
      if (trigger.expression() == notice) {
        Log::debug() << "notice " << trigger.humanReadableExpression()
                     << " triggered task " << task.fqtn();
        ParamSet overridingParams;
        foreach (QString key, trigger.overridingParams().keys())
          overridingParams
              .setValue(key, params
                        .value(trigger.overridingParams().rawValue(key)));
        QList<TaskInstance> requests
            = syncRequestTask(task.fqtn(), overridingParams);
        if (!requests.isEmpty())
          foreach (TaskInstance request, requests)
            Log::debug(task.fqtn(), request.id())
                << "notice " << trigger.humanReadableExpression()
                << " triggered task " << task.fqtn();
        else
          Log::debug(task.fqtn())
              << "notice " << trigger.humanReadableExpression()
              << " failed to trigger task " << task.fqtn();
      }
    }
  }
  emit noticePosted(notice, params);
  params.setValue("!notice", notice);
  foreach (EventSubscription sub, _config.onnotice())
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
        QString fqtn(r.task().fqtn());
        for (int j = 0; j < _queuedRequests.size(); ++j ) {
          TaskInstance r2 = _queuedRequests[j];
          if (fqtn == r2.task().fqtn() && r.groupId() != r2.groupId()) {
            Log::info(fqtn, r2.id())
                << "canceling task because another instance of the same task "
                   "is starting: " << fqtn << "/" << r.id();
            r2.setReturnCode(-1);
            r2.setSuccess(false);
            r2.setEndDatetime();
            emit taskFinished(r2);
            if (j < i)
              --i;
            _queuedRequests.removeAt(j--);
          }
        }
        emit taskChanged(r.task());
      }
    } else
      ++i;
  }
}

bool Scheduler::startQueuedTask(TaskInstance instance) {
  Task task(instance.task());
  QString fqtn(task.fqtn());
  Executor *executor = 0;
  if (!task.enabled())
    return false; // do not start disabled tasks
  if (_availableExecutors.isEmpty() && !instance.force()) {
    Log::info(fqtn, instance.id()) << "cannot execute task '" << fqtn
        << "' now because there are already too many tasks running "
           "(maxtotaltaskinstances reached)";
    _alerter->raiseAlert("scheduler.maxtotaltaskinstances.reached");
    return false;
  }
  _alerter->cancelAlert("scheduler.maxtotaltaskinstances.reached");
  if (instance.force())
    task.fetchAndAddInstancesCount(1);
  else if (task.fetchAndAddInstancesCount(1) >= task.maxInstances()) {
    task.fetchAndAddInstancesCount(-1);
    Log::warning() << "requested task '" << fqtn << "' cannot be executed "
                      "because maxinstances is already reached ("
                   << task.maxInstances() << ")";
    _alerter->raiseAlert("task.maxinstancesreached."+fqtn);
    return false;
  }
  _alerter->cancelAlert("task.maxinstancesreached."+fqtn);
  QString target = instance.target().id();
  if (target.isEmpty())
    target = task.target();
  QList<Host> hosts;
  Host host = _config.hosts().value(target);
  if (host.isNull())
    hosts.append(_config.clusters().value(target).hosts());
  else
    hosts.append(host);
  if (hosts.isEmpty()) {
    Log::error(fqtn, instance.id()) << "cannot execute task '" << fqtn
        << "' because its target '" << target << "' is invalid";
    _alerter->raiseAlert("task.failure."+fqtn);
    instance.setReturnCode(-1);
    instance.setSuccess(false);
    instance.setEndDatetime();
    task.fetchAndAddInstancesCount(-1);
    emit taskFinished(instance);
    return true;
  }
  // LATER implement other cluster balancing methods than "first"
  // LATER implement best effort resource check for forced requests
  QHash<QString,qint64> taskResources = task.resources();
  foreach (Host h, hosts) {
    QHash<QString,qint64> hostResources = _config.hostResources().value(h.id());
    if (!instance.force()) {
      foreach (QString kind, taskResources.keys()) {
        if (hostResources.value(kind) < taskResources.value(kind)) {
          Log::info(fqtn, instance.id())
              << "lacks resource '" << kind << "' on host '" << h.id()
              << "' for task '" << task.id() << "' (need "
              << taskResources.value(kind) << ", have "
              << hostResources.value(kind) << ")";
          goto nexthost;
        }
        Log::debug(fqtn, instance.id())
            << "resource '" << kind << "' ok on host '" << h.id()
            << "' for task '" << fqtn << "'";
      }
    }
    // a host with enough resources was found
    foreach (QString kind, taskResources.keys())
      hostResources.insert(kind, hostResources.value(kind)
                            -taskResources.value(kind));
    _config.hostResources().insert(h.id(), hostResources);
    emit hostsResourcesAvailabilityChanged(h.id(), hostResources);
    _alerter->cancelAlert("resource.exhausted."+target);
    instance.setTarget(h);
    instance.setStartDatetime();
    foreach (EventSubscription sub, _config.onstart())
      sub.triggerActions(instance);
    task.triggerStartEvents(instance);
    executor = _availableExecutors.takeFirst();
    if (!executor) {
      // this should only happen with force == true
      executor = new Executor(_alerter);
      executor->setTemporary();
      connect(executor, SIGNAL(taskFinished(TaskInstance,QPointer<Executor>)),
              this, SLOT(taskFinishing(TaskInstance,QPointer<Executor>)));
      connect(executor, SIGNAL(taskStarted(TaskInstance)),
              this, SIGNAL(taskStarted(TaskInstance)));
      connect(this, SIGNAL(noticePosted(QString,ParamSet)),
              executor, SLOT(noticePosted(QString,ParamSet)));
    }
    executor->execute(instance);
    ++_execCount;
    reevaluateQueuedRequests();
    _runningTasks.insert(instance, executor);
    return true;
nexthost:;
  }
  // no host has enough resources to execute the task
  task.fetchAndAddInstancesCount(-1);
  Log::warning(fqtn, instance.id())
      << "cannot execute task '" << fqtn
      << "' now because there is not enough resources on target '"
      << target << "'";
  // LATER suffix alert with resources kind (one alert per exhausted kind)
  _alerter->raiseAlert("resource.exhausted."+target);
  return false;
}

void Scheduler::taskFinishing(TaskInstance instance,
                              QPointer<Executor> executor) {
  Task requestedTask(instance.task());
  QString fqtn(requestedTask.fqtn());
  // configured and requested tasks are different if config reloaded meanwhile
  Task configuredTask(_config.tasks().value(fqtn));
  configuredTask.fetchAndAddInstancesCount(-1);
  if (executor) {
    Executor *e = executor.data();
    if (e->isTemporary())
      e->deleteLater(); // deleteLater() because it lives in its own thread
    else
      _availableExecutors.append(e);
  }
  _runningTasks.remove(instance);
  QHash<QString,qint64> taskResources = requestedTask.resources();
  QHash<QString,qint64> hostResources =
      _config.hostResources().value(instance.target().id());
  foreach (QString kind, taskResources.keys())
    hostResources.insert(kind, hostResources.value(kind)
                          +taskResources.value(kind));
  _config.hostResources().insert(instance.target().id(), hostResources);
  if (instance.success())
    _alerter->cancelAlert("task.failure."+fqtn);
  else
    _alerter->raiseAlert("task.failure."+fqtn);
  emit hostsResourcesAvailabilityChanged(instance.target().id(), hostResources);
  // LATER try resubmit if the host was not reachable (this can be usefull with clusters or when host become reachable again)
  if (!instance.startDatetime().isNull() && !instance.endDatetime().isNull()) {
    configuredTask.setLastExecution(instance.startDatetime());
    configuredTask.setLastSuccessful(instance.success());
    configuredTask.setLastReturnCode(instance.returnCode());
    configuredTask.setLastTotalMillis(instance.totalMillis());
  }
  emit taskFinished(instance);
  emit taskChanged(configuredTask);
  if (instance.success()) {
    foreach (EventSubscription sub, _config.onsuccess())
      sub.triggerActions(instance);
    configuredTask.triggerSuccessEvents(instance);
  } else {
    foreach (EventSubscription sub, _config.onfailure())
      sub.triggerActions(instance);
    configuredTask.triggerFailureEvents(instance);
  }
  // LATER implement onstatus events
  if (configuredTask.maxExpectedDuration() < LLONG_MAX) {
    if (configuredTask.maxExpectedDuration() < instance.totalMillis())
      _alerter->raiseAlert("task.toolong."+fqtn);
    else
      _alerter->cancelAlert("task.toolong."+fqtn);
  }
  if (configuredTask.minExpectedDuration() > 0) {
    if (configuredTask.minExpectedDuration() > instance.runningMillis())
      _alerter->raiseAlert("task.tooshort."+fqtn);
    else
      _alerter->cancelAlert("task.tooshort."+fqtn);
  }
  reevaluateQueuedRequests();
}

bool Scheduler::enableTask(QString fqtn, bool enable) {
  Task t = _config.tasks().value(fqtn);
  //Log::fatal() << "enableTask " << fqtn << " " << enable << " " << t.id();
  if (t.isNull())
    return false;
  t.setEnabled(enable);
  if (enable)
    reevaluateQueuedRequests();
  emit taskChanged(t);
  return true;
}

void Scheduler::enableAllTasks(bool enable) {
  QList<Task> tasks(_config.tasks().values());
  foreach (Task t, tasks) {
    t.setEnabled(enable);
    emit taskChanged(t);
  }
  if (enable)
    reevaluateQueuedRequests();
}

bool Scheduler::taskExists(QString fqtn) {
  return _config.tasks().contains(fqtn);
}

Task Scheduler::task(QString fqtn) {
  return _config.tasks().value(fqtn);
}

void Scheduler::periodicChecks() {
  // detect queued or running tasks that exceeded their max expected duration
  QList<TaskInstance> currentInstances;
  currentInstances.append(_queuedRequests);
  currentInstances.append(_runningTasks.keys());
  foreach (const TaskInstance r, currentInstances) {
    const Task t(r.task());
    if (t.maxExpectedDuration() < r.liveTotalMillis())
      _alerter->raiseAlert("task.toolong."+t.fqtn());
  }
  // restart timer for triggers if any was lost, this is never usefull apart
  // if current system time goes back (which btw should never occur on well
  // managed production servers, however it with naive sysops)
  checkTriggersForAllTasks();
}

Calendar Scheduler::calendarByName(QString name) const {
  return _config.namedCalendars().value(name);
}

void Scheduler::activateWorkflowTransition(TaskInstance workflowTaskInstance,
                                           QString transitionId,
                                           ParamSet eventContext) {
  QMetaObject::invokeMethod(this, "doActivateWorkflowTransition",
                            Qt::QueuedConnection,
                            Q_ARG(TaskInstance, workflowTaskInstance),
                            Q_ARG(QString, transitionId),
                            Q_ARG(ParamSet, eventContext));
}

void Scheduler::doActivateWorkflowTransition(TaskInstance workflowTaskInstance,
                                             QString transitionId,
                                             ParamSet eventContext) {
  Executor *executor = _runningTasks.value(workflowTaskInstance);
  if (executor)
    executor->activateWorkflowTransition(transitionId, eventContext);
  else
    Log::error() << "cannot activate workflow transition on non-running "
                    "workflow " << workflowTaskInstance.task().fqtn()
                 << "/" << workflowTaskInstance.id() << ": " << transitionId;
}
