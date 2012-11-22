#ifndef TASK_H
#define TASK_H

#include <QSharedData>
#include "paramset.h"
#include <QSet>

class TaskData;
class QDebug;
class PfNode;
class TaskGroup;
class CronTrigger;

class Task {
  QSharedDataPointer<TaskData> d;
public:
  Task();
  Task(const Task &other);
  Task(PfNode node);
  ~Task();
  Task &operator =(const Task &other);
  const ParamSet params() const;
  bool isNull() const;
  const QSet<QString> eventTriggers() const;
  /** Fully qualified task name (i.e. "taskGroupId.taskId")
    */
  QString id() const;
  QString fqtn() const;
  QString label() const;
  QString mean() const;
  QString command() const;
  QString target() const;
  void setTaskGroup(TaskGroup taskGroup);
  const QList<CronTrigger> cronTriggers() const;
};

QDebug operator<<(QDebug dbg, const Task &task);

#endif // TASK_H
