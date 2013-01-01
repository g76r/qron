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
#ifndef MAILALERTCHANNEL_H
#define MAILALERTCHANNEL_H

#include "alertchannel.h"
#include <QList>
#include <QHash>
#include "data/paramset.h"

class MailAlertQueue;
class MailSender;

class MailAlertChannel : public AlertChannel {
  Q_OBJECT
  QHash<QString,MailAlertQueue*> _queues;
  MailSender *_mailSender;
  QString _senderAddress;
  int _minDelayBetweenMails;

public:
  explicit MailAlertChannel(QObject *parent = 0);
  ~MailAlertChannel();
  void sendMessage(Alert alert, bool cancellation);

public slots:
  void setParams(ParamSet params);

private:
  Q_INVOKABLE void processQueue(const QVariant address);
};

#endif // MAILALERTCHANNEL_H
