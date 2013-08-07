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
#ifndef ALERTCHANNEL_H
#define ALERTCHANNEL_H

#include <QObject>
#include "config/alert.h"

class QThread;
class Alerter;

/** Base class for alert channels
 * @see MailAlertChannel
 * @see LogAlertChannel */
class AlertChannel : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(AlertChannel)
protected:
  QThread *_thread;
  QWeakPointer<Alerter> _alerter;

public:
  enum MessageType { Emit, Raise, Cancel };
  explicit AlertChannel(QObject *parent = 0, QWeakPointer<Alerter> alerter
                        = QWeakPointer<Alerter>());
  /** Asynchronously call implementation of doSendMessage() within dedicated
   * thread.
   * This method is thread-safe. */
  void sendMessage(Alert alert, MessageType type);

protected:
  Q_INVOKABLE virtual void doSendMessage(Alert alert,
                                         AlertChannel::MessageType type) = 0;
};

#endif // ALERTCHANNEL_H
