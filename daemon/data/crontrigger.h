#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>
#include "task.h"

class CronTriggerData;

class CronTrigger {
  QSharedDataPointer<CronTriggerData> d;

public:
  CronTrigger();
  CronTrigger(const CronTrigger &other);
  ~CronTrigger();
  Task task() const;
  QString cronExpression() const;
};

#endif // CRONTRIGGER_H
