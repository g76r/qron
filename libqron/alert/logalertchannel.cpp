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
#include "logalertchannel.h"
#include "log/log.h"
#include <QThread>

LogAlertChannel::LogAlertChannel(QObject *parent, QPointer<Alerter> alerter)
  : AlertChannel(parent, alerter) {
  _thread->setObjectName("LogAlertChannelThread");
}

void LogAlertChannel::doSendMessage(Alert alert, MessageType type) {
  switch(type) {
  case Emit:
  case Raise:
    if (!alert.rule().notifyEmit())
      return;
    Log::log(alert.rule().emitMessage(alert),
             Log::severityFromString(alert.rule().address(alert)));
    break;
  case Cancel:
    if (!alert.rule().notifyCancel())
      return;
    Log::log(alert.rule().cancelMessage(alert),
             Log::severityFromString(alert.rule().address(alert)));
  }

}
