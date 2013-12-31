/* Copyright 2013 Hallowyn and others.
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
#include "action_p.h"
#include <QSharedData>
#include "log/log.h"
#include "postnoticeaction.h"
#include "raisealertaction.h"
#include "cancelalertaction.h"
#include "emitalertaction.h"
#include "requesttaskaction.h"
#include "udpaction.h"
#include "logaction.h"
#include "stepaction.h"
#include "endaction.h"

Action::Action() {
}

Action::Action(const Action &rhs) : d(rhs.d) {
}

Action::Action(ActionData *data) : d(data) {
}

Action &Action::operator=(const Action &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Action::~Action() {
}

ActionData::~ActionData() {
}

void Action::trigger(EventSubscription subscription,
                     const ParamsProvider *context) const {
  if (d)
    d->trigger(subscription, context);
}

void Action::trigger(EventSubscription subscription,
                     TaskInstance context) const {
  if (d)
    d->triggerWithinTaskInstance(subscription, context);
}

QString ActionData::toString() const {
  return "Action";
}

QString ActionData::actionType() const {
  return "unknown";
}

void ActionData::trigger(EventSubscription subscription,
                         const ParamsProvider *context) const {
  Q_UNUSED(context)
  Log::error() << "ActionData::trigger() called whereas it should never, "
                  "from subscription " << subscription.subscriberName()
                  << "|" << subscription.eventName();
}

void ActionData::triggerWithinTaskInstance(EventSubscription subscription,
                                           TaskInstance context) const {
  // default behaviour for actions that do not need to be aware of task instance
  // context
  trigger(subscription, &context);
}

QString Action::toString() const {
  return d ? d->toString() : QString();
}

QString Action::actionType() const {
  return d ? d->actionType() : QString();
}

QStringList Action::toStringList(QList<Action> list) {
  QStringList sl;
  foreach (const Action e, list)
    sl.append(e.toString());
  return sl;
}

QString Action::targetName() const {
  return d ? d->targetName() : QString();
}

QString ActionData::targetName() const {
  return QString();
}

Action Action::createAction(PfNode node, Scheduler *scheduler) {
  // LATER implement HttpAction
  // LATER implement ExecAction
  if (node.name() == "postnotice") {
    return PostNoticeAction(scheduler, node.contentAsString());
  } else if (node.name() == "raisealert") {
    return RaiseAlertAction(scheduler, node.contentAsString());
  } else if (node.name() == "cancelalert") {
    return CancelAlertAction(scheduler, node.contentAsString());
  } else if (node.name() == "emitalert") {
    return EmitAlertAction(scheduler, node.contentAsString());
  } else if (node.name() == "requesttask") {
    ParamSet params;
    // TODO loadparams
    return RequestTaskAction(scheduler, node.contentAsString(), params,
                             node.hasChild("force"));
  } else if (node.name() == "udp") {
    return UdpAction(node.attribute("address"), node.attribute("message"));
  } else if (node.name() == "log") {
    return LogAction(
          Log::severityFromString(node.attribute("severity", "info")),
          node.contentAsString());
  } else if (node.name() == "step") {
    return StepAction(scheduler, node.contentAsString());
  } else if (node.name() == "end") {
    // TODO success and returncode should be evaluatable strings
    return EndAction(scheduler, node.boolAttribute("success", true),
                     node.longAttribute("returncode", 0));
  } else {
    Log::error() << "unknown action type: " << node.name();
    return Action();
  }
}