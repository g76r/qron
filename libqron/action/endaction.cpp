/* Copyright 2013-2014 Hallowyn and others.
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
#include "endaction.h"
#include "action_p.h"

class EndActionData : public ActionData {
public:
  bool _success;
  int _returnCode;
  EndActionData(Scheduler *scheduler, bool success = true, int returnCode = 0)
    : ActionData(scheduler), _success(success), _returnCode(returnCode) {
  }
  QString toString() const {
    return "->$end";
  }
  QString actionType() const {
    return "end";
  }
  void trigger(EventSubscription subscription,
                                 ParamSet eventContext,
                                 TaskInstance instance) const {
    if (instance.isNull()) {
      // this should never happen since no one should ever configure a step
      // action in global events subscriptions
      Log::error() << "EndAction::trigger() called outside a TaskInstance "
                      "context, for subscription "
                   << subscription.subscriberName() << "|"
                   << subscription.eventName();
      return;
    }
    QString transitionId = subscription.subscriberName()+"|"
        +subscription.eventName()+"|$end";
    TaskInstance workflow = instance.callerTask();
    if (workflow.isNull())
      workflow = instance;
    //Log::fatal(instance.task().fqtn(), instance.id())
    //    << "EndAction::triggerWithinTaskInstance "
    //    << transitionId << " " << instance.task().fqtn();
    if (workflow.task().mean() != "workflow") {
      Log::error(instance.task().fqtn(), instance.id())
          << "executing a end action in the context of a non-workflow task, "
             "for subscription " << subscription.subscriberName() << "|"
          << subscription.eventName();
      return;
    }
    eventContext.setValue("!success", (_success ? "true" : "false"));
    eventContext.setValue("!returncode", QString::number(_returnCode));
    if (_scheduler)
      _scheduler->activateWorkflowTransition(workflow, transitionId,
                                             eventContext);
  }

};

EndAction::EndAction(Scheduler *scheduler, bool success, int returnCode)
  : Action(new EndActionData(scheduler, success, returnCode)) {
}

EndAction::EndAction(const EndAction &rhs) : Action(rhs) {
}

EndAction &EndAction::operator=(const EndAction &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

EndAction::~EndAction() {
}
