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
#include "alerter.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>
#include "log/log.h"
#include "pf/pfnode.h"
#include "logalertchannel.h"
#include "udpalertchannel.h"

Alerter::Alerter(QObject *threadParent) : QObject(0),
  _thread(new QThread(threadParent)) {
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  qRegisterMetaType<QList<AlertRule> >("QList<AlertRule>");
  qRegisterMetaType<AlertRule>("AlertRule");
  qRegisterMetaType<Alert>("Alert");
  _channels.insert("log", new LogAlertChannel(this));
  _channels.insert("udp", new UdpAlertChannel(this));
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
        // LATER warn
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
      foreach (PfNode node, node.children()) {
        if (node.name() == "match" || node.name() == "stop") {
          // ignore
        } else {
          QString name = node.name();
          AlertChannel *channel = _channels.value(name);
          if (channel) {
            AlertRule rule(node, pattern, channel, stop, notifyCancel);
            _rules.append(rule);
            Log::debug() << "configured alert rule " << name << " " << pattern
                         << " " << stop << " "
                         << rule.patternRegExp().pattern();
          } else {
            Log::warning() << "alert channel '" << name << "' unknown in alert "
                              "rule with matching pattern " << pattern;
          }
        }
      }
    }
  }
  emit paramsChanged(_params);
  emit rulesChanged(_rules);
  return true;
}

void Alerter::emitAlert(QString alert) {
  Log::debug() << "emiting alert " << alert;
  int n = 0;
  foreach (AlertRule rule, _rules) {
    if (rule.patternRegExp().exactMatch(alert)) {
      Log::debug() << "alert matching rule #" << n;
      sendMessage(Alert(alert, rule), false);
      if (rule.stop())
        break;
    }
    ++n;
  }
  emit alertEmited(alert);
}

void Alerter::emitAlertCancellation(QString alert) {
  Log::debug() << "emiting alert cancellation " << alert;
  int n = 0;
  foreach (AlertRule rule, _rules) {
    if (rule.patternRegExp().exactMatch(alert)) {
      Log::debug() << "alert matching rule #" << n;
      if (rule.notifyCancel())
        sendMessage(Alert(alert, rule), true);
      if (rule.stop())
        break;
    }
    ++n;
  }
  emit alertEmited(alert);
}

void Alerter::sendMessage(Alert alert, bool cancellation) {
  QWeakPointer<AlertChannel> channel = alert.rule().channel();
  if (channel)
    QMetaObject::invokeMethod(channel.data(), "sendMessage",
                              Qt::QueuedConnection,
                              Q_ARG(Alert, alert),
                              Q_ARG(bool, cancellation));
}

void Alerter::raiseAlert(QString alert) {
  if (!_raisedAlerts.contains(alert)) {
    _raisedAlerts.insert(alert);
    Log::debug() << "raising alert " << alert;
    emit alertRaised(alert);
    emitAlert(alert);
  }
}

void Alerter::cancelAlert(QString alert) {
  if (_raisedAlerts.contains(alert)) {
    _raisedAlerts.remove(alert);
    Log::debug() << "lowering alert " << alert;
    emit alertCanceled(alert);
    emitAlertCancellation(alert);
  }
}