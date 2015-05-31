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
#ifndef ALERTER_H
#define ALERTER_H

#include <QObject>
#include "util/paramset.h"
#include "config/alertrule.h"
#include "alertchannel.h"
#include <QHash>
#include <QString>
#include <QDateTime>
#include "config/alerterconfig.h"

class QThread;
class PfNode;

/** Main class for the alert system.
 *
 * Expose alert API, consisting of the following methods:
 * - raiseAlert: schedule the rise of an alert as soon as the rise delay is
 *   over, or do nothing is the same alert is already raised, the alert will be
 *   emited when actually raised
 * - raiseAlertImmediatly: same, without delay
 * - cancelAlert: schedule cancellation of a raised alert as soon as the cancel
 *   delay is over, or do nothing is there are no such alert currently raised,
 *   an alert cancellation will be emited when actually canceled
 * - cancelAlertImmediately: same, without delay
 * - emitAlert: emit an alert, regardless the raise/cancel mechanism, which
 *   should be done with caution since it provides no protection against mass
 *   alert spam, whereas raise/cancel do
 *
 * Apart from direct emission, alerts can be in 5 states:
 * - nonexistent
 * - raising
 * - raised
 * - canceling
 * - canceled (turns into nonexistent just after cancellation notification)
 * - may_rise (when cancelAlert is called on a raising alert)
 *
 * The following table provides state transition according to methods and time
 * events:
 *
 * <pre>
 * Previous State        Event                Next State  Action
 * nonexistent|raising|  raise                raising     -
 *  may_rise
 * raised|canceling      raise                raised      -
 * raising               age > raise delay    raised      emit alert
 * nonexistent|raising|  raiseImmediately     raised      emit alert
 *  may_rise
 * raised|canceling      raiseImmediately     raised      -
 * nonexistent           cancel               nonexistent -
 * raising|              cancel               may_rise    -
 *  may_rise
 * raised|canceling      cancel               canceling   -
 * canceling             age > cancel delay   canceled    emit cancellation
 * may_rise              age > cancel delay   nonexistent -
 * nonexistent|raising|  cancelImmediately    nonexistent -
 *  may_rise
 * raised|canceling      cancelImmediately    canceled    emit cancellation
 * </pre>
 *
 * In addition to emitting alert and cancellation, some channels may emit
 * reminders for alerts that stay raised a long time (actually for alerts that
 * stay in raised and/or canceling states, but channels are not noticed of
 * raised -> canceling or canceling -> raised transitions, therefore they see
 * the two states as only one: raised).
 *
 * Internaly, this class is responsible for configuration, and alerts
 * dispatching among channels following the state machine described above.
 * Actual alert emission is performed by channels depending on the communication
 * mean they implement.
 *
 * Alerter only notifies channels of state transitions for which there is an
 * alert or a cancellation to emit, and of emitAlert() calls.
 * Therefore channels only see 3 alert states, folllowing this table:
 *
 * <pre>
 * Alerter alert state      AlertChannel alert state
 * nonexistent              (never seen)
 * raising                  (never seen)
 * raised                   raised
 * canceling                raised
 * canceled                 canceled
 * may_rise                 (never seen)
 * (direct emit)            nonexistent (alert has no state)
 * </pre>
 *
 * In other words, a user is only notified through a alert channel (i.e. through
 * a push communication mean) of alert that are raised, when they are raised
 * (emit alert) or canceled (emit cancellation), and optionaly when they stay
 * raised too long (emit reminder), or, obviously, if they are emitte without
 * the raise/cancel system, through emitAlert method.
 *
 * However, access to alerts in non-pushable states (raising, may_rise and the
 * difference between raised and canceling) is given through data models and
 * can be queried by users or external tools.
 */
class LIBQRONSHARED_EXPORT Alerter : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(Alerter)
  QThread *_thread;
  AlerterConfig _config;
  QHash<QString,AlertChannel*> _channels;
  QHash<QString,Alert> _alerts;

public:
  explicit Alerter();
  ~Alerter();
  /** This method is threadsafe. */
  void setConfig(AlerterConfig config);
  //AlerterConfig config() const;
  /** Immediatly emit an alert, regardless of raised alert, even if the same
   * alert has just been emited.
   * In most cases it is strongly recommanded to call raiseAlert() instead.
   * This method is threadsafe.
   * @see raiseAlert() */
  void emitAlert(QString alertId);
  /** Raise an alert and emit it if it is not already raised.
   * This is the prefered way to report an alert.
   * If the alert is already raised but has been canceled and is still in its
   * cancel grace period, the cancellation is unscheduled.
   * If the alert is already raised and not canceled, this method does nothing.
   * This method is threadsafe. */
  void raiseAlert(QString alertId);
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
  void cancelAlert(QString alertId);
  /** Immediatly cancel an alert, ignoring grace delay.
   * In most cases it is strongly recommanded to call cancelAlert() instead.
   * The only widespread reason to use cancelAlertImmediately() is when one
   * wants to manually cancel an alert to ensure that if it occurs again soon
   * (during what would have been the end of the grace delay) all events
   * (e.g. mails) are sent again.
   * This method is threadsafe.
   * @see cancelAlert() */
  void cancelAlertImmediately(QString alertId);
  // FIXME doc
  void raiseAlertImmediately(QString alertId);

signals:
  // FIXME doc
  void alertChanged(Alert newAlert, Alert oldAlert);
  /** An alert is emited through alert channels.
   * This can occur when raising an alert that is not already raised (through
   * raiseAlert()) or when directly an alert (through emitAlert()). */
  // FIXME doc: or when cancelation is emited
  void alertNotified(Alert alert);
  /** Config parameters changed.
   * Convenience signals emited just before configChanged(). */
  void paramsChanged(ParamSet params);
  /** Configuration has changed. */
  void configChanged(AlerterConfig config);

private slots:
  void asyncProcessing();

private:
  Q_INVOKABLE void doSetConfig(AlerterConfig config);
  Q_INVOKABLE void doRaiseAlert(QString alertId, bool immediately);
  Q_INVOKABLE void doCancelAlert(QString alertId, bool immediately);
  Q_INVOKABLE void doEmitAlert(QString alertId);
  inline void actionRaise(Alert *newAlert);
  inline void actionCancel(Alert *newAlert);
  inline void actionNoLongerCancel(Alert *newAlert);
  inline void notifyChannels(Alert newAlert);
  inline void commitChange(Alert *newAlert, Alert *oldAlert);
};

#endif // ALERTER_H
