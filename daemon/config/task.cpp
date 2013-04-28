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
#include "task.h"
#include "taskgroup.h"
#include <QString>
#include <QHash>
#include "pf/pfnode.h"
#include "crontrigger.h"
#include "log/log.h"
#include <QAtomicInt>
#include "event/event.h"
#include <QWeakPointer>
#include "sched/scheduler.h"
#include "config/configutils.h"
#include "requestformfield.h"

class TaskData : public QSharedData {
public:
  QString _id, _label, _mean, _command, _target, _info;
  TaskGroup _group;
  ParamSet _params, _setenv;
  QSet<QString> _noticeTriggers, _unsetenv;
  QHash<QString,qint64> _resources;
  int _maxInstances;
  QList<CronTrigger> _cronTriggers;
  QList<QRegExp> _stderrFilters;
  QList<Event> _onstart, _onsuccess, _onfailure;
  QWeakPointer<Scheduler> _scheduler;
  long long _maxExpectedDuration, _minExpectedDuration;
  Task::DiscardAliasesOnStart _discardAliasesOnStart;
  QList<RequestFormField> _requestFormField;

private:
  // LATER using qint64 on 32 bits systems is not thread-safe but only crash-free
  mutable long long _lastExecution, _nextScheduledExecution;
  mutable QAtomicInt _instancesCount;
  mutable bool _enabled, _lastSuccessful;
  mutable int _lastReturnCode;

public:
  TaskData() : _maxExpectedDuration(LLONG_MAX), _minExpectedDuration(0),
    _discardAliasesOnStart(Task::DiscardAll),
    _lastExecution(LLONG_MIN), _nextScheduledExecution(LLONG_MIN),
    _enabled(true), _lastSuccessful(true), _lastReturnCode(-1) { }
  TaskData(const TaskData &other) : QSharedData(), _id(other._id),
    _label(other._label), _mean(other._mean), _command(other._command),
    _target(other._target), _info(other._info),
    _group(other._group), _params(other._params), _setenv(other._setenv),
    _noticeTriggers(other._noticeTriggers), _unsetenv(other._unsetenv),
    _resources(other._resources),
    _maxInstances(other._maxInstances), _cronTriggers(other._cronTriggers),
    _stderrFilters(other._stderrFilters),
    _maxExpectedDuration(other._maxExpectedDuration),
    _minExpectedDuration(other._minExpectedDuration),
    _discardAliasesOnStart(other._discardAliasesOnStart),
    _lastExecution(other._lastExecution),
    _nextScheduledExecution(other._nextScheduledExecution),
    _instancesCount(other._instancesCount), _enabled(other._enabled),
    _lastSuccessful(other._lastSuccessful), _lastReturnCode(0) { }
  QDateTime lastExecution() const {
    return _lastExecution == LLONG_MIN
        ? QDateTime() : QDateTime::fromMSecsSinceEpoch(_lastExecution); }
  void setLastExecution(QDateTime timestamp) const {
    _lastExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN ; }
  QDateTime nextScheduledExecution() const {
    return _nextScheduledExecution == LLONG_MIN
        ? QDateTime()
        : QDateTime::fromMSecsSinceEpoch(_nextScheduledExecution); }
  void setNextScheduledExecution(QDateTime timestamp) const {
    _nextScheduledExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN ; }
  int instancesCount() const { return _instancesCount; }
  int fetchAndAddInstancesCount(int valueToAdd) const {
    return _instancesCount.fetchAndAddOrdered(valueToAdd); }
  int fetchAndStoreInstancesCount(int value) const {
    return _instancesCount.fetchAndStoreOrdered(value); }
  bool enabled() const { return _enabled; }
  void setEnabled(bool enabled) const { _enabled = enabled; }
  bool lastSuccessful() const { return _lastSuccessful; }
  void setLastSuccessful(bool successful) const {
    _lastSuccessful = successful; }
  int lastReturnCode() const { return _lastReturnCode; }
  void setLastReturnCode(int code) const {
    _lastReturnCode = code; }
};

Task::Task() {
}

Task::Task(const Task &other) : d(other.d) {
}

Task::Task(PfNode node, Scheduler *scheduler, const Task oldTask) {
  TaskData *td = new TaskData;
  td->_scheduler = scheduler;
  td->_id = node.attribute("id"); // LATER check uniqueness
  td->_label = node.attribute("label", td->_id);
  td->_mean = node.attribute("mean"); // LATER check validity
  td->_command = node.attribute("command");
  td->_target = node.attribute("target");
  if (td->_target.isEmpty()
      && (td->_mean == "local" || td->_mean == "donothing"))
    td->_target = "localhost";
  td->_info = node.stringChildrenByName("info").join(" ");
  td->_maxInstances = node.attribute("maxinstances", "1").toInt();
  if (td->_maxInstances <= 0) {
    td->_maxInstances = 1;
    Log::error() << "ignoring invalid task maxinstances " << node.toPf();
  }
  if (node.hasChild("disabled"))
    td->setEnabled(false);
  double f = node.doubleAttribute("maxexpectedduration", -1);
  td->_maxExpectedDuration = f < 0 ? LLONG_MAX : f*1000;
  f = node.doubleAttribute("minexpectedduration", -1);
  td->_minExpectedDuration = f < 0 ? 0 : f*1000;
  ConfigUtils::loadParamSet(node, td->_params);
  ConfigUtils::loadSetenv(node, td->_setenv);
  ConfigUtils::loadUnsetenv(node, td->_unsetenv);
  QHash<QString,CronTrigger> oldCronTriggers;
  // copy mutable fields from old task and build old cron triggers dictionary
  if (oldTask.d) {
    foreach (const CronTrigger ct, oldTask.d->_cronTriggers)
      oldCronTriggers.insert(ct.canonicalCronExpression(), ct);
    td->setLastExecution(oldTask.lastExecution());
    td->setNextScheduledExecution(oldTask.nextScheduledExecution());
    td->fetchAndStoreInstancesCount(oldTask.instancesCount());
    td->setLastSuccessful(oldTask.lastSuccessful());
    td->setLastReturnCode(oldTask.lastReturnCode());
    td->setEnabled(td->enabled() && oldTask.enabled());
  }
  // LATER load cron triggers last exec timestamp from on-disk log
  foreach (PfNode child, node.childrenByName("trigger")) {
    foreach (PfNode grandchild, child.children()) {
      QString content = grandchild.contentAsString();
      QString triggerType = grandchild.name();
      if (triggerType == "notice") {
        if (!content.isEmpty()) {
          td->_noticeTriggers.insert(content);
          //Log::debug() << "configured notice trigger '" << content
          //             << "' on task '" << td->_id << "'";
        } else
          Log::error() << "ignoring empty notice trigger on task '" << td->_id
                       << "' in configuration";
      } else if (triggerType == "cron") {
          CronTrigger trigger(content);
          if (trigger.isValid()) {
            CronTrigger oldTrigger =
                oldCronTriggers.value(trigger.canonicalCronExpression());
            if (oldTrigger.isValid())
              trigger.setLastTriggered(oldTrigger.lastTriggered());
            td->_cronTriggers.append(trigger);
            //Log::debug() << "configured cron trigger '" << content
            //             << "' on task '" << td->_id << "'";
          } else
            Log::error() << "ignoring invalid cron trigger '" << content
                         << "' parsed as '" << trigger.canonicalCronExpression()
                         << "' on task '" << td->_id;
          // LATER read misfire config
      } else {
        Log::error() << "ignoring unknown trigger type '" << triggerType
                     << "' on task '" << td->_id << "'";
      }
    }
  }
  foreach (PfNode child, node.childrenByName("onstart"))
    scheduler->loadEventListConfiguration(child, td->_onstart);
  foreach (PfNode child, node.childrenByName("onsuccess"))
    scheduler->loadEventListConfiguration(child, td->_onsuccess);
  foreach (PfNode child, node.childrenByName("onfailure"))
    scheduler->loadEventListConfiguration(child, td->_onfailure);
  foreach (PfNode child, node.childrenByName("onfinish")) {
    scheduler->loadEventListConfiguration(child, td->_onsuccess);
    scheduler->loadEventListConfiguration(child, td->_onfailure);
  }
  QListIterator<QPair<QString,qlonglong> > it(
        node.stringLongPairChildrenByName("resource"));
  while (it.hasNext()) {
    const QPair<QString,qlonglong> &p(it.next());
    if (p.second <= 0)
      Log::error() << "ignoring resource of kind " << p.first
                   << "with incorrect quantity in task " << node.toString();
    else
      td->_resources.insert(p.first, p.second);
  }
  QString doas = node.attribute("discardaliasesonstart", "all");
  td->_discardAliasesOnStart = discardAliasesOnStartFromString(doas);
  if (td->_discardAliasesOnStart == Task::DiscardUnknown) {
    td->_discardAliasesOnStart = Task::DiscardAll;
    Log::error() << "invalid discardaliasesonstart on task " << td->_id << ": '"
                 << doas << "'";
  }
  QList<PfNode> children = node.childrenByName("requestform");
  if (!children.isEmpty()) {
    if (children.size() > 1)
      Log::error() << "several requestform in task definition (ignoring all "
                      "but last one): " << node.toString();
    foreach (PfNode child, children.last().childrenByName("field")) {
      RequestFormField field(child);
      if (!field.isNull())
        td->_requestFormField.append(field);
    }
  }
  d = td;
}

Task::~Task() {
}

Task &Task::operator =(const Task &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool Task::operator==(const Task &other) {
  return (!d && !other.d) || (d && other.d && fqtn() == other.fqtn());
}

ParamSet Task::params() const {
  return d ? d->_params : ParamSet();
}

bool Task::isNull() const {
  return !d;
}

QSet<QString> Task::noticeTriggers() const {
  return d ? d->_noticeTriggers : QSet<QString>();
}

QString Task::id() const {
  return d ? d->_id : QString();
}

QString Task::fqtn() const {
  return d ? d->_group.id()+"."+d->_id : QString();
}

QString Task::label() const {
  return d ? d->_label : QString();
}

QString Task::mean() const {
  return d ? d->_mean : QString();
}

QString Task::command() const {
  return d ? d->_command : QString();
}

QString Task::target() const {
  return d ? d->_target : QString();
}

QString Task::info() const {
  return d ? d->_info : QString();
}

TaskGroup Task::taskGroup() const {
  return d ? d->_group : TaskGroup();
}

void Task::completeConfiguration(TaskGroup taskGroup) {
  if (!d)
    return;
  d->_group = taskGroup;
  d->_params.setParent(taskGroup.params());
  d->_setenv.setParent(taskGroup.setenv());
  d->_unsetenv |= taskGroup.unsetenv();
  QString filter = params().value("stderrfilter");
  if (!filter.isEmpty())
    d->_stderrFilters.append(QRegExp(filter));
}

QList<CronTrigger> Task::cronTriggers() const {
  return d ? d->_cronTriggers : QList<CronTrigger>();
}

QHash<QString, qint64> Task::resources() const {
  return d ? d->_resources : QHash<QString,qint64>();
}

QString Task::resourcesAsString() const {
  QString s;
  s.append("{ ");
  if (d)
    foreach(QString key, d->_resources.keys()) {
      s.append(key).append("=")
          .append(QString::number(d->_resources.value(key))).append(" ");
    }
  return s.append("}");
}

QString Task::triggersAsString() const {
  QString s;
  if (d) {
    foreach (CronTrigger t, d->_cronTriggers)
      s.append("(").append(t.cronExpression()).append(") ");
    foreach (QString t, d->_noticeTriggers)
      s.append("^").append(t).append(" ");
  }
  if (!s.isEmpty())
    s.chop(1); // remove last space
  return s;
}

QDateTime Task::lastExecution() const {
  return d ? d->lastExecution() : QDateTime();
}

QDateTime Task::nextScheduledExecution() const {
  return d ? d->nextScheduledExecution() : QDateTime();
}

void Task::setLastExecution(const QDateTime timestamp) const {
  if (d)
    d->setLastExecution(timestamp);
}

void Task::setNextScheduledExecution(const QDateTime timestamp) const {
  if (d)
    d->setNextScheduledExecution(timestamp);
}

int Task::maxInstances() const {
  return d ? d->_maxInstances : 0;
}

int Task::instancesCount() const {
  return d ? d->instancesCount() : 0;
}

int Task::fetchAndAddInstancesCount(int valueToAdd) const {
  return d ? d->fetchAndAddInstancesCount(valueToAdd) : 0;
}

const QList<QRegExp> Task::stderrFilters() const {
  return d ? d->_stderrFilters : QList<QRegExp>();
}

void Task::appendStderrFilter(QRegExp filter) {
  if (d)
    d->_stderrFilters.append(filter);
}

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.fqtn();
  return dbg.space();
}

void Task::triggerStartEvents(const ParamsProvider *context) const {
  if (d) {
    d->_group.triggerStartEvents(context);
    Scheduler::triggerEvents(d->_onstart, context);
  }
}

void Task::triggerSuccessEvents(const ParamsProvider *context) const {
  if (d) {
    d->_group.triggerSuccessEvents(context);
    Scheduler::triggerEvents(d->_onsuccess, context);
  }
}

void Task::triggerFailureEvents(const ParamsProvider *context) const {
  if (d) {
    d->_group.triggerFailureEvents(context);
    Scheduler::triggerEvents(d->_onfailure, context);
  }
}

const QList<Event> Task::onstartEvents() const {
  return d ? d->_onstart : QList<Event>();
}

const QList<Event> Task::onsuccessEvents() const {
  return d ? d->_onsuccess : QList<Event>();
}

const QList<Event> Task::onfailureEvents() const {
  return d ? d->_onfailure : QList<Event>();
}

bool Task::enabled() const {
  return d ? d->enabled() : false;
}
void Task::setEnabled(bool enabled) const {
  if (d)
    d->setEnabled(enabled);
}

bool Task::lastSuccessful() const {
  return d ? d->lastSuccessful() : false;
}

void Task::setLastSuccessful(bool successful) const {
  if (d)
    d->setLastSuccessful(successful);
}

int Task::lastReturnCode() const {
  return d ? d->lastReturnCode() : -1;
}

void Task::setLastReturnCode(int code) const {
  if (d)
    d->setLastReturnCode(code);
}

long long Task::maxExpectedDuration() const {
  return d ? d->_maxExpectedDuration : LLONG_MAX;
}
long long Task::minExpectedDuration() const {
  return d ? d->_minExpectedDuration : 0;
}

ParamSet Task::setenv() const {
  return d ? d->_setenv : ParamSet();
}

QSet<QString> Task::unsetenv() const {
  return d ? d->_unsetenv : QSet<QString>();
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStart() const {
  return d ? d->_discardAliasesOnStart : Task::DiscardNone;
}

QString Task::discardAliasesOnStartAsString(Task::DiscardAliasesOnStart v) {
  switch (v) {
  case Task::DiscardNone:
    return "none";
  case Task::DiscardAll:
    return "all";
  case Task::DiscardUnknown:
    ;
  }
  return "unknown"; // should never happen
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStartFromString(QString v) {
  if (v == "none")
    return Task::DiscardNone;
  if (v == "all")
    return Task::DiscardAll;
  return Task::DiscardUnknown;
}

QList<RequestFormField> Task::requestFormFields() const {
  return d ? d->_requestFormField : QList<RequestFormField>();
}
