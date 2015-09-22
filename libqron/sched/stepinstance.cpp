/* Copyright 2013-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#include "stepinstance.h"
#include <QSharedData>
#include "modelview/shareduiitemlist.h"

class StepInstanceData : public QSharedData {
public:
  Step _step;
  mutable bool _ready;
  // note that QSet is not thread safe, however it is only modified by
  // predecessorReady() which is only called by one thread (the executor thread
  // handling the workflow)
  mutable QSet<WorkflowTransition> _pendingPredecessors;
  TaskInstance _workflowTaskInstance;
  StepInstanceData(Step step = Step(),
                   TaskInstance workflowTaskInstance = TaskInstance())
    : _step(step), _ready(false), _pendingPredecessors(step.predecessors()),
      _workflowTaskInstance(workflowTaskInstance) { }
};

StepInstance::StepInstance() {
}

StepInstance::StepInstance(Step step, TaskInstance workflowTaskInstance)
  : d(new StepInstanceData(step, workflowTaskInstance)) {
}

StepInstance::~StepInstance() {
}

StepInstance::StepInstance(const StepInstance &rhs) : d(rhs.d) {
}

StepInstance &StepInstance::operator=(const StepInstance &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Step StepInstance::step() const {
  return d ? d->_step : Step();
}

bool StepInstance::isReady() const {
  return d ? d->_ready : false;
}

void StepInstance::predecessorReady(WorkflowTransition predecessor,
                                    ParamSet eventContext) const {
  if (!d)
    return;
  // if event was "onsuccess" or "onfailure", pretend it was "onfinish"
  if (predecessor.eventName() == "onsuccess"
      || predecessor.eventName() == "onfailure") {
    predecessor.setEventName("onfinish");
  }
  d->_pendingPredecessors.remove(predecessor);
  switch (d->_step.kind()) {
  case Step::AndJoin:
    //Log::fatal(d->_workflowTaskInstance.task().id(),
    //         d->_workflowTaskInstance.idAsLong())
    //  << "AndJoin predecessorReady called for predecessor "
    //  << predecessor.id() << " with pending predecessors "
    //  << SharedUiItemList(d->_pendingPredecessors.toList()).join(' ', false);
    if (!d->_ready && d->_pendingPredecessors.isEmpty()) {
      d->_ready = true;
      d->_step.triggerReadyEvents(d->_workflowTaskInstance, eventContext);
    }
    break;
  case Step::OrJoin:
  case Step::SubTask: // a subtask step is actually implicitly a or join
  case Step::Start: // the start step is triggered once to start first steps
    if (!d->_ready) {
      d->_ready = true;
      d->_step.triggerReadyEvents(d->_workflowTaskInstance, eventContext);
    }
    break;
  case Step::WorkflowTrigger:
  case Step::Unknown:
    Log::error(d->_workflowTaskInstance.task().id(),
               d->_workflowTaskInstance.idAsLong())
        << "StepInstance::predecessorReady called on a step of kind "
        << d->_step.kindAsString();
    break;
  case Step::End: // this should never happen
    ;
  }
}

TaskInstance StepInstance::workflowTaskInstance() const {
  return d ? d->_workflowTaskInstance : TaskInstance();
}
