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
#include "logaction.h"
#include "action_p.h"
#include "util/paramset.h"

class LogActionData : public ActionData {
public:
  QString _message;
  Log::Severity _severity;
  LogActionData(QString logMessage = QString(),
               Log::Severity severity = Log::Info)
    : _message(logMessage), _severity(severity) { }
  QString toString() const {
    return "log{ "+Log::severityToString(_severity)+" "+_message+" }";
  }
  QString actionType() const {
    return "log";
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance instance) const {
    Q_UNUSED(subscription)
    if (instance.isNull()) {
      Log::log(_severity) << eventContext.evaluate(_message);
    } else {
      TaskInstancePseudoParamsProvider ppp = instance.pseudoParams();
      Log::log(_severity, instance.task().id(), instance.idAsLong())
          << eventContext.evaluate(_message, &ppp);
    }
  }
  PfNode toPfNode() const{
    PfNode node(actionType(), _message);
    node.appendChild(PfNode("severity", Log::severityToString(_severity)));
    return node;
  }
};

LogAction::LogAction(Scheduler *scheduler, PfNode node)
  : Action(new LogActionData(
             node.contentAsString(),
             Log::severityFromString(node.attribute("severity", "info")))) {
  Q_UNUSED(scheduler)
}

LogAction::LogAction(const LogAction &rhs) : Action(rhs) {
}

LogAction::~LogAction() {
}
