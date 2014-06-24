/* Copyright 2012-2014 Hallowyn and others.
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
#include "alertchannel.h"
#include <QThread>
#include <QMetaObject>
#include <QMetaType>
#include "alerter.h"

AlertChannel::AlertChannel(QObject *parent, QPointer<Alerter> alerter)
  : QObject(parent), _thread(new QThread), _alerter(alerter) {
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  qRegisterMetaType<Alert>("Alert");
  qRegisterMetaType<AlertChannel::MessageType>("AlertChannel::MessageType");
}

void AlertChannel::sendMessage(Alert alert, AlertChannel::MessageType type) {
  QMetaObject::invokeMethod(this, "doSendMessage", Q_ARG(Alert, alert),
                            Q_ARG(AlertChannel::MessageType, type));
}

void AlertChannel::configChanged(AlerterConfig config) {
  Q_UNUSED(config)
}