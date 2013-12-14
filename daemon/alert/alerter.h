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
#ifndef ALERTER_H
#define ALERTER_H

#include <QObject>
#include "util/paramset.h"
#include "config/alertrule.h"
#include "config/alert.h"
#include "alertchannel.h"
#include <QHash>
#include <QString>
#include <QDateTime>

class QThread;
class PfNode;

#define ALERTER_DEFAULT_CANCEL_DELAY 900000
// 900" = 15'
#define ALERTER_DEFAULT_REMIND_FREQUENCY 3600000
// 3600" = 1h
#define ALERTER_DEFAULT_MIN_DELAY_BETWEEN_SEND 600000
// 600" = 10'
#define ALERTER_DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND 30000
// 30"

/** Main class for the alert system.
 */
class Alerter : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(Alerter)
  QThread *_thread;
  ParamSet _params;
  QHash<QString,AlertChannel*> _channels;
  QList<AlertRule> _rules;
  QHash<QString,QDateTime> _raisedAlerts; // alert + raise time
  QHash<QString,QDateTime> _soonCanceledAlerts; // alert + scheduled cancel time
  int _cancelDelay;
  int _minDelayBetweenSend;
  int _gracePeriodBeforeFirstSend;

public:
  explicit Alerter();
  ~Alerter();
  bool loadConfiguration(PfNode root);
  /** Immediatly emit an alert, regardless of raised alert, even if the same
   * alert has just been emited.
   * In most cases it is strongly recommanded to call raiseAlert() instead.
   * This method is threadsafe.
   * @see raiseAlert() */
  void emitAlert(QString alert);
  /** Raise an alert and emit it if it is not already raised.
   * This is the prefered way to report an alert.
   * If the alert is already raised but has been canceled and is still in its
   * cancel grace period, the cancellation is unscheduled.
   * If the alert is already raised and not canceled, this method does nothing.
   * This method is threadsafe. */
  void raiseAlert(QString alert);
  /** Tell the Alerter that an alert should no longer be raised.
   * This is the prefered way to report the end of an alert.
   * This schedule the alert for cancellation after a grace delay set to 15
   * minutes by default and configurable through "canceldelay" parameter (with
   * a value in seconds).
   * The grace delay should be configured longer than any time interval
   * between alerts send which recipients are human beings (e.g.
   * "mindelaybetweensend" for mail alerts, which default is 10 mintues)
   * since it avoids flip/flop spam (which otherwise would occur if the same
   * alerts is raised and cancel several time within the same time interval
   * between alerts send).
   * This method is threadsafe. */
  void cancelAlert(QString alert);
  /** Immediatly cancel an alert, ignoring grace delay.
   * In most cases it is strongly recommanded to call cancelAlert() instead.
   * The only widespread reason to use cancelAlertImmediately() is when one
   * wants to manually cancel an alert to ensure that if it occurs again soon
   * (during what would have been the end of the grace delay) all events
   * (e.g. mails) are sent again.
   * This method is threadsafe.
   * @see cancelAlert() */
  void cancelAlertImmediately(QString alert);
  /** Give access to alerts parameters.
   * This method is threadsafe. */
  ParamSet params() const { return _params; }
  /** In ms. */
  int cancelDelay() const { return _cancelDelay; }
  /** In ms. */
  int minDelayBetweenSend() const { return _minDelayBetweenSend; }
  /** In ms. */
  int gracePeriodBeforeFirstSend() const { return _gracePeriodBeforeFirstSend; }

signals:
  /** An alert has just been raised.
   * This signal is not emited when raising an alert that is already raised. */
  void alertRaised(QString alert);
  /** An alert has been scheduled for cancellation, e.g. through cancelAlert().
   * This signal is not emited when trying to cancel an alert that is not
   * currently raised.
   * This signal does not mean that the alert is yet actually cancelled.
   * It will be actually canceled only after a grace period called
   * 'canceldelay', if it is not raised again meanwhile.
   * @see alertCancellationEmited()
   * @see alertCancellationUnscheduled() */
  void alertCancellationScheduled(QString alert, QDateTime scheduledTime);
  /** An alert is raised again during then canceldelay grace period. */
  void alertCancellationUnscheduled(QString alert);
  /** An alert is emited through alert channels.
   * This can occur when raising an alert that is not already raised (through
   * raiseAlert()) or when directly an alert (through emitAlert()). */
  void alertEmited(QString alert);
  /** An alert has been actually canceled and emited through alert channels.
   * This signal is only emited after the 'canceldelay' grace period. */
  void alertCanceled(QString alert);
  /** Alert parameters (hence configuration) has changed. */
  void paramsChanged(ParamSet params);
  /** Alert rules (hence configuration) has changed. */
  void rulesChanged(QList<AlertRule> rules);
  /** Alert channels list changed. */
  void channelsChanged(QStringList channels);

private slots:
  void asyncProcessing();

private:
  Q_INVOKABLE void doEmitAlert(QString alert, AlertChannel::MessageType type,
                               QDateTime date = QDateTime::currentDateTime());
  Q_INVOKABLE void doRaiseAlert(QString alert);
  Q_INVOKABLE void doCancelAlert(QString alert, bool immediately = false);
};

#endif // ALERTER_H
