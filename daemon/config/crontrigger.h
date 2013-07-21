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
#ifndef CRONTRIGGER_H
#define CRONTRIGGER_H

#include <QSharedData>
#include <QString>
#include <QDateTime>
#include <QMetaType>

class CronTriggerData;

class CronTrigger {
  QSharedDataPointer<CronTriggerData> d;

public:
  explicit CronTrigger(const QString cronExpression = QString());
  CronTrigger(const CronTrigger &other);
  ~CronTrigger();
  CronTrigger &operator=(const CronTrigger &other);
  /** Cron expression as it was initialy given */
  QString cronExpression() const;
  /** Cron expression in a canonical/unique form */
  QString canonicalCronExpression() const;
  /** Cron expression is valid (hence not null or empty). */
  bool isValid() const;
  /** Return next triggering date, from cache or after computation if needed */
  QDateTime nextTriggering(QDateTime max) const;
  /** Set last triggered and compute next triggering date. */
  QDateTime nextTriggering(QDateTime lastTriggered, QDateTime max) const {
    setLastTriggered(lastTriggered);
    return nextTriggering(max); }
  /** Syntaxic sugar with max = lastTrigger + 10 years */
  QDateTime nextTriggering() const {
    return nextTriggering(QDateTime::currentDateTime().addYears(10)); }
  /** Syntaxic sugar returning msecs from current time
   * @return -1 if not available within 10 years */
  int nextTriggeringMsecs(QDateTime lastTriggered) const {
    setLastTriggered(lastTriggered);
    return nextTriggeringMsecs(); }
  /** Syntaxic sugar returning msecs from current time
   * @return -1 if not available within 10 years */
  int nextTriggeringMsecs() const;
  QDateTime lastTriggered() const;
  void setLastTriggered(const QDateTime lastTriggered) const;
  void clearLastTriggered() const { setLastTriggered(QDateTime()); }
};

Q_DECLARE_METATYPE(CronTrigger)

#endif // CRONTRIGGER_H
