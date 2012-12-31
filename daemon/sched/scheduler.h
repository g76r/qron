/* Copyright 2012 Hallowyn and others.
 * This file is part of qron, see <http://qron.hallowyn.com/>.
 * Qron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Qron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with qron. If not, see <http://www.gnu.org/licenses/>.
 */
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
#include "data/cluster.h"
#include "data/taskrequest.h"
#include <QMap>
#include "executor.h"
#include <QVariant>
#include "alert/alerter.h"

class PfNode;

class Scheduler : public QObject {
  Q_OBJECT
  ParamSet _globalParams;
  QMap<QString,TaskGroup> _tasksGroups;
  QMap<QString,Task> _tasks;
  QList<CronTrigger> _cronTriggers;
  QMap<QString,Cluster> _clusters;
  QMap<QString,Host> _hosts;
  QMap<QString,QMap<QString,qint64> > _resources;
  QSet<QString> _setFlags;
  QList<TaskRequest> _queuedRequests;
  QList<Executor*> _executors;
  Alerter *_alerter;

public:
  explicit Scheduler(QObject *parent = 0);
  bool loadConfiguration(QIODevice *source, QString &errorString,
                         bool appendToCurrentConfig = true);
  void customEvent(QEvent *event);
  Alerter *alerter() { return _alerter; }

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
  void triggerTrigger(QVariant trigger);
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

signals:
  void tasksConfigurationReset(QMap<QString,TaskGroup> tasksGroups,
                               QMap<QString,Task> tasks);
  void targetsConfigurationReset(QMap<QString,Cluster> clusters,
                                 QMap<QString,Host> hosts);
  void hostResourceAllocationChanged(QString host,
                                     QMap<QString,qint64> resources);
  void hostResourceConfigurationChanged(
      QMap<QString,QMap<QString,qint64> > resources);
  void taskStarted(TaskRequest request, Host host);
  void taskFinished(TaskRequest request, Host target, bool success,
                    int returnCode, QWeakPointer<Executor> executor);
  void globalParamsChanged(ParamSet globalParams);

private slots:
  void taskFinishing(TaskRequest request, Host target, bool success,
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
  void setTimerForCronTrigger(CronTrigger trigger, QDateTime previous
                              = QDateTime::currentDateTime());
  Q_DISABLE_COPY(Scheduler)
};

#endif // SCHEDULER_H
