#include "taskgroup.h"
#include "paramset.h"
#include <QString>
#include "task.h"

class TaskGroupData : public QSharedData {
  friend class TaskGroup;
  QString _id, _label;
  ParamSet _params;
  QList<Task> _tasks;
public:
  TaskGroupData() { }
  TaskGroupData(const TaskGroupData &other) : _id(other._id),
    _label(other._label), _params(other._params), _tasks(other._tasks) { }
  ~TaskGroupData() { }
};

TaskGroup::TaskGroup() : d(new TaskGroupData) {
}

TaskGroup::TaskGroup(const TaskGroup &other) : d(other.d) {
}

TaskGroup::~TaskGroup() {
}

TaskGroup &TaskGroup::operator =(const TaskGroup &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

QList<Task> TaskGroup::tasks() {
  return d->_tasks;
}

QString TaskGroup::id() const {
  return d->_id;
}
