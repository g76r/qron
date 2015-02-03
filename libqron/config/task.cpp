/* Copyright 2012-2015 Hallowyn and others.
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
#include "task.h"
#include "taskgroup.h"
#include <QString>
#include <QHash>
#include "pf/pfnode.h"
#include "trigger/crontrigger.h"
#include "log/log.h"
#include <QAtomicInt>
#include <QPointer>
#include "sched/scheduler.h"
#include "config/configutils.h"
#include "requestformfield.h"
#include "util/htmlutils.h"
#include "step.h"
#include "action/action.h"
#include "trigger/noticetrigger.h"
#include "config/eventsubscription.h"
#include "sched/stepinstance.h"
#include "ui/graphvizdiagramsbuilder.h"
#include "ui/qronuiutils.h"
#include "task_p.h"

class WorkflowTriggerSubscriptionData : public QSharedData {
public:
  Trigger _trigger;
  EventSubscription _eventSubscription;
  WorkflowTriggerSubscriptionData(
      Trigger trigger = Trigger(),
      EventSubscription eventSubscription = EventSubscription())
    : _trigger(trigger), _eventSubscription(eventSubscription) { }
};

WorkflowTriggerSubscription::WorkflowTriggerSubscription() {
}

WorkflowTriggerSubscription::WorkflowTriggerSubscription(
    Trigger trigger, EventSubscription eventSubscription)
  : d(new WorkflowTriggerSubscriptionData(trigger, eventSubscription)) {
}

WorkflowTriggerSubscription::WorkflowTriggerSubscription(
    const WorkflowTriggerSubscription &other) : d(other.d) {
}

WorkflowTriggerSubscription::~WorkflowTriggerSubscription() {
}

WorkflowTriggerSubscription &WorkflowTriggerSubscription::operator=(
    const WorkflowTriggerSubscription &other) {
  if (this != &other)
    d = other.d;
  return *this;
}

Trigger WorkflowTriggerSubscription::trigger() const {
  return d ? d->_trigger : Trigger();
}

EventSubscription WorkflowTriggerSubscription::eventSubscription() const {
  return d ? d->_eventSubscription : EventSubscription();
}

class TaskData : public SharedUiItemData {
public:
  QString _id, _shortId, _label, _command, _target, _info;
  Task::Mean _mean;
  TaskGroup _group;
  ParamSet _params, _setenv, _unsetenv;
  QList<NoticeTrigger> _noticeTriggers;
  QHash<QString,qint64> _resources;
  int _maxInstances;
  QList<CronTrigger> _cronTriggers;
  QList<QRegExp> _stderrFilters;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QPointer<Scheduler> _scheduler;
  long long _maxExpectedDuration, _minExpectedDuration, _maxDurationBeforeAbort;
  Task::DiscardAliasesOnStart _discardAliasesOnStart;
  QList<RequestFormField> _requestFormFields;
  QStringList _otherTriggers; // guessed indirect triggers, resulting from events
  QString _supertaskId;
  QHash<QString,Step> _steps;
  QSet<QString> _startSteps;
  QString _workflowDiagram;
  QHash<QString,WorkflowTriggerSubscription> _workflowTriggerSubscriptionsById;
  QMultiHash<QString,WorkflowTriggerSubscription> _workflowTriggerSubscriptionsByNotice;
  QHash<QString,CronTrigger> _workflowCronTriggersById;
  // note: since QDateTime (as most Qt classes) is not thread-safe, it cannot
  // be used in a mutable QSharedData field as soon as the object embedding the
  // QSharedData is used by several thread at a time, hence the qint64
  mutable qint64 _lastExecution, _nextScheduledExecution;
  mutable QAtomicInt _instancesCount;
  mutable bool _enabled, _lastSuccessful;
  mutable int _lastReturnCode, _lastTotalMillis;

  TaskData() : _maxExpectedDuration(LLONG_MAX), _minExpectedDuration(0),
    _maxDurationBeforeAbort(LLONG_MAX),
    _discardAliasesOnStart(Task::DiscardAll),
    _lastExecution(LLONG_MIN), _nextScheduledExecution(LLONG_MIN),
    _enabled(true), _lastSuccessful(true), _lastReturnCode(-1),
    _lastTotalMillis(-1)  { }
  QDateTime lastExecution() const;
  QDateTime nextScheduledExecution() const;
  QString resourcesAsString() const;
  QString triggersAsString() const;
  QString triggersWithCalendarsAsString() const;
  bool triggersHaveCalendar() const;
  QVariant uiData(int section, int role) const;
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 int role);
  Qt::ItemFlags uiFlags(int section) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  void setId(QString id) { _id = id; }
  QString idQualifier() const { return "task"; }
};

Task::Task() {
  //Log::fatal() << "Task::Task " << (sizeof(Task)) << " " << (sizeof(TaskData));
}

Task::Task(const Task &other) : SharedUiItem(other) {
  //Log::fatal() << "Task::Task copying " << other.id() << " " << other.td()
  //             << " " << ((const Task*)this)->td();
}

Task::Task(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QString supertaskId, QHash<QString,Calendar> namedCalendars) {
  TaskData *td = new TaskData;
  td->_scheduler = scheduler;
  td->_shortId =
      ConfigUtils::sanitizeId(node.contentAsString(),
                              supertaskId.isEmpty() ? ConfigUtils::TaskId
                                                    : ConfigUtils::SubTaskId);
  td->_label = node.attribute("label");
  td->_mean = meanFromString(node.attribute("mean", "local").trimmed()
                             .toLower());
  if (td->_mean == UnknownMean) {
    Log::error() << "task with invalid execution mean: "
                 << node.toString();
    delete td;
    return;
  }
  td->_command = node.attribute("command");
  td->_target =
      ConfigUtils::sanitizeId(node.attribute("target"), ConfigUtils::GroupId);
  // silently use "localhost" as target for "local" mean
  if (td->_target.isEmpty() && td->_mean == Local)
    td->_target = "localhost";
  td->_info = node.stringChildrenByName("info").join(" ");
  td->_id = taskGroup.id()+"."+td->_shortId;
  td->_group = taskGroup;
  td->_maxInstances = node.attribute("maxinstances", "1").toInt();
  if (td->_maxInstances <= 0) {
    Log::error() << "invalid task maxinstances: " << node.toPf();
    delete td;
    return;
  }
  double f = node.doubleAttribute("maxexpectedduration", -1);
  td->_maxExpectedDuration = f < 0 ? LLONG_MAX : (long long)(f*1000);
  f = node.doubleAttribute("minexpectedduration", -1);
  td->_minExpectedDuration = f < 0 ? 0 : (long long)(f*1000);
  f = node.doubleAttribute("maxdurationbeforeabort", -1);
  td->_maxDurationBeforeAbort = f < 0 ? LLONG_MAX : (long long)(f*1000);
  td->_params.setParent(taskGroup.params());
  ConfigUtils::loadParamSet(node, &td->_params, "param");
  QString filter = td->_params.value("stderrfilter");
  if (!filter.isEmpty())
    td->_stderrFilters.append(QRegExp(filter));
  td->_setenv.setParent(taskGroup.setenv());
  ConfigUtils::loadParamSet(node, &td->_setenv, "setenv");
  td->_unsetenv.setParent(taskGroup.unsetenv());
  ConfigUtils::loadFlagSet(node, &td->_unsetenv, "unsetenv");
  td->_supertaskId = supertaskId;
  // LATER load cron triggers last exec timestamp from on-disk log
  if (supertaskId.isEmpty()) { // subtasks do not have triggers
    foreach (PfNode child, node.childrenByName("trigger")) {
      foreach (PfNode grandchild, child.children()) {
        QString content = grandchild.contentAsString();
        QString triggerType = grandchild.name();
        if (triggerType == "notice") {
          NoticeTrigger trigger(grandchild, namedCalendars);
          if (trigger.isValid()) {
            td->_noticeTriggers.append(trigger);
            Log::debug() << "configured notice trigger '" << content
                         << "' on task '" << td->_shortId << "'";
          } else {
            Log::error() << "task with invalid notice trigger: "
                         << node.toString();
            delete td;
            return;
          }
        } else if (triggerType == "cron") {
          CronTrigger trigger(grandchild, namedCalendars);
          if (trigger.isValid()) {
            td->_cronTriggers.append(trigger);
            Log::debug() << "configured cron trigger "
                         << trigger.humanReadableExpression()
                         << " on task " << td->_shortId;
          } else {
            Log::error() << "task with invalid cron trigger: "
                         << node.toString();
            delete td;
            return;
          }
          // LATER read misfire config
        } else {
          Log::warning() << "ignoring unknown trigger type '" << triggerType
                         << "' on task " << td->_shortId;
        }
      }
    }
  } else {
    if (node.hasChild("trigger"))
      Log::warning() << "ignoring trigger in workflow subtask: "
                     << node.toString();
  }
  ConfigUtils::loadResourcesSet(node, &td->_resources, "resource");
  QString daos = node.attribute("discardaliasesonstart", "all");
  td->_discardAliasesOnStart = discardAliasesOnStartFromString(daos);
  if (td->_discardAliasesOnStart == Task::DiscardUnknown) {
    Log::error() << "invalid discardaliasesonstart on task " << td->_shortId
                 << ": '" << daos << "'";
    delete td;
    return;
  }
  QList<PfNode> children = node.childrenByName("requestform");
  if (!children.isEmpty()) {
    if (children.size() > 1) {
      Log::error() << "task with several requestform: " << node.toString();
      delete td;
      return;
    }
    foreach (PfNode child, children.last().childrenByName("field")) {
      RequestFormField field(child);
      if (!field.isNull())
        td->_requestFormFields.append(field);
    }
  }
  ConfigUtils::loadEventSubscription(node, "onstart", td->_id, &td->_onstart,
                                     scheduler);
  ConfigUtils::loadEventSubscription(node, "onsuccess", td->_id,
                                     &td->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", td->_id,
                                     &td->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfailure", td->_id,
                                     &td->_onfailure, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", td->_id,
                                     &td->_onfailure, scheduler);
  QList<PfNode> steps = node.childrenByName("subtask")
      +node.childrenByName("and")+node.childrenByName("or");
  if (td->_mean == Workflow) {
    if (steps.isEmpty())
      Log::warning() << "workflow task with no step: " << node.toString();
    if (!supertaskId.isNull()) {
      Log::error() << "workflow task not allowed as a workflow subtask: "
                   << node.toString();
      delete td;
      return;
    }
    foreach (PfNode child, steps) {
      Step step(child, scheduler, taskGroup, td->_shortId, namedCalendars);
      if (step.isNull()) {
        Log::error() << "workflow task " << td->_id
                     << " has at less one invalid step definition";
        delete td;
        return;
      } if (td->_steps.contains(step.id())) {
        Log::error() << "workflow task " << td->_id
                     << " has duplicate steps with id " << step.id();
        delete td;
        return;
      } else {
        td->_steps.insert(step.id(), step);
      }
    }
    td->_startSteps = node.stringListAttribute("start").toSet();
    foreach (QString id, td->_startSteps)
      if (!td->_steps.contains(id)) {
        Log::error() << "workflow task " << td->_id
                     << " has invalid or lacking start steps: "
                     << node.toString();
        delete td;
        return;
      }
    int tsCount = 0;
    QStringList ignoredChildren;
    ignoredChildren << "cron" << "notice";
    foreach (PfNode child, node.childrenByName("ontrigger")) {
      EventSubscription es(td->_id, child, scheduler, ignoredChildren);
      if (es.isNull() || es.actions().isEmpty()) {
        Log::warning() << "ignoring invalid or empty ontrigger: "
                       << node.toString();
        continue;
      }
      foreach (PfNode grandchild, child.childrenByName("cron")) {
        QString tsId = QString::number(tsCount++);
        CronTrigger trigger(grandchild, namedCalendars);
        if (trigger.isValid()) {
          td->_workflowCronTriggersById.insert(tsId, trigger);
          td->_workflowTriggerSubscriptionsById.insert(
                tsId, WorkflowTriggerSubscription(trigger, es));
        } else
          Log::warning() << "ignoring invalid cron trigger in ontrigger: "
                         << node.toString();
      }
      foreach (PfNode grandchild, child.childrenByName("notice")) {
        QString tsId = QString::number(tsCount++);
        NoticeTrigger trigger(grandchild, namedCalendars);
        if (trigger.isValid()) {
          td->_workflowTriggerSubscriptionsByNotice
              .insert(trigger.expression(),
                      WorkflowTriggerSubscription(trigger, es));
          td->_workflowTriggerSubscriptionsById.insert(
                tsId, WorkflowTriggerSubscription(trigger, es));
        } else
          Log::warning() << "ignoring invalid notice trigger in ontrigger: "
                         << node.toString();
      }
    }
    // Note about predecessors' transitionId conventions:
    // There are several kind of coexisting non-overlaping transitionId types
    // 1) Start transition:
    //     format: ${workflow_id}|start|${step_id}
    //     e.g. app1.group1.workflow1|start|step1
    // 2) Subtask to any step transition:
    //     format: ${source_id}|${event_name}|${target_step_id}
    //     e.g. app1.group1.workflow1-step1|onfinish|step2
    //     As a special case, if event name is "onsuccess" or "onfailure" it
    //     will be replaced with "onfinish" to implictely make more intuitive
    //     cases where both onsucces and onfailure would have been connected
    //     to the same and join (which is common since writing "onfinish" in
    //     a configuration file is actually implemented as subcribing to both
    //     onsuccess and onfailure events).
    // 3) Join to any step transition:
    //    format: ${source_fqsn}|${event_name}|${target_step_id}
    //    e.g. app1.group1.workflow1:step2|onready|step3
    //    Currently, the only event available in a join step is onready.
    // 4) End transition:
    //    format: ${source_id_or_id}|${event_name}|$end
    //    e.g. app1.group1.workflow1-step3|onfinish|$end
    //         app1.group1.workflow1:step4|onready|$end
    foreach (QString id, td->_startSteps) {
      td->_steps[id].insertPredecessor(td->_id+"|start|"+id);
      Log::debug() << "adding predecessor " << td->_id+"|start|"+id
                   << " to step " << td->_steps[id].fqsn();
    }
    foreach (QString source, td->_steps.keys()) {
      QList<EventSubscription> subList
          = td->_steps[source].onreadyEventSubscriptions()
          + td->_steps[source].subtask().allEventsSubscriptions()
          + td->_steps[source].subtask().taskGroup().allEventSubscriptions();
      foreach (EventSubscription es, subList) {
        foreach (Action a, es.actions()) {
          if (a.actionType() == "step") {
            QString target = a.targetName();
            QString eventName = es.eventName();
            if (eventName == "onsuccess" || eventName == "onfailure")
              eventName = "onfinish";
            QString transitionId;
            if (td->_steps[source].kind() == Step::SubTask) // task id
              transitionId = td->_id+"-"+source+"|"+eventName+"|"+target;
            else // fqsn
              transitionId = td->_id+":"+source+"|"+eventName+"|"+target;
            if (td->_steps.contains(target)) {
              Log::debug() << "registring predecessor " << transitionId
                           << " to step " << td->_steps[target].fqsn();
              td->_steps[target].insertPredecessor(transitionId);
            } else {
              Log::error() << "cannot register predecessor " << transitionId
                           << " with unknown target to workflow " << td->_id;
              delete td;
              return;
            }
          }
        }
      }
    }
    // TODO reject workflows w/ no start edge
    // TODO reject workflows w/ step w/o predecessors
    // TODO reject workflows w/ step w/o successors, at less on trivial cases (e.g. neither step nor end in onfailure)
  } else {
    if (!steps.isEmpty() || node.hasChild("start"))
      Log::warning() << "ignoring step definitions in non-workflow task: "
                     << node.toString();
  }
  setData(td);
  td->_workflowDiagram = GraphvizDiagramsBuilder::workflowTaskDiagram(*this);
}

void Task::copyLiveAttributesFromOldTask(Task oldTask) {
  TaskData *td = this->td();
  if (!td || oldTask.isNull())
    return;
  // copy mutable fields from old task (excepted _nextScheduledExecution)
  td->_lastExecution = oldTask.lastExecution().isValid()
      ? oldTask.lastExecution().toMSecsSinceEpoch() : LLONG_MIN;
  td->_instancesCount = oldTask.instancesCount();
  td->_lastSuccessful = oldTask.lastSuccessful();
  td->_lastReturnCode = oldTask.lastReturnCode();
  td->_lastTotalMillis = oldTask.lastTotalMillis();
  td->_enabled = oldTask.enabled();
  // keep last triggered timestamp from previously defined trigger
  QHash<QString,CronTrigger> oldCronTriggers;
  foreach (const CronTrigger trigger, oldTask.td()->_cronTriggers)
    oldCronTriggers.insert(trigger.canonicalExpression(), trigger);
  for (int i = 0; i < td->_cronTriggers.size(); ++i) {
    CronTrigger &trigger = td->_cronTriggers[i];
    CronTrigger oldTrigger =
        oldCronTriggers.value(trigger.canonicalExpression());
    if (oldTrigger.isValid())
      trigger.setLastTriggered(oldTrigger.lastTriggered());
  }
}

Task Task::templateTask() {
  Task t;
  t.setData(new TaskData);
  return t;
}

ParamSet Task::params() const {
  return !isNull() ? td()->_params : ParamSet();
}

QList<NoticeTrigger> Task::noticeTriggers() const {
  return !isNull() ? td()->_noticeTriggers : QList<NoticeTrigger>();
}

QString Task::shortId() const {
  return !isNull() ? td()->_shortId : QString();
}

QString Task::label() const {
  return !isNull() ? (td()->_label.isNull() ? td()->_shortId : td()->_label)
                   : QString();
}

Task::Mean Task::mean() const {
  return !isNull() ? td()->_mean : UnknownMean;
}

QString Task::command() const {
  return !isNull() ? td()->_command : QString();
}

QString Task::target() const {
  return !isNull() ? td()->_target : QString();
}

void Task::setTarget(QString target) {
  if (!isNull())
    td()->_target = target;
}

QString Task::info() const {
  return !isNull() ? td()->_info : QString();
}

TaskGroup Task::taskGroup() const {
  return !isNull() ? td()->_group : TaskGroup();
}

QHash<QString, qint64> Task::resources() const {
  return !isNull() ? td()->_resources : QHash<QString,qint64>();
}

QString Task::resourcesAsString() const {
  return !isNull() ? td()->resourcesAsString() : QString();
}

QString TaskData::resourcesAsString() const {
  return QronUiUtils::resourcesAsString(_resources);
}

QString Task::triggersAsString() const {
  return !isNull() ? td()->triggersAsString() : QString();
}

QString TaskData::triggersAsString() const {
  QString s;
  foreach (CronTrigger t, _cronTriggers)
    s.append(t.humanReadableExpression()).append(' ');
  foreach (NoticeTrigger t, _noticeTriggers)
    s.append(t.humanReadableExpression()).append(' ');
  foreach (QString t, _otherTriggers)
    s.append(t).append(' ');
  s.chop(1); // remove last space
  return s;
}

QString Task::triggersWithCalendarsAsString() const {
  return !isNull() ? td()->triggersWithCalendarsAsString() : QString();
}

QString TaskData::triggersWithCalendarsAsString() const {
  QString s;
  foreach (CronTrigger t, _cronTriggers)
    s.append(t.humanReadableExpressionWithCalendar()).append(' ');
  foreach (NoticeTrigger t, _noticeTriggers)
    s.append(t.humanReadableExpressionWithCalendar()).append(' ');
  foreach (QString t, _otherTriggers)
    s.append(t).append(" ");
  s.chop(1); // remove last space
  return s;
}

bool Task::triggersHaveCalendar() const {
  return !isNull() ? td()->triggersHaveCalendar() : false;
}

bool TaskData::triggersHaveCalendar() const {
  foreach (CronTrigger t, _cronTriggers)
    if (!t.calendar().isNull())
      return true;
  foreach (NoticeTrigger t, _noticeTriggers)
    if (!t.calendar().isNull())
      return true;
  return false;
}

QDateTime Task::lastExecution() const {
  return !isNull() ? td()->lastExecution() : QDateTime();
}

QDateTime TaskData::lastExecution() const {
  return _lastExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(_lastExecution) : QDateTime();
}

QDateTime Task::nextScheduledExecution() const {
  return !isNull() ? td()->nextScheduledExecution() : QDateTime();
}

QDateTime TaskData::nextScheduledExecution() const {
  return _nextScheduledExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(_nextScheduledExecution) : QDateTime();
}

void Task::setLastExecution(QDateTime timestamp) const {
  if (!isNull())
    td()->_lastExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

void Task::setNextScheduledExecution(QDateTime timestamp) const {
  if (!isNull())
    td()->_nextScheduledExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

int Task::maxInstances() const {
  return !isNull() ? td()->_maxInstances : 0;
}

int Task::instancesCount() const {
  return !isNull() ? td()->_instancesCount.load() : 0;
}

int Task::fetchAndAddInstancesCount(int valueToAdd) const {
  return !isNull() ? td()->_instancesCount.fetchAndAddOrdered(valueToAdd) : 0;
}

QList<QRegExp> Task::stderrFilters() const {
  return !isNull() ? td()->_stderrFilters : QList<QRegExp>();
}

void Task::appendStderrFilter(QRegExp filter) {
  if (!isNull())
    td()->_stderrFilters.append(filter);
}

void Task::triggerStartEvents(TaskInstance instance) const {
  if (!isNull()) {
    td()->_group.triggerStartEvents(instance);
    foreach (EventSubscription sub, td()->_onstart)
      sub.triggerActions(instance);
  }
}

void Task::triggerSuccessEvents(TaskInstance instance) const {
  if (!isNull()) {
    td()->_group.triggerSuccessEvents(instance);
    foreach (EventSubscription sub, td()->_onsuccess)
      sub.triggerActions(instance);
  }
}

void Task::triggerFailureEvents(TaskInstance instance) const {
  if (!isNull()) {
    td()->_group.triggerFailureEvents(instance);
    foreach (EventSubscription sub, td()->_onfailure)
      sub.triggerActions(instance);
  }
}

QList<EventSubscription> Task::onstartEventSubscriptions() const {
  return !isNull() ? td()->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> Task::onsuccessEventSubscriptions() const {
  return !isNull() ? td()->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> Task::onfailureEventSubscriptions() const {
  return !isNull() ? td()->_onfailure : QList<EventSubscription>();
}

QList<EventSubscription> Task::allEventsSubscriptions() const {
  // LATER avoid creating the collection at every call
  return !isNull() ? td()->_onstart + td()->_onsuccess + td()->_onfailure
                   : QList<EventSubscription>();
}

bool Task::enabled() const {
  return !isNull() ? td()->_enabled : false;
}
void Task::setEnabled(bool enabled) const {
  if (!isNull())
    td()->_enabled = enabled;
}

bool Task::lastSuccessful() const {
  return !isNull() ? td()->_lastSuccessful : false;
}

void Task::setLastSuccessful(bool successful) const {
  if (!isNull())
    td()->_lastSuccessful = successful;
}

int Task::lastReturnCode() const {
  return !isNull() ? td()->_lastReturnCode : -1;
}

void Task::setLastReturnCode(int code) const {
  if (!isNull())
    td()->_lastReturnCode = code;
}

int Task::lastTotalMillis() const {
  return !isNull() ? td()->_lastTotalMillis : -1;
}

void Task::setLastTotalMillis(int lastTotalMillis) const {
  if (!isNull())
    td()->_lastTotalMillis = lastTotalMillis;
}

long long Task::maxExpectedDuration() const {
  return !isNull() ? td()->_maxExpectedDuration : LLONG_MAX;
}
long long Task::minExpectedDuration() const {
  return !isNull() ? td()->_minExpectedDuration : 0;
}

long long Task::maxDurationBeforeAbort() const {
  return !isNull() ? td()->_maxDurationBeforeAbort : LLONG_MAX;
}

ParamSet Task::setenv() const {
  return !isNull() ? td()->_setenv : ParamSet();
}

ParamSet Task::unsetenv() const {
  return !isNull() ? td()->_unsetenv : ParamSet();
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStart() const {
  return !isNull() ? td()->_discardAliasesOnStart : Task::DiscardNone;
}

QString Task::discardAliasesOnStartAsString(Task::DiscardAliasesOnStart v) {
  switch (v) {
  case Task::DiscardNone:
    return "none";
  case Task::DiscardAll:
    return "all";
  case Task::DiscardUnknown:
    ;
  }
  return "unknown"; // should never happen
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStartFromString(QString v) {
  if (v == "none")
    return Task::DiscardNone;
  if (v == "all")
    return Task::DiscardAll;
  return Task::DiscardUnknown;
}

QList<RequestFormField> Task::requestFormFields() const {
  return !isNull() ? td()->_requestFormFields : QList<RequestFormField>();
}

QString Task::requestFormFieldsAsHtmlDescription() const {
  QList<RequestFormField> list = requestFormFields();
  if (list.isEmpty())
    return "(none)";
  QString v;
  foreach (const RequestFormField rff, list)
    v.append(rff.toHtmlHumanReadableDescription());
  return v;
}

QVariant TaskPseudoParamsProvider::paramValue(
    QString key, QVariant defaultValue) const {
  //Log::fatal() << "TaskPseudoParamsProvider::paramValue " << key;
  if (key.startsWith('!')) {
    if (key == "!taskshortid") {
      return _task.shortId();
    } else if (key == "!taskid") {
      return _task.id();
    } else if (key == "!taskgroupid") {
      return _task.taskGroup().id();
    } else if (key == "!target") {
      return _task.target();
    } else if (key == "!minexpectedms") {
      return _task.minExpectedDuration();
    } else if (key == "!minexpecteds") {
      return _task.minExpectedDuration()/1000.0;
    } else if (key == "!maxexpectedms") {
      long long ms = _task.maxExpectedDuration();
      return (ms == LLONG_MAX) ? defaultValue : ms;
    } else if (key == "!maxexpecteds") {
      long long ms = _task.maxExpectedDuration();
      return (ms == LLONG_MAX) ? defaultValue : ms/1000.0;
    } else if (key == "!maxbeforeabortms") {
      long long ms = _task.maxDurationBeforeAbort();
      return (ms == LLONG_MAX) ? defaultValue : ms;
    } else if (key == "!maxbeforeaborts") {
      long long ms = _task.maxDurationBeforeAbort();
      return (ms == LLONG_MAX) ? defaultValue : ms/1000.0;
    } else if (key == "!maxexpectedms0") {
      long long ms = _task.maxExpectedDuration();
      return (ms == LLONG_MAX) ? 0 : ms;
    } else if (key == "!maxexpecteds0") {
      long long ms = _task.maxExpectedDuration();
      return (ms == LLONG_MAX) ? 0.0 : ms/1000.0;
    } else if (key == "!maxbeforeabortms0") {
      long long ms = _task.maxDurationBeforeAbort();
      return (ms == LLONG_MAX) ? 0 : ms;
    } else if (key == "!maxbeforeaborts0") {
      long long ms = _task.maxDurationBeforeAbort();
      return (ms == LLONG_MAX) ? 0.0 : ms/1000.0;
    }
  }
  return defaultValue;
}

QList<CronTrigger> Task::cronTriggers() const {
  return !isNull() ? td()->_cronTriggers : QList<CronTrigger>();
}

QStringList Task::otherTriggers() const {
  return !isNull() ? td()->_otherTriggers : QStringList();
}

void Task::appendOtherTriggers(QString text) {
  if (!isNull())
    td()->_otherTriggers.append(text);
}

void Task::clearOtherTriggers() {
  if (!isNull())
    td()->_otherTriggers.clear();
}

QHash<QString, Step> Task::steps() const {
  return !isNull() ? td()->_steps : QHash<QString,Step>();
}

QString Task::supertaskId() const {
  return !isNull() ? td()->_supertaskId : QString();
}

QSet<QString> Task::startSteps() const {
  return !isNull() ? td()->_startSteps : QSet<QString>();
}

QString Task::workflowDiagram() const {
  return !isNull() ? td()->_workflowDiagram : QString();
}

QHash<QString,WorkflowTriggerSubscription>
Task::workflowTriggerSubscriptionsById() const {
  return !isNull() ? td()->_workflowTriggerSubscriptionsById
                   : QHash<QString,WorkflowTriggerSubscription>();
}

QMultiHash<QString, WorkflowTriggerSubscription>
Task::workflowTriggerSubscriptionsByNotice() const {
  return !isNull() ? td()->_workflowTriggerSubscriptionsByNotice
                   : QMultiHash<QString,WorkflowTriggerSubscription>();
}

QHash<QString,CronTrigger> Task::workflowCronTriggersById() const {
  return !isNull() ? td()->_workflowCronTriggersById
                   : QHash<QString,CronTrigger>();
}

QVariant TaskData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int TaskData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

QVariant TaskData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      if (role == Qt::EditRole) {
        // remove workflow id prefix from shortid
        return _supertaskId.isEmpty() ? _shortId
                                      : _shortId.mid(_shortId.indexOf(':')+1);
      }
      return _shortId;
    case 1:
      return _group.id();
    case 2:
      if (role == Qt::EditRole)
        return _label == _shortId ? QVariant() : _label;
      return _label.isEmpty() ? _shortId : _label;
    case 3:
      return Task::meanAsString(_mean);
    case 4:
      return _command;
    case 5:
      return _target;
    case 6:
      return triggersAsString();
    case 7:
      return _params.toString(false, false);
    case 8:
      return resourcesAsString();
    case 9:
      return lastExecution().toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 10:
      return nextScheduledExecution().toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 11:
      return _id;
    case 12:
      return _maxInstances;
    case 13:
      return _instancesCount.load();
    case 14:
      return EventSubscription::toStringList(_onstart).join("\n");
    case 15:
      return EventSubscription::toStringList(_onsuccess).join("\n");
    case 16:
      return EventSubscription::toStringList(_onfailure).join("\n");
    case 17:
      return QString::number(_instancesCount.load())+" / "
          +QString::number(_maxInstances);
    case 18:
      return QVariant(); // custom actions, handled by the model, if needed
    case 19: {
      QDateTime dt = lastExecution();
      if (dt.isNull())
        return QVariant();
      QString returnCode = QString::number(_lastReturnCode);
      QString returnCodeLabel =
          _params.value("return.code."+returnCode+".label");
      QString s = dt.toString("yyyy-MM-dd hh:mm:ss,zzz")
          .append(_lastSuccessful ? " success" : " failure")
          .append(" (code ").append(returnCode);
      if (!returnCodeLabel.isEmpty())
        s.append(" : ").append(returnCodeLabel);
      return s.append(')');
    }
    case 20:
      return QronUiUtils::sysenvAsString(_setenv, _unsetenv);
    case 21:
      return QronUiUtils::paramsAsString(_setenv);
    case 22:
      return QronUiUtils::paramsKeysAsString(_unsetenv);
    case 23:
      return (_minExpectedDuration > 0)
          ? QString::number(_minExpectedDuration*.001) : QString();
    case 24:
      return (_maxExpectedDuration < LLONG_MAX)
          ? QString::number(_maxExpectedDuration*.001) : QString();
    case 25: {
      QString s;
      foreach (const RequestFormField rff, _requestFormFields)
        s.append(rff.id()).append(' ');
      s.chop(1);
      return s;
    }
    case 26:
      return _lastTotalMillis >= 0
          ? QString::number(_lastTotalMillis/1000.0) : QString();
    case 27:
      return (_maxDurationBeforeAbort < LLONG_MAX)
          ? QString::number(_maxDurationBeforeAbort*.001) : QString();
    case 28:
      return triggersWithCalendarsAsString();
    case 29:
      return _enabled;
    case 30:
      return triggersHaveCalendar();
    case 31:
      return _supertaskId;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

bool Task::setUiData(int section, const QVariant &value, QString *errorString,
                     int role) {
  if (isNull())
    return false;
  detach<TaskData>();
  return ((TaskData*)constData())->setUiData(section, value, errorString, role);
}

void Task::setSuperTaskId(QString supertaskId) {
  if (!isNull()) {
    detach<TaskData>();
    TaskData *d = ((TaskData*)constData());
    d->_supertaskId = supertaskId;
    d->_shortId = supertaskId.mid(supertaskId.lastIndexOf('.')+1)+":"
        +d->_shortId.mid(d->_shortId.indexOf(':')+1);
  }
}

bool TaskData::setUiData(int section, const QVariant &value,
                         QString *errorString, int role) {
  if (role != Qt::EditRole) {
    if (errorString)
      *errorString = "cannot set other role than EditRole";
    return false;
  }
  switch(section) {
  case 0:
    if (value.toString().isEmpty()) {
      if (errorString)
        *errorString = "id cannot be empty";
      return false;
    }
    _shortId = ConfigUtils::sanitizeId(value.toString(), ConfigUtils::TaskId);
    if (!_supertaskId.isEmpty())
      _shortId = _supertaskId.mid(_supertaskId.lastIndexOf('.')+1)+":"+_shortId;
    _id = _group.id()+"."+_shortId;
    return true;
  case 2:
    _label = value.toString().trimmed();
    if (_label == _shortId)
      _label = QString();
    return true;
  case 3: {
    Task::Mean mean = Task::meanFromString(value.toString().toLower()
                                           .trimmed());
    if (mean == Task::UnknownMean) {
      if (errorString)
        *errorString = "unknown mean value: '"+value.toString()+"'";
      return false;
    }
    _mean = mean;
    return true;
  }
  case 5:
    _target = ConfigUtils::sanitizeId(value.toString(), ConfigUtils::GroupId);
    return true;
  case 8: {
    QHash<QString,qint64> resources;
    if (QronUiUtils::resourcesFromString(value.toString(), &resources,
                                            errorString)) {
      _resources = resources;
      return true;
    }
    return false;
  }
  }
  if (errorString)
    *errorString = "field \""+uiHeaderData(section, Qt::DisplayRole).toString()
      +"\" is not ui-editable";
  return false;
}

Qt::ItemFlags TaskData::uiFlags(int section) const {
  Q_UNUSED(section)
  // TODO more flags, maybe not same ones for every section
  // FIXME mark only editable sections as editable
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void Task::setParentParams(ParamSet parentParams) {
  if (!isNull())
    td()->_params.setParent(parentParams);
}

TaskData *Task::td() {
  //Log::fatal() << "Task::td() detach " << constData() << " " << id();
  detach<TaskData>();
  //Log::fatal() << "Task::td() detached " << constData() << " " << id();
  return (TaskData*)constData();
}

PfNode Task::toPfNode() const {
  const TaskData *td = this->td();
  if (!td)
    return PfNode();
  PfNode node("task", td->_shortId);

  // description and execution attributes
  node.setAttribute("taskgroup", td->_group.id());
  if (!td->_label.isEmpty() && td->_label != td->_id)
    node.setAttribute("label", td->_label);
  if (!td->_info.isEmpty())
    node.setAttribute("info", td->_info);
  node.setAttribute("mean", meanAsString(td->_mean));
  // do not set target attribute if it is empty,
  // or in case it is implicit ("localhost" for Local mean)
  if (!td->_target.isEmpty()
      && (td->_target != "localhost" || !td->_mean == Local))
    node.setAttribute("target", td->_target);
  // do not set command attribute if it is empty
  // or for means that do not use it (Workflow and DoNothing)
  if (!td->_command.isEmpty()
      && td->_mean != DoNothing
      && td->_mean != Workflow)
    node.setAttribute("command", td->_command);

  // triggering and constraints attributes
  PfNode triggers("trigger");
  foreach (const Trigger &ct, td->_cronTriggers)
    triggers.appendChild(ct.toPfNode());
  foreach (const Trigger &nt, td->_noticeTriggers)
    triggers.appendChild(nt.toPfNode());
  node.appendChild(triggers);
  if (td->_discardAliasesOnStart != DiscardAll)
    node.appendChild(
          PfNode("discardaliasesonstart",
                 discardAliasesOnStartAsString(td->_discardAliasesOnStart)));
  if (td->_maxInstances != 1)
    node.appendChild(PfNode("maxinstances",
                            QString::number(td->_maxInstances)));
  foreach (const QString &key, td->_resources.keys())
    node.appendChild(
          PfNode("resource",
                 key+" "+QString::number(td->_resources.value(key))));

  // workflow attributes
  if (td->_startSteps.size())
    ConfigUtils::writeFlagSet(&node, td->_startSteps, "start");
  if (!td->_workflowTriggerSubscriptionsById.isEmpty()) {
    foreach (const WorkflowTriggerSubscription &wts,
             td->_workflowTriggerSubscriptionsById.values()) {
      PfNode ontrigger = wts.eventSubscription().toPfNode();
      ontrigger.prependChild(wts.trigger().toPfNode());
      node.appendChild(ontrigger);
    }
  }
  QList<Step> steps = td->_steps.values();
  qSort(steps);
  foreach (const Step &step, steps)
    node.appendChild(step.toPfNode());

  // params and setenv attributes
  ConfigUtils::writeParamSet(&node, td->_params, "param");
  ConfigUtils::writeParamSet(&node, td->_setenv, "setenv");
  ConfigUtils::writeFlagSet(&node, td->_unsetenv, "unsetenv");

  // monitoring and alerting attributes
  if (td->_maxExpectedDuration < LLONG_MAX)
    node.appendChild(PfNode("maxexpectedduration",
                            QString::number(td->_maxExpectedDuration)));
  if (td->_minExpectedDuration > 0)
    node.appendChild(PfNode("minexpectedduration",
                            QString::number(td->_minExpectedDuration)));
  if (td->_maxDurationBeforeAbort < LLONG_MAX)
    node.appendChild(PfNode("maxdurationbeforeabort",
                            QString::number(td->_maxDurationBeforeAbort)));

  // events (but workflow-specific "ontrigger" events)
  ConfigUtils::writeEventSubscriptions(&node, td->_onstart);
  ConfigUtils::writeEventSubscriptions(&node, td->_onsuccess);
  ConfigUtils::writeEventSubscriptions(&node, td->_onfailure,
                                       QStringList("onfinish"));

  // user interface attributes
  if (!td->_requestFormFields.isEmpty()) {
    PfNode requestForm("requestform");
    foreach (const RequestFormField &field, td->_requestFormFields)
      requestForm.appendChild(field.toPfNode());
    node.appendChild(requestForm);
  }
  return node;
}

Task::Mean Task::meanFromString(QString mean) {
  if (mean == "donothing")
    return DoNothing;
  if (mean == "local")
    return Local;
  if (mean == "workflow")
    return Workflow;
  if (mean == "ssh")
    return Ssh;
  if (mean == "http")
    return Http;
  return UnknownMean;
}

QString Task::meanAsString(Task::Mean mean) {
  // LATER optimize with const QStrings
  switch(mean) {
  case DoNothing:
    return "donothing";
  case Local:
    return "local";
  case Workflow:
    return "workflow";
  case Ssh:
    return "ssh";
  case Http:
    return "http";
  case UnknownMean:
    ;
  }
  return QString();
}

QStringList Task::validMeanStrings() {
  QStringList means;
  means << "donothing" << "local" << "workflow" << "ssh" << "http";
  return means;
}
