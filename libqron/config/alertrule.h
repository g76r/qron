/* Copyright 2012-2014 Hallowyn and others.
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

#include "libqron_global.h"
#include <QSharedDataPointer>
#include <QPointer>
#include <QString>
#include <QRegExp>
#include "util/paramset.h"

class AlertRuleData;
class PfNode;
class Alert;

/** Alert rule is the configuration object defining the matching between an
 * alert id pattern and a alert channel, with optional additional parameters.
 * @see Alerter */
class LIBQRONSHARED_EXPORT AlertRule {
  QSharedDataPointer<AlertRuleData> d;

public:
  AlertRule();
  AlertRule(const AlertRule &);
  /** parses this form in Pf:
   * (rule
   *  (param foo bar)
   *  (match **)
   *  (log
   *   (param baz baz)
   *   (address debug)
   *   (emitmessage foo)
   *  )
   * )
   * Where rulenode is "rule" node and rulechannelnode is "log" node.
   * This is needed because input config format accepts several rulechannelnode
   * per rulenode, in this case config reader will create several AlertRule
   * objects.
   */
  AlertRule(PfNode rulenode, PfNode rulechannelnode, ParamSet parentParams);
  AlertRule &operator=(const AlertRule &);
  ~AlertRule();
  QString pattern() const;
  QRegExp patternRegExp() const;
  QString channelName() const;
  QString rawAddress() const;
  QString address(Alert alert) const;
  QString emitMessage(Alert alert) const;
  QString cancelMessage(Alert alert) const;
  QString reminderMessage(Alert alert) const;
  /** human readable description such as "emitmessage=foo remindermessage=bar"
   * or "" or "cancelmessage=baz" */
  QString messagesDescriptions() const;
  bool notifyEmit() const;
  bool notifyCancel() const;
  bool notifyReminder() const;
  bool isNull() const;
  ParamSet params() const;
  PfNode toPfNode() const;
};

#endif // ALERTRULE_H
