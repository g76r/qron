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
  TaskRequestData(Task task = Task(), ParamSet params = ParamSet())
    : _id(newId()), _task(task), _params(params) { }
  TaskRequestData(const TaskRequestData &other) : _id(other._id),
    _task(other._task), _params(other._params) { }
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
