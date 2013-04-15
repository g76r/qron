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

class QThread;

class Scheduler : public QObject {
  Q_OBJECT
  QThread *_thread;
  ParamSet _globalParams, _setenv;
  QMap<QString,TaskGroup> _tasksGroups;
  QMap<QString,Task> _tasks;
  QMap<QString,Cluster> _clusters;
  QMap<QString,Host> _hosts;
  QMap<QString,QMap<QString,qint64> > _resources;
  QSet<QString> _setFlags, _unsetenv;
  mutable QMutex _flagsMutex, _configMutex;
  QList<TaskRequest> _queuedRequests;
  QHash<TaskRequest,Executor*> _runningRequests;
  QList<Executor*> _availableExecutors;
  Alerter *_alerter;
  bool _firstConfigurationLoad;
  QList<Event> _onstart, _onsuccess, _onfailure;
  QList<Event> _onlog, _onnotice, _onschedulerstart;
  int _maxtotaltaskinstances, _maxqueuedrequests;
  volatile qint64 _startdate, _configdate;
  volatile long _execcount;

public:
  Scheduler();
  ~Scheduler();
  bool reloadConfiguration(QIODevice *source, QString &errorString);
  bool loadEventListConfiguration(PfNode listnode, QList<Event> &list,
                                  QString &errorString);
  inline bool loadEventListConfiguration(PfNode listnode, QList<Event> &list) {
    QString errorString;
    return loadEventListConfiguration(listnode, list, errorString); }
  static void triggerEvents(const QList<Event> list,
                            const ParamsProvider *context);
  void customEvent(QEvent *event);
  Alerter *alerter() { return _alerter; }
  /** Test a flag.
    * This method is thread-safe. */
  bool isFlagSet(const QString flag) const;
  QDateTime startdate() const {
    return QDateTime::fromMSecsSinceEpoch(_startdate); }
  QDateTime configdate() const {
    return _configdate == LLONG_MIN
        ? QDateTime() : QDateTime::fromMSecsSinceEpoch(_configdate); }
  long execcount() const { return _execcount; }
  int maxtotaltaskinstances() const { return _maxtotaltaskinstances; }
  int maxqueuedrequests() const { return _maxqueuedrequests; }

public slots:
  /** Explicitely request task execution now.
    * This method will block current thread until the request is either
    * queued either denied by Scheduler thread.
    * If current thread is the Scheduler thread, the method is a direct call.
    * @param fqtn fully qualified task name, on the form "taskGroupId.taskId"
    * @param params override some params at request time
    * @param force if true, any constraints or ressources are ignored
    * @return request id if task queued, isNull() if task cannot be queued
    */
  TaskRequest syncRequestTask(const QString fqtn, ParamSet params = ParamSet(),
                              bool force = false);
  /** Explicitely request task execution now, but do not wait for validity
   * check of the request, therefore do not wait for Scheduler thread
   * processing the request.
   * If current thread is the Scheduler thread, the call is queued anyway.
   */
  void asyncRequestTask(const QString fqtn, ParamSet params = ParamSet(),
                        bool force = false);
  /** Cancel a queued request.
   * @return TaskRequest.isNull() iff error (e.g. request not found or no longer
   * queued */
  TaskRequest cancelRequest(quint64 id);
  TaskRequest cancelRequest(TaskRequest request) {
    return cancelRequest(request.id()); }
  /** Abort a running request.
   * For local tasks aborting means killing, for ssh tasks aborting means
   * killing ssh client hence most of time killing actual task, for http tasks
   * aborting means closing the socket.
   * Beware that, but for local tasks, aborting a task does not guarantees that
   * the application processing is actually ended whereas it frees resources
   * and tasks instance counters, hence enabling immediate reexecution of the
   * same task.
   * @return TaskRequest.isNull() iff error (e.g. request not found or no longer
   * running */
  TaskRequest abortTask(quint64 id);
  TaskRequest abortTask(TaskRequest request) {
    return abortTask(request.id()); }
  /** Set a flag, which will be evaluated by any following task constraints
    * evaluation.
    * This method is thread-safe. */
  void setFlag(const QString flag);
  /** Clear a flag.
    * This method is thread-safe. */
  void clearFlag(const QString flag);
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
  /** Enable or disable a task.
    * This method is threadsafe */
  bool enableTask(const QString fqtn, bool enable);
  /** Enable or disable all tasks at once.
    * This method is threadsafe */
  void enableAllTasks(bool enable);
  //LATER enableAllTasksWithinGroup
  ParamSet globalParams() const { return _globalParams; }
  bool taskExists(QString fqtn);

signals:
  void tasksConfigurationReset(QMap<QString,TaskGroup> tasksGroups,
                               QMap<QString,Task> tasks);
  void targetsConfigurationReset(QMap<QString,Cluster> clusters,
                                 QMap<QString,Host> hosts);
  void eventsConfigurationReset(QList<Event> onstart, QList<Event> onsuccess,
                                QList<Event> onfailure, QList<Event> onlog,
                                QList<Event> onnotice,
                                QList<Event> onschedulerstart);
  void hostResourceAllocationChanged(QString host,
                                     QMap<QString,qint64> resources);
  void hostResourceConfigurationChanged(
      QMap<QString,QMap<QString,qint64> > resources);
  /** There is no guarantee that taskQueued() is emited, taskStarted() or
    * taskFinished() can be emited witout previous taskQueued(). */
  void taskQueued(TaskRequest request);
  /** There is no guarantee that taskStarted() is emited, taskFinished() can
    * be emited witout previous taskQueued(). */
  void taskStarted(TaskRequest request);
  void taskFinished(TaskRequest request, QWeakPointer<Executor> executor);
  /** Called whenever a task or taskrequest changes: queued, started, finished,
   * disabled, enabled... */
  void taskChanged(Task request);
  void globalParamsChanged(ParamSet globalParams);
  void noticePosted(QString notice);
  void flagSet(QString flag);
  void flagCleared(QString flag);

private slots:
  void taskFinishing(TaskRequest request, QWeakPointer<Executor> executor);
  void periodicChecks();
  /** Fire expired triggers for a given task. */
  void checkTriggersForTask(QVariant fqtn);
  /** Fire expired triggers for all tasks. */
  void checkTriggersForAllTasks();

private:
  /** Reevaluate queued requests and start any task that can be started.
    * @see reevaluateQueuedRequests()
    */
  void startQueuedTasks();
  /** Check if it is permitted for a task to run now, if yes start it.
   * If request.force() is true, start a task despite any constraint or limit,
   * even create a new (temporary) executor thread if needed.
   * @return true if the task was started or canceled
   */
  bool startQueuedTask(TaskRequest request);
  /** @return true iff the triggers fires a task request */
  bool checkTrigger(CronTrigger trigger, Task task, QString fqtn);
  bool reloadConfiguration(PfNode root, QString &errorString);
  void setTimerForCronTrigger(CronTrigger trigger, QDateTime previous
                              = QDateTime::currentDateTime());
  Q_INVOKABLE TaskRequest enqueueTaskRequest(const QString fqtn,
                                             ParamSet params, bool force);
  Q_INVOKABLE TaskRequest doCancelRequest(quint64 id);
  Q_INVOKABLE TaskRequest doAbortTask(quint64 id);
  Q_DISABLE_COPY(Scheduler)
};

#endif // SCHEDULER_H
