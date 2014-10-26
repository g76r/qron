/* Copyright 2012-2014 Hallowyn and others.
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
#ifndef TASK_H
#define TASK_H

#include "libqron_global.h"
#include <QSharedData>
#include "util/paramset.h"
#include <QSet>
#include "taskgroup.h"
#include "requestformfield.h"
#include <QList>
#include "modelview/shareduiitem.h"

class TaskData;
class QDebug;
class PfNode;
class Trigger;
class CronTrigger;
class NoticeTrigger;
class Scheduler;
class EventSubscription;
class Step;
class WorkflowTriggerSubscriptionData;
class StepInstance;
class Calendar;

/** Data holder for the association between a workflow trigger and actions to
  * be performed. */
class LIBQRONSHARED_EXPORT WorkflowTriggerSubscription {
private:
  QSharedDataPointer<WorkflowTriggerSubscriptionData> d;

public:
  WorkflowTriggerSubscription();
  WorkflowTriggerSubscription(Trigger trigger,
      EventSubscription eventSubscription);
  WorkflowTriggerSubscription(const WorkflowTriggerSubscription &other);
  ~WorkflowTriggerSubscription();
  WorkflowTriggerSubscription &operator=(
      const WorkflowTriggerSubscription &other);
  Trigger trigger() const;
  EventSubscription eventSubscription() const;
};

/** Core task definition object, being it a standalone task or workflow. */
class LIBQRONSHARED_EXPORT Task
    : public SharedUiItem, public ParamsProvider {
public:
  enum DiscardAliasesOnStart { DiscardNone, DiscardAll, DiscardUnknown };
  Task();
  Task(const Task &other);
  Task(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
       QHash<QString,Task> oldTasks, QString supertaskId,
       QHash<QString, Calendar> namedCalendars);
  /** Should only be used by SharedUiItemsModels to get size and headers from
   * a non-null item. */
  static Task templateTask();
  ~Task();
  Task &operator=(const Task &other) {
    SharedUiItem::operator=(other); return *this; }
  ParamSet params() const;
  void setParentParams(ParamSet parentParams);
  /** localId within group */
  QString shortId() const;
  QString label() const;
  QString mean() const;
  QString command() const;
  QString target() const;
  void setTarget(QString target);
  QString info() const;
  TaskGroup taskGroup() const;
  /** Resources consumed. */
  QHash<QString, qint64> resources() const;
  QString resourcesAsString() const;
  QDateTime lastExecution() const;
  void setLastExecution(QDateTime timestamp) const;
  QDateTime nextScheduledExecution() const;
  void setNextScheduledExecution(QDateTime timestamp) const;
  /** Maximum allowed simultaneous instances (includes running and queued
    * instances). Default: 1. */
  int maxInstances() const;
  /** Current intances count (includes running and queued instances). */
  int instancesCount() const;
  /** Atomic fetch-and-add of the current instances count. */
  int fetchAndAddInstancesCount(int valueToAdd) const;
  QList<QRegExp> stderrFilters() const;
  void appendStderrFilter(QRegExp filter);
  void triggerStartEvents(TaskInstance instance) const;
  void triggerSuccessEvents(TaskInstance instance) const;
  void triggerFailureEvents(TaskInstance instance) const;
  QList<EventSubscription> onstartEventSubscriptions() const;
  QList<EventSubscription> onsuccessEventSubscriptions() const;
  QList<EventSubscription> onfailureEventSubscriptions() const;
  /** Events hash with "onsuccess", "onfailure"... key, mainly for UI purpose.
   * Not including group events subscriptions. */
  QList<EventSubscription> allEventsSubscriptions() const;
  bool enabled() const;
  void setEnabled(bool enabled) const;
  bool lastSuccessful() const;
  void setLastSuccessful(bool successful) const;
  int lastReturnCode() const;
  void setLastReturnCode(int code) const;
  int lastTotalMillis() const;
  void setLastTotalMillis(int lastTotalMillis) const;
  /** in millis, LLONG_MAX if not set */
  long long maxExpectedDuration() const;
  /** in millis, 0 if not set */
  long long minExpectedDuration() const;
  /** in millis, LLONG_MAX if not set */
  long long maxDurationBeforeAbort() const;
  ParamSet setenv() const;
  ParamSet unsetenv() const;
  DiscardAliasesOnStart discardAliasesOnStart() const;
  inline QString discardAliasesOnStartAsString() const {
    return discardAliasesOnStartAsString(discardAliasesOnStart()); }
  static QString discardAliasesOnStartAsString(DiscardAliasesOnStart v);
  static DiscardAliasesOnStart discardAliasesOnStartFromString(QString v);
  QList<RequestFormField> requestFormFields() const;
  QString requestFormFieldsAsHtmlDescription() const;
  /** give only access to ! pseudo params, not to task params */
  QVariant paramValue(QString key, QVariant defaultValue = QVariant()) const;
  /** Human readable list of all triggers as one string, for UI purpose. */
  QString triggersAsString() const;
  QString triggersWithCalendarsAsString() const;
  bool triggersHaveCalendar() const;
  /** Cron triggers */
  QList<CronTrigger> cronTriggers() const;
  /** Notice triggers */
  QList<NoticeTrigger> noticeTriggers() const;
  /** Human readable list of other triggers, i.e. indirect triggers such
   * as the one implied by (onsuccess(requesttask foo)) on task bar.
   * Note that not all indirect triggers can be listed here since some cannot
   * be predicted, e.g. (onsuccess(requesttask %{baz})). Only predictable ones
   * are listed. */
  QStringList otherTriggers() const;
  void appendOtherTriggers(QString text);
  void clearOtherTriggers();
  /** Workflow steps. Empty list for standalone tasks. */
  QHash<QString,Step> steps() const;
  QStringList startSteps() const;
  /** Parent task (e.g. workflow task) to which this task belongs, if any. */
  QString supertaskId() const;
  QString workflowDiagram() const;
  QHash<QString,WorkflowTriggerSubscription> workflowTriggerSubscriptionsById() const;
  QMultiHash<QString,WorkflowTriggerSubscription> workflowTriggerSubscriptionsByNotice() const;
  QHash<QString,CronTrigger> workflowCronTriggersById() const;
  PfNode toPfNode() const;

private:
  TaskData *td();
  const TaskData *td() const { return (const TaskData*)constData(); }
};

#endif // TASK_H
