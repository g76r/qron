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
#include "requesttaskaction.h"
#include "action_p.h"
#include "config/configutils.h"

class RequestTaskActionData : public ActionData {
public:
  QString _id;
  ParamSet _overridingParams;
  bool _force;
  RequestTaskActionData(Scheduler *scheduler = 0, QString id = QString(),
                        ParamSet params = ParamSet(), bool force = false)
    : ActionData(scheduler), _id(id), _overridingParams(params),
      _force(force) { }
  inline ParamSet evaluatedOverrindingParams(ParamSet eventContext,
                                             TaskInstance instance) const {
    ParamSet overridingParams;
    TaskInstancePseudoParamsProvider ppp = instance.pseudoParams();
    foreach (QString key, _overridingParams.keys())
      overridingParams
          .setValue(key, eventContext
                    .value(_overridingParams.rawValue(key), &ppp));
    //Log::fatal() << "******************* " << requestParams;
    return overridingParams;
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance parentInstance) const {
    if (_scheduler) {
      QString id;
      if (parentInstance.isNull()) {
        id = _id;
      } else {
        TaskInstancePseudoParamsProvider ppp = parentInstance.pseudoParams();
        id = eventContext.evaluate(_id, &ppp);
        QString idIfLocalToGroup = parentInstance.task().taskGroup().id()
            +"."+_id;
        if (_scheduler.data()->taskExists(idIfLocalToGroup))
          id = idIfLocalToGroup;
      }
      QList<TaskInstance> instances = _scheduler.data()->syncRequestTask(
          id, evaluatedOverrindingParams(eventContext, parentInstance), _force,
          parentInstance);
      if (instances.isEmpty())
        Log::error(parentInstance.task().id(), parentInstance.idAsLong())
            << "requesttask action failed to request execution of task "
            << id << " within event subscription context "
            << subscription.subscriberName() << "|" << subscription.eventName();
      else
        foreach (TaskInstance childInstance, instances)
          Log::info(parentInstance.task().id(), parentInstance.idAsLong())
              << "requesttask action requested execution of task "
              << childInstance.task().id() << "/" << childInstance.groupId();
    }

  }
  QString toString() const {
    return "*" + _id;
  }
  QString actionType() const {
    return "requesttask";
  }
  QString targetName() const {
    return _id;
  }
  PfNode toPfNode() const{
    PfNode node(actionType(), _id);
    ConfigUtils::writeParamSet(&node, _overridingParams, "param");
    if (_force)
      node.appendChild(PfNode("force"));
    return node;
  }
};

RequestTaskAction::RequestTaskAction(Scheduler *scheduler, PfNode node)
  : Action(new RequestTaskActionData(scheduler, node.contentAsString(),
                                     ConfigUtils::loadParamSet(node, "param"),
                                     node.hasChild("force"))) {
}

RequestTaskAction::RequestTaskAction(Scheduler *scheduler, QString taskId)
  : Action(new RequestTaskActionData(scheduler, taskId)) {
}

RequestTaskAction::RequestTaskAction(const RequestTaskAction &rhs)
  : Action(rhs) {
}

RequestTaskAction::~RequestTaskAction() {
}
