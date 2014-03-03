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
#include "alerter.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>
#include "log/log.h"
#include "pf/pfnode.h"
#include "logalertchannel.h"
#include "udpalertchannel.h"
#include "mailalertchannel.h"
#include <QTimer>
#include <QCoreApplication>
#include "config/configutils.h"
#include "alert.h"

// LATER replace this 10" ugly batch with predictive timer (min(timestamps))
#define ASYNC_PROCESSING_INTERVAL 10000

Alerter::Alerter() : QObject(0), _thread(new QThread) {
  _thread->setObjectName("AlerterThread");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  _channels.insert("log", new LogAlertChannel(0, this));
  _channels.insert("udp", new UdpAlertChannel(0, this));
  MailAlertChannel *mailChannel = new MailAlertChannel(0, this);
  _channels.insert("mail", mailChannel);
  foreach (AlertChannel *channel, _channels)
    connect(this, SIGNAL(configChanged(AlerterConfig)),
            channel, SLOT(configChanged(AlerterConfig)));
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(asyncProcessing()));
  timer->start(ASYNC_PROCESSING_INTERVAL);
  moveToThread(_thread);
  qRegisterMetaType<QList<AlertRule> >("QList<AlertRule>");
  qRegisterMetaType<AlertRule>("AlertRule");
  qRegisterMetaType<Alert>("Alert");
  qRegisterMetaType<ParamSet>("ParamSet");
  qRegisterMetaType<QDateTime>("QDateTime");
  qRegisterMetaType<QStringList>("QStringList");
  qRegisterMetaType<AlertChannel::MessageType>("AlertChannel::MessageType");
}

Alerter::~Alerter() {
  foreach (AlertChannel *channel, _channels.values())
    channel->deleteLater(); // cant be a child cause it lives it its own thread
}

void Alerter::loadConfig(PfNode root) {
  AlerterConfig config(root);
  _config = config;
  emit paramsChanged(_config.params());
  emit configChanged(_config);
}

void Alerter::emitAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doEmitAlert", Q_ARG(QString, alert),
                            Q_ARG(AlertChannel::MessageType,
                                  AlertChannel::Emit));
}

void Alerter::doEmitAlert(QString alert, AlertChannel::MessageType type,
                          QDateTime date) {
  Log::debug() << "emiting alert " << alert << " " << type;
  int n = 0;
  foreach (AlertRule rule, _config.rules()) {
    if (rule.patternRegExp().exactMatch(alert)) {
      QString channelName = rule.channelName();
      //Log::debug() << "alert matching rule #" << n;
      if (channelName == "stop")
        break;
      QPointer<AlertChannel> channel = _channels.value(channelName);
      if (channel) {
        channel.data()->sendMessage(Alert(alert, rule, date), type);
      }
    }
    ++n;
  }
  if (type == AlertChannel::Cancel) {
    _soonCanceledAlerts.remove(alert);
    emit alertCanceled(alert);
  } else {
    emit alertEmited(alert);
  }
}

void Alerter::raiseAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doRaiseAlert", Q_ARG(QString, alert));
}

void Alerter::doRaiseAlert(QString alert) {
  if (!_raisedAlerts.contains(alert)) {
    if (_soonCanceledAlerts.contains(alert)) {
      _raisedAlerts.insert(alert, _soonCanceledAlerts.value(alert));
      _soonCanceledAlerts.remove(alert);
      emit alertCancellationUnscheduled(alert);
      Log::debug() << "alert is no longer scheduled for cancellation " << alert
                   << " (it was raised again within cancel delay)";
    } else {
      QDateTime now(QDateTime::currentDateTime());
      _raisedAlerts.insert(alert, now);
      Log::debug() << "raising alert " << alert;
      emit alertRaised(alert);
      doEmitAlert(alert, AlertChannel::Raise);
    }
  }
}

void Alerter::cancelAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doCancelAlert", Q_ARG(QString, alert),
                            Q_ARG(bool, false));
}

void Alerter::cancelAlertImmediately(QString alert) {
  QMetaObject::invokeMethod(this, "doCancelAlert", Q_ARG(QString, alert),
                            Q_ARG(bool, true));
}

void Alerter::doCancelAlert(QString alert, bool immediately) {
  if (immediately) {
    if (_raisedAlerts.contains(alert) || _soonCanceledAlerts.contains(alert)) {
      Log::debug() << "do cancel alert immediately: " << alert;
      doEmitAlert(alert, AlertChannel::Cancel);
      _raisedAlerts.remove(alert);
    }
  } else {
    if (_raisedAlerts.contains(alert) && !_soonCanceledAlerts.contains(alert)) {
      _raisedAlerts.remove(alert);
      QDateTime dt(QDateTime::currentDateTime()
                   .addMSecs(_config.cancelDelay()));
      _soonCanceledAlerts.insert(alert, dt);
      Log::debug() << "will cancel alert " << alert << " in "
                   << _config.cancelDelay()*.001 << " s";
      emit alertCancellationScheduled(alert, dt);
      //} else {
      //  Log::debug() << "would have canceled alert " << alert
      //               << " if it was raised";
    }
  }
}

void Alerter::asyncProcessing() {
  QDateTime now(QDateTime::currentDateTime());
  // process alerts cancellations after cancel delay
  foreach (QString alert, _soonCanceledAlerts.keys()) {
    QDateTime scheduledDate = _soonCanceledAlerts.value(alert);
    if (scheduledDate <= now)
      doEmitAlert(alert, AlertChannel::Cancel,
                  scheduledDate.addMSecs(-_config.cancelDelay()));
  }
}
