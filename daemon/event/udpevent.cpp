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
#include "udpevent.h"
#include "event_p.h"
#include <QUdpSocket>
#include <QStringList>
#include "log/log.h"

class UdpEventData : public EventData {
public:
  QString _message;
  QString _host;
  quint16 _port;
  UdpEventData(const QString address = QString(),
               const QString message = QString())
    : _message(message), _port(0) {
    // LATER support IPv6 numeric addresses (they contain colons)
    QStringList tokens(address.split(":"));
    int port;
    if (tokens.size() != 2 || (port = tokens.at(1).toInt()) <= 0
        || port > 65535) {
      Log::warning() << "unssupported UDP address for UDP event: "
                     << address;
    } else {
      _host = tokens.at(0);
      _port = (quint16)port;
    }
  }
  void trigger(const ParamsProvider *context) const {
    // TODO run in a separate thread to avoid network/dns/etc. hangups
    if (!_port) // address is invalid
      return;
    QUdpSocket *socket = new QUdpSocket;
    socket->connectToHost(_host, _port, QIODevice::WriteOnly);
    if (socket->waitForConnected(200/*2000*/)) {
      const QString message = ParamSet().evaluate(_message, context);
      qint64 rc = socket->write(message.toUtf8());
      if (rc < 0)
        Log::warning() << "error when emiting UDP event: " << socket->error()
                       << " " << socket->errorString();
      //else
      //  Log::debug() << "UDP event emited on " << _host << ":" << _port
      //               << " for " << rc << " bytes: " << message;
    } else {
      Log::warning() << "error when emiting UDP event: " << socket->error()
                     << " " << socket->errorString() << _host << _port;
    }
    socket->disconnectFromHost();
    while(socket->state() != QAbstractSocket::UnconnectedState
          && socket->waitForBytesWritten(200/*10000*/))
      ;
    socket->deleteLater();
  }
  QString toString() const {
    return "udp{"+_host+":"+QString::number(_port)+" "+_message+"}";
  }
};

UdpEvent::UdpEvent(const QString address, const QString message)
  : Event(new UdpEventData(address, message)) {
}

UdpEvent::UdpEvent(const UdpEvent &rhs) : Event(rhs) {
}

UdpEvent::~UdpEvent() {
}
