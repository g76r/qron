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
#include "alertrule.h"
#include <QSharedData>
#include "alert/alertchannel.h"
#include "pf/pfnode.h"
#include "util/paramset.h"
#include "alert/alert.h"
#include "configutils.h"

class AlertRuleData : public QSharedData {
public:
  QString _channelName, _pattern;
  QRegExp _patterRegExp;
  QString _address, _emitMessage, _cancelMessage, _reminderMessage;
  bool _notifyEmit, _notifyCancel, _notifyReminder;
  ParamSet _params;
  // FIXME delays
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

AlertRule::AlertRule(PfNode rulenode, PfNode rulechannelnode,
                     ParamSet parentParams)
  : d(new AlertRuleData) {
  d->_channelName = rulechannelnode.name();
  d->_pattern = rulenode.attribute("match", "**");
  d->_patterRegExp = ConfigUtils::readDotHierarchicalFilter(d->_pattern);
  if (d->_pattern.isEmpty() || !d->_patterRegExp.isValid())
    Log::warning() << "unsupported alert rule match pattern '" << d->_pattern
                   << "': " << rulenode.toString();
  d->_address = rulechannelnode.attribute("address"); // LATER check uniqueness
  d->_emitMessage = rulechannelnode.attribute("emitmessage"); // LATER check uniqueness
  d->_cancelMessage = rulechannelnode.attribute("cancelmessage"); // LATER check uniqueness
  d->_reminderMessage = rulechannelnode.attribute("remindermessage"); // LATER check uniqueness
  d->_notifyEmit = !rulechannelnode.hasChild("noemitnotify");
  d->_notifyCancel = !rulechannelnode.hasChild("nocancelnotify");
  d->_notifyReminder = d->_notifyEmit && !rulechannelnode.hasChild("noremindernotify");
  ConfigUtils::loadParamSet(rulenode, &d->_params, "param");
  ConfigUtils::loadParamSet(rulechannelnode, &d->_params, "param");
  // FIXME delays
  d->_params.setParent(parentParams);
}

PfNode AlertRule::toPfNode() const {
  if (!d)
    return PfNode();
  PfNode rulenode("rule");
  rulenode.appendChild(PfNode("match", d->_pattern));
  PfNode node(d->_channelName);
  if (!d->_address.isEmpty())
  node.appendChild(PfNode("address", d->_address));
  if (!d->_emitMessage.isEmpty())
    node.appendChild(PfNode("emitmessage", d->_emitMessage));
  if (!d->_cancelMessage.isEmpty())
    node.appendChild(PfNode("cancelmessage", d->_cancelMessage));
  if (!d->_reminderMessage.isEmpty())
    node.appendChild(PfNode("remindermessage", d->_reminderMessage));
  if (!d->_notifyEmit)
    node.appendChild(PfNode("noemitnotify"));
  if (!d->_notifyCancel)
    node.appendChild(PfNode("nocancelnotify"));
  if (!d->_notifyReminder)
    node.appendChild(PfNode("noremindernotify"));
  ConfigUtils::writeParamSet(&node, d->_params, "param");
  // FIXME delays
  rulenode.appendChild(node);
  return rulenode;
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

QString AlertRule::rawAddress() const {
  return d ? d->_address : QString();
}

QString AlertRule::address(Alert alert) const {
  QString rawAddress = d ? d->_address : QString();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawAddress, &ppp);
}

QString AlertRule::emitMessage(Alert alert) const {
  QString rawMessage = d ? d->_emitMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert emited: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

QString AlertRule::cancelMessage(Alert alert) const {
  QString rawMessage = d ? d->_cancelMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert canceled: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

QString AlertRule::reminderMessage(Alert alert) const {
  QString rawMessage = d ? d->_reminderMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert still raised: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

QString AlertRule::messagesDescriptions() const {
  if (!d)
    return QString();
  QString desc, msg;
  msg = d->_emitMessage;
  if (!msg.isEmpty())
    desc.append("emitmessage=").append(msg).append(' ');
  msg = d->_cancelMessage;
  if (!msg.isEmpty())
    desc.append("cancelmessage=").append(msg).append(' ');
  msg = d->_reminderMessage;
  if (!msg.isEmpty())
    desc.append("remindermessage=").append(msg).append(' ');
  if (desc.size())
    desc.chop(1);
  // FIXME delays ?
  return desc;
}

bool AlertRule::notifyEmit() const {
  return d ? d->_notifyEmit : false;
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

ParamSet AlertRule::params() const {
  return d ? d->_params : ParamSet();
}

// FIXME delays
