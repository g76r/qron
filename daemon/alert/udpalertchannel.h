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
#ifndef UDPALERTCHANNEL_H
#define UDPALERTCHANNEL_H

#include "alertchannel.h"

class QUdpSocket;

/** Log channel that send alerts as arbitrary UDP packets.
 */
class UdpAlertChannel : public AlertChannel {
  Q_OBJECT
  QUdpSocket *_socket;

public:
  explicit UdpAlertChannel(QObject *parent = 0);
  void doSendMessage(Alert alert, MessageType type);
};

#endif // UDPALERTCHANNEL_H
