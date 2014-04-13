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

static QString _uiHeaderNames[] = {
  "Id", // 0
  "TaskGroup Id",
  "Label",
  "Mean",
  "Command",
  "Target", // 5
  "Trigger",
  "Parameters",
  "Resources",
  "Last execution",
  "Next execution", // 10
  "Short id",
  "Max intances",
  "Instances count",
  "On start",
  "On success", // 15
  "On failure",
  "Instances / max",
  "Actions",
  "Last execution status",
  "System environment", // 20
  "Setenv",
  "Unsetenv",
  "Min expected duration",
  "Max expected duration",
  "Request-time overridable params", // 25
  "Last execution duration",
  "Max duration before abort",
  "Triggers with calendars",
  "Enabled",
  "Has triggers with calendars", // 30
  "Workflow"
};

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
  QString _fqtn, _shortId, _label, _mean, _command, _target, _info; // FIXME rename fqtn to id everywhere
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
  QStringList _otherTriggers;
  QString _supertaskFqtn;
  QHash<QString,Step> _steps;
  QStringList _startSteps;
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
  QString id() const { return _fqtn; }
  QString idQualifier() const { return "task"; }
};

Task::Task() {
  //Log::fatal() << "Task::Task " << (sizeof(Task)) << " " << (sizeof(TaskData));
}

Task::Task(const Task &other) : SharedUiItem(other) {
  //Log::fatal() << "Task::Task copying " << other.fqtn() << " " << other.td()
  //             << " " << ((const Task*)this)->td();
}

Task::Task(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QHash<QString, Task> oldTasks, QString supertaskFqtn,
           QHash<QString,Calendar> namedCalendars) {
  TaskData *td = new TaskData;
  td->_scheduler = scheduler;
  td->_shortId = ConfigUtils::sanitizeId(node.contentAsString());
  td->_label = node.attribute("label", td->_shortId);
  td->_mean = ConfigUtils::sanitizeId(node.attribute("mean"));
  if (td->_mean != "local" && td->_mean != "ssh" && td->_mean != "http"
      && td->_mean != "workflow" && td->_mean != "donothing") {
    Log::error() << "task with invalid execution mean: "
                 << node.toString();
    delete td;
    return;
  }
  td->_command = node.attribute("command");
  td->_target = ConfigUtils::sanitizeId(node.attribute("target"));
  if (td->_target.isEmpty()
      && (td->_mean == "local" || td->_mean == "donothing"
          || td->_mean == "workflow"))
    td->_target = "localhost";
  td->_info = node.stringChildrenByName("info").join(" ");
  td->_fqtn = taskGroup.id()+"."+td->_shortId;
  td->_group = taskGroup;
  td->_maxInstances = node.attribute("maxinstances", "1").toInt();
  if (td->_maxInstances <= 0) {
    Log::error() << "invalid task maxinstances: " << node.toPf();
    delete td;
    return;
  }
  if (node.hasChild("disabled"))
    td->_enabled = false;
  double f = node.doubleAttribute("maxexpectedduration", -1);
  td->_maxExpectedDuration = f < 0 ? LLONG_MAX : (long long)(f*1000);
  f = node.doubleAttribute("minexpectedduration", -1);
  td->_minExpectedDuration = f < 0 ? 0 : (long long)(f*1000);
  f = node.doubleAttribute("maxdurationbeforeabort", -1);
  td->_maxDurationBeforeAbort = f < 0 ? LLONG_MAX : (long long)(f*1000);
  td->_params.setParent(taskGroup.params());
  ConfigUtils::loadParamSet(node, &td->_params);
  QString filter = td->_params.value("stderrfilter");
  if (!filter.isEmpty())
    td->_stderrFilters.append(QRegExp(filter));
  td->_setenv.setParent(taskGroup.setenv());
  ConfigUtils::loadSetenv(node, &td->_setenv);
  td->_unsetenv.setParent(taskGroup.unsetenv());
  ConfigUtils::loadUnsetenv(node, &td->_unsetenv);
  QHash<QString,CronTrigger> oldCronTriggers;
  td->_supertaskFqtn = supertaskFqtn;
  // copy mutable fields from old task and build old cron triggers dictionary
  Task oldTask = oldTasks.value(td->_fqtn);
  if (!oldTask.isNull()) {
    foreach (const CronTrigger ct, oldTask.td()->_cronTriggers)
      oldCronTriggers.insert(ct.canonicalExpression(), ct);
    td->_lastExecution = oldTask.lastExecution().isValid()
        ? oldTask.lastExecution().toMSecsSinceEpoch() : LLONG_MIN;
    td->_nextScheduledExecution = oldTask.nextScheduledExecution().isValid()
        ? oldTask.nextScheduledExecution().toMSecsSinceEpoch() : LLONG_MIN;
    td->_instancesCount = oldTask.instancesCount();
    td->_lastSuccessful = oldTask.lastSuccessful();
    td->_lastReturnCode = oldTask.lastReturnCode();
    td->_lastTotalMillis = oldTask.lastTotalMillis();
    td->_enabled = td->_enabled && oldTask.enabled();
  }
  // LATER load cron triggers last exec timestamp from on-disk log
  if (supertaskFqtn.isEmpty()) { // subtasks do not have triggers
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
            // keep last triggered timestamp from previously defined trigger
            CronTrigger oldTrigger =
                oldCronTriggers.value(trigger.canonicalExpression());
            if (oldTrigger.isValid())
              trigger.setLastTriggered(oldTrigger.lastTriggered());
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
  QListIterator<QPair<QString,qlonglong> > it(
        node.stringLongPairChildrenByName("resource"));
  while (it.hasNext()) {
    const QPair<QString,qlonglong> &p(it.next());
    if (p.second <= 0) {
      Log::error() << "task with incorrect resource quantity for kind '"
                   << p.first << "': " << node.toString();
      delete td;
      return;
    } else
      td->_resources.insert(ConfigUtils::sanitizeId(p.first), p.second);
  }
  QString doas = node.attribute("discardaliasesonstart", "all");
  td->_discardAliasesOnStart = discardAliasesOnStartFromString(doas);
  if (td->_discardAliasesOnStart == Task::DiscardUnknown) {
    Log::error() << "invalid discardaliasesonstart on task " << td->_shortId << ": '"
                 << doas << "'";
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
  ConfigUtils::loadEventSubscription(node, "onstart", td->_fqtn, &td->_onstart,
                                     scheduler);
  ConfigUtils::loadEventSubscription(node, "onsuccess", td->_fqtn,
                                     &td->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", td->_fqtn,
                                     &td->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfailure", td->_fqtn,
                                     &td->_onfailure, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", td->_fqtn,
                                     &td->_onfailure, scheduler);
  QList<PfNode> steps = node.childrenByName("task")+node.childrenByName("and")
      +node.childrenByName("or");
  if (td->_mean == "workflow") {
    if (steps.isEmpty())
      Log::warning() << "workflow task with no step: " << node.toString();
    if (!supertaskFqtn.isNull()) {
      Log::error() << "workflow task not allowed as a workflow subtask: "
                   << node.toString();
      delete td;
      return;
    }
    foreach (PfNode child, steps) {
      Step step(child, scheduler, taskGroup, td->_shortId, oldTasks, namedCalendars);
      if (step.isNull()) {
        Log::error() << "workflow task " << td->_fqtn
                     << " has at less one invalid step definition";
        delete td;
        return;
      } if (td->_steps.contains(step.id())) {
        Log::error() << "workflow task " << td->_fqtn
                     << " has duplicate steps with id " << step.id();
        delete td;
        return;
      } else {
        td->_steps.insert(step.id(), step);
      }
    }
    td->_startSteps = node.stringListAttribute("start");
    foreach (QString id, td->_startSteps)
      if (!td->_steps.contains(id)) {
        Log::error() << "workflow task " << td->_fqtn
                     << " has invalid or lacking start steps: "
                     << node.toString();
        delete td;
        return;
      }
    int tsCount = 0;
    QStringList ignoredChildren;
    ignoredChildren << "cron" << "notice";
    foreach (PfNode child, node.childrenByName("ontrigger")) {
      EventSubscription es(td->_fqtn, child, scheduler, ignoredChildren);
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
    //     format: ${workflow_fqtn}|start|${step_id}
    //     e.g. app1.group1.workflow1|start|step1
    // 2) Subtask to any step transition:
    //     format: ${source_fqtn}|${event_name}|${target_step_id}
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
    //    Currently, the only event availlable in a join step is onready.
    // 4) End transition:
    //    format: ${source_fqtn_or_fqtn}|${event_name}|$end
    //    e.g. app1.group1.workflow1-step3|onfinish|$end
    //         app1.group1.workflow1:step4|onready|$end
    foreach (QString id, td->_startSteps) {
      td->_steps[id].insertPredecessor(td->_fqtn+"|start|"+id);
      Log::debug() << "adding predecessor " << td->_fqtn+"|start|"+id
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
            if (td->_steps[source].kind() == Step::SubTask) // fqtn
              transitionId = td->_fqtn+"-"+source+"|"+eventName+"|"+target;
            else // fqsn
              transitionId = td->_fqtn+":"+source+"|"+eventName+"|"+target;
            if (td->_steps.contains(target)) {
              Log::debug() << "registring predecessor " << transitionId
                           << " to step " << td->_steps[target].fqsn();
              td->_steps[target].insertPredecessor(transitionId);
            } else {
              Log::error() << "cannot register predecessor " << transitionId
                           << " with unknown target to workflow " << td->_fqtn;
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

Task::~Task() {
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
  return !isNull() ? td()->_label : QString();
}

QString Task::mean() const {
  return !isNull() ? td()->_mean : QString();
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

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.shortId(); // FIXME id()
  return dbg.space();
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
  QString v("<ul>");
  foreach (const RequestFormField rff, list)
    v.append("<li>")
        .append(HtmlUtils::htmlEncode(rff.toHumanReadableDescription()));
  v.append("</ul>");
  return v;
}

QVariant Task::paramValue(QString key, QVariant defaultValue) const {
  //Log::fatal() << "Task::paramvalue " << key;
  if (key == "!taskshortid") {
    return shortId();
  } else if (key == "!fqtn") {
    return id();
  } else if (key == "!taskgroupid") {
    return taskGroup().id();
  } else if (key == "!target") {
    return target();
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

QString Task::supertaskFqtn() const {
  return !isNull() ? td()->_supertaskFqtn : QString();
}

QStringList Task::startSteps() const {
  return !isNull() ? td()->_startSteps : QStringList();
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

QVariant Task::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int Task::uiDataCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

QVariant TaskData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
      return _fqtn;
    case 1:
      return _group.id();
    case 2:
      return _label;
    case 3:
      return _mean;
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
      return _shortId;
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
    case 20: {
      QString env;
      foreach(const QString key, _setenv.keys(false))
        env.append(key).append('=').append(_setenv.rawValue(key)).append(' ');
      foreach(const QString key, _unsetenv.keys(false))
        env.append('-').append(key).append(' ');
      env.chop(1);
      return env;
    }
    case 21: {
      QString env;
      foreach(const QString key, _setenv.keys(false))
        env.append(key).append('=').append(_setenv.rawValue(key)).append(' ');
      env.chop(1);
      return env;
    }
    case 22: {
      QString env;
      foreach(const QString key, _unsetenv.keys(false))
        env.append(key).append(' ');
      env.chop(1);
      return env;
    }
    case 23:
      return (_minExpectedDuration > 0)
          ? QString::number(_minExpectedDuration*.001) : QString();
    case 24:
      return (_maxExpectedDuration < LLONG_MAX)
          ? QString::number(_maxExpectedDuration*.001) : QString();
    case 25: {
      QString s;
      foreach (const RequestFormField rff, _requestFormFields)
        s.append(rff.param()).append(' ');
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
      return _supertaskFqtn;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

void Task::setParentParams(ParamSet parentParams) {
  if (!isNull())
    td()->_params.setParent(parentParams);
}

TaskData *Task::td() {
  //Log::fatal() << "Task::td() detach " << constData() << " " << fqtn();
  detach<TaskData>();
  //Log::fatal() << "Task::td() detached " << constData() << " " << fqtn();
  return (TaskData*)constData();
}
