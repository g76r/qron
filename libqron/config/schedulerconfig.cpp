/* Copyright 2014 Hallowyn and others.
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
#include "schedulerconfig.h"
#include "eventsubscription.h"
#include "action/action.h"
#include "log/log.h"
#include "log/filelogger.h"
#include "configutils.h"
#include "step.h"
#include <QCryptographicHash>
#include <QMutex>

#define DEFAULT_MAXQUEUEDREQUESTS 128

class RequestTaskActionLink {
public:
  Action _action;
  QString _eventType;
  QString _contextLabel;
  Task _contextTask;
  // FIXME clarify "contextLabel" and "contextTask" names, contextTask is useless since only its id is used (and contextLabel=id)
  RequestTaskActionLink(Action action, QString eventType, QString contextLabel,
                        Task contextTask)
    : _action(action), _eventType(eventType), _contextLabel(contextLabel),
      _contextTask(contextTask) { }
};

class SchedulerConfigData : public QSharedData{
public:
  ParamSet _globalParams, _setenv, _unsetenv;
  QHash<QString,TaskGroup> _tasksGroups;
  QHash<QString,Task> _tasks;
  QHash<QString,Cluster> _clusters;
  QHash<QString,Host> _hosts;
  QHash<QString,QHash<QString,qint64> > _hostResources; // FIXME remove (replace with Host._resources)
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QList<EventSubscription> _onlog, _onnotice, _onschedulerstart, _onconfigload;
  qint32 _maxtotaltaskinstances, _maxqueuedrequests;
  PfNode _accessControlNode;
  QHash<QString,Calendar> _namedCalendars;
  AlerterConfig _alerterConfig;
  AccessControlConfig _accessControlConfig;
  QList<LogFile> _logfiles;
  QList<Logger*> _loggers;
  mutable QMutex _mutex;
  mutable QString _hash;
  SchedulerConfigData() : _maxtotaltaskinstances(0), _maxqueuedrequests(0) { }
  SchedulerConfigData(PfNode root, Scheduler *scheduler, bool applyLogConfig);
  SchedulerConfigData(const SchedulerConfigData &other)
    : QSharedData(other),
      _globalParams(other._globalParams), _setenv(other._setenv),
      _unsetenv(other._unsetenv),
      _tasks(other._tasks), _clusters(other._clusters), _hosts(other._hosts),
      _hostResources(other._hostResources),
      _onstart(other._onstart), _onsuccess(other._onsuccess),
      _onfailure(other._onfailure), _onlog(other._onlog),
      _onnotice(other._onnotice), _onschedulerstart(other._onschedulerstart),
      _onconfigload(other._onconfigload),
      _maxtotaltaskinstances(other._maxtotaltaskinstances),
      _maxqueuedrequests(other._maxqueuedrequests),
      _accessControlNode(other._accessControlNode),
      _namedCalendars(other._namedCalendars),
      _alerterConfig(other._alerterConfig),
      _accessControlConfig(other._accessControlConfig),
      _logfiles(other._logfiles), _loggers(other._loggers) {
  }
};

SchedulerConfig::SchedulerConfig() {
}

SchedulerConfig::SchedulerConfig(const SchedulerConfig &other) : d(other.d) {
}

SchedulerConfig::SchedulerConfig(PfNode root, Scheduler *scheduler,
                                 bool applyLogConfig)
  : d (new SchedulerConfigData(root, scheduler, applyLogConfig)) {
}

static inline void recordTaskActionLinks(
    PfNode parentNode, QString childName,
    QList<RequestTaskActionLink> *requestTaskActionLinks,
    QString contextLabel, Task contextTask = Task()) {
  // FIXME clarify "contextLabel" and "contextTask" names
  QStringList ignoredChildren;
  ignoredChildren << "cron" << "notice";
  foreach (PfNode listnode, parentNode.childrenByName(childName)) {
    EventSubscription sub("", listnode, 0, ignoredChildren);
    foreach (Action a, sub.actions()) {
      if (a.actionType() == "requesttask") {
        requestTaskActionLinks
            ->append(RequestTaskActionLink(a, childName, contextLabel,
                                           contextTask));
      }
    }
  }
}

SchedulerConfigData::SchedulerConfigData(PfNode root, Scheduler *scheduler,
                                         bool applyLogConfig) {
  QList<RequestTaskActionLink> requestTaskActionLinks;
  QList<Logger*> loggers;
  QList<LogFile> logfiles;
  foreach (PfNode node, root.childrenByName("log")) {
    QString level = node.attribute("level");
    QString filename = node.attribute("file");
    bool buffered = !node.hasChild("unbuffered");
    if (level.isEmpty()) {
      Log::warning() << "invalid log level in configuration: "
                     << node.toPf();
    } else if (filename.isEmpty()) {
      Log::warning() << "invalid log filename in configuration: "
                     << node.toPf();
    } else {
      Log::debug() << "adding logger " << node.toPf();
      Log::Severity minimumSeverity = Log::severityFromString(level);
      loggers.append(new FileLogger(filename, minimumSeverity, buffered));
      logfiles.append(LogFile(filename, minimumSeverity, buffered));
    }
  }
  _logfiles = logfiles;
  _loggers = loggers;
  if (applyLogConfig)
    Log::replaceLoggersPlusConsole(Log::Fatal, loggers);
  _unsetenv.clear();
  _globalParams.clear();
  _setenv.clear();
  _setenv.setValue("TASKINSTANCEID", "%!taskinstanceid");
  _setenv.setValue("TASKID", "%!taskid");
  ConfigUtils::loadParamSet(root, &_globalParams, "param");
  ConfigUtils::loadParamSet(root, &_setenv, "setenv");
  ConfigUtils::loadFlagSet(root, &_unsetenv, "unsetenv");
  _namedCalendars.clear();
  foreach (PfNode node, root.childrenByName("calendar")) {
    QString name = node.contentAsString();
    Calendar calendar(node);
    if (name.isEmpty())
      Log::error() << "ignoring anonymous calendar: " << node.toPf();
    else if (calendar.isNull())
      Log::error() << "ignoring empty calendar: " << node.toPf();
    else
      _namedCalendars.insert(name, calendar);
  }
  _hosts.clear();
  _hostResources.clear();
  foreach (PfNode node, root.childrenByName("host")) {
    Host host(node);
    if (_hosts.contains(host.id())) {
      Log::error() << "ignoring duplicate host: " << host.id();
    } else {
      _hosts.insert(host.id(), host);
      _hostResources.insert(host.id(), host.resources());
    }
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
    if (cluster.hosts().isEmpty())
      Log::warning() << "cluster '" << cluster.id() << "' has no member";
    if (_clusters.contains(cluster.id()))
      Log::error() << "ignoring duplicate cluster: " << cluster.id();
    else if (_hosts.contains(cluster.id()))
      Log::error() << "ignoring cluster which id conflicts with a host: "
                   << cluster.id();
    else
      _clusters.insert(cluster.id(), cluster);
  }
  _tasksGroups.clear();
  foreach (PfNode node, root.childrenByName("taskgroup")) {
    TaskGroup taskGroup(node, _globalParams, _setenv, _unsetenv, scheduler);
    QString id = taskGroup.id();
    if (taskGroup.isNull() || id.isEmpty())
      Log::error() << "ignoring invalid taskgroup: " << node.toPf();
    else if (_tasksGroups.contains(id))
      Log::error() << "ignoring duplicate taskgroup: " << id;
    else {
      recordTaskActionLinks(node, "onstart", &requestTaskActionLinks,
                            taskGroup.id()+".*");
      recordTaskActionLinks(node, "onsuccess", &requestTaskActionLinks,
                            taskGroup.id()+".*");
      recordTaskActionLinks(node, "onfailure", &requestTaskActionLinks,
                            taskGroup.id()+".*");
      recordTaskActionLinks(node, "onfinish", &requestTaskActionLinks,
                            taskGroup.id()+".*");
      _tasksGroups.insert(taskGroup.id(), taskGroup);
    }
  }
  QHash<QString,Task> oldTasks = _tasks;
  _tasks.clear();
  foreach (PfNode node, root.childrenByName("task")) {
    QString taskGroupId = node.attribute("taskgroup");
    TaskGroup taskGroup = _tasksGroups.value(taskGroupId);
    Task task(node, scheduler, taskGroup, oldTasks, QString(), _namedCalendars);
    if (taskGroupId.isEmpty() || taskGroup.isNull()) {
      Log::error() << "ignoring task with invalid taskgroup: " << node.toPf();
      goto ignore_task;
    }
    if (task.isNull()) { // Task cstr detected an error
      Log::error() << "ignoring invalid task: " << node.toString();
      goto ignore_task;
    }
    if (_tasks.contains(task.id())) {
      Log::error() << "ignoring duplicate task " << task.id();
      goto ignore_task;
    }
    foreach (Step s, task.steps()) { // check for uniqueness of subtasks ids
      Task subtask = s.subtask();
      if (!subtask.isNull()) {
        QString subtaskId = subtask.id();
        if (_tasks.contains(subtaskId)) {
          Log::error() << "ignoring task " << task.id() << " since its subtask "
                       << subtaskId << " has a duplicate id";
          goto ignore_task;
        }
      }
    }
    /*if (!_hosts.contains(task.target()) && !_clusters.contains(task.target())) {
      Log::error() << "ignoring task " << task.id()
                   << " since its target is unknown: '"<< task.target() << "'";
      goto ignore_task;
    }*/
    _tasks.insert(task.id(), task);
    recordTaskActionLinks(node, "onstart", &requestTaskActionLinks, task.id(),
                          task);
    recordTaskActionLinks(node, "onsuccess", &requestTaskActionLinks,
                          task.id(), task);
    recordTaskActionLinks(node, "onfailure", &requestTaskActionLinks,
                          task.id(), task);
    recordTaskActionLinks(node, "onfinish", &requestTaskActionLinks,
                          task.id(), task); // FIXME rather use a QStringList than a QString for childname
    if (task.mean() == "workflow") {
      recordTaskActionLinks(node, "ontrigger", &requestTaskActionLinks,
                            task.id(), task); // FIXME check that this is consistent
      foreach(PfNode child, node.childrenByName("task")) {
        recordTaskActionLinks(node, "onstart", &requestTaskActionLinks,
                              task.id()+":"+child.contentAsString(),
                              task);
        recordTaskActionLinks(node, "onsuccess", &requestTaskActionLinks,
                              task.id()+":"+child.contentAsString(),
                              task);
        recordTaskActionLinks(node, "onfailure", &requestTaskActionLinks,
                              task.id()+":"+child.contentAsString(),
                              task);
        recordTaskActionLinks(node, "onfinish", &requestTaskActionLinks,
                              task.id()+":"+child.contentAsString(),
                              task);
      }
    }
    foreach (Step s, task.steps()) {
      Task subtask = s.subtask();
      if (!subtask.isNull())
        _tasks.insert(subtask.id(), subtask);
    }
ignore_task:;
  }
  foreach (QString taskId, _tasks.keys()) {
    Task &task = _tasks[taskId];
    QString supertaskId = task.supertaskId();
    if (_tasks.contains(supertaskId))
      task.setParentParams(_tasks[supertaskId].params());
  }
  int maxtotaltaskinstances = 0;
  foreach (PfNode node, root.childrenByName("maxtotaltaskinstances")) {
    int n = node.contentAsString().toInt(0, 0);
    if (n > 0) {
      if (maxtotaltaskinstances > 0)
        Log::error() << "overriding maxtotaltaskinstances "
                     << maxtotaltaskinstances << " with " << n;
      maxtotaltaskinstances = n;
    } else
      Log::error() << "ignoring maxtotaltaskinstances with incorrect "
                      "value: " << node.toPf();
  }
  if (maxtotaltaskinstances <= 0) {
    Log::debug() << "configured 16 task executors (default "
                    "maxtotaltaskinstances value)";
    maxtotaltaskinstances = 16;
  }
  _maxtotaltaskinstances = maxtotaltaskinstances;
  int maxqueuedrequests = 0;
  foreach (PfNode node, root.childrenByName("maxqueuedrequests")) {
    int n = node.contentAsString().toInt(0, 0);
    if (n > 0) {
      if (maxqueuedrequests > 0) {
        Log::warning() << "overriding maxqueuedrequests "
                       << maxqueuedrequests << " with " << n;
      }
      maxqueuedrequests = n;
    } else {
      Log::warning() << "ignoring maxqueuedrequests with incorrect "
                        "value: " << node.toPf();
    }
  }
  if (maxqueuedrequests <= 0)
    maxqueuedrequests = DEFAULT_MAXQUEUEDREQUESTS;
  _maxqueuedrequests = maxqueuedrequests;
  QList<PfNode> alerts = root.childrenByName("alerts");
  if (alerts.size() > 1)
    Log::error() << "multiple alerts configuration (ignoring all but last one)";
  if (alerts.size())
    _alerterConfig = AlerterConfig(alerts.last());
  _onstart.clear();
  ConfigUtils::loadEventSubscription(root, "onstart", "*", &_onstart,
                                     scheduler);
  recordTaskActionLinks(root, "onstart", &requestTaskActionLinks, "*");
  _onsuccess.clear();
  ConfigUtils::loadEventSubscription(root, "onsuccess", "*", &_onsuccess,
                                     scheduler);
  recordTaskActionLinks(root, "onsuccess", &requestTaskActionLinks, "*");
  ConfigUtils::loadEventSubscription(root, "onfinish", "*", &_onsuccess,
                                     scheduler);
  _onfailure.clear();
  ConfigUtils::loadEventSubscription(root, "onfailure", "*", &_onfailure,
                                     scheduler);
  recordTaskActionLinks(root, "onfailure", &requestTaskActionLinks, "*");
  ConfigUtils::loadEventSubscription(root, "onfinish", "*", &_onfailure,
                                     scheduler);
  recordTaskActionLinks(root, "onfinish", &requestTaskActionLinks, "*");
  _onlog.clear();
  ConfigUtils::loadEventSubscription(root, "onlog", "*", &_onlog, scheduler);
  recordTaskActionLinks(root, "onlog", &requestTaskActionLinks, "*");
  _onnotice.clear();
  ConfigUtils::loadEventSubscription(root, "onnotice", "*", &_onnotice,
                                     scheduler);
  recordTaskActionLinks(root, "onnotice", &requestTaskActionLinks, "*");
  _onschedulerstart.clear();
  ConfigUtils::loadEventSubscription(root, "onschedulerstart", "*",
                                     &_onschedulerstart, scheduler);
  recordTaskActionLinks(root, "onschedulerstart", &requestTaskActionLinks, "*");
  _onconfigload.clear();
  ConfigUtils::loadEventSubscription(root, "onconfigload", "*",
                                     &_onconfigload, scheduler);
  recordTaskActionLinks(root, "onconfigload", &requestTaskActionLinks, "*");
  // LATER onschedulershutdown
  foreach (const RequestTaskActionLink &link, requestTaskActionLinks) {
    QString targetName = link._action.targetName();
    QString taskId = link._contextTask.id();
    if (!link._contextTask.isNull() && !targetName.contains('.')) { // id to group
      targetName = taskId.left(taskId.lastIndexOf('.')+1)+targetName;
    }
    if (_tasks.contains(targetName)) {
      QString context(link._contextLabel);
      if (!link._contextTask.isNull())
        context = taskId;
      if (context.isEmpty())
        context = "*";
      _tasks[targetName].appendOtherTriggers("*"+link._eventType+"("+context+")");
      //Log::fatal() << "*** " << _tasks[targetName].otherTriggers() << " *** " << link._contextLabel << " *** " << taskId;
    } else {
      Log::debug() << "cannot translate event " << link._eventType
                   << " for task '" << targetName << "'";
    }
  }
}

SchedulerConfig::~SchedulerConfig() {
}

SchedulerConfig &SchedulerConfig::operator=(const SchedulerConfig &other) {
  d = other.d;
  return *this;
}

bool SchedulerConfig::isNull() const {
  return !d;
}

ParamSet SchedulerConfig::globalParams() const {
  return d ? d->_globalParams : ParamSet();
}

ParamSet SchedulerConfig::setenv() const {
  return d ? d->_setenv : ParamSet();
}

ParamSet SchedulerConfig::unsetenv() const {
  return d ? d->_unsetenv : ParamSet();
}

QHash<QString,TaskGroup> SchedulerConfig::tasksGroups() const {
  return d ? d->_tasksGroups : QHash<QString,TaskGroup>();
}

QHash<QString,Task> SchedulerConfig::tasks() const {
  return d ? d->_tasks : QHash<QString,Task>();
}

QHash<QString,Cluster> SchedulerConfig::clusters() const {
  return d ? d->_clusters : QHash<QString,Cluster>();
}

QHash<QString,Host> SchedulerConfig::hosts() const {
  return d ? d->_hosts : QHash<QString,Host>();
}

QHash<QString,QHash<QString,qint64> > SchedulerConfig::hostResources() const {
  return d ? d->_hostResources : QHash<QString,QHash<QString,qint64> >();
}

QHash<QString,Calendar> SchedulerConfig::namedCalendars() const {
  return d ? d->_namedCalendars : QHash<QString,Calendar>();
}

QList<EventSubscription> SchedulerConfig::onstart() const {
  return d ? d->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onsuccess() const {
  return d ? d->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onfailure() const {
  return d ? d->_onfailure : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onlog() const {
  return d ? d->_onlog : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onnotice() const {
  return d ? d->_onnotice : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onschedulerstart() const {
  return d ? d->_onschedulerstart : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::onconfigload() const {
  return d ? d->_onconfigload : QList<EventSubscription>();
}

QList<EventSubscription> SchedulerConfig::allEventsSubscriptions() const {
  return d ? d->_onstart + d->_onsuccess + d->_onfailure + d->_onlog
             + d->_onnotice + d->_onschedulerstart + d->_onconfigload
           : QList<EventSubscription>();
}

qint32 SchedulerConfig::maxtotaltaskinstances() const {
  return d ? d->_maxtotaltaskinstances : 0;
}

qint32 SchedulerConfig::maxqueuedrequests() const {
  return d ? d->_maxqueuedrequests : 0;
}

AlerterConfig SchedulerConfig::alerterConfig() const {
  return d ? d->_alerterConfig : AlerterConfig();
}

AccessControlConfig SchedulerConfig::accessControlConfig() const {
  return d ? d->_accessControlConfig : AccessControlConfig();
}

QList<LogFile> SchedulerConfig::logfiles() const {
  return d ? d ->_logfiles : QList<LogFile>();
}

QList<Logger*> SchedulerConfig::loggers() const {
  return d ? d ->_loggers : QList<Logger*>();
}

Cluster SchedulerConfig::renameCluster(QString oldName, QString newName) {
  if (d && d->_clusters.contains(oldName)) {
    Cluster cluster = d->_clusters[oldName];
    cluster.setId(newName);
    d->_clusters.remove(oldName);
    d->_clusters.insert(newName, cluster);
    return cluster;
  }
  return Cluster();
}

Task SchedulerConfig::updateTask(Task task) {
  if (d && d->_tasks.contains(task.id())) {
    d->_tasks.insert(task.id(), task);
    return task;
  }
  return Task();
}

QString SchedulerConfig::hash() const {
  if (!d)
    return QString();
  QMutexLocker locker(&d->_mutex);
  if (d->_hash.isNull()) {
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QBuffer buf;
    buf.open(QIODevice::ReadWrite);
    writeAsPf(&buf);
    buf.seek(0);
    hash.addData(&buf);
    d->_hash = hash.result().toHex();
  }
  return d->_hash;
}

qint64 SchedulerConfig::writeAsPf(QIODevice *device) const {
  PfNode node = toPfNode();
  if (node.isNull() || !device)
    return -1;
  QString s;
  s.append("#(pf (version 1.0))\n");
  s.append(QString::fromUtf8(
             node.toPf(PfOptions().setShouldIndent()
                       .setShouldWriteContentBeforeSubnodes())));
  qDebug() << "**** SchedulerConfig::writeAsPf" << s;
  return device->write(s.toUtf8());
}

PfNode SchedulerConfig::toPfNode() const {
  if (!d)
    return PfNode();
  PfNode node("qrontab");
  ParamSet configuredSetenv = d->_setenv;
  configuredSetenv.removeValue("TASKID");
  configuredSetenv.removeValue("TASKINSTANCEID");
  ConfigUtils::writeParamSet(&node, d->_globalParams, "param");
  ConfigUtils::writeParamSet(&node, configuredSetenv, "setenv");
  ConfigUtils::writeFlagSet(&node, d->_unsetenv, "unsetenv");
  ConfigUtils::writeEventSubscriptions(&node, d->_onstart);
  ConfigUtils::writeEventSubscriptions(&node, d->_onsuccess);
  ConfigUtils::writeEventSubscriptions(&node, d->_onfailure,
                                       QStringList("onfinish"));
  ConfigUtils::writeEventSubscriptions(&node, d->_onlog);
  ConfigUtils::writeEventSubscriptions(&node, d->_onnotice);
  ConfigUtils::writeEventSubscriptions(&node, d->_onschedulerstart);
  ConfigUtils::writeEventSubscriptions(&node, d->_onconfigload);
  QList<TaskGroup> taskGroups = d->_tasksGroups.values();
  qSort(taskGroups);
  foreach(const TaskGroup &taskGroup, taskGroups)
    node.appendChild(taskGroup.toPfNode());
  QList<Task> tasks = d->_tasks.values();
  qSort(tasks);
  foreach(const Task &task, tasks)
    node.appendChild(task.toPfNode());
  // FIXME hosts, clusters, incl. host resources
  return node;
}

/*

  QHash<QString,Cluster> _clusters;
  QHash<QString,Host> _hosts;
  QHash<QString,QHash<QString,qint64> > _hostResources;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QList<EventSubscription> _onlog, _onnotice, _onschedulerstart, _onconfigload;
  qint32 _maxtotaltaskinstances, _maxqueuedrequests;
  PfNode _accessControlNode;
  QHash<QString,Calendar> _namedCalendars;
  AlerterConfig _alerterConfig;
  AccessControlConfig _accessControlConfig;
  QList<LogFile> _logfiles;
  QList<Logger*> _loggers;


*/
