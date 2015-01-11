/* Copyright 2014-2015 Hallowyn and others.
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
#include "requesturlaction.h"
#include "action_p.h"
#include "log/log.h"
#include "config/configutils.h"
#include "sysutil/parametrizednetworkrequest.h"
#include "sysutil/parametrizedudpsender.h"

class GlobalNetworkActionHub {
public:
  QNetworkAccessManager *_nam;
  GlobalNetworkActionHub() : _nam(new QNetworkAccessManager) { }
};

Q_GLOBAL_STATIC(GlobalNetworkActionHub, globalNetworkActionHub)

class LIBQRONSHARED_EXPORT RequestUrlActionData : public ActionData {
public:
  QString _address, _message;
  ParamSet _params;

  RequestUrlActionData(QString address = QString(), QString message = QString(),
                ParamSet params = ParamSet())
    : _address(address), _message(message), _params(params) {
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance taskContext) const {
    Q_UNUSED(subscription)
    // LATER support binary payloads
    ParamsProviderList evaluationContext(&eventContext);
    TaskInstancePseudoParamsProvider ppp = taskContext.pseudoParams();
    evaluationContext.append(&ppp);
    if (_address.startsWith("udp:", Qt::CaseInsensitive)) {
      // LATER run UDP in a separate thread to avoid network/dns/etc. hangups
      ParametrizedUdpSender sender(_address, _params, &evaluationContext,
                                   taskContext.task().id(), taskContext.idAsLong());
      sender.performRequest(_message, &evaluationContext);
    } else {
      ParametrizedNetworkRequest request(
            _address, _params, &evaluationContext, taskContext.task().id(),
            taskContext.idAsLong());
      QNetworkReply *reply = request.performRequest(
            globalNetworkActionHub->_nam, _message, &evaluationContext);
      if (reply) {
        QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                         reply, SLOT(deleteLater()));
        QObject::connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
      }
    }
  }
  QString toString() const {
    return "requesturl{ "+_address+" }";
  }
  QString actionType() const {
    return "requesturl";
  }
  PfNode toPfNode() const{
    PfNode node(actionType(), _message);
    node.appendChild(PfNode("address", _address));
    ConfigUtils::writeParamSet(&node, _params, "param");
    return node;
  }
};

RequestUrlAction::RequestUrlAction(Scheduler *scheduler, PfNode node)
  : Action(new RequestUrlActionData(node.attribute("address"),
                                    node.contentAsString(),
                                    ConfigUtils::loadParamSet(node, "param"))) {
  Q_UNUSED(scheduler)
}

RequestUrlAction::RequestUrlAction(const RequestUrlAction &rhs) : Action(rhs) {
}
