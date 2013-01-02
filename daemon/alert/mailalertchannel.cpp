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
#include "mailalertchannel.h"
#include "log/log.h"
#include <QDateTime>
#include "util/timerwithargument.h"
#include "mail/mailsender.h"
#include <QThread>

class MailAlertQueue {
public:
  QString _address;
  QList<Alert> _alerts, _cancellations;
  QDateTime _lastMail;
  bool _processingScheduled;
  MailAlertQueue(const QString address = QString()) : _address(address),
    _processingScheduled(false) { }
};

MailAlertChannel::MailAlertChannel(QObject *parent)
  : AlertChannel(parent), _mailSender(0) {
  _thread->setObjectName("MailAlertChannelThread");
  qRegisterMetaType<MailAlertQueue>("MailAlertQueue");
}

void MailAlertChannel::setParams(ParamSet params) {
  // LATER make server specification more user friendly, e.g. "localhost:25" or "localhost"
  QString relay = params.value("mail.relay", "smtp://127.0.0.1");
  if (_mailSender)
    delete _mailSender;
  _mailSender = new MailSender(relay);
  _senderAddress = params.value("mail.senderaddress",
                                "please-do-not-reply@localhost");
  _minDelayBetweenMails = params.value("mail.mindelaybetweenmails").toInt();
  if (_minDelayBetweenMails < 60) // hard coded 1 minute minimum
    _minDelayBetweenMails = 600;
  Log::debug() << "MailAlertChannel configured " << relay << " "
               << _senderAddress << " " << _minDelayBetweenMails
               << " " << params.toString();
}

MailAlertChannel::~MailAlertChannel() {
  if (_mailSender)
    delete _mailSender;
  qDeleteAll(_queues);
}

void MailAlertChannel::sendMessage(Alert alert, bool cancellation) {
  const QString address = alert.rule().address();
  MailAlertQueue *queue = _queues.value(address);
  if (!queue) {
    queue = new MailAlertQueue(address);
    _queues.insert(address, queue);
  }
  if (cancellation)
    queue->_cancellations.append(alert);
  else
    queue->_alerts.append(alert);
  if (!queue->_processingScheduled) {
    // wait for a while before sending a mail with only 1 alert, in case some
    // related alerts are coming soon after this one
    // LATER parametrized the hard-coded 10" before first mail
    TimerWithArgument::singleShot(10000, this, "processQueue", address);
    queue->_processingScheduled = true;
  }
}

// LATER also send reminders of long raised alerts every e.g. 1 hour

void MailAlertChannel::processQueue(const QVariant address) {
  const QString addr(address.toString());
  MailAlertQueue *queue = _queues.value(addr);
  if (queue && (queue->_alerts.size() || queue->_cancellations.size())) {
    QString errorString;
    int s = queue->_lastMail.secsTo(QDateTime::currentDateTime());
    if (queue->_lastMail.isNull() || s >= _minDelayBetweenMails) {
      Log::debug() << "MailAlertChannel::processQueue trying to send alerts "
                      "mail to " << addr << ": " << queue->_alerts.size()
                   << " + " << queue->_cancellations.size();
      QStringList recipients(addr);
      QString body;
      QMap<QString,QString> headers;
      // LATER parametrize mail subject
      headers.insert("Subject", "qron alerts");
      // LATER HTML alert mails
      body.append("This message contains ")
          .append(QString::number(queue->_alerts.size()))
          .append(" new raised alerts and ")
          .append(QString::number(queue->_cancellations.size()))
          .append(" alert cancellations.\r\n\r\n");
      body.append("NEW RAISED ALERTS:\r\n\r\n");
      if (queue->_alerts.isEmpty())
        body.append("(none)\r\n");
      else
        foreach (Alert alert, queue->_alerts)
          body.append(alert.datetime().toString(Qt::ISODate)).append(" ")
              .append(alert.rule().message(alert)).append("\r\n");
      body.append("\r\nFormer alerts cancelled:\r\n\r\n");
      if (queue->_cancellations.isEmpty())
        body.append("(none)\r\n");
      else
        foreach (Alert alert, queue->_cancellations)
          body.append(alert.datetime().toString(Qt::ISODate)).append(" ")
              .append(alert.rule().cancelMessage(alert)).append("\r\n");
      bool queued = _mailSender->send(_senderAddress, recipients, body,
                                         headers, QList<QVariant>(),
                                         errorString);
      if (queued) {
        Log::info() << "successfuly sent an alert mail to " << addr;
        queue->_alerts.clear();
        queue->_cancellations.clear();
        queue->_lastMail = QDateTime::currentDateTime();
        queue->_processingScheduled = false;
      } else {
        Log::warning() << "cannot send mail alert to " << addr
                       << " error in SMTP communication: " << errorString;
        // LATER parametrize retry delay, set a maximum data retention, etc.
        TimerWithArgument::singleShot(60000, this, "processQueue", addr);
      }
    } else {
      if (!queue->_processingScheduled) { // this avoids stacking calls
        Log::debug() << "MailAlertChannel::processQueue postponing send";
        TimerWithArgument::singleShot((_minDelayBetweenMails-s)*1000, this,
                                      "processQueue", addr);
        queue->_processingScheduled = true;
      } else
        Log::debug() << "MailAlertChannel::processQueue not even postponing";
    }
  } else {
    Log::debug() << "MailAlertChannel::processQueue called for nothing";
  }
}
