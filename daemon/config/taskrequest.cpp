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
#include "taskrequest.h"
#include <QSharedData>
#include <QDateTime>
#include <QAtomicInt>

static QAtomicInt _sequence;

class TaskRequestData : public QSharedData {
public:
  quint64 _id;
  Task _task;
  ParamSet _params;
  QDateTime _submission;
  bool _force;

private:
  // LATER using qint64 on 32 bits systems is not thread-safe but only crash-free
  mutable qint64 _start, _end;
  mutable bool _success;
  mutable int _returnCode;
  mutable Host _target; // FIXME not thread safe

public:
  TaskRequestData(Task task, ParamSet params, bool force)
    : _id(newId()), _task(task), _params(params),
      _submission(QDateTime::currentDateTime()), _force(force),
      _start(LLONG_MIN), _end(LLONG_MIN), _success(false), _returnCode(0) { }
  TaskRequestData() : _id(0), _start(LLONG_MIN), _end(LLONG_MIN),
    _success(false), _returnCode(0) { }
  TaskRequestData(const TaskRequestData &other) : QSharedData(), _id(other._id),
    _task(other._task), _params(other._params), _submission(other._submission),
    _force(other._force), _start(other._start), _end(other._end),
    _success(other._success), _returnCode(other._returnCode),
    _target(other._target) { }
  QDateTime start() const {
    return _start == LLONG_MIN
        ? QDateTime() : QDateTime::fromMSecsSinceEpoch(_start); }
  qint64 startMillis() const { return _start; }
  void setStart(QDateTime timestamp) const {
    _start = timestamp.toMSecsSinceEpoch(); }
  QDateTime end() const {
    return _end == LLONG_MIN
        ? QDateTime() : QDateTime::fromMSecsSinceEpoch(_end); }
  qint64 endMillis() const { return _end; }
  void setEnd(QDateTime timestamp) const {
    _end = timestamp.toMSecsSinceEpoch(); }
  bool success() const { return _success; }
  void setSuccess(bool success) const { _success = success; }
  int returnCode() const { return _returnCode; }
  void setReturnCode(int returnCode) const { _returnCode = returnCode; }
  Host target() const { return _target; } // FIXME not thread safe
  void setTarget(Host host) const { _target = host; } // FIXME not thread safe
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

TaskRequest::TaskRequest() {
}

TaskRequest::TaskRequest(const TaskRequest &other) : d(other.d) {
}

TaskRequest::TaskRequest(Task task, ParamSet params, bool force)
  : d(new TaskRequestData(task, params, force)) {
}

TaskRequest::~TaskRequest() {
}

TaskRequest &TaskRequest::operator=(const TaskRequest &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool TaskRequest::operator==(const TaskRequest &other) {
  return (!d && !other.d) || (d && other.d && d->_id == other.d->_id);
}

const Task TaskRequest::task() const {
  return d ? d->_task : Task();
}

const ParamSet TaskRequest::params() const {
  return d ? d->_params : ParamSet();
}

quint64 TaskRequest::id() const {
  return d ? d->_id : 0;
}

QDateTime TaskRequest::submissionDatetime() const {
  return d ? d->_submission : QDateTime();
}

QDateTime TaskRequest::startDatetime() const {
  return d ? d->start() : QDateTime();
}

void TaskRequest::setStartDatetime(QDateTime datetime) const {
  if (d)
    d->setStart(datetime);
}

QDateTime TaskRequest::endDatetime() const {
  return d ? d->end() : QDateTime();
}

void TaskRequest::setEndDatetime(QDateTime datetime) const {
  if (d)
    d->setEnd(datetime);
}

bool TaskRequest::success() const {
  return d ? d->success() : false;
}

void TaskRequest::setSuccess(bool success) const {
  if (d)
    d->setSuccess(success);
}

int TaskRequest::returnCode() const {
  return d ? d->returnCode() : 0;
}

void TaskRequest::setReturnCode(int returnCode) const {
  if (d)
    d->setReturnCode(returnCode);
}

Host TaskRequest::target() const {
  return d ? d->target() : Host();
}

void TaskRequest::setTarget(Host target) const {
  if (d)
    d->setTarget(target);
}

QString TaskRequest::paramValue(const QString key,
                                const QString defaultValue) const {
  //Log::debug() << "TaskRequest::paramvalue " << key;
  if (!d)
    return defaultValue;
  if (key == "!taskid") {
    return task().id();
  } else if (key == "!fqtn") {
    return task().fqtn();
  } else if (key == "!taskgroupid") {
    return task().taskGroup().id();
  } else if (key == "!taskrequestid") {
    return QString::number(id());
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

ParamSet TaskRequest::setenv() const {
  return d ? d->_task.setenv() : ParamSet();
}

void TaskRequest::setTask(Task task) {
  if (d)
    d->_task = task;
}

bool TaskRequest::force() const {
  return d ? d->_force : false;
}

TaskRequest::TaskRequestStatus TaskRequest::status() const {
  if (d) {
    if (d->endMillis() != LLONG_MIN) {
      if (d->startMillis() == LLONG_MIN)
        return Canceled;
      else
        return d->success() ? Success : Failure;
    }
    if (d->startMillis() != LLONG_MIN)
      return Running;
  }
  return Queued;
}

QString TaskRequest::statusAsString(
    TaskRequest::TaskRequestStatus status) {
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
