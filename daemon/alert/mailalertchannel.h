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
#ifndef MAILALERTCHANNEL_H
#define MAILALERTCHANNEL_H

#include "alertchannel.h"
#include <QList>
#include <QHash>
#include "util/paramset.h"

class MailAlertQueue;
class MailSender;
class QTimer;

/** Log channel that send alerts as mails.
 * It performs alerts (including cancellations and reminders) aggregation within
 * at most one mail every %mindelaybetweensend seconds (default: 600" = 10'). */
class MailAlertChannel : public AlertChannel {
  Q_OBJECT
  Q_DISABLE_COPY(MailAlertChannel)
  QHash<QString,MailAlertQueue*> _queues;
  MailSender *_mailSender;
  QString _senderAddress, _webConsoleUrl, _alertSubject, _reminderSubject,
  _cancelSubject, _alertStyle, _reminderStyle, _cancelStyle;
  bool _enableHtmlBody;
  int _remindFrequency;
  QTimer *_asyncProcessingTimer;

public:
  explicit MailAlertChannel(QObject *parent, QPointer<Alerter> alerter);
  ~MailAlertChannel();
  void doSendMessage(Alert alert, MessageType type);

public slots:
  void setParams(ParamSet params);

private slots:
  void asyncProcessing();

private:
  Q_INVOKABLE void processQueue(QVariant address);
};

#endif // MAILALERTCHANNEL_H
