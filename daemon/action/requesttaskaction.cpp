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
#include "requesttaskaction.h"
#include "action_p.h"

class RequestTaskActionData : public ActionData {
public:
  QString _idOrFqtn;
  ParamSet _overridingParams;
  bool _force;
  RequestTaskActionData(Scheduler *scheduler = 0, QString idOrFqtn = QString(),
                        ParamSet params = ParamSet(), bool force = false)
    : ActionData(scheduler), _idOrFqtn(idOrFqtn), _overridingParams(params),
      _force(force) { }
  inline ParamSet evaluatedOverrindingParams(ParamSet eventContext,
                                             TaskInstance instance) const {
    ParamSet overridingParams;
    foreach (QString key, _overridingParams.keys())
      overridingParams
          .setValue(key, eventContext
                    .value(_overridingParams.rawValue(key), &instance));
    //Log::fatal() << "******************* " << requestParams;
    return overridingParams;
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance instance) const {
    if (_scheduler) {
      QString fqtn;
      if (instance.isNull()) {
        fqtn = _idOrFqtn;
      } else {
        fqtn = eventContext.evaluate(_idOrFqtn, &instance);
        QString fqtnLocalToGroup = instance.task().taskGroup().id()+"."+_idOrFqtn;
        if (_scheduler.data()->taskExists(fqtnLocalToGroup))
          fqtn = fqtnLocalToGroup;
      }
      QList<TaskInstance> instances = _scheduler.data()->syncRequestTask(
          fqtn, evaluatedOverrindingParams(eventContext, instance), _force,
          instance);
      if (instances.isEmpty())
        Log::error(instance.task().fqtn(), instance.id())
            << "requesttask action failed to request execution of task "
            << fqtn << " within event subscription context "
            << subscription.subscriberName() << "|" << subscription.eventName();
      else
        foreach (TaskInstance instance, instances)
          Log::info(instance.task().fqtn(), instance.id())
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
