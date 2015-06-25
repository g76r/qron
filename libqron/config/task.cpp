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
#include "modelview/shareduiitemdocumentmanager.h"

static QSet<QString> excludedDescendantsForComments;

static class ExcludedDescendantsForCommentsInitializer {
public:
  ExcludedDescendantsForCommentsInitializer() {
    excludedDescendantsForComments.insert("subtask");
    excludedDescendantsForComments.insert("trigger");
    excludedDescendantsForComments.insert("onsuccess");
    excludedDescendantsForComments.insert("onfailure");
    excludedDescendantsForComments.insert("onfinish");
    excludedDescendantsForComments.insert("onstart");
    excludedDescendantsForComments.insert("ontrigger");
  }
} excludedDescendantsForCommentsInitializer;

class WorkflowTransitionData : public SharedUiItemData {
public:
  QString _workflowId, _sourceLocalId, _eventName, _targetLocalId, _id,
  _localId;

  WorkflowTransitionData();
  WorkflowTransitionData(QString workflowId, QString sourceLocalId,
                     QString eventName, QString targetLocalId)
    : _workflowId(workflowId), _sourceLocalId(sourceLocalId),
      _eventName(eventName), _targetLocalId(targetLocalId) {
    computeIds();
  }
  void computeIds() {
    _id = _workflowId+":"+_sourceLocalId+":"+_eventName+":"+_targetLocalId;
    _localId = _sourceLocalId+":"+_eventName+":"+_targetLocalId;
  }
  QString id() const { return _id; }
  QString idQualifier() const { return "workflowtransition"; }
};

WorkflowTransition::WorkflowTransition() {
}

WorkflowTransition::WorkflowTransition(const WorkflowTransition &other)
  : SharedUiItem(other) {
}

WorkflowTransition::WorkflowTransition(
    QString workflowId, QString sourceLocalId, QString eventName,
    QString targetLocalId)
  : SharedUiItem(new WorkflowTransitionData(
                   workflowId, sourceLocalId, eventName, targetLocalId)) {
}

WorkflowTransition::~WorkflowTransition() {
}

void WorkflowTransition::detach() {
  SharedUiItem::detach<WorkflowTransitionData>();
}

WorkflowTransitionData *WorkflowTransition::data() {
  SharedUiItem::detach<WorkflowTransitionData>();
  return (WorkflowTransitionData*)SharedUiItem::data();
}

QString WorkflowTransition::workflowId() const {
  return isNull() ? QString() : data()->_workflowId;
}

void WorkflowTransition::setWorkflowId(QString workflowId) {
  if (!isNull()) {
    data()->_workflowId = workflowId;
    data()->computeIds();
  }
}

QString WorkflowTransition::sourceLocalId() const {
  return isNull() ? QString() : data()->_sourceLocalId;
}

QString WorkflowTransition::eventName() const {
  return isNull() ? QString() : data()->_eventName;
}

void WorkflowTransition::setEventName(QString eventName) {
  if (!isNull()) {
    data()->_eventName = eventName;
    data()->computeIds();
  }
}

QString WorkflowTransition::targetLocalId() const {
  return isNull() ? QString() : data()->_targetLocalId;
}

QString WorkflowTransition::localId() const {
  return isNull() ? QString() : data()->_localId;
}

class TaskData : public SharedUiItemData {
public:
  QString _id, _localId, _label, _command, _target, _info;
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
  QStringList _otherTriggers; // guessed indirect triggers resulting from events
  QString _workflowTaskId;
  QHash<QString,Step> _steps;
  QString _graphvizWorkflowDiagram;
  QMultiHash<QString,WorkflowTransition> _transitionsBySourceLocalId;
  QHash<QString,CronTrigger> _workflowCronTriggersByLocalId;
  QStringList _commentsList;
  // note: since QDateTime (as most Qt classes) is not thread-safe, it cannot
  // be used in a mutable QSharedData field as soon as the object embedding the
  // QSharedData is used by several thread at a time, hence the qint64
  mutable qint64 _lastExecution, _nextScheduledExecution;
  mutable QAtomicInt _instancesCount;
  mutable bool _enabled, _lastSuccessful;
  mutable int _lastReturnCode, _lastTotalMillis;
  mutable quint64 _lastTaskInstanceId;

  TaskData() : _maxExpectedDuration(LLONG_MAX), _minExpectedDuration(0),
    _maxDurationBeforeAbort(LLONG_MAX),
    _discardAliasesOnStart(Task::DiscardAll),
    _lastExecution(LLONG_MIN), _nextScheduledExecution(LLONG_MIN),
    _enabled(true), _lastSuccessful(true), _lastReturnCode(-1),
    _lastTotalMillis(-1), _lastTaskInstanceId(0)  { }
  QDateTime lastExecution() const;
  QDateTime nextScheduledExecution() const;
  QString resourcesAsString() const;
  QString triggersAsString() const;
  QString triggersWithCalendarsAsString() const;
  bool triggersHaveCalendar() const;
  QVariant uiData(int section, int role) const;
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 int role, const SharedUiItemDocumentManager *dm);
  Qt::ItemFlags uiFlags(int section) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  void setId(QString id) { _id = id; }
  QString idQualifier() const { return "task"; }
  PfNode toPfNode() const;
};

Task::Task() {
}

Task::Task(const Task &other) : SharedUiItem(other) {
}

Task::Task(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QString workflowTaskId, QHash<QString,Calendar> namedCalendars) {
  TaskData *d = new TaskData;
  d->_scheduler = scheduler;
  d->_localId =
      ConfigUtils::sanitizeId(node.contentAsString(),
                              workflowTaskId.isEmpty() ? ConfigUtils::LocalId
                                                    : ConfigUtils::SubTaskId);
  d->_label = node.attribute("label");
  d->_mean = meanFromString(node.attribute("mean", "local").trimmed()
                             .toLower());
  if (d->_mean == UnknownMean) {
    Log::error() << "task with invalid execution mean: "
                 << node.toString();
    delete d;
    return;
  }
  d->_command = node.attribute("command");
  d->_target =
      ConfigUtils::sanitizeId(node.attribute("target"), ConfigUtils::FullyQualifiedId);
  // silently use "localhost" as target for "local" mean
  if (d->_target.isEmpty() && d->_mean == Local)
    d->_target = "localhost";
  d->_info = node.stringChildrenByName("info").join(" ");
  d->_id = taskGroup.id()+"."+d->_localId;
  d->_group = taskGroup;
  d->_maxInstances = node.attribute("maxinstances", "1").toInt();
  if (d->_maxInstances <= 0) {
    Log::error() << "invalid task maxinstances: " << node.toPf();
    delete d;
    return;
  }
  double f = node.doubleAttribute("maxexpectedduration", -1);
  d->_maxExpectedDuration = f < 0 ? LLONG_MAX : (long long)(f*1000);
  f = node.doubleAttribute("minexpectedduration", -1);
  d->_minExpectedDuration = f < 0 ? 0 : (long long)(f*1000);
  f = node.doubleAttribute("maxdurationbeforeabort", -1);
  d->_maxDurationBeforeAbort = f < 0 ? LLONG_MAX : (long long)(f*1000);
  d->_params.setParent(taskGroup.params());
  ConfigUtils::loadParamSet(node, &d->_params, "param");
  QString filter = d->_params.value("stderrfilter");
  if (!filter.isEmpty())
    d->_stderrFilters.append(QRegExp(filter));
  d->_setenv.setParent(taskGroup.setenv());
  ConfigUtils::loadParamSet(node, &d->_setenv, "setenv");
  d->_unsetenv.setParent(taskGroup.unsetenv());
  ConfigUtils::loadFlagSet(node, &d->_unsetenv, "unsetenv");
  d->_workflowTaskId = workflowTaskId;
  // LATER load cron triggers last exec timestamp from on-disk log
  if (workflowTaskId.isEmpty()) { // subtasks do not have triggers
    foreach (PfNode child, node.childrenByName(QStringLiteral("trigger"))) {
      foreach (PfNode grandchild, child.children()) {
        QList<PfNode> inheritedComments;
        foreach (PfNode commentNode, child.children())
          if (commentNode.isComment())
            inheritedComments.append(commentNode);
        std::reverse(inheritedComments.begin(), inheritedComments.end());
        foreach (PfNode commentNode, inheritedComments)
          grandchild.prependChild(commentNode);
        //grandchild.appendChild(commentGrandChild);
        QString content = grandchild.contentAsString();
        QString triggerType = grandchild.name();
        if (triggerType == "notice") {
          NoticeTrigger trigger(grandchild, namedCalendars);
          if (trigger.isValid()) {
            d->_noticeTriggers.append(trigger);
            Log::debug() << "configured notice trigger '" << content
                         << "' on task '" << d->_localId << "'";
          } else {
            Log::error() << "task with invalid notice trigger: "
                         << node.toString();
            delete d;
            return;
          }
        } else if (triggerType == "cron") {
          CronTrigger trigger(grandchild, namedCalendars);
          if (trigger.isValid()) {
            d->_cronTriggers.append(trigger);
            Log::debug() << "configured cron trigger "
                         << trigger.humanReadableExpression()
                         << " on task " << d->_localId;
          } else {
            Log::error() << "task with invalid cron trigger: "
                         << node.toString();
            delete d;
            return;
          }
          // LATER read misfire config
        } else {
          Log::warning() << "ignoring unknown trigger type '" << triggerType
                         << "' on task " << d->_localId;
        }
      }
    }
  } else {
    if (node.hasChild("trigger"))
      Log::warning() << "ignoring trigger in workflow subtask: "
                     << node.toString();
  }
  ConfigUtils::loadResourcesSet(node, &d->_resources, "resource");
  QString daos = node.attribute("discardaliasesonstart", "all");
  d->_discardAliasesOnStart = discardAliasesOnStartFromString(daos);
  if (d->_discardAliasesOnStart == Task::DiscardUnknown) {
    Log::error() << "invalid discardaliasesonstart on task " << d->_localId
                 << ": '" << daos << "'";
    delete d;
    return;
  }
  QList<PfNode> children = node.childrenByName("requestform");
  if (!children.isEmpty()) {
    if (children.size() > 1) {
      Log::error() << "task with several requestform: " << node.toString();
      delete d;
      return;
    }
    foreach (PfNode child, children.last().childrenByName("field")) {
      RequestFormField field(child);
      if (!field.isNull())
        d->_requestFormFields.append(field);
    }
  }
  ConfigUtils::loadEventSubscription(node, "onstart", d->_id, &d->_onstart,
                                     scheduler);
  ConfigUtils::loadEventSubscription(node, "onsuccess", d->_id,
                                     &d->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", d->_id,
                                     &d->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfailure", d->_id,
                                     &d->_onfailure, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", d->_id,
                                     &d->_onfailure, scheduler);
  QList<PfNode> steps = node.childrenByName("subtask")
      +node.childrenByName("and")+node.childrenByName("or");
  if (d->_mean == Workflow) {
    if (steps.isEmpty())
      Log::warning() << "workflow task with no step: " << node.toString();
    if (!workflowTaskId.isEmpty()) {
      Log::error() << "workflow task not allowed as a workflow subtask: "
                   << node.toString();
      delete d;
      return;
    }
    // Reading subtask and join steps.
    foreach (PfNode child, steps) {
      Step step(child, scheduler, taskGroup, d->_id, namedCalendars);
      if (step.isNull()) {
        Log::error() << "workflow task " << d->_id
                     << " has at less one invalid step definition";
        delete d;
        return;
      } if (d->_steps.contains(step.id())) {
        Log::error() << "workflow task " << d->_id
                     << " has duplicate steps with id " << step.id();
        delete d;
        return;
      } else {
        d->_steps.insert(step.id(), step);
      }
    }
    // Creating $start and $end steps and reading start transitions, which are
    // converted from (start) list in PF config to step actions in $start step
    // onready event subscription.
    QStringList startSteps = node.stringListAttribute("start").toSet().toList();
    qSort(startSteps);
    Step startStep(PfNode("start"), scheduler, taskGroup, d->_id,
                   namedCalendars);
    Step endStep(PfNode("end"), scheduler, taskGroup, d->_id,
                 namedCalendars);
    foreach (QString localId, startSteps) {
      QString id = d->_id+":"+localId;
      if (!d->_steps.contains(id)) {
        Log::error() << "workflow task " << d->_id
                     << " has invalid or lacking start steps: "
                     << node.toString();
        delete d;
        return;
      }
      startStep.appendOnReadyStep(scheduler, localId);
    }
    d->_steps.insert(startStep.id(), startStep);
    d->_steps.insert(endStep.id(), endStep);
    // Reading workflow triggers.
    int triggerCount = 0;
    QStringList ignoredChildren;
    ignoredChildren << "cron" << "notice";
    foreach (PfNode child, node.childrenByName("ontrigger")) {
      EventSubscription es(QString(), child, scheduler, ignoredChildren);
      if (es.isNull() || es.actions().isEmpty()) {
        Log::warning() << "ignoring invalid or empty ontrigger: "
                       << node.toString();
        continue;
      }
      foreach (PfNode grandchild, child.childrenByName("cron")) {
        QString triggerId = "$crontrigger_"+QString::number(triggerCount++);
        CronTrigger trigger(grandchild, namedCalendars);
        if (trigger.isValid()) {
          es.setSubscriberName(triggerId);
          d->_steps.insert(triggerId,
                           Step(Step(triggerId, trigger, es, d->_id)));
          d->_workflowCronTriggersByLocalId.insert(triggerId, trigger);
        } else
          Log::warning() << "ignoring invalid cron trigger in ontrigger: "
                         << node.toString();
      }
      foreach (PfNode grandchild, child.childrenByName("notice")) {
        NoticeTrigger trigger(grandchild, namedCalendars);
        QString triggerId = "$noticetrigger_"+trigger.expression();
        if (trigger.isValid()) {
          es.setSubscriberName(triggerId);
          d->_steps.insert(triggerId,
                           Step(Step(triggerId, trigger, es, d->_id)));
        } else
          Log::warning() << "ignoring invalid notice trigger in ontrigger: "
                         << node.toString();
      }
    }
    // Deducing workflow transitions and predecessors from step and end actions.
    foreach (QString sourceId, d->_steps.keys()) {
      QString sourceLocalId = sourceId.mid(sourceId.indexOf(':')+1);
      QList<EventSubscription> subList
          = d->_steps[sourceId].onreadyEventSubscriptions()
          + d->_steps[sourceId].subtask().allEventsSubscriptions()
          + d->_steps[sourceId].subtask().taskGroup().allEventSubscriptions();
      QSet<QString> alreadyInsertedTransitionIds;
      foreach (EventSubscription es, subList) {
        foreach (Action a, es.actions()) {
          if (a.actionType() == "step" || a.actionType() == "end") {
            QString targetLocalId = a.targetName();
            WorkflowTransition transition(d->_id, sourceLocalId, es.eventName(),
                                          targetLocalId);
            if (alreadyInsertedTransitionIds.contains(transition.id()))
              continue; // avoid processing onfinish transitions twice
            else
              alreadyInsertedTransitionIds.insert(transition.id());
            d->_transitionsBySourceLocalId.insert(sourceLocalId, transition);
            QString targetId = d->_id+":"+targetLocalId;
            if (d->_steps.contains(targetId)) {
              Log::debug() << "registring predecessor " << transition.id()
                           << " to step " << d->_steps[targetId].id();
              d->_steps[targetId].insertPredecessor(transition);
            } else {
              Log::error() << "cannot register predecessor " << transition.id()
                           << " with unknown target to workflow " << d->_id;
              delete d;
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
  ConfigUtils::loadComments(node, &d->_commentsList,
                            excludedDescendantsForComments);
  setData(d);
  // update subtasks with any other information about their workflow task apart
  // from its id which has already been given through their cstr
  // e.g. reparenting _setenv
  foreach (Step step, d->_steps) {
    Task subtask = step.subtask();
    // FIXME should probably call changeStep() or changeSubtask()
    if (!subtask.isNull()) {
      subtask.setWorkflowTask(*this);
      step.setSubtask(subtask);
      d->_steps.insert(step.id(), step);
    }
  }
  d->_graphvizWorkflowDiagram = GraphvizDiagramsBuilder::workflowTaskDiagram(*this);
}

void Task::copyLiveAttributesFromOldTask(Task oldTask) {
  TaskData *d = this->data();
  if (!d || oldTask.isNull())
    return;
  // copy mutable fields from old task (excepted _nextScheduledExecution)
  d->_lastExecution = oldTask.lastExecution().isValid()
      ? oldTask.lastExecution().toMSecsSinceEpoch() : LLONG_MIN;
  d->_instancesCount = oldTask.instancesCount();
  d->_lastSuccessful = oldTask.lastSuccessful();
  d->_lastReturnCode = oldTask.lastReturnCode();
  d->_lastTotalMillis = oldTask.lastTotalMillis();
  d->_lastTaskInstanceId = oldTask.lastTaskInstanceId();
  d->_enabled = oldTask.enabled();
  // keep last triggered timestamp from previously defined trigger
  QHash<QString,CronTrigger> oldCronTriggers;
  foreach (const CronTrigger trigger, oldTask.data()->_cronTriggers)
    oldCronTriggers.insert(trigger.canonicalExpression(), trigger);
  for (int i = 0; i < d->_cronTriggers.size(); ++i) {
    CronTrigger &trigger = d->_cronTriggers[i];
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
  return !isNull() ? data()->_params : ParamSet();
}

QList<NoticeTrigger> Task::noticeTriggers() const {
  return !isNull() ? data()->_noticeTriggers : QList<NoticeTrigger>();
}

QString Task::localId() const {
  return !isNull() ? data()->_localId : QString();
}

QString Task::label() const {
  return !isNull() ? (data()->_label.isNull() ? data()->_localId : data()->_label)
                   : QString();
}

Task::Mean Task::mean() const {
  return !isNull() ? data()->_mean : UnknownMean;
}

QString Task::command() const {
  return !isNull() ? data()->_command : QString();
}

QString Task::target() const {
  return !isNull() ? data()->_target : QString();
}

void Task::setTarget(QString target) {
  if (!isNull())
    data()->_target = target;
}

QString Task::info() const {
  return !isNull() ? data()->_info : QString();
}

TaskGroup Task::taskGroup() const {
  return !isNull() ? data()->_group : TaskGroup();
}

void Task::setTaskGroup(TaskGroup taskGroup) {
  if (!isNull())
    data()->_group = taskGroup;
}

QHash<QString, qint64> Task::resources() const {
  return !isNull() ? data()->_resources : QHash<QString,qint64>();
}

QString Task::resourcesAsString() const {
  return !isNull() ? data()->resourcesAsString() : QString();
}

QString TaskData::resourcesAsString() const {
  return QronUiUtils::resourcesAsString(_resources);
}

QString Task::triggersAsString() const {
  return !isNull() ? data()->triggersAsString() : QString();
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
  return !isNull() ? data()->triggersWithCalendarsAsString() : QString();
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
  return !isNull() ? data()->triggersHaveCalendar() : false;
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
  return !isNull() ? data()->lastExecution() : QDateTime();
}

QDateTime TaskData::lastExecution() const {
  return _lastExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(_lastExecution) : QDateTime();
}

QDateTime Task::nextScheduledExecution() const {
  return !isNull() ? data()->nextScheduledExecution() : QDateTime();
}

QDateTime TaskData::nextScheduledExecution() const {
  return _nextScheduledExecution != LLONG_MIN
      ? QDateTime::fromMSecsSinceEpoch(_nextScheduledExecution) : QDateTime();
}

void Task::setLastExecution(QDateTime timestamp) const {
  if (!isNull())
    data()->_lastExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

void Task::setNextScheduledExecution(QDateTime timestamp) const {
  if (!isNull())
    data()->_nextScheduledExecution = timestamp.isValid()
        ? timestamp.toMSecsSinceEpoch() : LLONG_MIN;
}

int Task::maxInstances() const {
  return !isNull() ? data()->_maxInstances : 0;
}

int Task::instancesCount() const {
  return !isNull() ? data()->_instancesCount.load() : 0;
}

int Task::fetchAndAddInstancesCount(int valueToAdd) const {
  return !isNull() ? data()->_instancesCount.fetchAndAddOrdered(valueToAdd) : 0;
}

QList<QRegExp> Task::stderrFilters() const {
  return !isNull() ? data()->_stderrFilters : QList<QRegExp>();
}

void Task::appendStderrFilter(QRegExp filter) {
  if (!isNull())
    data()->_stderrFilters.append(filter);
}

void Task::triggerStartEvents(TaskInstance instance) const {
  if (!isNull()) {
    data()->_group.triggerStartEvents(instance);
    foreach (EventSubscription sub, data()->_onstart)
      sub.triggerActions(instance);
  }
}

void Task::triggerSuccessEvents(TaskInstance instance) const {
  if (!isNull()) {
    data()->_group.triggerSuccessEvents(instance);
    foreach (EventSubscription sub, data()->_onsuccess)
      sub.triggerActions(instance);
  }
}

void Task::triggerFailureEvents(TaskInstance instance) const {
  if (!isNull()) {
    data()->_group.triggerFailureEvents(instance);
    foreach (EventSubscription sub, data()->_onfailure)
      sub.triggerActions(instance);
  }
}

QList<EventSubscription> Task::onstartEventSubscriptions() const {
  return !isNull() ? data()->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> Task::onsuccessEventSubscriptions() const {
  return !isNull() ? data()->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> Task::onfailureEventSubscriptions() const {
  return !isNull() ? data()->_onfailure : QList<EventSubscription>();
}

QList<EventSubscription> Task::allEventsSubscriptions() const {
  // LATER avoid creating the collection at every call
  return !isNull() ? data()->_onstart + data()->_onsuccess + data()->_onfailure
                   : QList<EventSubscription>();
}

bool Task::enabled() const {
  return !isNull() ? data()->_enabled : false;
}
void Task::setEnabled(bool enabled) const {
  if (!isNull())
    data()->_enabled = enabled;
}

bool Task::lastSuccessful() const {
  return !isNull() ? data()->_lastSuccessful : false;
}

void Task::setLastSuccessful(bool successful) const {
  if (!isNull())
    data()->_lastSuccessful = successful;
}

int Task::lastReturnCode() const {
  return !isNull() ? data()->_lastReturnCode : -1;
}

void Task::setLastReturnCode(int code) const {
  if (!isNull())
    data()->_lastReturnCode = code;
}

int Task::lastTotalMillis() const {
  return !isNull() ? data()->_lastTotalMillis : -1;
}

void Task::setLastTotalMillis(int lastTotalMillis) const {
  if (!isNull())
    data()->_lastTotalMillis = lastTotalMillis;
}

quint64 Task::lastTaskInstanceId() const {
  return !isNull() ? data()->_lastTaskInstanceId : 0;
}
void Task::setLastTaskInstanceId(quint64 lastTaskInstanceId) const {
  if (!isNull())
    data()->_lastTaskInstanceId = lastTaskInstanceId;
}

long long Task::maxExpectedDuration() const {
  return !isNull() ? data()->_maxExpectedDuration : LLONG_MAX;
}
long long Task::minExpectedDuration() const {
  return !isNull() ? data()->_minExpectedDuration : 0;
}

long long Task::maxDurationBeforeAbort() const {
  return !isNull() ? data()->_maxDurationBeforeAbort : LLONG_MAX;
}

ParamSet Task::setenv() const {
  return !isNull() ? data()->_setenv : ParamSet();
}

ParamSet Task::unsetenv() const {
  return !isNull() ? data()->_unsetenv : ParamSet();
}

Task::DiscardAliasesOnStart Task::discardAliasesOnStart() const {
  return !isNull() ? data()->_discardAliasesOnStart : Task::DiscardNone;
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
  return !isNull() ? data()->_requestFormFields : QList<RequestFormField>();
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
    QString key, QVariant defaultValue, QSet<QString> alreadyEvaluated) const {
  Q_UNUSED(alreadyEvaluated)
  //Log::fatal() << "TaskPseudoParamsProvider::paramValue " << key;
  if (key.startsWith('!')) {
    if (key == "!tasklocalid") {
      return _task.localId();
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
  return !isNull() ? data()->_cronTriggers : QList<CronTrigger>();
}

QStringList Task::otherTriggers() const {
  return !isNull() ? data()->_otherTriggers : QStringList();
}

void Task::appendOtherTriggers(QString text) {
  if (!isNull())
    data()->_otherTriggers.append(text);
}

void Task::clearOtherTriggers() {
  if (!isNull())
    data()->_otherTriggers.clear();
}

QHash<QString, Step> Task::steps() const {
  return !isNull() ? data()->_steps : QHash<QString,Step>();
}

QString Task::workflowTaskId() const {
  return !isNull() ? data()->_workflowTaskId : QString();
}

QString Task::graphvizWorkflowDiagram() const {
  return !isNull() ? data()->_graphvizWorkflowDiagram : QString();
}

QMultiHash<QString, WorkflowTransition>
Task::workflowTransitionsBySourceLocalId() const {
  return !isNull() ? data()->_transitionsBySourceLocalId
                   : QMultiHash<QString,WorkflowTransition>();
}

QHash<QString,CronTrigger> Task::workflowCronTriggersByLocalId() const {
  return !isNull() ? data()->_workflowCronTriggersByLocalId
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
        // remove workflow id prefix from localid
        return _workflowTaskId.isEmpty() ? _localId
                                      : _localId.mid(_localId.indexOf(':')+1);
      }
      return _localId;
    case 1:
      return _group.id();
    case 2:
      if (role == Qt::EditRole)
        return _label == _localId ? QVariant() : _label;
      return _label.isEmpty() ? _localId : _label;
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
          ? _minExpectedDuration*.001 : QVariant();
    case 24:
      return (_maxExpectedDuration < LLONG_MAX)
          ? _maxExpectedDuration*.001 : QVariant();
    case 25: {
      QString s;
      foreach (const RequestFormField rff, _requestFormFields)
        s.append(rff.id()).append(' ');
      s.chop(1);
      return s;
    }
    case 26:
      return _lastTotalMillis >= 0 ? _lastTotalMillis/1000.0 : QVariant();
    case 27:
      return (_maxDurationBeforeAbort < LLONG_MAX)
          ? _maxDurationBeforeAbort*.001 : QVariant();
    case 28:
      return triggersWithCalendarsAsString();
    case 29:
      return _enabled;
    case 30:
      return triggersHaveCalendar();
    case 31:
      return _workflowTaskId;
    case 32:
      return _lastTaskInstanceId > 0 ? _lastTaskInstanceId : QVariant();
    case 33:
      return _info;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

void Task::setWorkflowTask(Task workflowTask) {
  if (!isNull()) {
    TaskData *d = data();
    QString workflowTaskId = workflowTask.id();
    d->_workflowTaskId = workflowTaskId;
    d->_localId = workflowTaskId.mid(workflowTaskId.lastIndexOf('.')+1)+":"
        +d->_localId.mid(d->_localId.indexOf(':')+1);
    if (workflowTask.isNull()) {
      d->_setenv.setParent(d->_group.setenv());
      d->_unsetenv.setParent(d->_group.unsetenv());
    } else {
      d->_setenv.setParent(workflowTask.setenv());
      d->_unsetenv.setParent(workflowTask.unsetenv());
    }
  }
}

bool Task::setUiData(int section, const QVariant &value, QString *errorString,
                     int role, const SharedUiItemDocumentManager *dm) {
  if (isNull())
    return false;
  return data()->setUiData(section, value, errorString, role, dm);
}

bool TaskData::setUiData(int section, const QVariant &value,
                         QString *errorString, int role,
                         const SharedUiItemDocumentManager *dm) {
  if (!dm) {
    if (errorString)
      *errorString = "cannot set ui data without document manager";
    return false;
  }
  if (role != Qt::EditRole) {
    if (errorString)
      *errorString = "cannot set other role than EditRole";
    return false;
  }
  QString s = value.toString().trimmed(), s2;
  switch(section) {
  case 0:
    if (value.toString().isEmpty()) {
      if (errorString)
        *errorString = "id cannot be empty";
      return false;
    }
    s = ConfigUtils::sanitizeId(s, ConfigUtils::LocalId);
    if (!_workflowTaskId.isEmpty())
      s = _workflowTaskId.mid(_workflowTaskId.lastIndexOf('.')+1)+":"+s;
    s2 = _group.id()+"."+s;
    if (!dm->itemById("task", s2).isNull()) {
      if (errorString)
        *errorString = "New id is already used by another task: "+s;
      return false;
    }
    _localId = s;
    _id = s2;
    if (_mean == Task::Workflow) {
      // update transitions
      QMultiHash<QString,WorkflowTransition> newTransitions;
      foreach (const QString &sourceLocalId,
               _transitionsBySourceLocalId.uniqueKeys()) {
        foreach (WorkflowTransition newTransition, // FIXME must iterate in reverse order to preserve QMultiHash insertion order
                 _transitionsBySourceLocalId.values(sourceLocalId)) {
          newTransition.setWorkflowId(_id);
          newTransitions.insert(sourceLocalId, newTransition);
        }
      }
      _transitionsBySourceLocalId = newTransitions;
      // update steps' workflowid (for its id and predecessors)
      QHash<QString,Step> newSteps;
      foreach (Step newStep, _steps) {
        newStep.setWorkflowId(_id);
        newSteps.insert(newStep.id(), newStep);
      }
      _steps = newSteps;
      // update diagram
      // FIXME
      //d->_graphvizWorkflowDiagram = GraphvizDiagramsBuilder::workflowTaskDiagram(*this);
    }
    return true;
  case 1: {
    SharedUiItem group = dm->itemById("taskgroup", s);
    if (group.isNull()) {
      if (errorString)
        *errorString = "No group with such id: \""+s+"\"";
      return false;
    }
    if (!_workflowTaskId.isEmpty()) {
      SharedUiItem workflowTaskItem = dm->itemById("task", _workflowTaskId);
      if (!workflowTaskItem.isNull()) {
        Task workflowTask = reinterpret_cast<Task&>(workflowTaskItem);
        if (s != workflowTask.taskGroup().id()) {
          if (errorString)
            *errorString = "Cannot make a subtask belong to another group than "
                           "its parent task's group: \""+s+"\" instead of \""
              +workflowTask.taskGroup().id()+"\"";
          return false;
        }
      }
    }
    _group = reinterpret_cast<TaskGroup&>(group);
    return true;
  }
  case 2:
    _label = value.toString().trimmed();
    if (_label == _localId)
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
    _target = ConfigUtils::sanitizeId(value.toString(), ConfigUtils::FullyQualifiedId);
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
  Qt::ItemFlags flags = SharedUiItemData::uiFlags(section);
  switch (section) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 5:
  case 8:
    flags |= Qt::ItemIsEditable;
  }
  if (_mean != Task::Workflow)
    flags |= Qt::ItemNeverHasChildren;
  return flags;
}

void Task::setParentParams(ParamSet parentParams) {
  if (!isNull())
    data()->_params.setParent(parentParams);
}

TaskData *Task::data() {
  detach<TaskData>();
  return (TaskData*)SharedUiItem::data();
}

PfNode Task::toPfNode() const {
  const TaskData *d = data();
  return d ? d->toPfNode() : PfNode();
}

PfNode TaskData::toPfNode() const {
  PfNode node("task", _localId);

  // comments
  ConfigUtils::writeComments(&node, _commentsList);

  // description and execution attributes
  node.setAttribute("taskgroup", _group.id());
  if (!_label.isEmpty() && _label != _id)
    node.setAttribute("label", _label);
  if (!_info.isEmpty())
    node.setAttribute("info", _info);
  node.setAttribute("mean", Task::meanAsString(_mean));
  // do not set target attribute if it is empty,
  // or in case it is implicit ("localhost" for Local mean)
  if (!_target.isEmpty()
      && (_target != "localhost" || !_mean == Task::Local))
    node.setAttribute("target", _target);
  // do not set command attribute if it is empty
  // or for means that do not use it (Workflow and DoNothing)
  if (!_command.isEmpty()
      && _mean != Task::DoNothing
      && _mean != Task::Workflow)
    node.setAttribute("command", _command);

  // triggering and constraints attributes
  PfNode triggers("trigger");
  foreach (const Trigger &ct, _cronTriggers)
    triggers.appendChild(ct.toPfNode());
  foreach (const Trigger &nt, _noticeTriggers)
    triggers.appendChild(nt.toPfNode());
  node.appendChild(triggers);
  if (_discardAliasesOnStart != Task::DiscardAll)
    node.appendChild(
          PfNode("discardaliasesonstart",
                 Task::discardAliasesOnStartAsString(_discardAliasesOnStart)));
  if (_maxInstances != 1)
    node.appendChild(PfNode("maxinstances",
                            QString::number(_maxInstances)));
  foreach (const QString &key, _resources.keys())
    node.appendChild(
          PfNode("resource",
                 key+" "+QString::number(_resources.value(key))));

  // workflow attributes
  Step startStep = _steps.value(id()+":$start");
  QSet<QString> startSteps;
  foreach (const EventSubscription &es, startStep.onreadyEventSubscriptions())
    foreach (const Action &a, es.actions()) {
      PfNode node = a.toPfNode();
      if (node.name() == "step") { // should always be true
        QString stepLocalId = node.contentAsString();
        if (!stepLocalId.isEmpty()) // should always be true
          startSteps.insert(stepLocalId);
      }
    }
  if (startSteps.size())
    ConfigUtils::writeFlagSet(&node, startSteps, "start");
  QList<Step> steps = _steps.values();
  qSort(steps);
  foreach(const Step &step, steps) {
    const Trigger trigger = step.trigger();
    if (trigger.isValid()) {
      // TODO should be only one es per step, even for non-trigger steps
      foreach (const EventSubscription &es, step.onreadyEventSubscriptions()) {
        PfNode ontrigger = es.toPfNode();
        ontrigger.prependChild(trigger.toPfNode());
        node.appendChild(ontrigger);
      }
    }
  }
  foreach (const Step &step, steps)
    if (!step.id().contains('$'))
      node.appendChild(step.toPfNode());

  // params and setenv attributes
  ConfigUtils::writeParamSet(&node, _params, "param");
  ConfigUtils::writeParamSet(&node, _setenv, "setenv");
  ConfigUtils::writeFlagSet(&node, _unsetenv, "unsetenv");

  // monitoring and alerting attributes
  if (_maxExpectedDuration < LLONG_MAX)
    node.appendChild(PfNode("maxexpectedduration",
                            QString::number(_maxExpectedDuration/1e3)));
  if (_minExpectedDuration > 0)
    node.appendChild(PfNode("minexpectedduration",
                            QString::number(_minExpectedDuration/1e3)));
  if (_maxDurationBeforeAbort < LLONG_MAX)
    node.appendChild(PfNode("maxdurationbeforeabort",
                            QString::number(_maxDurationBeforeAbort/1e3)));

  // events (but workflow-specific "ontrigger" events)
  ConfigUtils::writeEventSubscriptions(&node, _onstart);
  ConfigUtils::writeEventSubscriptions(&node, _onsuccess);
  ConfigUtils::writeEventSubscriptions(&node, _onfailure,
                                       QStringList("onfinish"));

  // user interface attributes
  if (!_requestFormFields.isEmpty()) {
    PfNode requestForm("requestform");
    foreach (const RequestFormField &field, _requestFormFields)
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

void Task::changeWorkflowTransition(WorkflowTransition newItem,
                                    WorkflowTransition oldItem) {
  // FIXME
  // probably strongly related to changeStep
}

void Task::changeStep(Step newItem, Step oldItem) {
  // FIXME
  // probably strongly related to changeWorkflowTransition
  // must probably also manage triggers collections (since triggers are steps)
  if (!oldItem.isNull()) {
    // FIXME
  }
  if (!newItem.isNull()) {
    // FIXME
  }
}

void Task::changeSubtask(Task newItem, Task oldItem) {
  TaskData *d = data();
  if (!d)
    return;
  Step oldStep = d->_steps.value(oldItem.id());
  if (oldStep.isNull() // cannot create subtasks
      || oldStep.kind() != Step::SubTask // only applicable to subtask steps
      || newItem.isNull() // cannot delete subtasks
      || newItem.id() != oldItem.id() // cannot rename subtasks
      ) {
    Log::debug() << "Task::changeSubtask() called in an unsupported way";
    return;
  }
  Step newStep = oldStep;
  newStep.setSubtask(newItem);
  changeStep(newStep, oldStep);
}
