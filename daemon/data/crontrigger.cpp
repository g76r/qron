#include "crontrigger.h"

class CronTriggerData : public QSharedData {

};

CronTrigger::CronTrigger() {
}

CronTrigger::CronTrigger(const CronTrigger &other) {
}

CronTrigger::~CronTrigger() {
}

Task CronTrigger::task() const {
  // TODO
  return Task();
}

QString CronTrigger::cronExpression() const {
  // TODO
  return "* * * * * *";
}
