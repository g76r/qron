/* Copyright 2012-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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

LogAlertChannel::LogAlertChannel(Alerter *alerter)
  : AlertChannel(alerter) {
  _thread->setObjectName("LogAlertChannelThread");
}

void LogAlertChannel::doNotifyAlert(Alert alert) {
  switch(alert.status()) {
  case Alert::Nonexistent:
  case Alert::Raised:
    if (!alert.subscription().notifyEmit())
      return;
    Log::log(alert.subscription().emitMessage(alert),
             Log::severityFromString(alert.subscription().address(alert)));
    break;
  case Alert::Canceled:
    if (!alert.subscription().notifyCancel())
      return;
    Log::log(alert.subscription().cancelMessage(alert),
             Log::severityFromString(alert.subscription().address(alert)));
    break;
  case Alert::Rising:
  case Alert::MayRise:
  case Alert::Dropping:
    ; // should never happen
  }
}
