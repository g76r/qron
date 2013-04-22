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
#include "mailalertchannel.h"
#include "log/log.h"
#include <QDateTime>
#include "util/timerwitharguments.h"
#include "mail/mailsender.h"
#include <QThread>
#include "alerter.h"

class MailAlertQueue {
public:
  QString _address;
  QList<Alert> _alerts, _cancellations, _reminders;
  QDateTime _lastMail;
  bool _processingScheduled;
  MailAlertQueue(const QString address = QString()) : _address(address),
    _processingScheduled(false) { }
};

MailAlertChannel::MailAlertChannel(QObject *parent,
                                   QWeakPointer<Alerter> alerter)
  : AlertChannel(parent, alerter), _mailSender(0) {
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
  _webConsoleUrl = params.value("webconsoleurl");
  Log::debug() << "MailAlertChannel configured " << relay << " "
               << _senderAddress << " " << params.toString();
}

MailAlertChannel::~MailAlertChannel() {
  if (_mailSender)
    delete _mailSender;
  qDeleteAll(_queues);
}

void MailAlertChannel::doSendMessage(Alert alert, MessageType type) {
  // LATER support more complex mail addresses with quotes and so on
  QStringList addresses = alert.rule().address().split(',');
  foreach (QString address, addresses) {
    address = address.trimmed();
    MailAlertQueue *queue = _queues.value(address);
    if (!queue) {
      queue = new MailAlertQueue(address);
      _queues.insert(address, queue);
    }
    switch (type) {
    case Emit:
      queue->_alerts.append(alert);
      break;
    case Cancel:
      queue->_cancellations.append(alert);
      break;
    case Remind:
      queue->_reminders.append(alert);
    };
    if (!queue->_processingScheduled) {
      // wait for a while before sending a mail with only 1 alert, in case some
      // related alerts are coming soon after this one
      int period = _alerter ? _alerter.data()->gracePeriodBeforeFirstSend()
                            : ALERTER_DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND;
      TimerWithArguments::singleShot(period, this, "processQueue", address);
      queue->_processingScheduled = true;
    }
  }
}

void MailAlertChannel::processQueue(const QVariant address) {
  const QString addr(address.toString());
  MailAlertQueue *queue = _queues.value(addr);
  if (!queue) { // should never happen
    Log::debug() << "MailAlertChannel::processQueue called for an address with "
                    "no queue: " << address;
    return;
  }
  queue->_processingScheduled = false;
  if (queue->_alerts.size() || queue->_cancellations.size()
      || queue->_reminders.size()) {
    QString errorString;
    int ms = queue->_lastMail.msecsTo(QDateTime::currentDateTime());
    int minDelayBetweenSend = _alerter
        ? _alerter.data()->minDelayBetweenSend()
        : ALERTER_DEFAULT_MIN_DELAY_BETWEEN_SEND;
    //Log::fatal() << "minDelayBetweenSend: " << minDelayBetweenSend << " / "
    //             << ms;
    if (queue->_lastMail.isNull() || ms >= minDelayBetweenSend) {
      Log::debug() << "MailAlertChannel::processQueue trying to send alerts "
                      "mail to " << addr << ": " << queue->_alerts.size()
                   << " new alerts + " << queue->_cancellations.size()
                   << " cancellations + " << queue->_reminders.size()
                   << " reminders";
      QStringList recipients(addr);
      QString body;
      QHash<QString,QString> headers;
      // LATER parametrize mail subject
      headers.insert("Subject", "qron alerts");
      headers.insert("To", addr);
      headers.insert("User-Agent", "qron free scheduler (www.qron.eu)");
      headers.insert("X-qron-previous-mail", queue->_lastMail.isNull()
                     ? "none" : queue->_lastMail.toString(Qt::ISODate));
      headers.insert("X-qron-alerts-count",
                     QString::number(queue->_alerts.size()));
      headers.insert("X-qron-cancellations-count",
                     QString::number(queue->_cancellations.size()));
      headers.insert("X-qron-reminders-count",
                     QString::number(queue->_reminders.size()));
      if (!_webConsoleUrl.isEmpty())
        body.append("Alerts can also be viewed here:\r\n")
            .append(_webConsoleUrl).append("\r\n\r\n");
      // LATER HTML alert mails
      body.append("This message contains ")
          .append(QString::number(queue->_alerts.size()))
          .append(" new emited alerts, ")
          .append(QString::number(queue->_cancellations.size()))
          .append(" alert cancellations and ")
          .append(QString::number(queue->_reminders.size()))
          .append(" reminders.\r\n\r\n");
      body.append("NEW EMITED ALERTS:\r\n\r\n");
      if (queue->_alerts.isEmpty())
        body.append("(none)\r\n");
      else
        foreach (Alert alert, queue->_alerts)
          body.append(alert.datetime().toString("yyyy-MM-dd hh:mm:ss,zzz"))
              .append(" ").append(alert.rule().emitMessage(alert))
              .append("\r\n");
      body.append("\r\nAlerts reminders (alerts still raised):\r\n\r\n");
      if (queue->_reminders.isEmpty())
        body.append("(none)\r\n");
      else
        foreach (Alert alert, queue->_reminders)
          body.append(alert.datetime().toString("yyyy-MM-dd hh:mm:ss,zzz"))
              .append(" ").append(alert.rule().reminderMessage(alert))
              .append("\r\n");
      body.append("\r\nFormer alerts canceled:\r\n\r\n");
      if (queue->_cancellations.isEmpty())
        body.append("(none)\r\n");
      else {
        foreach (Alert alert, queue->_cancellations)
          body.append(alert.datetime().toString("yyyy-MM-dd hh:mm:ss,zzz"))
              .append(" ").append(alert.rule().cancelMessage(alert))
              .append("\r\n");
        body.append(
              "\r\n"
              "Please note that there is a delay between alert cancellation\r\n"
              "request (timestamps above) and the actual time this mail is\r\n"
              "sent (send timestamp of the mail).\r\n");
        if (_alerter)
          body.append(
                "This is the 'canceldelay' parameter, currently configured to"
                "\r\n"+QString::number(_alerter.data()->cancelDelay()*.001)
                +" seconds.");
      }
      bool queued = _mailSender->send(_senderAddress, recipients, body,
                                      headers, QList<QVariant>(), errorString);
      if (queued) {
        Log::info() << "successfuly sent an alert mail to " << addr;
        queue->_alerts.clear();
        queue->_cancellations.clear();
        queue->_reminders.clear();
        queue->_lastMail = QDateTime::currentDateTime();
      } else {
        Log::warning() << "cannot send mail alert to " << addr
                       << " error in SMTP communication: " << errorString;
        // LATER parametrize retry delay, set a maximum data retention, etc.
        TimerWithArguments::singleShot(60000, this, "processQueue", addr);
        queue->_processingScheduled = true;
      }
    } else {
      Log::debug() << "MailAlertChannel::processQueue postponing send";
      TimerWithArguments::singleShot(std::max(minDelayBetweenSend-ms,1),
                                     this, "processQueue", addr);
      queue->_processingScheduled = true;
    }
  } else {
    Log::debug() << "MailAlertChannel::processQueue called for nothing";
  }
}
