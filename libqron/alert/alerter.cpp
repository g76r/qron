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

Alerter::Alerter() : QObject(0), _thread(new QThread),
  _emitRequestsCounter(0), _raiseRequestsCounter(0), _cancelRequestsCounter(0),
  _raiseImmediateRequestsCounter(0), _cancelImmediateRequestsCounter(0),
  _emitNotificationsCounter(0), _raiseNotificationsCounter(0),
  _cancelNotificationsCounter(0), _totalChannelsNotificationsCounter(0),
  _rulesCacheSize(0), _rulesCacheHwm(0), _deduplicatingAlertsCount(0),
  _deduplicatingAlertsHwm(0) {
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
  qRegisterMetaType<QList<AlertSubscription> >("QList<AlertSubscription>");
  qRegisterMetaType<AlertSubscription>("AlertSubscription");
  qRegisterMetaType<AlertSettings>("AlertSettings");
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
  _rulesCacheHwm = qMax(_rulesCacheHwm, qMax(_alertSubscriptionsCache.size(),
                                             _alertSettingsCache.size()));
  _rulesCacheSize = 0;
  _alertSubscriptionsCache.clear();
  _alertSettingsCache.clear();
  _config = config;
  _gridboards = config.gridboards(); // FIXME merge with current data
  // LATER recompute visibilityDate and cancellationDate in raisableAlerts and emittedAlerts
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
  Alert oldAlert = _raisableAlerts.value(alertId);
  Alert newAlert = oldAlert.isNull() ? Alert(alertId) : oldAlert;
  ++_raiseRequestsCounter;
//  if (alertId.startsWith("task.maxinstancesreached")
//      || alertId.startsWith("task.maxinstancesreached")) {
//    qDebug() << "doRaiseAlert:" << alertId << immediately << oldAlert.statusToString();
//  }
  if (immediately) {
    ++_raiseImmediateRequestsCounter;
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
      goto nothing_changed;
    }
  } else {
    switch (oldAlert.status()) {
    case Alert::Rising:
    case Alert::Raised:
      goto nothing_changed;
    case Alert::MayRise:
      if (newAlert.visibilityDate() <= QDateTime::currentDateTime()) {
        actionRaise(&newAlert);
        break;
      }
      // else fall through next case
    case Alert::Nonexistent:
    case Alert::Canceled: // should not happen
      newAlert.setVisibilityDate(newAlert.riseDate().addMSecs(
                                   riseDelay(alertId)));
      newAlert.setStatus(Alert::Rising);
      break;
    case Alert::Dropping:
      actionNoLongerCancel(&newAlert);
      break;
    }
  }
 commitChange(&newAlert, &oldAlert);
nothing_changed:
 if (newAlert.isNull()) // gridboards need an id to identify canceled alert
   newAlert = Alert(alertId); // TODO not sure it is usefull for raise
 notifyGridboards(newAlert);
}

void Alerter::doCancelAlert(QString alertId, bool immediately) {
  Alert oldAlert = _raisableAlerts.value(alertId), newAlert = oldAlert;
  ++_cancelRequestsCounter;
//  if (alertId.startsWith("task.maxinstancesreached")
//      || alertId.startsWith("task.maxinstancesreached")) {
//    qDebug() << "doCancelAlert:" << alertId << immediately << oldAlert.statusToString();
//  }
  if (immediately) {
    ++_cancelImmediateRequestsCounter;
    switch (oldAlert.status()) {
    case Alert::Nonexistent:
    case Alert::Canceled: // should not happen
      goto nothing_changed;
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
      goto nothing_changed;
    case Alert::Rising:
      newAlert.setStatus(Alert::MayRise);
      newAlert.setCancellationDate(QDateTime::currentDateTime().addMSecs(
                                     mayriseDelay(alertId)));
      break;
    case Alert::Raised:
      newAlert.setStatus(Alert::Dropping);
      newAlert.setCancellationDate(QDateTime::currentDateTime().addMSecs(
                                     mayriseDelay(alertId)));
    }
  }
  commitChange(&newAlert, &oldAlert);
nothing_changed:
  if (newAlert.isNull()) // gridboards need an id to identify canceled alert
    newAlert = Alert(alertId);
  notifyGridboards(newAlert);
}

void Alerter::doEmitAlert(QString alertId) {
  Log::debug() << "emit alert: " << alertId;
  ++_emitRequestsCounter;
  Alert alert = _emittedAlerts.value(alertId);
  QDateTime now = QDateTime::currentDateTime();
  if (alert.isNull()) {
    // never seen: record in emittedAlerts and notify channels
    alert = Alert(alertId);
    notifyChannels(alert);
    alert.resetCount();
    alert.setVisibilityDate(now.addMSecs(duplicateEmitDelay(alertId)));
    _emittedAlerts.insert(alertId, alert);
    ++_deduplicatingAlertsCount;
    _deduplicatingAlertsHwm = qMax(_deduplicatingAlertsCount,
                                   _deduplicatingAlertsHwm);
  } else if (alert.visibilityDate() >= now) {
    // alert already emitted recently : increment counter and don't notify
    alert.incrementCount();
    _emittedAlerts.insert(alertId, alert);
  } else {
    // alert emitted long enough to be notified again
    alert.incrementCount();
    alert.setRiseDate(now);
    notifyChannels(alert);
    alert.resetCount();
    alert.setVisibilityDate(now.addMSecs(duplicateEmitDelay(alertId)));
    _emittedAlerts.insert(alertId, alert);
  }
}

void Alerter::asyncProcessing() {
  QDateTime now = QDateTime::currentDateTime();
  foreach (Alert oldAlert, _raisableAlerts) {
    switch(oldAlert.status()) {
    case Alert::Nonexistent: // should never happen
    case Alert::Canceled: // should never happen
    case Alert::Raised:
      continue; // nothing to do
    case Alert::Rising:
    case Alert::MayRise:
      if (oldAlert.visibilityDate() <= now) {
        Alert newAlert = oldAlert;
        actionRaise(&newAlert);
        commitChange(&newAlert, &oldAlert);
        break;
      }
      if (oldAlert.status() == Alert::MayRise
          && oldAlert.cancellationDate() <= now) {
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
  foreach (Alert alert, _emittedAlerts) {
    if (alert.visibilityDate() <= now) {
      if (alert.count() > 0) {
        notifyChannels(alert);
      }
      _emittedAlerts.remove(alert.id());
      --_deduplicatingAlertsCount;
    }
  }
  _rulesCacheSize = qMax(_alertSubscriptionsCache.size(),
                         _alertSettingsCache.size());
  _rulesCacheHwm = qMax(_rulesCacheHwm, _rulesCacheSize);
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
               << " (it was raised again within drop delay)";
}

void Alerter::notifyChannels(Alert newAlert) {
  switch(newAlert.status()) {
  case Alert::Raised:
    ++_raiseNotificationsCounter;
    break;
  case Alert::Canceled:
    ++_cancelNotificationsCounter;
    break;
  case Alert::Nonexistent:
    ++_emitNotificationsCounter;
    break;
  default: // should never happen
    ;
  }
  foreach (AlertSubscription sub, alertSubscriptions(newAlert.id())) {
    QPointer<AlertChannel> channel = _channels.value(sub.channelName());
    if (channel) { // should never be false
      ++_totalChannelsNotificationsCounter;
      newAlert.setSubscription(sub);
      channel.data()->notifyAlert(newAlert);
    }
  }
  emit alertNotified(newAlert);
}

void Alerter::notifyGridboards(Alert newAlert) {
  // FIXME move to another thread
  QList<Gridboard> gridboards = _gridboards;
  qDebug() << "Alerter::notifyGridboards" << gridboards.size() << newAlert.id()
           << newAlert.statusToString();
  for (int i = 0; i < gridboards.size(); ++i) {
    QRegularExpressionMatch match =
        gridboards[i].patternRegexp().match(newAlert.id());
    qDebug() << " " << gridboards[i].id() << match.hasMatch();
    if (match.hasMatch()) {
      // FIXME stats: here
      gridboards[i].update(match, newAlert);
    }
  }
  // FIXME stats: or here
  _gridboards = gridboards;
}

void Alerter::commitChange(Alert *newAlert, Alert *oldAlert) {
  switch (newAlert->status()) {
  case Alert::Nonexistent: // should not happen
  case Alert::Canceled:
    _raisableAlerts.remove(oldAlert->id());
    *newAlert = Alert();
    break;
  default:
    _raisableAlerts.insert(newAlert->id(), *newAlert);
  }
//  if (newAlert->id().startsWith("task.maxinstancesreached")
//      || oldAlert->id().startsWith("task.maxinstancesreached")) {
//    qDebug() << "commitChange:" << newAlert->id() << newAlert->statusToString() << oldAlert->statusToString();
//  }
  emit raisableAlertChanged(*newAlert, *oldAlert);
}

QList<AlertSubscription> Alerter::alertSubscriptions(QString alertId) {
  if (!_alertSubscriptionsCache.contains(alertId)) {
    QList<AlertSubscription> list;
    foreach (const AlertSubscription &sub, _config.alertSubscriptions()) {
      if (sub.patternRegexp().match(alertId).hasMatch()) {
        QString channelName = sub.channelName();
        if (channelName == QStringLiteral("stop"))
          break;
        if (!_channels.contains(channelName)) // should never happen
          continue;
        list.append(sub);
      }
    }
    _alertSubscriptionsCache.insert(alertId, list);
  }
  return _alertSubscriptionsCache.value(alertId);
}

AlertSettings Alerter::alertSettings(QString alertId) {
  if (!_alertSettingsCache.contains(alertId)) {
    foreach (const AlertSettings &settings, _config.alertSettings()) {
      if (settings.patternRegexp().match(alertId).hasMatch()) {
        _alertSettingsCache.insert(alertId, settings);
        goto found;
      }
    }
    _alertSettingsCache.insert(alertId, AlertSettings());
found:;
    //Log::debug() << "adding settings to cache: " << alertId << " "
    //             << _alertSettingsCache.value(alertId).toPfNode().toString();
  }
  return _alertSettingsCache.value(alertId);
}

qint64 Alerter::riseDelay(QString alertId) {
  qint64 delay = alertSettings(alertId).riseDelay();
  return delay > 0 ? delay : _config.riseDelay();
}

qint64 Alerter::mayriseDelay(QString alertId) {
  qint64 delay = alertSettings(alertId).mayriseDelay();
  return delay > 0 ? delay : _config.mayriseDelay();
}

qint64 Alerter::dropDelay(QString alertId) {
  qint64 delay = alertSettings(alertId).dropDelay();
  return delay > 0 ? delay : _config.dropDelay();
}

qint64 Alerter::duplicateEmitDelay(QString alertId) {
  qint64 delay = alertSettings(alertId).duplicateEmitDelay();
  return delay > 0 ? delay : _config.duplicateEmitDelay();
}

Gridboard Alerter::gridboard(QString gridboardId) const {
  foreach (const Gridboard &gridboard, _gridboards.data())
    if (gridboard.id() == gridboardId)
      return gridboard;
  return Gridboard();
}
