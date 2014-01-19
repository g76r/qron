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
#include "ui/graphviz_styles.h"
#include "sched/stepinstance.h"

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

class TaskData : public QSharedData {
public:
  QString _id, _label, _mean, _command, _target, _info, _fqtn;
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
  QList<RequestFormField> _requestFormField;
  QStringList _otherTriggers;
  Task _supertask;
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
};

Task::Task() {
}

Task::Task(const Task &other) : d(other.d) {
}

Task::Task(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QHash<QString, Task> oldTasks, Task supertask) {
  TaskData *td = new TaskData;
  td->_scheduler = scheduler;
  td->_id = ConfigUtils::sanitizeId(node.contentAsString());
  td->_label = node.attribute("label", td->_id);
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
  td->_fqtn = taskGroup.id()+"."+td->_id;
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
  td->_params.setParent(supertask.isNull() ? taskGroup.params()
                                           : supertask.params());
  ConfigUtils::loadParamSet(node, &td->_params);
  QString filter = td->_params.value("stderrfilter");
  if (!filter.isEmpty())
    d->_stderrFilters.append(QRegExp(filter));
  td->_setenv.setParent(taskGroup.setenv());
  ConfigUtils::loadSetenv(node, &td->_setenv);
  td->_unsetenv.setParent(taskGroup.unsetenv());
  ConfigUtils::loadUnsetenv(node, &td->_unsetenv);
  QHash<QString,CronTrigger> oldCronTriggers;
  td->_supertask = supertask;
  // copy mutable fields from old task and build old cron triggers dictionary
  Task oldTask = oldTasks.value(td->_fqtn);
  if (oldTask.d) {
    foreach (const CronTrigger ct, oldTask.d->_cronTriggers)
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
  if (supertask.isNull()) { // subtasks do not have triggers
    foreach (PfNode child, node.childrenByName("trigger")) {
      foreach (PfNode grandchild, child.children()) {
        QString content = grandchild.contentAsString();
        QString triggerType = grandchild.name();
        if (triggerType == "notice") {
          NoticeTrigger trigger(grandchild, scheduler->namedCalendars());
          if (trigger.isValid()) {
            td->_noticeTriggers.append(trigger);
            Log::debug() << "configured notice trigger '" << content
                         << "' on task '" << td->_id << "'";
          } else {
            Log::error() << "task with invalid notice trigger: "
                         << node.toString();
            delete td;
            return;
          }
        } else if (triggerType == "cron") {
          CronTrigger trigger(grandchild, scheduler->namedCalendars());
          if (trigger.isValid()) {
            // keep last triggered timestamp from previously defined trigger
            CronTrigger oldTrigger =
                oldCronTriggers.value(trigger.canonicalExpression());
            if (oldTrigger.isValid())
              trigger.setLastTriggered(oldTrigger.lastTriggered());
            td->_cronTriggers.append(trigger);
            Log::debug() << "configured cron trigger "
                         << trigger.humanReadableExpression()
                         << " on task " << td->_id;
          } else {
            Log::error() << "task with invalid cron trigger: "
                         << node.toString();
            delete td;
            return;
          }
          // LATER read misfire config
        } else {
          Log::warning() << "ignoring unknown trigger type '" << triggerType
                         << "' on task " << td->_id;
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
    Log::error() << "invalid discardaliasesonstart on task " << td->_id << ": '"
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
        td->_requestFormField.append(field);
    }
  }
  d = td; // needed to give a non empty *this to loadEventListConfiguration() and Step()
  foreach (PfNode child, node.childrenByName("onstart"))
    scheduler->loadEventSubscription(
          d->_fqtn, child, &d->_onstart, td->_id, *this);
  foreach (PfNode child, node.childrenByName("onsuccess"))
    scheduler->loadEventSubscription(
          d->_fqtn, child, &d->_onsuccess, d->_id, *this);
  foreach (PfNode child, node.childrenByName("onfailure"))
    scheduler->loadEventSubscription(
          d->_fqtn, child, &d->_onfailure, d->_id, *this);
  foreach (PfNode child, node.childrenByName("onfinish")) {
    scheduler->loadEventSubscription(
          d->_fqtn, child, &d->_onsuccess, d->_id, *this);
    scheduler->loadEventSubscription(
          d->_fqtn, child, &d->_onfailure, d->_id, *this);
  }
  QList<PfNode> steps = node.childrenByName("task")+node.childrenByName("and")
      +node.childrenByName("or");
  if (d->_mean == "workflow") {
    if (steps.isEmpty())
      Log::warning() << "workflow task with no step: " << node.toString();
    if (!d->_supertask.isNull()) {
      Log::error() << "workflow task not allowed as a workflow subtask: "
                   << node.toString();
      d = 0;
      return;
    }
    foreach (PfNode child, steps) {
      Step step(child, scheduler, *this, oldTasks);
      if (step.isNull()) {
        Log::error() << "workflow task " << d->_fqtn
                     << " has at less one invalid step definition";
        d = 0;
        return;
      } if (d->_steps.contains(step.id())) {
        Log::error() << "workflow task " << d->_fqtn
                     << " has duplicate steps with id " << step.id();
        d = 0;
        return;
      } else {
        d->_steps.insert(step.id(), step);
      }
    }
    d->_startSteps = node.stringListAttribute("start");
    foreach (QString id, d->_startSteps)
      if (!d->_steps.contains(id)) {
        Log::error() << "workflow task " << d->_fqtn
                     << " has invalid or lacking start steps: "
                     << node.toString();
        d = 0;
        return;
      }
    int tsCount = 0;
    foreach (PfNode child, node.childrenByName("ontrigger")) {
      EventSubscription es(d->_fqtn, child, scheduler); // FIXME avoid warnings on cron and notice
      if (es.isNull() || es.actions().isEmpty()) {
        Log::warning() << "ignoring invalid or empty ontrigger: "
                       << node.toString();
        continue;
      }
      foreach (PfNode grandchild, child.childrenByName("cron")) {
        QString tsId = QString::number(tsCount++);
        CronTrigger trigger(grandchild, scheduler->namedCalendars());
        if (trigger.isValid()) {
          d->_workflowCronTriggersById.insert(tsId, trigger);
          d->_workflowTriggerSubscriptionsById.insert(
                tsId, WorkflowTriggerSubscription(trigger, es));
        } else
          Log::warning() << "ignoring invalid cron trigger in ontrigger: "
                         << node.toString();
      }
      foreach (PfNode grandchild, child.childrenByName("notice")) {
        QString tsId = QString::number(tsCount++);
        NoticeTrigger trigger(grandchild, scheduler->namedCalendars());
        if (trigger.isValid()) {
          d->_workflowTriggerSubscriptionsByNotice
              .insert(trigger.expression(),
                      WorkflowTriggerSubscription(trigger, es));
          d->_workflowTriggerSubscriptionsById.insert(
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
    foreach (QString id, d->_startSteps) {
      d->_steps[id].insertPredecessor(d->_fqtn+"|start|"+id);
      Log::debug() << "adding predecessor " << d->_fqtn+"|start|"+id
                   << " to step " << d->_steps[id].fqsn();
    }
    foreach (QString source, d->_steps.keys()) {
      QList<EventSubscription> subList
          = d->_steps[source].onreadyEventSubscriptions()
          + d->_steps[source].subtask().allEventsSubscriptions()
          + d->_steps[source].subtask().taskGroup().allEventSubscriptions();
      foreach (EventSubscription es, subList) {
        foreach (Action a, es.actions()) {
          if (a.actionType() == "step") {
            QString target = a.targetName();
            QString eventName = es.eventName();
            if (eventName == "onsuccess" || eventName == "onfailure")
              eventName = "onfinish";
            QString transitionId;
            if (d->_steps[source].kind() == Step::SubTask) // fqtn
              transitionId = d->_fqtn+"-"+source+"|"+eventName+"|"+target;
            else // fqsn
              transitionId = d->_fqtn+":"+source+"|"+eventName+"|"+target;
            if (d->_steps.contains(target)) {
              Log::debug() << "registring predecessor " << transitionId
                           << " to step " << d->_steps[target].fqsn();
              d->_steps[target].insertPredecessor(transitionId);
            } else {
              Log::error() << "cannot register predecessor " << transitionId
                           << " with unknown target to workflow " << d->_fqtn;
              d = 0;
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
  d->_workflowDiagram
      = workflowInstanceDiagram(QHash<QString,StepInstance>());
}

Task::~Task() {
}

Task &Task::operator=(const Task &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool Task::operator==(const Task &other) const {
  return fqtn() == other.fqtn();
}

bool Task::operator<(const Task &other) const {
  return fqtn() < other.fqtn();
}

ParamSet Task::params() const {
  return d ? d->_params : ParamSet();
}

bool Task::isNull() const {
  return !d;
}

QList<NoticeTrigger> Task::noticeTriggers() const {
  return d ? d->_noticeTriggers : QList<NoticeTrigger>();
}

QString Task::id() const {
  return d ? d->_id : QString();
}

QString Task::fqtn() const {
  return d ? d->_fqtn : QString();
}

QString Task::label() const {
  return d ? d->_label : QString();
}

QString Task::mean() const {
  return d ? d->_mean : QString();
}

QString Task::command() const {
  return d ? d->_command : QString();
}

QString Task::target() const {
  return d ? d->_target : QString();
}

QString Task::info() const {
  return d ? d->_info : QString();
}

TaskGroup Task::taskGroup() const {
  return d ? d->_group : TaskGroup();
}

QHash<QString, qint64> Task::resources() const {
  return d ? d->_resources : QHash<QString,qint64>();
}

QString Task::resourcesAsString() const {
  QString s;
  if (d) {
    bool first = true;
    foreach(QString key, d->_resources.keys()) {
      if (first)
        first = false;
      else
        s.append(' ');
      s.append(key).append('=')
          .append(QString::number(d->_resources.value(key)));
    }
  }
  return s;
}

QString Task::triggersAsString() const {
  QString s;
  if (d) {
    foreach (CronTrigger t, d->_cronTriggers)
      s.append(t.humanReadableExpression()).append(' ');
    foreach (NoticeTrigger t, d->_noticeTriggers)
      s.append(t.humanReadableExpression()).append(' ');
    foreach (QString t, d->_otherTriggers)
      s.append(t).append(' ');
  }
  if (!s.isEmpty())
    s.chop(1); // remove last space
  return s;
}

QString Task::triggersWithCalendarsAsString() const {
  QString s;
  if (d) {
    foreach (CronTrigger t, d->_cronTriggers)
      s.append(t.humanReadableExpressionWithCalendar()).append(' ');
    foreach (NoticeTrigger t, d->_noticeTriggers)
      s.append(t.humanReadableExpressionWithCalendar()).append(' ');
    foreach (QString t, d->_otherTriggers)
      s.append(t).append(" ");
  }
  if (!s.isEmpty())
    s.chop(1); // remove last space
  return s;
}

bool Task::triggersHaveCalendar() const {
  if (d) {
    foreach (CronTrigger t, d->_cronTriggers)
      if (!t.calendar().isNull())
        return true;
    foreach (NoticeTrigger t, d->_noticeTriggers)
      if (!t.calendar().isNull())
        return true;
  }
  return false;
}

QDateTime Task::lastExecution() const {
  return d && d->_lastExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(d->_lastExecution) : QDateTime();
}

QDateTime Task::nextScheduledExecution() const {
  return d && d->_nextScheduledExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(d->_nextScheduledExecution)
      : QDateTime();
}

void Task::setLastExecution(QDateTime timestamp) const {
  if (d)
    d->_lastExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

void Task::setNextScheduledExecution(QDateTime timestamp) const {
  if (d)
    d->_nextScheduledExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

int Task::maxInstances() const {
  return d ? d->_maxInstances : 0;
}

int Task::instancesCount() const {
  // LATER what did happen to operator int() in Qt5 ?
  return d ? d->_instancesCount.fetchAndAddRelaxed(0) : 0;
}

int Task::fetchAndAddInstancesCount(int valueToAdd) const {
  return d ? d->_instancesCount.fetchAndAddOrdered(valueToAdd) : 0;
}

QList<QRegExp> Task::stderrFilters() const {
  return d ? d->_stderrFilters : QList<QRegExp>();
}

void Task::appendStderrFilter(QRegExp filter) {
  if (d)
    d->_stderrFilters.append(filter);
}

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.fqtn();
  return dbg.space();
}

void Task::triggerStartEvents(TaskInstance instance) const {
  if (d) {
    d->_group.triggerStartEvents(instance);
    foreach (EventSubscription sub, d->_onstart)
      sub.triggerActions(instance);
  }
}

void Task::triggerSuccessEvents(TaskInstance instance) const {
  if (d) {
    d->_group.triggerSuccessEvents(instance);
    foreach (EventSubscription sub, d->_onsuccess)
      sub.triggerActions(instance);
  }
}

void Task::triggerFailureEvents(TaskInstance instance) const {
  if (d) {
    d->_group.triggerFailureEvents(instance);
    foreach (EventSubscription sub, d->_onfailure)
      sub.triggerActions(instance);
  }
}

QList<EventSubscription> Task::onstartEventSubscriptions() const {
  return d ? d->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> Task::onsuccessEventSubscriptions() const {
  return d ? d->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> Task::onfailureEventSubscriptions() const {
  return d ? d->_onfailure : QList<EventSubscription>();
}

QList<EventSubscription> Task::allEventsSubscriptions() const {
  // LATER avoid creating the collection at every call
  return d ? d->_onstart+d->_onsuccess+d->_onfailure : QList<EventSubscription>();
}

bool Task::enabled() const {
  return d ? d->_enabled : false;
}
void Task::setEnabled(bool enabled) const {
  if (d)
    d->_enabled = enabled;
}

bool Task::lastSuccessful() const {
  return d ? d->_lastSuccessful : false;
}

void Task::setLastSuccessful(bool successful) const {
  if (d)
    d->_lastSuccessful = successful;
}

int Task::lastReturnCode() const {
  return d ? d->_lastReturnCode : -1;
}

void Task::setLastReturnCode(int code) const {
  if (d)
    d->_lastReturnCode = code;
}

int Task::lastTotalMillis() const {
  return d ? d->_lastTotalMillis : -1;
}

void Task::setLastTotalMillis(int lastTotalMillis) const {
  if (d)
    d->_lastTotalMillis = lastTotalMillis;
}

long long Task::maxExpectedDuration() const {
  return d ? d->_maxExpectedDuration : LLONG_MAX;
}
long long Task::minExpectedDuration() const {
  return d ? d->_minExpectedDuration : 0;
}

long long Task::maxDurationBeforeAbort() const {
  return d ? d->_maxDurationBeforeAbort : LLONG_MAX;
}

ParamSet Task::setenv() const {
  return d ? d->_setenv : ParamSet();
}

ParamSet Task::unsetenv() const {
  return d ? d->_unsetenv : ParamSet();
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStart() const {
  return d ? d->_discardAliasesOnStart : Task::DiscardNone;
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
  return d ? d->_requestFormField : QList<RequestFormField>();
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
  if (d) {
    if (key == "!taskid") {
      return id();
    } else if (key == "!fqtn") {
      return fqtn();
    } else if (key == "!taskgroupid") {
      return taskGroup().id();
    } else if (key == "!target") {
      return target();
    }
  }
  return defaultValue;
}

QList<CronTrigger> Task::cronTriggers() const {
  return d ? d->_cronTriggers : QList<CronTrigger>();
}

QStringList Task::otherTriggers() const {
  return d ? d->_otherTriggers : QStringList();
}

void Task::appendOtherTriggers(QString text) {
  if (d)
    d->_otherTriggers.append(text);
}

void Task::clearOtherTriggers() {
  if (d)
    d->_otherTriggers.clear();
}

QHash<QString, Step> Task::steps() const {
  return d ? d->_steps : QHash<QString,Step>();
}

Task Task::supertask() const {
  return d ? d->_supertask : Task();
}

QStringList Task::startSteps() const {
  return d ? d->_startSteps : QStringList();
}

QString Task::workflowDiagram() const {
  return d ? d->_workflowDiagram : QString();
}

QString Task::workflowInstanceDiagram(
    QHash<QString,StepInstance> stepInstances) const {
  // LATER implement instanciated workflow diagram for real
  if (!d || d->_mean != "workflow")
    return "graph g{graph[" WORKFLOW_GRAPH ",label=\"not a workflow\"]}";
  QString gv("graph g{\n"
             "  graph[" WORKFLOW_GRAPH "]\n"
             "  start[" START_NODE "]\n"
             "  end[" END_NODE "]\n");
  foreach (Step s, d->_steps.values()) {
    StepInstance si = stepInstances.value(s.id());
    switch (s.kind()) {
    case Step::SubTask:
      gv.append("  step_"+s.id()+"[label=\""+s.id()+"\"," TASK_NODE
                +(si.isReady() ? ",color=orange" : "")+"]\n"); // LATER red if failure, green if ok
      break;
    case Step::AndJoin:
      gv.append("  step_"+s.id()+"[" ANDJOIN_NODE
                +(si.isReady() ? ",color=greenforest" : "")+"]\n");
      break;
    case Step::OrJoin:
      gv.append("  step_"+s.id()+"[" ORJOIN_NODE
                +(si.isReady() ? ",color=greenforest" : "")+"]\n");
      break;
    case Step::Unknown:
      ;
    }
  }
  QSet<QString> gvedges; // use a QSet to remove duplicate onfinish edges
  foreach (QString tsId, d->_workflowTriggerSubscriptionsById.keys()) {
    const WorkflowTriggerSubscription &ts
        = d->_workflowTriggerSubscriptionsById.value(tsId);
    QString expr = ts.trigger().humanReadableExpression().remove('"');
    // LATER do not rely on human readable expr to determine trigger style
    gv.append("  trigger_"+tsId+"[label=\""+expr+"\","
              +(expr.startsWith('^')?NOTICE_NODE:CRON_TRIGGER_NODE)+"]\n");
    foreach (Action a, ts.eventSubscription().actions()) {
      if (a.actionType() == "step") {
        QString target = a.targetName();
        if (d->_steps.contains(target)) {
          gvedges.insert("  trigger_"+tsId+" -- step_"+target
                         +"[label=\"\"," TRIGGER_STEP_EDGE  "]\n");
        } else {
          // do nothing, the warning is issued in another loop
        }
      } else if (a.actionType() == "end") {
        gvedges.insert("  trigger_"+tsId+" -- end [label=\"\","
                TRIGGER_STEP_EDGE "]\n");
      }
    }
  }
  foreach (QString id, d->_startSteps) {
    gvedges.insert("  start -- step_"+id+"\n");
  }
  foreach (QString source, d->_steps.keys()) {
    QList<EventSubscription> subList
        = d->_steps[source].onreadyEventSubscriptions()
        + d->_steps[source].subtask().allEventsSubscriptions()
        + d->_steps[source].subtask().taskGroup().allEventSubscriptions();
    foreach (EventSubscription es, subList) {
      foreach (Action a, es.actions()) {
        if (a.actionType() == "step") {
          QString target = a.targetName();
          if (d->_steps.contains(target)) {
            gvedges.insert("  step_"+source+" -- step_"+target+"[label=\""
                           +es.eventName()+"\"" STEP_EDGE "]\n");
          }
        } else if (a.actionType() == "end") {
          gvedges.insert("  step_"+source+" -- end [label=\""+es.eventName()
                         +"\"" STEP_EDGE "]\n");
        }
      }
    }
  }
  foreach(QString s, gvedges)
    gv.append(s);
  gv.append("}");
  return gv;
}

QHash<QString,WorkflowTriggerSubscription> Task::workflowTriggerSubscriptionsById() const {
  return d ? d->_workflowTriggerSubscriptionsById
           : QHash<QString,WorkflowTriggerSubscription>();
}

QMultiHash<QString, WorkflowTriggerSubscription> Task::workflowTriggerSubscriptionsByNotice() const {
  return d ? d->_workflowTriggerSubscriptionsByNotice
           : QMultiHash<QString,WorkflowTriggerSubscription>();
}

QHash<QString,CronTrigger> Task::workflowCronTriggersById() const {
  return d ? d->_workflowCronTriggersById : QHash<QString,CronTrigger>();
}
