#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>
#include <QString>

class CronTriggerData;
class Task;

class CronTrigger {
  QSharedDataPointer<CronTriggerData> d;

public:
  explicit CronTrigger(const QString cronExpression = QString());
  CronTrigger(const CronTrigger &other);
  ~CronTrigger();
  CronTrigger &operator =(const CronTrigger &other);
  Task task() const;
  void setTask(Task task);
  QString cronExpression() const;
};

#endif // CRONTRIGGER_H
