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
#include "taskinstance.h"
#include <QSharedData>
#include <QDateTime>
#include <QAtomicInt>

static QAtomicInt _sequence;

class TaskInstanceData : public QSharedData {
public:
  quint64 _id, _groupId;
  Task _task;
  ParamSet _params;
  QDateTime _submission;
  bool _force;
  QString _command;
  ParamSet _setenv;
  TaskInstance _supertask;
  // note: since QDateTime (as most Qt classes) is not thread-safe, it cannot
  // be used in a mutable QSharedData field as soon as the object embedding the
  // QSharedData is used by several thread at a time, hence the qint64
  mutable qint64 _start, _end;
  mutable bool _success;
  mutable int _returnCode;
  // note: Host is not thread-safe either, however setTarget() is not likely to
  // be called without at less one other reference of the Host object, therefore
  // ~Host() would never be called within setTarget() context which would make
  // it thread-safe.
  // There may be a possibility if configuration reload or ressource management
  // or any other Host change occurs. Therefore in addition setTarget() makes a
  // deep copy (through Host.detach()) of original object, so there are no race
  // conditions as soon as target() return value is never modified, which should
  // be the case forever.
  mutable Host _target;
  mutable bool _abortable;

  TaskInstanceData(Task task, ParamSet params, bool force,
                   TaskInstance supertask, quint64 groupId = 0)
    : _id(newId()), _groupId(groupId ? groupId : _id),
      _task(task), _params(params),
      _submission(QDateTime::currentDateTime()), _force(force),
      _command(task.command()), _setenv(task.setenv()), _supertask(supertask),
      _start(LLONG_MIN), _end(LLONG_MIN),
      _success(false), _returnCode(0), _abortable(false) { }
  TaskInstanceData() : _id(0), _groupId(0), _force(false),
      _start(LLONG_MIN), _end(LLONG_MIN), _success(false), _returnCode(0),
    _abortable(false) { }

private:
  static quint64 newId() {
    QDateTime now = QDateTime::currentDateTime();
    return now.date().year() * 100000000000000LL
        + now.date().month() * 1000000000000LL
        + now.date().day() * 10000000000LL
        + now.time().hour() * 100000000LL
        + now.time().minute() * 1000000LL
        + now.time().second() * 10000LL
        + _sequence.fetchAndAddOrdered(1)%10000;
  }
};

TaskInstance::TaskInstance() {
}

TaskInstance::TaskInstance(const TaskInstance &other)
  : ParamsProvider(), d(other.d) {
}

TaskInstance::TaskInstance(Task task, bool force, TaskInstance supertask)
  : d(new TaskInstanceData(task, task.params().createChild(), force,
                           supertask)) {
}

TaskInstance::TaskInstance(Task task, quint64 groupId,
                           bool force, TaskInstance supertask)
  : d(new TaskInstanceData(task, task.params().createChild(), force, supertask,
                           groupId)) {
}


TaskInstance::~TaskInstance() {
}

TaskInstance &TaskInstance::operator=(const TaskInstance &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool TaskInstance::operator==(const TaskInstance &other) const {
  return (!d && !other.d) || (d && other.d && d->_id == other.d->_id);
}

Task TaskInstance::task() const {
  return d ? d->_task : Task();
}

ParamSet TaskInstance::params() const {
  return d ? d->_params : ParamSet();
}

void TaskInstance::overrideParam(QString key, QString value) {
  if (d)
    d->_params.setValue(key, value);
}

quint64 TaskInstance::id() const {
  return d ? d->_id : 0;
}

quint64 TaskInstance::groupId() const {
  return d ? d->_groupId : 0;
}

QDateTime TaskInstance::submissionDatetime() const {
  return d ? d->_submission : QDateTime();
}


QDateTime TaskInstance::startDatetime() const {
  return d && d->_start != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(d->_start) : QDateTime();
}

void TaskInstance::setStartDatetime(QDateTime datetime) const {
  if (d)
    d->_start = datetime.isValid() ? datetime.toMSecsSinceEpoch() : LLONG_MIN;
}

QDateTime TaskInstance::endDatetime() const {
  return d && d->_end != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(d->_end) : QDateTime();
}

void TaskInstance::setEndDatetime(QDateTime datetime) const {
  if (d)
    d->_end = datetime.isValid() ? datetime.toMSecsSinceEpoch() : LLONG_MIN;
}

bool TaskInstance::success() const {
  return d ? d->_success : false;
}

void TaskInstance::setSuccess(bool success) const {
  if (d)
    d->_success = success;
}

int TaskInstance::returnCode() const {
  return d ? d->_returnCode : -1;
}

void TaskInstance::setReturnCode(int returnCode) const {
  if (d)
    d->_returnCode = returnCode;
}

Host TaskInstance::target() const {
  return d ? d->_target : Host();
}

void TaskInstance::setTarget(Host target) const {
  if (d) {
    target.detach();
    d->_target = target;
  }
}

QVariant TaskInstance::paramValue(QString key, QVariant defaultValue) const {
  //Log::fatal() << "TaskInstance::paramvalue " << key;
  if (!d)
    return defaultValue;
  if (key == "!taskid") {
    return task().id();
  } else if (key == "!fqtn") {
    return task().fqtn();
  } else if (key == "!taskgroupid") {
    return task().taskGroup().id();
  } else if (key == "!taskinstanceid") {
    return QString::number(id());
  } else if (key == "!supertaskinstanceid") {
    return QString::number(supertask().id());
  } else if (key == "!maintaskinstanceid") {
    return QString::number(supertask().isNull() ? id() : supertask().id());
  } else if (key == "!taskinstancegroupid") {
    return QString::number(groupId());
  } else if (key == "!runningms") {
    return QString::number(runningMillis());
  } else if (key == "!runnings") {
    return QString::number(runningMillis()/1000);
  } else if (key == "!queuedms") {
    return QString::number(queuedMillis());
  } else if (key == "!queueds") {
    return QString::number(queuedMillis()/1000);
  } else if (key == "!totalms") {
    return QString::number(queuedMillis()+runningMillis());
  } else if (key == "!totals") {
    return QString::number((queuedMillis()+runningMillis())/1000);
  } else if (key == "!returncode") {
    return QString::number(returnCode());
  } else if (key == "!status") {
    if (startDatetime().isNull())
      return "queued";
    if (endDatetime().isNull())
      return "running";
    return success() ? "success" : "failure";
  } else if (key == "!submissiondate") {
    return submissionDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
  } else if (key == "!startdate") {
    return startDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
  } else if (key == "!enddate") {
    return endDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
  } else if (key == "!target") {
    return target().hostname();
  }
  return defaultValue;
}

ParamSet TaskInstance::setenv() const {
  return d ? d->_setenv : ParamSet();
}

void TaskInstance::setTask(Task task) {
  if (d)
    d->_task = task;
}

bool TaskInstance::force() const {
  return d ? d->_force : false;
}

TaskInstance::TaskInstanceStatus TaskInstance::status() const {
  if (d) {
    if (d->_end != LLONG_MIN) {
      if (d->_start == LLONG_MIN)
        return Canceled;
      else
        return d->_success ? Success : Failure;
    }
    if (d->_start != LLONG_MIN)
      return Running;
  }
  return Queued;
}

QString TaskInstance::statusAsString(
    TaskInstance::TaskInstanceStatus status) {
  switch(status) {
  case Queued:
    return "queued";
  case Running:
    return "running";
  case Success:
    return "success";
  case Failure:
    return "failure";
  case Canceled:
    return "canceled";
  }
  return "unknown";
}

bool TaskInstance::isNull() {
  return !d || d->_id == 0;
}

uint qHash(const TaskInstance &instance) {
  return (uint)instance.id();
}

QString TaskInstance::command() const {
  return d ? d->_command : QString();
}

void TaskInstance::overrideCommand(QString command) {
  if (d)
    d->_command = command;
}

void TaskInstance::overrideSetenv(QString key, QString value) {
  if (d)
    d->_setenv.setValue(key, value);
}

bool TaskInstance::abortable() const {
  return d && d->_abortable;
}

void TaskInstance::setAbortable(bool abortable) const {
  if (d)
    d->_abortable = abortable;
}

TaskInstance TaskInstance::supertask() const {
  return d ? d->_supertask : TaskInstance();
}
