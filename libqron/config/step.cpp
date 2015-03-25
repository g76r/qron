/* Copyright 2013-2015 Hallowyn and others.
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
#include "step.h"
#include <QSharedData>
#include "pf/pfnode.h"
#include "task.h"
#include "sched/scheduler.h"
#include "log/log.h"
#include "configutils.h"
#include "action/requesttaskaction.h"
#include "modelview/shareduiitemdocumentmanager.h"
#include "action/stepaction.h"
#include "trigger/trigger.h"
#include "modelview/shareduiitemlist.h"

static QString _uiHeaderNames[] = {
  "Local Id", // 0
  "Id",
  "Step Kind",
  "Workflow",
  "Subtask",
  "Predecessors", // 5
  "On Ready",
  "On Subtask Start",
  "On Subtask Success",
  "On Subtask Failure",
  "Trigger Expression" // 10
};

class StepData : public SharedUiItemData {
public:
  QString _id, _localId, _workflowId;
  Step::Kind _kind;
  Task _subtask;
  QSet<WorkflowTransition> _predecessors;
  QList<EventSubscription> _onready;
  Trigger _trigger;
  StepData() : _kind(Step::Unknown) { }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  QString idQualifier() const { return "step"; }
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 int role, const SharedUiItemDocumentManager *dm);
  Qt::ItemFlags uiFlags(int section) const;
  void setWorkflowId(QString workflowId);
};

Step::Step(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QString workflowTaskId, QHash<QString, Calendar> namedCalendars) {
  StepData *d = new StepData;
  d->_localId = ConfigUtils::sanitizeId(node.contentAsString(),
                                        ConfigUtils::LocalId);
  d->_id = workflowTaskId+":"+d->_localId;
  d->_workflowId = workflowTaskId;
  d->_kind = kindFromString(node.name());
  switch (d->_kind) {
  case AndJoin:
    ConfigUtils::loadEventSubscription(node, "onready", d->_id,
                                       &d->_onready, scheduler);
    if (node.hasChild("onsuccess") || node.hasChild("onfailure")
        || node.hasChild("onfinish") || node.hasChild("onstart"))
      Log::warning() << "ignoring other events than onready in non-subtask "
                        "step: " << node.toString();
    setData(d);
    return;
  case OrJoin:
    ConfigUtils::loadEventSubscription(node, "onready", d->_id,
                                       &d->_onready, scheduler);
    if (node.hasChild("onsuccess") || node.hasChild("onfailure")
        || node.hasChild("onfinish") || node.hasChild("onstart"))
      Log::warning() << "ignoring other events than onready in non-subtask "
                        "step: " << node.toString();
    setData(d);
    return;
  case SubTask:
    // TODO move this check in Task ?
    if (node.hasChild("taskgroup"))
      Log::warning() << "ignoring subtask taskgroup: " << node.toString();
    if (node.hasChild("trigger")) // TODO duplicate check in Task
      Log::warning() << "ignoring subtask triggers: " << node.toString();
    if (node.hasChild("onready"))
      Log::warning() << "ignoring subtask onready event: " << node.toString();
    // compute subtask localid: app1.group1.workflow1:step1 -> workflow1:step1
    // following line is failsafe even without . since -1+1=0
    node.setContent(d->_id.mid(d->_id.lastIndexOf('.')+1));
    d->_subtask = Task(node, scheduler, taskGroup, d->_workflowId,
                       namedCalendars);
    if (d->_subtask.isNull()) {
      Log::error() << "step with invalid subtask: " << node.toString();
      delete d;
      return;
    }
    d->_onready.append(
          EventSubscription(d->_id, "onready",
                            RequestTaskAction(scheduler, d->_subtask.id())));
    setData(d);
    return;
  case Start:
    // must force ids since $ is not an allowed char
    d->_localId = "$start";
    d->_id = workflowTaskId+":$start";
    setData(d);
    return;
  case End:
    // must force ids since $ is not an allowed char
    d->_localId = "$end";
    d->_id = workflowTaskId+":$end";
    setData(d);
    return;
  default:
    Log::error() << "unsupported step kind: " << node.toString();
    delete d;
    return;
  }
}

Step::Step(QString localId, Trigger trigger, EventSubscription es,
           QString workflowTaskId) {
  StepData *d = new StepData;
  d->_localId = localId;
  d->_id = workflowTaskId+":"+d->_localId;
  d->_workflowId = workflowTaskId;
  d->_kind = WorkflowTrigger;
  d->_onready.append(es);
  d->_trigger = trigger;
  setData(d);
}

Step::Step() {
}

Step::Step(const Step &other) : SharedUiItem(other) {
}

QString Step::localId() const {
  return !isNull() ? data()->_localId : QString();
}

Step::Kind Step::kind() const {
  return !isNull() ? data()->_kind : Step::Unknown;
}

Task Step::subtask() const {
  return !isNull() ? data()->_subtask : Task();
}

QString Step::workflowId() const {
  return !isNull() ? data()->_workflowId : QString();
}

Trigger Step::trigger() const {
  return !isNull() ? data()->_trigger : Trigger();
}

QSet<WorkflowTransition> Step::predecessors() const {
  return !isNull() ? data()->_predecessors : QSet<WorkflowTransition>();
}

void Step::insertPredecessor(WorkflowTransition predecessor) {
  if (!isNull()) {
    // if event was "onsuccess" or "onfailure", pretend it was "onfinish"
    if (predecessor.eventName() == "onsuccess"
        || predecessor.eventName() == "onfailure") {
      predecessor.setEventName("onfinish");
    }
    data()->_predecessors.insert(predecessor);
  }
}

void Step::triggerReadyEvents(TaskInstance workflowTaskInstance,
                              ParamSet eventContext) const {
  if (!isNull()) {
    //eventContext.setValue("!stepid", d->_id);
    //eventContext.setValue("!fqsn", d->_fqsn);
    foreach (EventSubscription sub, data()->_onready)
      sub.triggerActions(eventContext, workflowTaskInstance);
  }
}

QList<EventSubscription> Step::onreadyEventSubscriptions() const {
  return !isNull() ? data()->_onready : QList<EventSubscription>();
}

QString Step::kindAsString(Kind kind) {
  switch (kind) {
  case SubTask:
    return "subtask";
  case AndJoin:
    return "and";
  case OrJoin:
    return "or";
  case Start:
    return "start";
  case End:
    return "end";
  case WorkflowTrigger:
    return "trigger";
  case Unknown:
    ;
  }
  return "unknown";
}

Step::Kind Step::kindFromString(QString kind) {
  if (kind == "subtask")
    return SubTask;
  if (kind == "or")
    return OrJoin;
  if (kind == "and")
    return AndJoin;
  if (kind == "start")
    return Start;
  if (kind == "end")
    return End;
  if (kind == "trigger")
    return WorkflowTrigger;
  return Unknown;
}

PfNode Step::toPfNode() const {
  const StepData *d = data();
  if (!d)
    return PfNode();
  PfNode node(kindAsString(d->_kind), d->_localId);
  foreach (const PfNode &child, d->_subtask.toPfNode().children()) {
    const QString &name = child.name();
    if (name != "taskgroup" && name != "trigger")
      node.appendChild(child);
  }
  if (d->_kind != SubTask)
    ConfigUtils::writeEventSubscriptions(&node, d->_onready);
  return node;
}

StepData *Step::data() {
  detach<StepData>();
  return (StepData*)SharedUiItem::data();
}

QVariant StepData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _localId;
    case 1:
      return _id;
    case 2:
      return Step::kindAsString(_kind);
    case 3:
      return _workflowId;
    case 4:
      return _subtask.id();
    case 5:
      return SharedUiItemList(_predecessors.toList()).join(' ', false);
    case 6:
      return EventSubscription::toStringList(_onready).join('\n');
    case 7:
      return EventSubscription::toStringList(
            _subtask.onstartEventSubscriptions()).join('\n');
    case 8:
      return EventSubscription::toStringList(
            _subtask.onsuccessEventSubscriptions()).join('\n');
    case 9:
      return EventSubscription::toStringList(
            _subtask.onfailureEventSubscriptions()).join('\n');
    case 10:
      return _trigger.humanReadableExpressionWithCalendar();
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant StepData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int StepData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

bool Step::setUiData(
    int section, const QVariant &value, QString *errorString,
    int role, const SharedUiItemDocumentManager *dm) {
  if (isNull())
    return false;
  return data()->setUiData(section, value, errorString, role, dm);
}

bool StepData::setUiData(
    int section, const QVariant &value, QString *errorString, int role,
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
  case 1:
    if (!s.startsWith(_workflowId+":")) {
      if (errorString)
        *errorString = "id must start with workflow id and a colon";
      return false;
    }
    s = s.mid(_workflowId.size()+1);
    // falling into next case
  case 0:
    if (value.toString().isEmpty()) {
      if (errorString)
        *errorString = "id cannot be empty";
      return false;
    }
    s = ConfigUtils::sanitizeId(s, ConfigUtils::FullyQualifiedId);
    s2 = _workflowId+":"+s;
    if (!dm->itemById("step", s2).isNull()) {
      if (errorString)
        *errorString = "New id is already used by another step: "+s;
      return false;
    }
    _localId = s;
    _id = s2;
    return true;
  case 2:
    // TODO step kind, limited to AndJoin <-> OrJoin
    break;
  /*case 3: {
    s = ConfigUtils::sanitizeId(s, ConfigUtils::FullyQualifiedId);
    SharedUiItem item = dm->itemById("task", s);
    if (item.isNull()
        || reinterpret_cast<Task&>(item).mean() != Task::Workflow) {
      if (errorString)
        *errorString = "New workflowid does not match a workflow task: "+s;
      return false;
    }
    setWorkflowId(s);
    return true;
  }*/
  case 10:
    // TODO trigger expression
    ;
  }
  if (errorString)
    *errorString = "field \""+uiHeaderData(section, Qt::DisplayRole).toString()
      +"\" is not ui-editable";
  return false;
}

void Step::setWorkflowId(QString workflowId) {
  if (!isNull())
    data()->setWorkflowId(workflowId);
}

void StepData::setWorkflowId(QString workflowId) {
  _workflowId = workflowId;
  QSet<WorkflowTransition> newPredecessors;
  foreach (WorkflowTransition t, _predecessors) {
    t.setWorkflowId(workflowId);
    newPredecessors.insert(t);
  }
  _predecessors = newPredecessors;
}

Qt::ItemFlags StepData::uiFlags(int section) const {
  Qt::ItemFlags flags = SharedUiItemData::uiFlags(section);
  switch (section) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 10:
    flags |= Qt::ItemIsEditable;
  }
  return flags;
}

void Step::appendOnReadyStep(Scheduler *scheduler, QString localStepId) {
  StepData *d = data();
  if (d)
    d->_onready.append(
          EventSubscription(d->_id, "onready",
                            StepAction(scheduler, localStepId)));
}
