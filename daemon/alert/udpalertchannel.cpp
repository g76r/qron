/* Copyright 2012-2013 Hallowyn and others.
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
#include "udpalertchannel.h"
#include <QStringList>
#include <QUdpSocket>
#include "log/log.h"
#include <QThread>

UdpAlertChannel::UdpAlertChannel(QObject *parent, QPointer<Alerter> alerter)
  : AlertChannel(parent, alerter), _socket(0) {
  _thread->setObjectName("UdpAlertChannelThread");
}

void UdpAlertChannel::doSendMessage(Alert alert, MessageType type) {
  if (!_socket)
    _socket = new QUdpSocket(this);
  // LATER support IPv6 numeric addresses (they contain colons)
  QStringList tokens(alert.rule().address().split(":"));
  int port;
  if (tokens.size() != 2 || (port = tokens.at(1).toInt()) <= 0
      || port > 65535) {
    Log::warning() << "unssupported UDP address for UDP alert channel: "
                   << alert.rule().address();
    return;
  }
  const QString host = tokens.at(0);
  _socket->connectToHost(host, (quint16)port, QIODevice::WriteOnly);
  if (_socket->waitForConnected(2000)) {
    const QString message =
        type == Cancel ? alert.rule().cancelMessage(alert)
                       : alert.rule().emitMessage(alert);
    qint64 rc = _socket->write(message.toUtf8());
    if (rc < 0)
      Log::warning() << "error when emiting UDP alert: " << _socket->error()
                     << " " << _socket->errorString();
    else
      Log::debug() << "UDP alert emited on " << host << ":" << port << " for "
                   << rc << " bytes: " << message;
  } else {
    Log::warning() << "error when emiting UDP alert: " << _socket->error()
                   << " " << _socket->errorString();
  }
  _socket->disconnectFromHost();
  while(_socket->state() != QAbstractSocket::UnconnectedState
        && _socket->waitForBytesWritten(10000))
    ;
}
