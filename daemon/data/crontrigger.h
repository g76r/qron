#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>
#include <QString>
#include <QDateTime>

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
  QString parsedCronExpression() const;
  /** Cron expression is valid (hence not null or empty). */
  bool isValid() const;
  QDateTime nextTrigger(QDateTime lastTrigger, QDateTime max) const;
  QDateTime nextTrigger(QDateTime lastTrigger) const;
};

#endif // CRONTRIGGER_H
