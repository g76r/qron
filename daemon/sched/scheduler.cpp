#include "scheduler.h"
#include <QtDebug>
#include <QCoreApplication>
#include <QEvent>

#define REEVALUATE_QUEUED_REQUEST_EVENT (QEvent::Type(QEvent::User+1))

Scheduler::Scheduler(QObject *parent) : QObject(parent) {
}

bool Scheduler::loadConfiguration(QIODevice *source,
                                  bool appendToCurrentConfig) {
  if (!source->isOpen())
    if (!source->open(QIODevice::ReadOnly)) {
      qWarning() << "cannot read configuration" << source->errorString();
      return false;
    }
  // TODO read PF config
  // TODO set QTimers on CronTriggers to be notified on next trigger time
  // LATER fire cron triggers if they were missed since last task exec
  return false;
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
    startTaskNowAnyway(request);
  } else {
    // note: a request must always be queued even if the task can be started
    // immediately, to avoid the new tasks being started before queued ones
    qDebug() << "queuing task" << task << params;
    _queuedRequests.append(request);
    reevaluateQueuedRequests();
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
  reevaluateQueuedRequests();
}

void Scheduler::clearFlag(QString flag) {
  qDebug() << "clearing flag" << flag << (_setFlags.contains(flag)
                                          ? ""
                                          : "which was already cleared anyway");
  _setFlags.remove(flag);
  reevaluateQueuedRequests();
}

bool Scheduler::tryStartTaskNow(TaskRequest request) {
  bool runnable = true;
  // check maxtotaltasks
  if (_executors.isEmpty()) {
    qDebug() << "cannot execute task" << request.task() << "now because there "
                "are already too many tasks running (maxtotaltasks)";
    runnable = false;
  }
  // TODO check flags
  // TODO check resources, choose target and consume resources
  // TODO start task if possible
  reevaluateQueuedRequests();
  return runnable;
}

void Scheduler::startTaskNowAnyway(TaskRequest request) {
  Executor *e;
  if (_executors.isEmpty()) {
    e = new Executor(this);
    e->setTemporary();
    connect(e, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
            this, SLOT(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)));
  } else {
    e = _executors.takeFirst();
  }
  Host target;
  // TODO consume resources, even setting them < 0
  // TODO choose target
  e->execute(request, target);
  reevaluateQueuedRequests();
}

void Scheduler::taskFinished(TaskRequest request, Host target, bool success,
                             int returnCode, QWeakPointer<Executor> executor) {
  qDebug() << "task" << request.task() << "finished"
           << (success ? "successfully" : "in failure") << "with return code"
           << returnCode << "on host"; // << target;
  Executor *e = executor.data();
  if (e) {
    if (e->isTemporary())
      e->deleteLater();
    else
      _executors.append(e);
  }
  // TODO give resources back
  // LATER try resubmit if the host was not reachable (this can be usefull with hostgroups or when host become reachable again)
  reevaluateQueuedRequests();
}

void Scheduler::reevaluateQueuedRequests() {
  QCoreApplication::postEvent(this,
                              new QEvent(REEVALUATE_QUEUED_REQUEST_EVENT));
}

void Scheduler::customEvent(QEvent *event) {
  switch (event->type()) {
  case REEVALUATE_QUEUED_REQUEST_EVENT:
    QCoreApplication::removePostedEvents(this, REEVALUATE_QUEUED_REQUEST_EVENT);
    startQueuedTasksIfPossible();
    break;
  default:
    QObject::customEvent(event);
  }
}

void Scheduler::startQueuedTasksIfPossible() {
  for (int i = 0; i < _queuedRequests.size(); ) {
    TaskRequest r = _queuedRequests[i];
    if (tryStartTaskNow(r)) {
      _queuedRequests.removeAt(i);
      // LATER remove other queued instances of same task if needed
    } else
      ++i;
  }
}
