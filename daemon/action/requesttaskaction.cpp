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
#include "requesttaskaction.h"
#include "action_p.h"

class RequestTaskActionData : public ActionData {
public:
  QString _idOrFqtn;
  ParamSet _params;
  bool _force;
  RequestTaskActionData(Scheduler *scheduler = 0, QString idOrFqtn = QString(),
                       ParamSet params = ParamSet(), bool force = false)
    : ActionData(scheduler), _idOrFqtn(idOrFqtn), _params(params), _force(force) { }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler) {
      // LATER implement triggerWithinTaskInstance rather than guessing if we are in a TaskInstance context
      QString fqtn = context
          ? context->paramValue("!fqtn").toString() : QString();
      QString id = context
          ? context->paramValue("!taskinstanceid").toString() : QString();
      QString group = context
          ? context->paramValue("!taskgroupid").toString() : QString();
      Log::log(fqtn.isNull() ? Log::Debug : Log::Info, fqtn, id.toLongLong())
          << "requesttask action requesting execution of task " << _idOrFqtn;
      QString idOrFqtn = _params.evaluate(_idOrFqtn, context);
      QString local = group+"."+_idOrFqtn;
      // TODO evaluate _params in the context of triggering event before pass them to asyncRequestTask
      if (_scheduler.data()->taskExists(local))
        _scheduler.data()->asyncRequestTask(local, _params, _force);
      else
        _scheduler.data()->asyncRequestTask(idOrFqtn, _params, _force);
      // LATER if requestTask returns the TaskRequest object, we can track child taskinstanceid
      // LATER this special case should be logged to a special data model to enable drawing parent-child diagrams
    }
  }
  QString toString() const {
    return "*" + _idOrFqtn;
  }
  QString actionType() const {
    return "requesttask";
  }
  QString targetName() const {
    return _idOrFqtn;
  }
};

RequestTaskAction::RequestTaskAction(Scheduler *scheduler, QString idOrFqtn,
                                   ParamSet params, bool force)
  : Action(new RequestTaskActionData(scheduler, idOrFqtn, params, force)) {
}

RequestTaskAction::RequestTaskAction(const RequestTaskAction &rhs) : Action(rhs) {
}

RequestTaskAction::~RequestTaskAction() {
}
