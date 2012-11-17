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

class Scheduler : public QObject {
  Q_OBJECT
  ParamSet _globalParams;
  QMap<QString,TaskGroup> _taskGroups;
  QMap<QString,Task> _tasks;
  QList<CronTrigger> _cronTriggers;
  QList<HostGroup> _hostGroups;
  QList<Host> _hosts;
  QSet<QString> _setFlags;
  QList<TaskRequest> _requestsQueue;

public:
  explicit Scheduler(QObject *parent = 0);
  void loadConfiguration(QIODevice *source, bool appendToCurrentConfig = true);

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
  
private:
  bool tryRunTaskNow(TaskRequest request);
  void runTaskNowAnyway(TaskRequest request);
  Q_DISABLE_COPY(Scheduler)
};

#endif // SCHEDULER_H
