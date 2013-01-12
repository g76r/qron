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
  mutable QDateTime _start, _end;
  mutable bool _success;
  mutable int _returnCode;
  mutable Host _target;
  TaskRequestData(Task task = Task(), ParamSet params = ParamSet())
    : _id(newId()), _task(task), _params(params),
      _submission(QDateTime::currentDateTime()), _success(false),
      _returnCode(0) { }
  TaskRequestData(const TaskRequestData &other) : QSharedData(), _id(other._id),
    _task(other._task), _params(other._params), _submission(other._submission),
    _start(other._start), _end(other._end), _success(other._success),
    _returnCode(other._returnCode), _target(other._target) { }
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

TaskRequest::TaskRequest() : d(new TaskRequestData) {
}

TaskRequest::TaskRequest(const TaskRequest &other) : d(other.d) {
}

TaskRequest::TaskRequest(Task task, ParamSet params)
  : d(new TaskRequestData(task, params)) {
}

TaskRequest::~TaskRequest() {
}

TaskRequest &TaskRequest::operator=(const TaskRequest &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

const Task TaskRequest::task() const {
  return d->_task;
}

const ParamSet TaskRequest::params() const {
  return d->_params;
}

quint64 TaskRequest::id() const {
  return d->_id;
}

QDateTime TaskRequest::submissionDatetime() const {
  return d->_submission;
}

QDateTime TaskRequest::startDatetime() const {
  return d->_start;
}

void TaskRequest::setStartDatetime(QDateTime datetime) const {
  d->_start = datetime;
}

QDateTime TaskRequest::endDatetime() const {
  return d->_end;
}

void TaskRequest::setEndDatetime(QDateTime datetime) const {
  d->_end = datetime;
}

bool TaskRequest::success() const {
  return d->_success;
}

void TaskRequest::setSuccess(bool success) const {
  d->_success = success;
}

int TaskRequest::returnCode() const {
  return d->_returnCode;
}

void TaskRequest::setReturnCode(int returnCode) const {
  d->_returnCode = returnCode;
}

Host TaskRequest::target() const {
  return d->_target;
}

void TaskRequest::setTarget(Host target) const {
  d->_target = target;
}

QString TaskRequest::paramValue(const QString key,
                                const QString defaultValue) const {
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
  } else if (key == "!returncode") {
    return QString::number(returnCode());
  } else if (key == "!status") {
    if (startDatetime().isNull())
      return "queued";
    if (endDatetime().isNull())
      return "running";
    return success() ? "success" : "failure";
  } else if (key == "!submissiondate") {
    return submissionDatetime().toString(Qt::ISODate);
  } else if (key == "!startdate") {
    return startDatetime().toString(Qt::ISODate);
  } else if (key == "!enddate") {
    return endDatetime().toString(Qt::ISODate);
  } else if (key == "!target") {
    return target().hostname();
  }
  return defaultValue;
}
