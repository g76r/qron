#ifndef TASKGROUP_H
#define TASKGROUP_H

#include <QSharedData>
#include <QList>

class TaskGroupData;
class Task;
class PfNode;
class QDebug;

class TaskGroup {
  QSharedDataPointer<TaskGroupData> d;

public:
  TaskGroup();
  TaskGroup(const TaskGroup &other);
  TaskGroup(PfNode node);
  ~TaskGroup();
  TaskGroup &operator =(const TaskGroup &other);
  //QList<Task> tasks();
  QString id() const;
  bool isNull() const;
};

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup);

#endif // TASKGROUP_H
