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
#include "stepaction.h"
#include "action_p.h"
#include "config/configutils.h"

class StepActionData : public ActionData {
  // using QStringList would be nice but inconsistent with targetName()
  QString _stepLocalId;

public:
  StepActionData(Scheduler *scheduler = 0, QString stepLocalId = QString())
    : ActionData(scheduler), _stepLocalId(stepLocalId) {
  }
  QString toString() const {
    return "->" + _stepLocalId;
  }
  QString actionType() const {
    return "step";
  }
  QString targetName() const {
    return _stepLocalId;
  }
  void trigger(EventSubscription subscription,
               ParamSet eventContext,
               TaskInstance instance) const {
    if (instance.isNull()) {
      // this should never happen since no one should ever configure a step
      // action in global events subscriptions
      Log::error() << "StepAction::trigger() called outside a TaskInstance "
                      "context, for subscription "
                   << subscription.subscriberName() << "|"
                   << subscription.eventName()
                   << " with stepLocalId " << _stepLocalId;
      return;
    }
    QString transitionId = subscription.subscriberName()+"|"
        +subscription.eventName()+"|"+_stepLocalId;
    TaskInstance workflow = instance.workflowInstanceTask();
    if (workflow.isNull())
      workflow = instance;
    //Log::fatal(instance.task().id(), instance.id())
    //    << "StepAction::triggerWithinTaskInstance "
    //    << transitionId << " " << instance.task().id()
    //    << " " << eventContext;
    if (workflow.task().mean() != Task::Workflow) {
      Log::warning(instance.task().id(), instance.idAsLong())
          << "executing a step action in the context of a non-workflow task, "
             "for subscription " << subscription.subscriberName() << "|"
          << subscription.eventName();
      return;
    }
    if (_scheduler)
      _scheduler->activateWorkflowTransition(workflow, transitionId,
                                             eventContext);
  }
  PfNode toPfNode() const {
    // following line is failsafe if not found since -1+1=0
    QString stepLocalId = _stepLocalId.mid(_stepLocalId.indexOf(':')+1);
    return PfNode(actionType(), stepLocalId);
  }
};

StepAction::StepAction(Scheduler *scheduler, PfNode node)
  : Action(new StepActionData(
             scheduler,
             ConfigUtils::sanitizeId(node.contentAsString(),
                                     ConfigUtils::TaskId))) {
}

StepAction::StepAction(Scheduler *scheduler, QString stepLocalId)
  : Action(new StepActionData(scheduler, stepLocalId)) {
}

StepAction::StepAction(const StepAction &rhs) : Action(rhs) {
}

StepAction &StepAction::operator=(const StepAction &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}
