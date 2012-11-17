#ifndef TASK_H
#define TASK_H

#include <QSharedData>
#include "paramset.h"
#include <QSet>

class TaskData;
class QDebug;

class Task {
  QSharedDataPointer<TaskData> d;
public:
  Task();
  Task(const Task &other);
  ~Task();
  Task &operator =(const Task &other);
  const ParamSet params() const;
  bool isNull() const;
  const QSet<QString> eventTriggers() const;
  /** Fully qualified task name (i.e. "taskGroupId.taskId")
    */
  QString fqtn() const;
  QString id() const;
};

QDebug operator<<(QDebug dbg, const Task &task);

#endif // TASK_H
