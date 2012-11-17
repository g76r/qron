#include "task.h"
#include "taskgroup.h"
#include <QString>
#include <QMap>
#include <QtDebug>

class TaskData : public QSharedData {
  friend class Task;
  QString _id, _label;
  TaskGroup _group;
  ParamSet _params;
  QSet<QString> _eventTriggers;
  QMap<QString,quint64> _resources;
  QString _target;

public:
  TaskData() { }
  TaskData(const TaskData &other) : _id(other._id), _label(other._label),
    _group(other._group), _params(other._params),
    _eventTriggers(other._eventTriggers), _resources(other._resources),
    _target(_target) { }
  ~TaskData() { }
};

Task::Task() : d(new TaskData) {
}

Task::Task(const Task &other) : d(other.d) {
}

Task::~Task() {
}

Task &Task::operator =(const Task &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

const ParamSet Task::params() const {
  return d->_params;
}

bool Task::isNull() const {
  return d->_id.isNull();
}

const QSet<QString> Task::eventTriggers() const {
  return d->_eventTriggers;
}

QString Task::id() const {
  return d->_id;
}

QString Task::fqtn() const {
  return d->_group.id()+"."+d->_id;
}

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.fqtn(); // LATER display more
  return dbg.space();
}
