#include "crontrigger.h"
#include "task.h"

class CronTriggerData : public QSharedData {
  friend class CronTrigger;
  QString _cronExpression;
  Task _task;
  CronTriggerData(const QString cronExpression = QString()) { }
  void parseCronExpression(const QString cronExpression) {
    // TODO
  }
};

CronTrigger::CronTrigger(const QString cronExpression)
  : d(new CronTriggerData(cronExpression)) {
}

CronTrigger::CronTrigger(const CronTrigger &other) : d(other.d) {
}

CronTrigger::~CronTrigger() {
}

CronTrigger &CronTrigger::operator =(const CronTrigger &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

Task CronTrigger::task() const {
  return d->_task;
}

void CronTrigger::setTask(Task task) {
  d->_task = task;
}

QString CronTrigger::cronExpression() const {
  return d->_cronExpression;
}
