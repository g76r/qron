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

#define DEFAULT_CANCEL_DELAY 900
// 900" = 15'

Alerter::Alerter() : QObject(0), _thread(new QThread),
  _cancelDelay(DEFAULT_CANCEL_DELAY) {
  _thread->setObjectName("AlerterThread");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  _channels.insert("log", new LogAlertChannel);
  _channels.insert("udp", new UdpAlertChannel);
  MailAlertChannel *mailChannel = new MailAlertChannel;
  _channels.insert("mail", mailChannel);
  connect(this, SIGNAL(paramsChanged(ParamSet)),
          mailChannel, SLOT(setParams(ParamSet)));
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(processCancellation()));
  // LATER replace this 10" ugly batch with predictive timer (min(timestamps))
  timer->start(10000);
  moveToThread(_thread);
  qRegisterMetaType<QList<AlertRule> >("QList<AlertRule>");
  qRegisterMetaType<AlertRule>("AlertRule");
  qRegisterMetaType<Alert>("Alert");
  qRegisterMetaType<ParamSet>("ParamSet");
  qRegisterMetaType<QDateTime>("QDateTime");
}

Alerter::~Alerter() {
  foreach (AlertChannel *channel, _channels.values())
    channel->deleteLater();
}

bool Alerter::loadConfiguration(PfNode root, QString &errorString) {
  Q_UNUSED(errorString) // currently no fatal error, only warnings
  QList<PfNode> children;
  children += root.childrenByName("param");
  children += root.childrenByName("rule");
  foreach (PfNode node, children) {
    if (node.name() == "param") {
      QString key = node.attribute("key");
      QString value = node.attribute("value");
      if (key.isNull() || value.isNull()) {
        Log::warning() << "invalid alerts param " << node.toPf();
      } else {
        Log::debug() << "configured alerts param " << key << "=" << value;
        _params.setValue(key, value);
      }
    } else if (node.name() == "rule") {
      // LATER check uniqueness of attributes
      QString pattern = node.attribute("match", "**");
      bool stop = !node.attribute("stop").isNull();
      bool notifyCancel = node.attribute("nocancelnotify").isNull();
      //Log::debug() << "found alert rule section " << pattern << " " << stop;
      int channelsCount = 0;
      foreach (PfNode node, node.children()) {
        if (node.name() == "match" || node.name() == "stop") {
          // ignore
        } else {
          QString name = node.name();
          AlertChannel *channel = _channels.value(name);
          if (channel) {
            if (stop && channelsCount++) {
              Log::error() << "do not support several channel for the same "
                              "alert rule if (stop) is set, ignoring channel '"
                           << QString::fromUtf8(node.toPf())
                           << "' with matching pattern " << pattern;
            } else {
              AlertRule rule(node, pattern, channel, name, stop, notifyCancel);
              _rules.append(rule);
              Log::debug() << "configured alert rule " << name << " " << pattern
                           << " " << stop << " "
                           << rule.patternRegExp().pattern();
            }
          } else {
            Log::warning() << "alert channel '" << name << "' unknown in alert "
                              "rule with matching pattern " << pattern;
          }
        }
      }
    }
  }
  QString cancelDelayString = _params.value("canceldelay");
  if (!cancelDelayString.isNull())
    _cancelDelay = cancelDelayString.toInt();
  emit paramsChanged(_params);
  emit rulesChanged(_rules);
  return true;
}

void Alerter::emitAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doEmitAlert", Q_ARG(QString, alert));
}

void Alerter::doEmitAlert(QString alert) {
  Log::debug() << "emiting alert " << alert;
  int n = 0;
  foreach (AlertRule rule, _rules) {
    if (rule.patternRegExp().exactMatch(alert)) {
      //Log::debug() << "alert matching rule #" << n;
      sendMessage(Alert(alert, rule), false);
      if (rule.stop())
        break;
    }
    ++n;
  }
  emit alertEmited(alert);
}

void Alerter::doEmitAlertCancellation(QString alert) {
  Log::debug() << "emiting alert cancellation " << alert;
  int n = 0;
  foreach (AlertRule rule, _rules) {
    if (rule.patternRegExp().exactMatch(alert)) {
      //Log::debug() << "alert matching rule #" << n;
      if (rule.notifyCancel())
        sendMessage(Alert(alert, rule), true);
      if (rule.stop())
        break;
    }
    ++n;
  }
  emit alertCancellationEmited(alert);
}

void Alerter::sendMessage(Alert alert, bool cancellation) {
  QWeakPointer<AlertChannel> channel = alert.rule().channel();
  if (channel)
    QMetaObject::invokeMethod(channel.data(), "sendMessage",
                              Q_ARG(Alert, alert),
                              Q_ARG(bool, cancellation));
}

void Alerter::raiseAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doRaiseAlert", Q_ARG(QString, alert));
}

void Alerter::doRaiseAlert(QString alert) {
  if (!_raisedAlerts.contains(alert)) {
    _raisedAlerts.insert(alert, QDateTime::currentDateTime());
    if (_soonCanceledAlerts.remove(alert)) {
      Log::debug() << "alert is no longer scheduled for cancellation " << alert
                   << " (it was raised again within cancel delay)";
    } else {
      Log::debug() << "raising alert " << alert;
      emit alertRaised(alert);
      doEmitAlert(alert);
    }
  }
}

void Alerter::cancelAlert(QString alert) {
  QMetaObject::invokeMethod(this, "doCancelAlert", Q_ARG(QString, alert));
}

void Alerter::doCancelAlert(QString alert) {
  if (_raisedAlerts.contains(alert) && !_soonCanceledAlerts.contains(alert)) {
    _raisedAlerts.remove(alert);
    QDateTime dt(QDateTime::currentDateTime().addSecs(_cancelDelay));
    _soonCanceledAlerts.insert(alert, dt);
    Log::debug() << "will cancel alert " << alert << " in " << _cancelDelay
                 << " s";
    emit alertCancellationScheduled(alert, dt);
  //} else {
  //  Log::debug() << "would have canceled alert " << alert
  //               << " if it was raised";
  }
}

void Alerter::processCancellation() {
  QDateTime now(QDateTime::currentDateTime());
  foreach (QString alert, _soonCanceledAlerts.keys()) {
    QDateTime dt(_soonCanceledAlerts.value(alert));
    if (dt <= now) {
      Log::debug() << "do cancel alert " << alert;
      emit alertCanceled(alert);
      doEmitAlertCancellation(alert);
      _soonCanceledAlerts.remove(alert);
    }
  }
}
