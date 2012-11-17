#include "taskrequest.h"
#include <QSharedData>

class TaskRequestData : public QSharedData {
  friend class TaskRequest;
  Task _task;
  ParamSet _params;
public:
  TaskRequestData(Task task = Task(), ParamSet params = ParamSet())
    : _task(task), _params(params) { }
  TaskRequestData(const TaskRequestData &other) : _task(other._task),
    _params(other._params) { }
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
