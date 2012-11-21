#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>
#include <QString>
#include <QDateTime>
#include <QMetaType>

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
  /** Syntaxic sugar with max = lastTrigger + 10 years */
  QDateTime nextTrigger(QDateTime lastTrigger) const;
  /** Syntaxic sugar returning msecs from current time
    * @return -1 if not available within 10 years
    */
  int nextTriggerMsecs(QDateTime lastTirgger) const;
};

Q_DECLARE_METATYPE(CronTrigger)

#endif // CRONTRIGGER_H
