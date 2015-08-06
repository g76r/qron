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
#include "config/alertsubscription.h"
#include "alertchannel.h"
#include <QHash>
#include <QString>
#include <QDateTime>
#include "config/alerterconfig.h"
#include "gridboard.h"
#include "thread/atomicvalue.h"

class QThread;
class PfNode;
class GridboardThread;

/** Main class for the alert system.
 *
 * Expose alert API, for two kind of alerts: raisable alerts and one-shot
 * alerts.
 *
 * The raisable alerts API consists of the following methods:
 * - raiseAlert: schedule the rise of an alert as soon as the rise delay is
 *   over, or do nothing is the same alert is already raised, the alert will be
 *   emited when actually raised
 * - raiseAlertImmediatly: same, without delay
 * - cancelAlert: schedule cancellation of a raised alert as soon as the cancel
 *   delay is over, or do nothing is there are no such alert currently raised,
 *   an alert cancellation will be emited when actually canceled
 * - cancelAlertImmediately: same, without delay
 *
 * The one-shot alerts API consists of the following methods:
 * - emitAlert: emit an alert, regardless the raise/cancel mechanism, if the
 *  same alert has already been emitted recently, it will only been emitted
 *  after a while, and only once for all same alerts for which emission will be
 *  requested until then.
 *
 * One-shot alerts have no state, but raisable alerts can have the following
 * states:
 * - nonexistent
 * - rising
 * - raised
 * - dropping
 * - canceled (turns into nonexistent just after cancellation notification)
 * - mayrise (when cancelAlert is called on a rising alert)
 *
 * The following table provides state transition according to methods and time
 * events:
 *
 * <pre>
 * Previous State        Event                Next State  Action
 * nonexistent|rising|  raise                rising       -
 *  mayrise
 * raised|dropping      raise                raised       -
 * rising               age > raise delay    raised       emit alert
 * nonexistent|rising|  raiseImmediately     raised       emit alert
 *  mayrise
 * raised|dropping      raiseImmediately     raised       -
 * nonexistent          cancel               nonexistent  -
 * rising|mayrise       cancel               mayrise      -
 * raised|dropping      cancel               dropping     -
 * dropping             age > cancel delay   canceled     emit cancellation
 * mayrise              age > mayrise delay  nonexistent  -
 * nonexistent|rising|  cancelImmediately    nonexistent  -
 *  mayrise
 * raised|dropping      cancelImmediately    canceled     emit cancellation
 * canceled             (immediate)          nonexistent  -
 * </pre>
 *
 * In addition to emitting alert and cancellation, some channels may emit
 * reminders for alerts that stay raised a long time (actually for alerts that
 * stay in raised and/or dropping states, but channels are not noticed of
 * raised -> dropping or dropping -> raised transitions, therefore they see
 * the two states as only one: raised).
 *
 * Internaly, this class is responsible for configuration, and alerts
 * dispatching among channels following the state machine described above.
 * Actual alert emission is performed by channels depending on the communication
 * mean they implement.
 *
 * Alerter only notifies channels of state transitions for which there is an
 * alert or a cancellation to emit, and of one-shot alerts.
 * Therefore channels only see 3 alert states, folllowing this table:
 *
 * <pre>
 * Alerter alert state    AlertChannel alert state
 * nonexistent            (never seen)
 * rising                 (never seen)
 * raised                 raised
 * dropping               raised
 * canceled               canceled
 * mayrise                (never seen)
 * (one-shot alert)       nonexistent (alert has no state)
 * </pre>
 *
 * In other words, a user is only notified through a alert channel (i.e. through
 * a push communication mean) of alert that are raised, when they are raised
 * (emit alert) or canceled (emit cancellation), and optionaly when they stay
 * raised too long (emit reminder), or, obviously, of one-shot alerts.
 *
 * However, access to alerts in non-pushable states (raising, mayrise and the
 * difference between raised and canceling) is given through data models and
 * can be queried by users or external tools.
 */
class LIBQRONSHARED_EXPORT Alerter : public QObject {
  friend class GridboardThread;
  Q_OBJECT
  Q_DISABLE_COPY(Alerter)
  QThread *_alerterThread;
  GridboardThread *_gridboardThread;
  AlerterConfig _config;
  QHash<QString,AlertChannel*> _channels;
  QHash<QString,Alert> _raisableAlerts;
  QHash<QString,Alert> _emittedAlerts;
  QHash<QString, QList<AlertSubscription> > _alertSubscriptionsCache;
  QHash<QString, AlertSettings> _alertSettingsCache;
  qint64 _emitRequestsCounter, _raiseRequestsCounter, _cancelRequestsCounter,
  _raiseImmediateRequestsCounter, _cancelImmediateRequestsCounter;
  qint64 _emitNotificationsCounter, _raiseNotificationsCounter,
  _cancelNotificationsCounter, _totalChannelsNotificationsCounter;
  int _rulesCacheSize, _rulesCacheHwm, _deduplicatingAlertsCount,
  _deduplicatingAlertsHwm;
  AtomicValue<QList<Gridboard>> _gridboards;

public:
  explicit Alerter();
  ~Alerter();
  /** This method is threadsafe. */
  void setConfig(AlerterConfig config);
  /** Emit a one-shot alert, regardless of raised alerts.
   *
   * This method is threadsafe.
   * @see raiseAlert() */
  void emitAlert(QString alertId);
  /** Raise an alert and emit it after a rise delay if it is not already raised.
   *
   * This is the prefered way to report an alert, along with cancelAlert().
   * If the alert is already raised but has been canceled and is still in its
   * cancel grace period, the cancellation is unscheduled.
   * If the alert is already raised and not canceled, this method does nothing.
   * This method is threadsafe. */
  void raiseAlert(QString alertId);
  /** Tell the Alerter that an alert should no longer be raised.
   *
   * This is the prefered way to report the end of an alert.
   * This schedule the alert for cancellation after a grace delay set to 15
   * minutes by default and configurable through "canceldelay" parameter (with
   * a value in seconds).
   * The grace delay should be configured longer than any time interval
   * between alerts send which recipients are human beings
   * ("mindelaybetweensend", which defaults to 10 minutes) since it avoids
   * flip/flop spam (which otherwise would occur if the same
   * alerts is raised and cancel several time within the same time interval
   * between alerts send).
   * This method is threadsafe. */
  void cancelAlert(QString alertId);
  /** Immediatly cancel an alert, ignoring grace delay.
   *
   * In most cases it is strongly recommanded to call cancelAlert() instead.
   * The only widespread reason to use cancelAlertImmediately() is when one
   * wants to manually cancel an alert to ensure that if it occurs again soon
   * (during what would have been the end of the grace delay) all events
   * (e.g. mails) are sent again.
   * This method is threadsafe.
   * @see cancelAlert() */
  void cancelAlertImmediately(QString alertId);
  /** Immediatly raise an alert, ignoring rise delay.
   *
   * In most cases it is strongly recommanded to call raiseAlert() instead.
   * This method is threadsafe.
   * @see cancelAlert() */
  void raiseAlertImmediately(QString alertId);
  /** Count of emitAlert() requests since startup. */
  qint64 emitRequestsCounter() const { return _emitRequestsCounter; }
  /** Count of raiseAlert() and raiseAlertImmediately() requests since startup,
   * regardless the number of alert rises actually notified to subscriptions
   * and/or channels. */
  qint64 raiseRequestsCounter() const { return _raiseRequestsCounter; }
  /** Count of cancelAlert() and cancelAlertImmediately() requests since
   * startup, regardless the number of alert cancellations actually notified
   * to subscriptions and/or channels. */
  qint64 cancelRequestsCounter() const { return _cancelRequestsCounter; }
  /** Count of raiseAlertImmediately() requests since startup. */
  qint64 raiseImmediateRequestsCounter() const {
    return _raiseImmediateRequestsCounter; }
  /** Count of cancelAlertImmediately() requests since startup. */
  qint64 cancelImmediateRequestsCounter() const {
    return _cancelImmediateRequestsCounter; }
  /** Count of alerts notified to channels due to emitAlert() requests,
   * regardless the number of subscriptions and/or channels notified, even if 0.
   */
  qint64 emitNotificationsCounter() const { return _emitNotificationsCounter; }
  /** Count of alerts notified to channels due to raiseAlert() or
   * raiseAlertImmediately() requests, regardless the number of subscriptions
   * and/or channels notified, even if 0. */
  qint64 raiseNotificationsCounter() const {
    return _raiseNotificationsCounter; }
  /** Count of alerts notified to channels due to cancelAlert() or
   * cancelAlertImmediately() requests, regardless the number of subscriptions
   * and/or channels notified, even if 0. */
  qint64 cancelNotificationsCounter() const {
    return _cancelNotificationsCounter; }
  /** Count of alert notifications sent to channels since startup, which depends
   * on subscriptions since an alert can be raised 100 times and notified 0
   * times if it has no subscription, or in the other hand can be raised 3 times
   * and notified 18 times if 6 different subscriptions match it. */
  qint64 totalChannelsNotificationsCounter() const {
    return _totalChannelsNotificationsCounter; }
  /** Current size of rules cache. The cache is reset on reload. */
  int rulesCacheSize() const { return _rulesCacheSize; }
  /** Highest size of rules cache since startup. */
  int rulesCacheHwm() const { return _rulesCacheHwm; }
  /** Current number of alerts that have not been yet emitted due to duplicate
   * emit delay and thus being deduplicated. */
  int deduplicatingAlertsCount() const { return _deduplicatingAlertsCount; }
  /** Highest value of duplicateEmitCount() since startup. */
  int deduplicatingAlertsHwm() const { return _deduplicatingAlertsHwm; }
  /** Count of alerts that have been tested against gridboards pattern since
   * startup. */
  qint64 gridboardsEvaluationsCounter() const;
  /** Times a gridboard has been updated by an alert since startup. */
  qint64 gridboardsUpdatesCounter() const;
  /** Provide a gridboard given its id.
   * This method is thread-safe. */
  Gridboard gridboard(QString gridboardId) const;
  /** Remove any current data from a gridboard given its id.
   * This method is thread-safe. */
  void clearGridboard(QString gridboardId);

signals:
  /** A raisable alert (i.e. an alert handled through raiseAlert()/cancelAlert()
   * calls, not through emitAlert()) has been created or destroyed or modified.
   * Can be connected to a SharedUiItemsModel. */
  void raisableAlertChanged(Alert newAlert, Alert oldAlert,
                            QString idQualifier);
  /** An alert is emited through alert channels.
   * This occurs for raisable alerts when raising an alert that is not
   * already raised (through raiseAlert()) or when canceling a raised alert
   * (through cancelAlert()).
   * And this occurs too for one-shots alerts when they are emitted (through
   * emitAlert()) immediately or after a while if the same alert is emitted
   * several time and aggregation occurs. */
  void alertNotified(Alert alert);
  /** Config parameters changed.
   * Convenience signal emited just before configChanged(). */
  // LATER remove and use lambdas instead of convenience signals
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
  inline void notifyGridboards(Alert newAlert);
  inline void commitChange(Alert *newAlert, Alert *oldAlert);
  inline QList<AlertSubscription> alertSubscriptions(QString alertId);
  inline AlertSettings alertSettings(QString alertId);
  /** Delay according to (settings) or by default AlertConfig-level delay,
   * in ms. */
  inline qint64 riseDelay(QString alertId);
  /** Delay according to (settings) or by default AlertConfig-level delay,
   * in ms. */
  inline qint64 mayriseDelay(QString alertId);
  /** Delay according to (settings) or by default AlertConfig-level delay,
   * in ms. */
  inline qint64 dropDelay(QString alertId);
  /** Delay according to (settings) or by default AlertConfig-level delay,
   * in ms. */
  inline qint64 duplicateEmitDelay(QString alertId);
};

#endif // ALERTER_H
