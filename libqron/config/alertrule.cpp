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
#include "alertrule.h"
#include <QSharedData>
#include "alert/alertchannel.h"
#include "pf/pfnode.h"
#include "util/paramset.h"
#include "alert/alert.h"
#include "configutils.h"

class AlertRuleData : public QSharedData {
public:
  QString _pattern;
  QRegExp _patterRegExp;
  QString _address, _emitMessage, _cancelMessage, _reminderMessage,
  _channelName;
  bool _notifyCancel, _notifyReminder;
};

AlertRule::AlertRule() {
}

AlertRule::AlertRule(const AlertRule &rhs) : d(rhs.d) {
}

AlertRule &AlertRule::operator=(const AlertRule &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

AlertRule::~AlertRule() {
}

AlertRule::AlertRule(PfNode node, QString pattern, QString channelName,
                     bool notifyCancel, bool notifyReminder)
  : d(new AlertRuleData) {
  d->_pattern = pattern;
  d->_patterRegExp = ConfigUtils::readDotHierarchicalFilter(pattern);
  d->_channelName = channelName;
  d->_address = node.attribute("address"); // LATER check uniqueness
  d->_emitMessage = node.attribute("emitmessage"); // LATER check uniqueness
  d->_cancelMessage = node.attribute("cancelmessage"); // LATER check uniqueness
  d->_reminderMessage = node.attribute("remindermessage"); // LATER check uniqueness
  d->_notifyCancel = notifyCancel;
  d->_notifyReminder = notifyReminder;
}

QString AlertRule::pattern() const {
  return d ? d->_pattern : QString();
}

QRegExp AlertRule::patternRegExp() const {
  return d ? d->_patterRegExp : QRegExp();
}

QString AlertRule::channelName() const {
  return d ? d->_channelName : QString();
}

QString AlertRule::address() const {
  return d ? d->_address : QString();

}

QString AlertRule::emitMessage(Alert alert) const {
  QString rawMessage = d ? d->_emitMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert emited: "+alert.id();
  return ParamSet().evaluate(rawMessage, &alert);
}

QString AlertRule::cancelMessage(Alert alert) const {
  QString rawMessage = d ? d->_cancelMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert canceled: "+alert.id();
  return ParamSet().evaluate(rawMessage, &alert);
}

QString AlertRule::reminderMessage(Alert alert) const {
  QString rawMessage = d ? d->_reminderMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert still raised: "+alert.id();
  return ParamSet().evaluate(rawMessage, &alert);
}

QString AlertRule::rawMessage() const {
  return d ? d->_emitMessage : QString();
}

QString AlertRule::rawCancelMessage() const {
  return d ? d->_cancelMessage : QString();
}

bool AlertRule::notifyCancel() const {
  return d ? d->_notifyCancel : false;
}

bool AlertRule::notifyReminder() const {
  return d ? d->_notifyReminder : false;
}

bool AlertRule::isNull() const {
  return !d;
}
