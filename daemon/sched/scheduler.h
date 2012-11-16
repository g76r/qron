#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QSet>
#include "data/paramset.h"
#include "data/taskgroup.h"
#include "data/task.h"
#include "data/crontrigger.h"
#include "data/host.h"
#include "data/hostgroup.h"

class Scheduler : public QObject {
  Q_OBJECT
  ParamSet _globalParams;
  QList<TaskGroup> _taskGroups;
  QList<Task> _tasks;
  QList<CronTrigger> _cronTriggers;
  QList<HostGroup> _hostGroups;
  QList<Host> _hosts;
  QSet<QString> _setFlags;

public:
  explicit Scheduler(QObject *parent = 0);
  
signals:
  
public slots:
  
};

#endif // SCHEDULER_H
