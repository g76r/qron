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
#include "udpaction.h"
#include "action_p.h"
#include <QUdpSocket>
#include <QStringList>
#include "log/log.h"

class UdpActionData : public ActionData {
public:
  QString _message;
  QString _host;
  quint16 _port;
  UdpActionData(QString address = QString(), QString message = QString())
    : _message(message), _port(0) {
    // LATER support IPv6 numeric addresses (they contain colons)
    QStringList tokens(address.split(":"));
    int port;
    if (tokens.size() != 2 || (port = tokens.at(1).toInt()) <= 0
        || port > 65535) {
      Log::warning() << "unssupported UDP address for UDP action: "
                     << address;
    } else {
      _host = tokens.at(0);
      _port = (quint16)port;
    }
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance taskContext) const {
    Q_UNUSED(subscription)
    // LATER run in a separate thread to avoid network/dns/etc. hangups
    if (!_port) // address is invalid
      return;
    QUdpSocket socket;
    socket.connectToHost(_host, _port, QIODevice::WriteOnly);
    if (socket.waitForConnected(200/*2000*/)) {
      QString message = eventContext.evaluate(_message, &taskContext);
      qint64 rc = socket.write(message.toUtf8());
      if (rc < 0)
        Log::warning() << "error when emiting UDP action: " << socket.error()
                       << " " << socket.errorString();
      //else
      //  Log::debug() << "UDP action emited on " << _host << ":" << _port
      //               << " for " << rc << " bytes: " << message;
    } else {
      Log::warning() << "error when emiting UDP action: " << socket.error()
                     << " " << socket.errorString() << _host << _port;
    }
    socket.disconnectFromHost();
    while(socket.state() != QAbstractSocket::UnconnectedState
          && socket.waitForBytesWritten(200/*10000*/))
      ;
  }
  QString toString() const {
    return "udp{"+_host+":"+QString::number(_port)+" "+_message+"}";
  }
  QString actionType() const {
    return "udp";
  }
  PfNode toPfNode() const{
    PfNode node(actionType());
    node.appendChild(PfNode("message", _message));
    node.appendChild(PfNode("address", _host+":"+QString::number(_port)));
    return node;
  }
};

UdpAction::UdpAction(Scheduler *scheduler, PfNode node)
  : Action(new UdpActionData(node.attribute("address"),
                             node.attribute("message"))) {
  Q_UNUSED(scheduler)
}

UdpAction::UdpAction(const UdpAction &rhs) : Action(rhs) {
}

UdpAction::~UdpAction() {
}
