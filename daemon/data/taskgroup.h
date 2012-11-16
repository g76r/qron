#ifndef TASKGROUP_H
#define TASKGROUP_H

#include <QSharedData>

class TaskGroupData;

class TaskGroup {
  QSharedDataPointer<TaskGroupData> d;

public:
  TaskGroup();
  ~TaskGroup();
};

#endif // TASKGROUP_H
