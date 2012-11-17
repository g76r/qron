#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QSet>
#include <QIODevice>
#include "data/paramset.h"
#include "data/taskgroup.h"
#include "data/task.h"
#include "data/crontrigger.h"
#include "data/host.h"
#include "data/hostgroup.h"
#include "taskrequest.h"
#include <QMap>
#include "executor.h"

class PfNode;

class Scheduler : public QObject {
  Q_OBJECT
  ParamSet _globalParams;
  QMap<QString,TaskGroup> _taskGroups;
  QMap<QString,Task> _tasks;
  QList<CronTrigger> _cronTriggers;
  QMap<QString,HostGroup> _hostGroups;
  QMap<QString,Host> _hosts;
  QSet<QString> _setFlags;
  QList<TaskRequest> _queuedRequests;
  QList<Executor*> _executors;

public:
  explicit Scheduler(QObject *threadParent = 0);
  bool loadConfiguration(QIODevice *source, QString &errorString,
                         bool appendToCurrentConfig = true);
  void customEvent(QEvent *event);

public slots:
  /** Expicitely request task execution now.
    * @param fqtn fully qualified task name, on the form "taskGroupId.taskId"
    * @param params override some params at request time
    * @param force if true, any constraints or ressources are ignored
    */
  void requestTask(const QString fqtn, ParamSet params = ParamSet(),
                   bool force = false);
  /** Emit an event, triggering whatever this event is configured to trigger.
    */
  void triggerEvent(QString event);
  /** Fire a cron trigger, triggering whatever this trigger is configured to do.
    */
  void triggerTrigger(CronTrigger trigger);
  /** Set a flag, which will be evaluated by any following task constraints
    * evaluation.
    */
  void setFlag(QString flag);
  /** Clear a flag.
    */
  void clearFlag(QString flag);
  /** Ask for queued requests to be reevaluated during next event loop
    * iteration.
    * This method must be called every time something occurs that could make a
    * queued task runnable. Calling this method several time within the same
    * event loop iteration will trigger reevaluation only once (same pattern as
    * QWidget::update()).
    */
  void reevaluateQueuedRequests();

private slots:
  void taskFinished(TaskRequest request, Host target, bool success,
                    int returnCode, QWeakPointer<Executor> executor);

private:
  /** Check if it is permitted for a task to run now, if yes start it.
    * @return true if the task was started
    */
  bool tryStartTaskNow(TaskRequest request);
  /** Start a task despite any constraint or limit. Even create a new executor
    * thread if needed.
    */
  void startTaskNowAnyway(TaskRequest request);
  /** Reevaluate queued requests and start any task that can be started.
    * @see reevaluateQueuedRequests()
    */
  void startQueuedTasksIfPossible();
  bool loadConfiguration(PfNode root, QString &errorString);
  Q_DISABLE_COPY(Scheduler)
};

#endif // SCHEDULER_H
