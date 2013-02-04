/* Copyright 2012-2013 Hallowyn and others.
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
#include "util/paramset.h"
#include "config/taskgroup.h"
#include "config/task.h"
#include "config/crontrigger.h"
#include "config/host.h"
#include "config/cluster.h"
#include "config/taskrequest.h"
#include <QMap>
#include "executor.h"
#include <QVariant>
#include "alert/alerter.h"
#include <QMutex>
#include "event/event.h"
#include "pf/pfnode.h"

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
  mutable QMutex _flagsMutex;
  QList<TaskRequest> _queuedRequests;
  QList<Executor*> _executors;
  Alerter *_alerter;
  bool _firstConfigurationLoad;
  QList<Event> _onstart, _onsuccess, _onfailure;
  QList<Event> _onlog, _onnotice, _onschedulerstart;

public:
  explicit Scheduler(QObject *parent = 0);
  ~Scheduler();
  bool loadConfiguration(QIODevice *source, QString &errorString,
                         bool appendToCurrentConfig = true);
  bool loadEventListConfiguration(PfNode listnode, QList<Event> &list,
                                  QString &errorString);
  inline bool loadEventListConfiguration(PfNode listnode, QList<Event> &list) {
    QString errorString;
    return loadEventListConfiguration(listnode, list, errorString);
  }
  static void triggerEvents(const QList<Event> list,
                            const ParamsProvider *context);
  void customEvent(QEvent *event);
  Alerter *alerter() { return _alerter; }

public slots:
  /** Expicitely request task execution now.
    * @param fqtn fully qualified task name, on the form "taskGroupId.taskId"
    * @param params override some params at request time
    * @param force if true, any constraints or ressources are ignored
    * @return true if task queued, false if task cannot be queued
    */
  bool requestTask(const QString fqtn, ParamSet params = ParamSet(),
                   bool force = false);
  /** Fire a cron trigger, triggering whatever this trigger is configured to do.
    */
  void triggerTrigger(QVariant trigger);
  /** Set a flag, which will be evaluated by any following task constraints
    * evaluation.
    * This method is thread-safe. */
  void setFlag(const QString flag);
  /** Clear a flag.
    * This method is thread-safe. */
  void clearFlag(const QString flag);
  /** Clear a flag.
    * This method is thread-safe. */
  bool isFlagSet(const QString flag) const;
  /** Post a notice.
    * This method is thread-safe. */
  void postNotice(const QString notice);
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
  /** There is no guarantee that taskQueued() is emited, taskStarted() or
    * taskFinished() can be emited witout previous taskQueued(). */
  void taskQueued(TaskRequest request);
  /** There is no guarantee that taskStarted() is emited, taskFinished() can
    * be emited witout previous taskQueued().
    * @param delayedMillis time between request and start, in ms */
  void taskStarted(TaskRequest request);
  /** @param durationMillis time between start and termination, in ms */
  void taskFinished(TaskRequest request, QWeakPointer<Executor> executor);
  void globalParamsChanged(ParamSet globalParams);
  void noticePosted(QString notice);

private slots:
  void taskFinishing(TaskRequest request, QWeakPointer<Executor> executor);

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
