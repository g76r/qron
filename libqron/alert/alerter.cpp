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
#include "alerter.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>
#include "log/log.h"
#include "pf/pfnode.h"
#include "logalertchannel.h"
#include "urlalertchannel.h"
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
  _channels.insert("url", new UrlAlertChannel(0, this));
  _channels.insert("mail", new MailAlertChannel(0, this));
  foreach (AlertChannel *channel, _channels)
    connect(this, SIGNAL(configChanged(AlerterConfig)),
            channel, SLOT(setConfig(AlerterConfig)),
            Qt::BlockingQueuedConnection);
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
}

Alerter::~Alerter() {
  foreach (AlertChannel *channel, _channels.values())
    channel->deleteLater(); // cant be a child cause it lives it its own thread
}

void Alerter::setConfig(AlerterConfig config) {
  if (this->thread() == QThread::currentThread())
    doSetConfig(config);
  else
    QMetaObject::invokeMethod(this, "doSetConfig", Qt::BlockingQueuedConnection,
                              Q_ARG(AlerterConfig, config));
}

void Alerter::doSetConfig(AlerterConfig config) {
  _config = config;
  emit paramsChanged(_config.params());
  emit configChanged(_config);
}

void Alerter::emitAlert(QString alertId) {
  QMetaObject::invokeMethod(this, "doEmitAlert", Q_ARG(QString, alertId));
}

void Alerter::raiseAlert(QString alertId) {
  QMetaObject::invokeMethod(this, "doRaiseAlert", Q_ARG(QString, alertId),
                            Q_ARG(bool, false));
}

void Alerter::raiseAlertImmediately(QString alertId) {
  QMetaObject::invokeMethod(this, "doRaiseAlert", Q_ARG(QString, alertId),
                            Q_ARG(bool, true));
}

void Alerter::cancelAlert(QString alertId) {
  QMetaObject::invokeMethod(this, "doCancelAlert", Q_ARG(QString, alertId),
                            Q_ARG(bool, false));
}

void Alerter::cancelAlertImmediately(QString alertId) {
  QMetaObject::invokeMethod(this, "doCancelAlert", Q_ARG(QString, alertId),
                            Q_ARG(bool, true));
}

void Alerter::doRaiseAlert(QString alertId, bool immediately) {
  Alert oldAlert = _alerts.value(alertId);
  Alert newAlert = oldAlert.isNull() ? Alert(alertId) : oldAlert;
//  if (alertId.startsWith("task.maxinstancesreached")
//      || alertId.startsWith("task.maxinstancesreached")) {
//    qDebug() << "doRaiseAlert:" << alertId << immediately << oldAlert.statusToString();
//  }
  if (immediately) {
    switch (oldAlert.status()) {
    case Alert::Rising:
    case Alert::Nonexistent:
    case Alert::Canceled: // should not happen
    case Alert::MayRise:
      actionRaise(&newAlert);
      break;
    case Alert::Dropping:
      actionNoLongerCancel(&newAlert);
      break;
    case Alert::Raised:
      return; // nothing to do
    }
  } else {
    switch (oldAlert.status()) {
    case Alert::Rising:
    case Alert::Raised:
      return; // nothing to do
    case Alert::MayRise:
      if (newAlert.visibilityDate() <= QDateTime::currentDateTime()) {
        actionRaise(&newAlert);
        break;
      }
      // else fall through next case
    case Alert::Nonexistent:
    case Alert::Canceled: // should not happen
      // FIXME handle rise delay, both default and from rules
      newAlert.setVisibilityDate(newAlert.riseDate().addSecs(42));
      newAlert.setStatus(Alert::Rising);
      break;
    case Alert::Dropping:
      actionNoLongerCancel(&newAlert);
      break;
    }
  }
 commitChange(&newAlert, &oldAlert);
}

void Alerter::doCancelAlert(QString alertId, bool immediately) {
  Alert oldAlert = _alerts.value(alertId), newAlert = oldAlert;
//  if (alertId.startsWith("task.maxinstancesreached")
//      || alertId.startsWith("task.maxinstancesreached")) {
//    qDebug() << "doCancelAlert:" << alertId << immediately << oldAlert.statusToString();
//  }
  if (immediately) {
    switch (oldAlert.status()) {
    case Alert::Nonexistent:
    case Alert::Canceled: // should not happen
      return; // nothing to do
    case Alert::Rising:
    case Alert::MayRise:
      newAlert = Alert();
      break;
    case Alert::Raised:
    case Alert::Dropping:
      actionCancel(&newAlert);
      break;
    }
  } else {
    switch (oldAlert.status()) {
    case Alert::Nonexistent:
    case Alert::MayRise:
    case Alert::Dropping:
    case Alert::Canceled: // should not happen
      return; // nothing to do
    case Alert::Rising:
      newAlert.setStatus(Alert::MayRise);
      // FIXME handle may_rise delay, both default and from rules, default rise delay / 2
      newAlert.setCancellationDate(QDateTime::currentDateTime().addSecs(21));
      break;
    case Alert::Raised:
      newAlert.setStatus(Alert::Dropping);
      // FIXME handle cancel delay, both default and from rules
      newAlert.setCancellationDate(newAlert.riseDate().addSecs(_config.defaultCancelDelay()));
    }
  }
  commitChange(&newAlert, &oldAlert);
}

void Alerter::doEmitAlert(QString alertId) {
  Log::debug() << "emit alert: " << alertId;
  notifyChannels(Alert(alertId));
}

void Alerter::asyncProcessing() {
  QDateTime now = QDateTime::currentDateTime();
  foreach (Alert oldAlert, _alerts) {
    switch(oldAlert.status()) {
    case Alert::Nonexistent: // should never happen
    case Alert::Canceled: // should never happen
    case Alert::Raised:
      continue; // nothing to do
    case Alert::Rising:
      if (oldAlert.visibilityDate() <= now) {
        Alert newAlert = oldAlert;
        actionRaise(&newAlert);
        commitChange(&newAlert, &oldAlert);
      }
      break;
    case Alert::MayRise:
      if (oldAlert.cancellationDate() <= now) {
        Alert newAlert;
        commitChange(&newAlert, &oldAlert);
      }
      break;
    case Alert::Dropping:
      if (oldAlert.cancellationDate() <= now) {
        Alert newAlert = oldAlert;
        actionCancel(&newAlert);
        commitChange(&newAlert, &oldAlert);
      }
      break;
    }
  }
}

void Alerter::actionRaise(Alert *newAlert) {
  newAlert->setStatus(Alert::Raised);
  newAlert->setVisibilityDate(QDateTime());
  Log::debug() << "alert raised: " << newAlert->id();
  notifyChannels(*newAlert);
}

void Alerter::actionCancel(Alert *newAlert) {
  newAlert->setStatus(Alert::Canceled);
  Log::debug() << "do cancel alert: " << newAlert->id();
  notifyChannels(*newAlert);
}

void Alerter::actionNoLongerCancel(Alert *newAlert) {
  newAlert->setStatus(Alert::Raised);
  newAlert->setCancellationDate(QDateTime());
  Log::debug() << "alert is no longer scheduled for cancellation "
               << newAlert->id()
               << " (it was raised again within cancel delay)";
}

void Alerter::notifyChannels(Alert newAlert) {
  emit alertNotified(newAlert);
  foreach (AlertRule rule, _config.rules()) {
    if (rule.patternRegExp().exactMatch(newAlert.id())) {
      QString channelName = rule.channelName();
      if (channelName == QStringLiteral("stop"))
        break;
      QPointer<AlertChannel> channel = _channels.value(channelName);
      if (channel) {
        newAlert.setRule(rule);
        channel.data()->notifyAlert(newAlert);
      }
    }
  }
}

void Alerter::commitChange(Alert *newAlert, Alert *oldAlert) {
  switch (newAlert->status()) {
  case Alert::Nonexistent: // should not happen
  case Alert::Canceled:
    _alerts.remove(oldAlert->id());
    *newAlert = Alert();
    break;
  default:
    _alerts.insert(newAlert->id(), *newAlert);
  }
//  if (newAlert->id().startsWith("task.maxinstancesreached")
//      || oldAlert->id().startsWith("task.maxinstancesreached")) {
//    qDebug() << "commitChange:" << newAlert->id() << newAlert->statusToString() << oldAlert->statusToString();
//  }
  emit alertChanged(*newAlert, *oldAlert);
}
