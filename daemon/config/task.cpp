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
#include <QMap>
#include "pf/pfnode.h"
#include "crontrigger.h"
#include "log/log.h"
#include <QAtomicInt>
#include "event/event.h"
#include <QWeakPointer>
#include "sched/scheduler.h"

class TaskData : public QSharedData {
public:
  QString _id, _label, _mean, _command, _target;
  TaskGroup _group;
  ParamSet _params;
  QSet<QString> _noticeTriggers;
  QMap<QString,qint64> _resources;
  quint32 _maxInstances;
  QList<CronTrigger> _cronTriggers;
  QList<QRegExp> _stderrFilters;
  QList<Event> _onstart, _onsuccess, _onfailure;
  mutable QDateTime _lastExecution, _nextScheduledExecution;
  mutable QAtomicInt _instancesCount;
  QWeakPointer<Scheduler> _scheduler;

  TaskData() { }
  TaskData(const TaskData &other) : QSharedData(), _id(other._id),
    _label(other._label), _mean(other._mean), _command(other._command),
    _target(other._target), _group(other._group), _params(other._params),
    _noticeTriggers(other._noticeTriggers), _resources(other._resources),
    _maxInstances(other._maxInstances), _cronTriggers(other._cronTriggers),
    _stderrFilters(other._stderrFilters), _lastExecution(other._lastExecution),
    _nextScheduledExecution(other._nextScheduledExecution) { }
};

Task::Task() : d(new TaskData) {
}

Task::Task(const Task &other) : d(other.d) {
}

Task::Task(PfNode node, Scheduler *scheduler) {
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
  td->_maxInstances = node.attribute("maxinstances", "1").toInt();
  if (td->_maxInstances <= 0) {
    td->_maxInstances = 1;
    Log::warning() << "invalid task maxinstances " << node.toPf();
  }
  foreach (PfNode child, node.childrenByName("param")) {
    QString key = child.attribute("key");
    QString value = child.attribute("value");
    if (key.isNull() || value.isNull()) {
      Log::warning() << "invalid task param " << child.toPf();
    } else {
      Log::debug() << "configured task param " << key << "=" << value
                   << "for task '" << td->_id << "'";
      td->_params.setValue(key, value);
    }
  }
  foreach (PfNode child, node.childrenByName("trigger")) {
    QString notice = child.attribute("notice");
    if (!notice.isNull()) {
      td->_noticeTriggers.insert(notice);
      Log::debug() << "configured notice trigger '" << notice << "' on task '"
                   << td->_id << "'";
      continue;
    }
    QString cron = child.attribute("cron");
    if (!cron.isNull()) {
      CronTrigger trigger(cron);
      if (trigger.isValid()) {
        td->_cronTriggers.append(trigger);
        Log::debug() << "configured cron trigger '" << cron << "' on task '"
                     << td->_id << "'";
      } else
        Log::warning() << "ignoring invalid cron trigger '" << cron
                       << "' parsed as '" << trigger.canonicalCronExpression()
                       << "' on task '" << td->_id;
      continue;
      // LATER read misfire config
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
  foreach (PfNode child, node.childrenByName("resource")) {
    QString kind = child.attribute("kind");
    qint64 quantity = child.attribute("quantity").toLong(0, 0);
    if (kind.isNull())
      Log::warning() << "ignoring resource with no or empty kind in task"
                     << td->_id;
    else if (quantity <= 0)
      Log::warning() << "ignoring resource of kind " << kind
                     << "with incorrect quantity in task" << td->_id;
    else
      td->_resources.insert(kind, quantity);
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

ParamSet Task::params() const {
  return d->_params;
}

bool Task::isNull() const {
  return d->_id.isNull();
}

QSet<QString> Task::noticeTriggers() const {
  return d->_noticeTriggers;
}

QString Task::id() const {
  return d->_id;
}

QString Task::fqtn() const {
  return d->_group.id()+"."+d->_id;
}

QString Task::label() const {
  return d->_label;
}

QString Task::mean() const {
  return d->_mean;
}

QString Task::command() const {
  return d->_command;
}

QString Task::target() const {
  return d->_target;
}

TaskGroup Task::taskGroup() const {
  return d->_group;
}

void Task::completeConfiguration(TaskGroup taskGroup) {
  d->_group = taskGroup;
  d->_params.setParent(taskGroup.params());
  QString filter = params().value("stderrfilter");
  if (!filter.isEmpty())
    d->_stderrFilters.append(QRegExp(filter));
}

QList<CronTrigger> Task::cronTriggers() const {
  return d->_cronTriggers;
}

QMap<QString, qint64> Task::resources() const {
  return d->_resources;
}

QString Task::resourcesAsString() const {
  QString s;
  s.append("{ ");
  if (!isNull())
    foreach(QString key, d->_resources.keys()) {
      s.append(key).append("=")
          .append(QString::number(d->_resources.value(key))).append(" ");
    }
  return s.append("}");
}

QString Task::triggersAsString() const {
  QString s;
  if (!isNull()) {
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
  return d->_lastExecution;
}

QDateTime Task::nextScheduledExecution() const {
  return d->_nextScheduledExecution;
}

void Task::setLastExecution(const QDateTime timestamp) const {
  d->_lastExecution = timestamp;
}

void Task::setNextScheduledExecution(const QDateTime timestamp) const {
  d->_nextScheduledExecution = timestamp;
}

int Task::maxInstances() const {
  return d->_maxInstances;
}

int Task::instancesCount() const {
  return d->_instancesCount;
}

int Task::fetchAndAddInstancesCount(int valueToAdd) const {
  return d->_instancesCount.fetchAndAddOrdered(valueToAdd);
}

const QList<QRegExp> Task::stderrFilters() const {
  return d->_stderrFilters;
}

void Task::appendStderrFilter(QRegExp filter) {
  d->_stderrFilters.append(filter);
}

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.fqtn();
  return dbg.space();
}

void Task::triggerStartEvents(const ParamsProvider *context) const {
  d->_group.triggerStartEvents(context);
  Scheduler::triggerEvents(d->_onstart, context);
}

void Task::triggerSuccessEvents(const ParamsProvider *context) const {
  d->_group.triggerSuccessEvents(context);
  Scheduler::triggerEvents(d->_onsuccess, context);
}

void Task::triggerFailureEvents(const ParamsProvider *context) const {
  d->_group.triggerFailureEvents(context);
  Scheduler::triggerEvents(d->_onfailure, context);
}

const QList<Event> Task::onstartEvents() const {
  return d->_onstart;
}

const QList<Event> Task::onsuccessEvents() const {
  return d->_onsuccess;
}

const QList<Event> Task::onfailureEvents() const {
  return d->_onfailure;
}
