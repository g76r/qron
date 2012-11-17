#include "scheduler.h"
#include <QtDebug>

Scheduler::Scheduler(QObject *parent) : QObject(parent) {
}

void Scheduler::loadConfiguration(QIODevice *source,
                                  bool appendToCurrentConfig) {
  // TODO
  // LATER cron trigger misfire is a config time issue
}

void Scheduler::requestTask(const QString fqtn, ParamSet params, bool force) {
  Task task = _tasks.value(fqtn);
  if (task.isNull()) {
    qWarning() << "requested task not found:" << fqtn << params << force;
    return;
  }
  if (!params.parent().isNull()) {
    qWarning() << "task requested with non null params' parent (parent will be "
                  "ignored)";
  }
  params.setParent(task.params());
  TaskRequest request(task, params);
  if (force) {
    runTaskNowAnyway(request);
  } else {
    if (!tryRunTaskNow(request)) {
      qDebug() << "queuing task" << task << params;
      _requestsQueue.append(request);
    }
  }
}

void Scheduler::triggerEvent(QString event) {
  foreach (Task task, _tasks.values()) {
    if (task.eventTriggers().contains(event)) {
      qDebug() << "event" << event << "triggers task" << task.fqtn();
      requestTask(task.fqtn());
    }
  }
}

void Scheduler::triggerTrigger(CronTrigger trigger) {
  Task task = trigger.task();
  qDebug() << "trigger" << trigger.cronExpression() << "triggers task"
           << task.fqtn();
  requestTask(task.fqtn());
}

void Scheduler::setFlag(QString flag) {
  qDebug() << "setting flag" << flag << (_setFlags.contains(flag)
                                         ? "which was already set anyway" : "");
  _setFlags.insert(flag);
}

void Scheduler::clearFlag(QString flag) {
  qDebug() << "clearing flag" << flag << (_setFlags.contains(flag)
                                          ? ""
                                          : "which was already cleared anyway");
  _setFlags.remove(flag);
}

bool Scheduler::tryRunTaskNow(TaskRequest request) {
  // TODO
  return false;
}

void Scheduler::runTaskNowAnyway(TaskRequest request) {
  // TODO
}
