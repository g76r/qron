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
#ifndef ALERT_H
#define ALERT_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include <QDateTime>
#include "util/paramsprovider.h"
#include "modelview/shareduiitem.h"

class AlertData;
class AlertPseudoParamsProvider;
class AlertSubscription;

/** Class used to represent alert data during emission, in or between Alerter
 * and AlertChannel classes. */
class LIBQRONSHARED_EXPORT Alert : public SharedUiItem {
public:
  enum AlertStatus { Nonexistent, Rising, MayRise, Raised, Dropping,
                     Canceled };

  Alert();
  explicit Alert(QString id,
                 QDateTime riseDate = QDateTime::currentDateTime());
  Alert(const Alert&other);
  Alert &operator=(const Alert &other) {
    SharedUiItem::operator=(other); return *this; }
  Alert::AlertStatus status() const;
  void setStatus(Alert::AlertStatus status);
  static Alert::AlertStatus statusFromString(QString string);
  static QString statusToString(Alert::AlertStatus status);
  QString statusToString() const { return statusToString(status()); }
  /** Initial raise() call date, before rising period.
   * Won't change in the alert lifetime. */
  QDateTime riseDate() const;
  /** End of rise or cancellation delays.
   * Applicable to may_rise and dropping status. */
  QDateTime cancellationDate() const;
  void setCancellationDate(QDateTime cancellationDate);
  /** End of rising delay.
   * Applicable to rising and may_rise status. */
  QDateTime visibilityDate() const;
  void setVisibilityDate(QDateTime visibilityDate);
  AlertPseudoParamsProvider pseudoParams() const;
  /** Timestamp a raised alert was last reminded.
   * Only set by channels which handle reminders, not by Alerter. */
  QDateTime lastRemindedDate() const;
  void setLastRemindedDate(QDateTime lastRemindedDate);
  /** Subscription for which an Alert is notified to an AlertChannel.
   * Set by Alerter just before notifying an AlertChannel. */
  AlertSubscription subscription() const;
  void setSubscription(AlertSubscription subscription);

private:
  AlertData *data();
  const AlertData *data() const {
    return (const AlertData*)SharedUiItem::data(); }
};

/** ParamsProvider wrapper for pseudo params. */
class LIBQRONSHARED_EXPORT AlertPseudoParamsProvider : public ParamsProvider {
  Alert _alert;

public:
  inline AlertPseudoParamsProvider(Alert alert) : _alert(alert) { }
  QVariant paramValue(QString key, QVariant defaultValue = QVariant(),
                      QSet<QString> alreadyEvaluated = QSet<QString>()) const;
};

inline AlertPseudoParamsProvider Alert::pseudoParams() const {
  return AlertPseudoParamsProvider(*this);
}

#endif // ALERT_H
