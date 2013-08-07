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
#ifndef ALERTRULE_H
#define ALERTRULE_H

#include <QSharedDataPointer>
#include <QWeakPointer>
#include <QString>
#include <QRegExp>

class AlertRuleData;
class AlertChannel;
class PfNode;
class Alert;

class AlertRule {
  QSharedDataPointer<AlertRuleData> d;

public:
  AlertRule();
  AlertRule(const AlertRule &);
  AlertRule(const PfNode node, const QString pattern,
            AlertChannel *channel, QString channelName, bool stop,
            bool notifyCancel, bool notifyReminder);
  AlertRule &operator=(const AlertRule &);
  ~AlertRule();
  QString pattern() const;
  QRegExp patternRegExp() const;
  /** Null if stop rule or being shuting down and channel already destroyed. */
  QWeakPointer<AlertChannel> channel() const;
  QString channelName() const;
  QString address() const;
  QString emitMessage(Alert alert) const;
  QString cancelMessage(Alert alert) const;
  QString reminderMessage(Alert alert) const;
  QString rawMessage() const;
  QString rawCancelMessage() const;
  bool stop() const;
  bool notifyCancel() const;
  bool notifyReminder() const;
  bool isNull() const;
  /** Convert patterns like "some.path.**" "some.*.path" "some.**.path" or
   * "some.path.with.\*.star.and.\\.backslash" into regular expressions.
   */
  static QRegExp compilePattern(const QString pattern);
};

#endif // ALERTRULE_H
