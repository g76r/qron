/* Copyright 2012-2015 Hallowyn and others.
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
#include <QPointer>
#include "alert.h"
#include "config/alerterconfig.h"

class QThread;
class Alerter;

/** Base class for alert channels.
 * @see MailAlertChannel
 * @see LogAlertChannel
 * @see UdpAlertChannel
 * @see Alerter */
class LIBQRONSHARED_EXPORT AlertChannel : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(AlertChannel)

protected:
  QThread *_thread;
  QPointer<Alerter> _alerter;

public:
  // can't have a parent because it lives it its own thread
  AlertChannel(QPointer<Alerter> alerter);
  /** Asynchronously call implementation of doNotifyAlert() within dedicated
   * thread.
   * This method is thread-safe. */
  void notifyAlert(Alert alert);

public slots:
  virtual void setConfig(AlerterConfig config);

protected:
  Q_INVOKABLE virtual void doNotifyAlert(Alert alert) = 0;
};

#endif // ALERTCHANNEL_H
