#ifndef TASKGROUP_H
#define TASKGROUP_H

#include <QSharedData>
#include <QList>

class TaskGroupData;
class Task;

class TaskGroup {
  QSharedDataPointer<TaskGroupData> d;

public:
  TaskGroup();
  TaskGroup(const TaskGroup &other);
  ~TaskGroup();
  TaskGroup &operator =(const TaskGroup &other);
  QList<Task> tasks();
  QString id() const;
};

#endif // TASKGROUP_H
