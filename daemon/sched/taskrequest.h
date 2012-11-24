#ifndef TASKREQUEST_H
#define TASKREQUEST_H

#include <QSharedDataPointer>
#include "data/task.h"
#include "data/paramset.h"

class TaskRequestData;

class TaskRequest {
  QSharedDataPointer<TaskRequestData> d;
public:
  TaskRequest();
  TaskRequest(const TaskRequest &);
  TaskRequest(Task task, ParamSet params);
  ~TaskRequest();
  TaskRequest &operator=(const TaskRequest &);
  const Task task() const;
  const ParamSet params() const;
  quint64 id() const;
};

#endif // TASKREQUEST_H
