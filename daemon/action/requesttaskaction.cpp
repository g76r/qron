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
  void triggerWithinTaskInstance(EventSubscription subscription,
                                 TaskInstance context) const {
    Q_UNUSED(subscription)
    if (_scheduler) {
      QString fqtn = context.task().fqtn();
      quint64 id = context.id();
      QString group = context.task().taskGroup().id();
      QString idOrFqtn = _params.evaluate(_idOrFqtn, &context);
      QString local = group+"."+_idOrFqtn;
      // TODO evaluate _params in the context of triggering event before pass them to syncRequestTask
      QList<TaskInstance> instances
          = _scheduler.data()->syncRequestTask(
            _scheduler.data()->taskExists(local) ? local : idOrFqtn,
            _params, _force, context);
      if (instances.isEmpty())
        Log::error(fqtn, id)
            << "requesttask action failed to request execution of task "
            << idOrFqtn << " within event subscription context "
            << subscription.subscriberName() << "|" << subscription.eventName();
      else
        foreach (TaskInstance instance, instances)
          Log::info(fqtn, id)
              << "requesttask action requested execution of task "
              << instance.task().fqtn() << "/" << instance.groupId();
    }

  }
  void trigger(EventSubscription subscription,
               const ParamsProvider *context) const {
    Q_UNUSED(subscription)
    Q_UNUSED(context)
    if (_scheduler) {
      // TODO evaluate _params in the context of triggering event before pass them to syncRequestTask
      QList<TaskInstance> instances
          = _scheduler.data()->syncRequestTask(_idOrFqtn, _params, _force);
      if (instances.isEmpty())
        Log::error()
            << "requesttask action failed to request execution of task "
            << _idOrFqtn << " within event subscription context "
            << subscription.subscriberName() << "|" << subscription.eventName();
      else
        foreach (TaskInstance instance, instances)
          Log::info()
              << "requesttask action requested execution of task "
              << instance.task().fqtn() << "/" << instance.groupId();
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

RequestTaskAction::RequestTaskAction(const RequestTaskAction &rhs)
  : Action(rhs) {
}

RequestTaskAction::~RequestTaskAction() {
}
