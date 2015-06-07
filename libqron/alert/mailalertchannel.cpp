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
#include "mailalertchannel.h"
#include "log/log.h"
#include <QDateTime>
#include "util/timerwitharguments.h"
#include "mail/mailsender.h"
#include <QThread>
#include "alerter.h"
#include <QSet>

// LATER replace this 60" ugly batch with _remindFrequency and make reminding no longer drift
#define ASYNC_PROCESSING_INTERVAL 60000
// LATER parametrize retry delay, set a maximum data retention, etc.
#define RETRY_INTERVAL 60000

class MailAlertQueue {
public:
  QString _address;
  QList<Alert> _alerts, _cancellations;
  QSet<Alert> _reminders;
  QDateTime _lastMail, _nextProcessing;
  MailAlertQueue(const QString address = QString()) : _address(address) { }
  void scheduleNext(MailAlertChannel *channel, qint64 ms) {
    QDateTime scheduling = QDateTime::currentDateTime().addMSecs(ms);
    _nextProcessing = scheduling;
    TimerWithArguments::singleShot(ms, channel, "processQueue", _address);
  }
  QString toString() const {
    QString s = "{\n  address: "+_address+"\n  alerts: [ ";
    foreach(const Alert &alert, _alerts)
      s.append(alert.id()).append(' ');
    s.append("]\n  cancellations: [ ");
    foreach(const Alert &alert, _cancellations)
      s.append(alert.id()).append(' ');
    s.append("]\n  reminders: [ ");
    foreach(const Alert &alert, _reminders)
      s.append(alert.id()).append(' ');
    s.append("]\n  last mail: ").append(_lastMail.toString())
        .append("\n  next processing: ").append(_nextProcessing.toString());
    s.append("\n}");
    return s;
  }
};

MailAlertChannel::MailAlertChannel(QObject *parent,
                                   QPointer<Alerter> alerter)
  : AlertChannel(parent, alerter), _mailSender(0),
    _asyncProcessingTimer(new QTimer) {
  _thread->setObjectName("MailAlertChannelThread");
  qRegisterMetaType<MailAlertQueue>("MailAlertQueue");
  connect(_asyncProcessingTimer, SIGNAL(timeout()),
          this, SLOT(asyncProcessing()));
  connect(this, SIGNAL(destroyed()),
          _asyncProcessingTimer, SLOT(deleteLater()));
  _asyncProcessingTimer->start(ASYNC_PROCESSING_INTERVAL);
}

void MailAlertChannel::setConfig(AlerterConfig config) {
  _config = config;
  // LATER make server specification more user friendly, e.g. "localhost:25" or "localhost"
  QString relay = _config.params().value("mail.relay", "smtp://127.0.0.1");
  if (_mailSender)
    delete _mailSender;
  _mailSender = new MailSender(relay);
  QString queueDump;
  foreach(const MailAlertQueue *queue, _queues.values()) {
    queueDump.append(queue->toString()).append("\n");
    if (queue->_alerts.size() + queue->_cancellations.size() // FIXME or if there are no longer subsription
        + queue->_reminders.size() == 0) {
      _queues.remove(queue->_address);
      delete queue;
    }
  }
  Log::debug() << "mail queues before config reload:\n" << queueDump;
  // LATER: also delete queues to address that are no longer referenced by any rule since they are likely to be only a spam source
  QMetaObject::invokeMethod(this, "asyncProcessing", Qt::QueuedConnection);
  Log::debug() << "MailAlertChannel configured " << relay << " "
               << config.params().toString();
}

MailAlertChannel::~MailAlertChannel() {
  if (_mailSender)
    delete _mailSender;
  foreach (MailAlertQueue *queue, _queues)
    delete queue;
}

void MailAlertChannel::doNotifyAlert(Alert alert) {
  // LATER support more complex mail addresses with quotes and so on
  QStringList addresses = alert.rule().address(alert).split(',');
  foreach (QString address, addresses) {
    address = address.trimmed();
    MailAlertQueue *queue = _queues.value(address);
    if (!queue) {
      queue = new MailAlertQueue(address); // LATER garbage collect queues
      _queues.insert(address, queue);
    }
    switch (alert.status()) {
    case Alert::Raised:
      if (alert.rule().notifyReminder()) {
        alert.setLastRemindedDate(QDateTime());
        queue->_reminders.insert(alert);
      }
      // fall into next case
    case Alert::Nonexistent:
      if (!alert.rule().notifyEmit())
        return;
      queue->_alerts.append(alert);
      break;
    case Alert::Canceled:
      queue->_reminders.remove(alert);
      if (!alert.rule().notifyCancel())
        return;
      queue->_cancellations.append(alert);
      break;
    case Alert::Rising:
    case Alert::MayRise:
    case Alert::Dropping:
      ; // should never happen
    };
    if (queue->_nextProcessing.isNull()) {
      // wait for a while before sending a mail with only 1 alert, in case some
      // related alerts are coming soon after this one
      queue->scheduleNext(this, _config.delayBeforeFirstSend());
    }
  }
}

void MailAlertChannel::processQueue(QVariant address) {
  const QString addr(address.toString());
  MailAlertQueue *queue = _queues.value(addr);
  if (!queue) { // should never happen
    Log::debug() << "MailAlertChannel::processQueue called for an address with "
                    "no queue: " << address;
    return;
  }
  QDateTime now = QDateTime::currentDateTime();
  queue->_nextProcessing = QDateTime();
  bool haveJob = false;
  if (queue->_alerts.size() || queue->_cancellations.size())
    haveJob = true;
  else {
    foreach (const Alert &alert, queue->_reminders) {
      QDateTime dt = alert.lastRemindedDate();
      if (dt.isValid() && dt.msecsTo(now) > _config.remindPeriod()) {
        haveJob = true;
        break;
      }
    }
  }
  if (haveJob) {
    QList<Alert> reminders;
    foreach (Alert alert, queue->_reminders) {
      if (!queue->_alerts.contains(alert)) // ignore alerts also reported as new ones
        reminders.append(alert);
      alert.setLastRemindedDate(now);
      queue->_reminders.insert(alert); // update all timestamps
    }
    qSort(reminders);
    QString errorString;
    int ms = queue->_lastMail.msecsTo(QDateTime::currentDateTime());
    int minDelayBetweenSend = _config.minDelayBetweenSend();
    //Log::fatal() << "minDelayBetweenSend: " << minDelayBetweenSend << " / "
    //             << ms;
    if (queue->_lastMail.isNull() || ms >= minDelayBetweenSend) {
      //Log::debug() << "MailAlertChannel::processQueue trying to send alerts "
      //                "mail to " << addr << ": " << queue->_alerts.size()
      //             << " new alerts + " << queue->_cancellations.size()
      //             << " cancellations + " << reminders.size()
      //             << " reminders";
      QStringList recipients(addr);
      QString text, subject, boundary("THISISTHEMIMEBOUNDARY"), s;
      QString html;
      QHash<QString,QString> headers;
      // headers
      if (queue->_alerts.size())
        subject = _config.params()
            .value("mail.alertsubject."+queue->_address,
                   _config.params().value("mail.alertsubject",
                                          "NEW QRON ALERT"));
      else if (reminders.size())
        subject = _config.params()
            .value("mail.remindersubject."+queue->_address,
                   _config.params().value("mail.remindersubject",
                                          "qron alert reminder"));
      else
        subject = _config.params()
            .value("mail.cancelsubject."+queue->_address,
                   _config.params().value("mail.cancelsubject",
                                          "canceling qron alert"));
      headers.insert("Subject", subject);
      headers.insert("To", addr);
      headers.insert("User-Agent", "qron free scheduler (www.qron.eu)");
      headers.insert("X-qron-previous-mail", queue->_lastMail.isNull()
                     ? "none" : queue->_lastMail.toString(Qt::ISODate));
      headers.insert("X-qron-alerts-count",
                     QString::number(queue->_alerts.size()));
      headers.insert("X-qron-cancellations-count",
                     QString::number(queue->_cancellations.size()));
      headers.insert("X-qron-reminders-count",
                     QString::number(reminders.size()));
      // body
      html = "<html><head><title>"+subject+"</title></head><body>";
      QString webConsoleUrl = _config.params()
          .value("webconsoleurl."+queue->_address,
                 _config.params().value("webconsoleurl"));
      if (!webConsoleUrl.isEmpty()) {
        text.append("Alerts can also be viewed here:\r\n")
            .append(webConsoleUrl).append("\r\n\r\n");
        html.append("<p>Alerts can also be viewed here: <a href=\"")
            .append(webConsoleUrl).append("\">").append(webConsoleUrl)
            .append("</a>.\n");
      }
      s = "This message contains ";
      s.append(QString::number(queue->_alerts.size()))
          .append(" new emited alerts, ")
          .append(QString::number(queue->_cancellations.size()))
          .append(" alert cancellations and ")
          .append(QString::number(reminders.size()))
          .append(" reminders.");
      text.append(s).append("\r\n\r\n");
      html.append("<p>").append(s).append("\n");
      text.append("NEW EMITED ALERTS:\r\n\r\n");
      html.append("<p>NEW EMITED ALERTS:\n<ul>\n");
      if (queue->_alerts.isEmpty()) {
        text.append("(none)\r\n");
        html.append("<li>(none)\n");
      } else {
        foreach (Alert alert, queue->_alerts) {
          s = alert.riseDate().toString("yyyy-MM-dd hh:mm:ss,zzz");
          s.append(" ").append(alert.rule().emitMessage(alert))
              .append("\r\n");
          text.append(s);
          QString style =
              _config.params()
              .value("mail.alertstyle."+queue->_address,
                     _config.params()
                     .value("mail.alertstyle",
                            "background:#ff0000;color:#ffffff;"));
          html.append("<li style=\"").append(style).append("\">")
              .append(s);
        }
      }
      text.append("\r\nFormer alerts canceled:\r\n\r\n");
      html.append("</ul>\n<p>Former alerts canceled:\n<ul>\n");
      if (queue->_cancellations.isEmpty()) {
        text.append("(none)\r\n");
        html.append("<li>(none)\n");
      } else {
        foreach (Alert alert, queue->_cancellations) {
          s = alert.riseDate().toString("yyyy-MM-dd hh:mm:ss,zzz");
          s.append(" ").append(alert.rule().cancelMessage(alert))
              .append("\r\n");
          text.append(s);
          QString style =
              _config.params()
              .value("mail.cancelstyle."+queue->_address,
                     _config.params().value("mail.cancelstyle",
                                            "background:#8080ff"));
          html.append("<li style=\"").append(style).append("\">")
              .append(s);
        }
      }
      text.append("\r\nAlerts reminders (alerts still raised):\r\n\r\n");
      html.append("</ul>\n<p>Alerts reminders (alerts still raised):\n<ul>\n");
      if (reminders.isEmpty()) {
        text.append("(none)\r\n");
        html.append("<li>(none)\n");
      } else {
        foreach (Alert alert, reminders) {
          s = alert.riseDate().toString("yyyy-MM-dd hh:mm:ss,zzz");
          s.append(" ").append(alert.rule().reminderMessage(alert))
              .append("\r\n");
          text.append(s);
          QString style =
              _config.params()
              .value("mail.reminderstyle."+queue->_address,
                     _config.params().value("mail.reminderstyle",
                                            "background:#ffff80"));
          html.append("<li style=\"").append(style).append("\">")
              .append(s);
        }
      }
      // LATER clarify this message
      text.append(
            "\r\n"
            "Please note that there is a delay between alert cancellation\r\n"
            "request (timestamps above) and the actual time this mail is\r\n"
            "sent (send timestamp of the mail).\r\n");
      html.append(
            "</ul>\n"
            "<p>Please note that there is a delay between alert cancellation "
            "request (timestamps above) and the actual time this mail is "
            "sent (send timestamp of the mail).\n");
      if (_alerter) {
        s = "This is the 'canceldelay' parameter, currently configured to "
            +QString::number(_config.defaultCancelDelay()*.001)
            +" seconds.";
        text.append(s);
        html.append("<p>").append(s);
      }
      html.append("</body></html>\n");
      // mime handling
      QString body;
      if (_config.params().valueAsBool("mail.enablehtml", true)) {
        headers.insert("Content-Type",
                       "multipart/alternative; boundary="+boundary);
        body = "--"+boundary+"\r\nContent-Type: text/plain\r\n\r\n"+text
            +"\r\n\r\n--"+boundary+"\r\nContent-Type: text/html\r\n\r\n"+html
            +"\r\n\r\n--"+boundary+"--\r\n";
      } else
        body = text;
      // queuing
      QString senderAddress =
          _config.params()
          .value("mail.senderaddress."+queue->_address,
                 _config.params().value("mail.senderaddress",
                                        "please-do-not-reply@localhost"));
      bool queued = _mailSender->send(senderAddress, recipients, body,
                                      headers, QList<QVariant>(), errorString);
      if (queued) {
        Log::info() << "successfuly sent an alert mail to " << addr
                    << " with " << queue->_alerts.size()
                    << " new alerts + " << queue->_cancellations.size()
                    << " cancellations + " << reminders.size()
                    << " reminders";      queue->_alerts.clear();
        queue->_cancellations.clear();
        queue->_lastMail = QDateTime::currentDateTime();
      } else {
        Log::warning() << "cannot send mail alert to " << addr
                       << " error in SMTP communication: " << errorString;
        queue->scheduleNext(this, RETRY_INTERVAL);
      }
    } else {
      Log::debug() << "MailAlertChannel::processQueue postponing send";
      queue->scheduleNext(this, std::max(minDelayBetweenSend-ms,1));
    }
  } else {
    Log::debug() << "MailAlertChannel::processQueue called for nothing";
  }
}

void MailAlertChannel::asyncProcessing() {
  QDateTime now = QDateTime::currentDateTime();
  QDateTime tenSecondsAgo = now.addSecs(-10);
  QDateTime remindFrequencyAgo = now.addMSecs(-_config.remindPeriod());
  foreach(const MailAlertQueue *queue, _queues.values()) {
    // check for queues with processing scheduled in the past
    // this should not happen but if time is not monotonic and some timers
    // cannot fire
    if (!queue->_nextProcessing.isNull()
        && queue->_nextProcessing < tenSecondsAgo)
      processQueue(queue->_address);
    // check for queues with pending reminders and no scheduled processing
    else if (!queue->_reminders.isEmpty()
             && queue->_lastMail < remindFrequencyAgo)
      processQueue(queue->_address);
  }
}
