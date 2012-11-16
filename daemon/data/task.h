#ifndef TASK_H
#define TASK_H

#include <QSharedData>

class TaskData;

class Task {
  QSharedDataPointer<TaskData> d;

public:
  Task();
  ~Task();
};

#endif // TASK_H
