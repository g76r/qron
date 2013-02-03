/* Copyright 2012 Hallowyn and others.
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

class AlertChannel : public QObject {
  Q_OBJECT
protected:
  QThread *_thread;

public:
  explicit AlertChannel(QObject *parent = 0);
  Q_INVOKABLE virtual void sendMessage(Alert alert, bool cancellation) = 0;
};

#endif // ALERTCHANNEL_H
