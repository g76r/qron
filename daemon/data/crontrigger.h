#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>

class CronTriggerData;

class CronTrigger {
  QSharedDataPointer<CronTriggerData> d;

public:
  CronTrigger();
  CronTrigger(const CronTrigger &other);
  ~CronTrigger();
};

#endif // CRONTRIGGER_H
