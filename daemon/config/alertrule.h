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
#ifndef ALERTRULE_H
#define ALERTRULE_H

#include <QSharedDataPointer>
#include <QPointer>
#include <QString>
#include <QRegExp>

class AlertRuleData;
class AlertChannel;
class PfNode;
class Alert;

/** Alert rule is the configuration object defining the matching between an
 * alert id pattern and a alert channel, with optional additional parameters.
 * @see Alerter */
class AlertRule {
  QSharedDataPointer<AlertRuleData> d;

public:
  AlertRule();
  AlertRule(const AlertRule &);
  AlertRule(PfNode node, QString pattern, AlertChannel *channel,
            QString channelName, bool stop, bool notifyCancel,
            bool notifyReminder);
  AlertRule &operator=(const AlertRule &);
  ~AlertRule();
  QString pattern() const;
  QRegExp patternRegExp() const;
  /** Null if stop rule or being shuting down and channel already destroyed. */
  QPointer<AlertChannel> channel() const;
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
};

#endif // ALERTRULE_H
