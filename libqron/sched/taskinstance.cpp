/* Copyright 2012-2015 Hallowyn and others.
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
#include "util/timeformats.h"

static QString _uiHeaderNames[] = {
  "Instance Id", // 0
  "Task Id",
  "Status",
  "Submission",
  "Start Time",
  "End Time", // 5
  "Seconds Queued",
  "Seconds Running",
  "Actions",
  "Abortable" // 9
};

static QAtomicInt _sequence;

class TaskInstanceData : public SharedUiItemData {
public:
  quint64 _id, _groupId;
  QString _idAsString;
  Task _task;
  ParamSet _params;
  QDateTime _submission;
  bool _force;
  QString _command;
  ParamSet _setenv;
  TaskInstance _workflowInstanceTask;
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
                   TaskInstance workflowInstanceTask, quint64 groupId = 0)
    : _id(newId()), _groupId(groupId ? groupId : _id),
      _idAsString(QString::number(_id)),
      _task(task), _params(params),
      _submission(QDateTime::currentDateTime()), _force(force),
      _command(task.command()), _setenv(task.setenv()),
      _workflowInstanceTask(workflowInstanceTask),
      _start(LLONG_MIN), _end(LLONG_MIN),
      _success(false), _returnCode(0), _abortable(false) {
    _params.setParent(task.params()); }
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

  // SharedUiItemData interface
public:
  QString id() const { return _idAsString; }
  QString idQualifier() const { return "taskinstance"; }
  int uiSectionCount() const;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  QDateTime inline submissionDatetime() const { return _submission; }
  QDateTime inline startDatetime() const { return _start != LLONG_MIN
        ? QDateTime::fromMSecsSinceEpoch(_start) : QDateTime(); }
  void inline setStartDatetime(QDateTime datetime) const {
    _start = datetime.isValid() ? datetime.toMSecsSinceEpoch() : LLONG_MIN; }
  QDateTime inline endDatetime() const { return _end != LLONG_MIN
        ? QDateTime::fromMSecsSinceEpoch(_end) : QDateTime(); }
  void inline setEndDatetime(QDateTime datetime) const {
    _end = datetime.isValid() ? datetime.toMSecsSinceEpoch() : LLONG_MIN; }
  qint64 inline queuedMillis() const { return _submission.msecsTo(startDatetime()); }
  qint64 inline runningMillis() const {
    return _start != LLONG_MIN && _end != LLONG_MIN ? _end - _start : 0; }
  qint64 inline totalMillis() const {
    return _submission.isValid() && _end != LLONG_MIN
        ? _end - _submission.toMSecsSinceEpoch() : 0; }
  qint64 inline liveTotalMillis() const {
    return (_end != LLONG_MIN ? _end : QDateTime::currentMSecsSinceEpoch())
        - _submission.toMSecsSinceEpoch(); }
  TaskInstance::TaskInstanceStatus inline status() const {
    if (_end != LLONG_MIN) {
      if (_start == LLONG_MIN)
        return TaskInstance::Canceled;
      else
        return _success ? TaskInstance::Success : TaskInstance::Failure;
    }
    if (_start != LLONG_MIN)
      return TaskInstance::Running;
    return TaskInstance::Queued;
  }
};

TaskInstance::TaskInstance() {
}

TaskInstance::TaskInstance(const TaskInstance &other) : SharedUiItem(other) {
}

// TODO ensure that overridingParams is null when empty, since there are plenty of TaskInstances in memory
TaskInstance::TaskInstance(
    Task task, bool force, TaskInstance workflowInstanceTask,
    ParamSet overridingParams)
  : SharedUiItem(new TaskInstanceData(task, overridingParams, force,
                                      workflowInstanceTask)) {
}

TaskInstance::TaskInstance(Task task, quint64 groupId,
                           bool force, TaskInstance workflowInstanceTask,
                           ParamSet overridingParams)
  : SharedUiItem(new TaskInstanceData(task, overridingParams, force,
                                      workflowInstanceTask, groupId)) {
}

Task TaskInstance::task() const {
  const TaskInstanceData *d = data();
  return d ? d->_task : Task();
}

ParamSet TaskInstance::params() const {
  const TaskInstanceData *d = data();
  return d ? d->_params : ParamSet();
}

void TaskInstance::overrideParam(QString key, QString value) {
  TaskInstanceData *d = data();
  if (d)
    d->_params.setValue(key, value);
}

quint64 TaskInstance::idAsLong() const {
  const TaskInstanceData *d = data();
  return d ? d->_id : 0;
}

quint64 TaskInstance::groupId() const {
  const TaskInstanceData *d = data();
  return d ? d->_groupId : 0;
}

QDateTime TaskInstance::submissionDatetime() const {
  const TaskInstanceData *d = data();
  return d ? d->submissionDatetime() : QDateTime();
}

QDateTime TaskInstance::startDatetime() const {
  const TaskInstanceData *d = data();
  return d ? d->submissionDatetime() : QDateTime();
}

void TaskInstance::setStartDatetime(QDateTime datetime) const {
  const TaskInstanceData *d = data();
  if (d)
    d->setStartDatetime(datetime);
}

void TaskInstance::setEndDatetime(QDateTime datetime) const {
  const TaskInstanceData *d = data();
  if (d)
    d->setEndDatetime(datetime);
}

QDateTime TaskInstance::endDatetime() const {
  const TaskInstanceData *d = data();
  return d ? d->endDatetime() : QDateTime();
}

qint64 TaskInstance::queuedMillis() const {
  const TaskInstanceData *d = data();
  return d ? d->queuedMillis() : 0;
}

qint64 TaskInstance::runningMillis() const {
  const TaskInstanceData *d = data();
  return d ? d->runningMillis() : 0;
}

qint64 TaskInstance::totalMillis() const {
  const TaskInstanceData *d = data();
  return d ? d->totalMillis() : 0;
}

qint64 TaskInstance::liveTotalMillis() const {
  const TaskInstanceData *d = data();
  return d ? d->liveTotalMillis() : 0;
}

TaskInstance::TaskInstanceStatus TaskInstance::status() const {
  const TaskInstanceData *d = data();
  return d ? d->status() : Queued;
}

bool TaskInstance::success() const {
  const TaskInstanceData *d = data();
  return d ? d->_success : false;
}

void TaskInstance::setSuccess(bool success) const {
  const TaskInstanceData *d = data();
  if (d)
    d->_success = success;
}

int TaskInstance::returnCode() const {
  const TaskInstanceData *d = data();
  return d ? d->_returnCode : -1;
}

void TaskInstance::setReturnCode(int returnCode) const {
  const TaskInstanceData *d = data();
  if (d)
    d->_returnCode = returnCode;
}

Host TaskInstance::target() const {
  const TaskInstanceData *d = data();
  return d ? d->_target : Host();
}

void TaskInstance::setTarget(Host target) const {
  const TaskInstanceData *d = data();
  if (d) {
    target.detach();
    d->_target = target;
  }
}

QVariant TaskInstancePseudoParamsProvider::paramValue(
    QString key, QVariant defaultValue, QSet<QString> alreadyEvaluated) const {
  if (key.startsWith('!')) {
    // LATER optimize
    if (key == "!taskinstanceid") {
      return _taskInstance.id();
    } else if (key == "!workflowtaskinstanceid") {
      return _taskInstance.workflowInstanceTask().id();
    } else if (key == "!taskinstancegroupid") {
      return QString::number(_taskInstance.groupId());
    } else if (key == "!runningms") {
      return QString::number(_taskInstance.runningMillis());
    } else if (key == "!runnings") {
      return QString::number(_taskInstance.runningMillis()/1000);
    } else if (key == "!queuedms") {
      return QString::number(_taskInstance.queuedMillis());
    } else if (key == "!queueds") {
      return QString::number(_taskInstance.queuedMillis()/1000);
    } else if (key == "!totalms") {
      return QString::number(_taskInstance.queuedMillis()
                             +_taskInstance.runningMillis());
    } else if (key == "!totals") {
      return QString::number((_taskInstance.queuedMillis()
                              +_taskInstance.runningMillis())/1000);
    } else if (key == "!returncode") {
      return QString::number(_taskInstance.returnCode());
    } else if (key == "!status") {
      return _taskInstance.statusAsString();
    } else if (key.startsWith("!submissiondate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.submissionDatetime(), key.mid(15));
    } else if (key.startsWith("!startdate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.startDatetime(), key.mid(10));
    } else if (key.startsWith("!enddate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.endDatetime(), key.mid(8));
    } else if (key.startsWith("!workflowsubmissiondate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.workflowInstanceTask().submissionDatetime(),
            key.mid(23));
    } else if (key.startsWith("!workflowstartdate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.workflowInstanceTask().startDatetime(),
            key.mid(18));
    } else if (key.startsWith("!workflowenddate")) {
      return TimeFormats::toMultifieldSpecifiedCustomTimestamp(
            _taskInstance.workflowInstanceTask().endDatetime(),
            key.mid(16));
    } else if (key == "!target") {
      return _taskInstance.target().hostname();
    } else {
      return _taskPseudoParams.paramValue(key, defaultValue, alreadyEvaluated);
    }
  }
  return defaultValue;
}

ParamSet TaskInstance::setenv() const {
  const TaskInstanceData *d = data();
  return d ? d->_setenv : ParamSet();
}

void TaskInstance::setTask(Task task) {
  TaskInstanceData *d = data();
  if (d)
    d->_task = task;
}

bool TaskInstance::force() const {
  const TaskInstanceData *d = data();
  return d ? d->_force : false;
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

QString TaskInstance::command() const {
  const TaskInstanceData *d = data();
  return d ? d->_command : QString();
}

void TaskInstance::overrideCommand(QString command) {
  TaskInstanceData *d = data();
  if (d)
    d->_command = command;
}

void TaskInstance::overrideSetenv(QString key, QString value) {
  TaskInstanceData *d = data();
  if (d)
    d->_setenv.setValue(key, value);
}

bool TaskInstance::abortable() const {
  const TaskInstanceData *d = data();
  return d && d->_abortable;
}

void TaskInstance::setAbortable(bool abortable) const {
  const TaskInstanceData *d = data();
  if (d)
    d->_abortable = abortable;
}

TaskInstance TaskInstance::workflowInstanceTask() const {
  const TaskInstanceData *d = data();
  if (d) {
    if (d->_task.mean() == Task::Workflow)
      return *this;
    return d->_workflowInstanceTask;
  }
  return TaskInstance();
}

QVariant TaskInstanceData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int TaskInstanceData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

QVariant TaskInstanceData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
      return _idAsString;
    case 1:
      return _task.id();
    case 2:
      return TaskInstance::statusAsString(status());
    case 3:
      return submissionDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 4:
      return startDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 5:
      return endDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 6:
      return startDatetime().isNull() || submissionDatetime().isNull()
          ? QVariant() : QString::number(queuedMillis()/1000.0);
    case 7:
      return endDatetime().isNull() || startDatetime().isNull()
          ? QVariant() : QString::number(runningMillis()/1000.0);
    case 8:
      return QVariant(); // custom actions, handled by the model, if needed
    case 9:
      return _abortable;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

TaskInstanceData *TaskInstance::data() {
  detach<TaskInstanceData>();
  return (TaskInstanceData*)SharedUiItem::data();
}
