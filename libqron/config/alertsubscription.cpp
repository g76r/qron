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
#include "alertsubscription.h"
#include <QSharedData>
#include "alert/alertchannel.h"
#include "pf/pfnode.h"
#include "util/paramset.h"
#include "alert/alert.h"
#include "configutils.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Pattern",
  "Channel",
  "Address",
  "Messages",
  "Options", // 5
  "Emit Message",
  "Cancel Message",
  "Reminder Message",
  "Notify Emit",
  "Notify Cancel", // 10
  "Notify Reminder",
  "Parameters"
};

static QAtomicInt _sequence;

class AlertSubscriptionData : public SharedUiItemData {
public:
  QString _id, _channelName, _pattern;
  QRegularExpression _patternRegexp;
  QString _address, _emitMessage, _cancelMessage, _reminderMessage;
  bool _notifyEmit, _notifyCancel, _notifyReminder;
  ParamSet _params;
  AlertSubscriptionData()
    : _id(QString::number(_sequence.fetchAndAddOrdered(1))), _notifyEmit(false),
      _notifyCancel(false), _notifyReminder(false) {
  }
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("alertsubscription"); }
  int uiSectionCount() const {
    return sizeof _uiHeaderNames / sizeof *_uiHeaderNames; }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const {
    return role == Qt::DisplayRole && section >= 0
        && (unsigned)section < sizeof _uiHeaderNames
        ? _uiHeaderNames[section] : QVariant();
  }
};

AlertSubscription::AlertSubscription() {
}

AlertSubscription::AlertSubscription(const AlertSubscription &other)
  : SharedUiItem(other) {
}

AlertSubscription::AlertSubscription(
    PfNode subscriptionnode, PfNode channelnode, ParamSet parentParams) {
  AlertSubscriptionData *d = new AlertSubscriptionData;
  d->_channelName = channelnode.name();
  d->_pattern = subscriptionnode.attribute(QStringLiteral("pattern"),
                                           QStringLiteral("**"));
  d->_patternRegexp = ConfigUtils::readDotHierarchicalFilter(d->_pattern);
  if (d->_pattern.isEmpty() || !d->_patternRegexp.isValid())
    Log::warning() << "unsupported alert subscription match pattern '"
                   << d->_pattern << "': " << subscriptionnode.toString();
  d->_address = channelnode.attribute("address"); // LATER check uniqueness
  d->_emitMessage = channelnode.attribute("emitmessage"); // LATER check uniqueness
  d->_cancelMessage = channelnode.attribute("cancelmessage"); // LATER check uniqueness
  d->_reminderMessage = channelnode.attribute("remindermessage"); // LATER check uniqueness
  d->_notifyEmit = !channelnode.hasChild("noemitnotify");
  d->_notifyCancel = !channelnode.hasChild("nocancelnotify");
  d->_notifyReminder = d->_notifyEmit && !channelnode.hasChild("noremindernotify");
  ConfigUtils::loadParamSet(subscriptionnode, &d->_params, "param");
  ConfigUtils::loadParamSet(channelnode, &d->_params, "param");
  d->_params.setParent(parentParams);
  setData(d);
}

PfNode AlertSubscription::toPfNode() const {
  const AlertSubscriptionData *d = data();
  if (!d)
    return PfNode();
  PfNode subscriptionNode("subscription");
  subscriptionNode.appendChild(PfNode(QStringLiteral("pattern"), d->_pattern));
  PfNode node(d->_channelName);
  if (!d->_address.isEmpty())
  node.appendChild(PfNode(QStringLiteral("address"), d->_address));
  if (!d->_emitMessage.isEmpty())
    node.appendChild(PfNode(QStringLiteral("emitmessage"), d->_emitMessage));
  if (!d->_cancelMessage.isEmpty())
    node.appendChild(PfNode(QStringLiteral("cancelmessage"),
                            d->_cancelMessage));
  if (!d->_reminderMessage.isEmpty())
    node.appendChild(PfNode(QStringLiteral("remindermessage"),
                            d->_reminderMessage));
  if (!d->_notifyEmit)
    node.appendChild(PfNode(QStringLiteral("noemitnotify")));
  if (!d->_notifyCancel)
    node.appendChild(PfNode(QStringLiteral("nocancelnotify")));
  if (!d->_notifyReminder)
    node.appendChild(PfNode(QStringLiteral("noremindernotify")));
  ConfigUtils::writeParamSet(&node, d->_params, QStringLiteral("param"));
  subscriptionNode.appendChild(node);
  return subscriptionNode;
}

QString AlertSubscription::pattern() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_pattern : QString();
}

QRegularExpression AlertSubscription::patternRegexp() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_patternRegexp : QRegularExpression();
}

QString AlertSubscription::channelName() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_channelName : QString();
}

QString AlertSubscription::rawAddress() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_address : QString();
}

QString AlertSubscription::address(Alert alert) const {
  const AlertSubscriptionData *d = data();
  QString rawAddress = d ? d->_address : QString();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawAddress, &ppp);
}

QString AlertSubscription::emitMessage(Alert alert) const {
  const AlertSubscriptionData *d = data();
  QString rawMessage = d ? d->_emitMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert emited: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

QString AlertSubscription::cancelMessage(Alert alert) const {
  const AlertSubscriptionData *d = data();
  QString rawMessage = d ? d->_cancelMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert canceled: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

QString AlertSubscription::reminderMessage(Alert alert) const {
  const AlertSubscriptionData *d = data();
  QString rawMessage = d ? d->_reminderMessage : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert still raised: "+alert.id();
  AlertPseudoParamsProvider ppp = alert.pseudoParams();
  return d->_params.evaluate(rawMessage, &ppp);
}

bool AlertSubscription::notifyEmit() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_notifyEmit : false;
}

bool AlertSubscription::notifyCancel() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_notifyCancel : false;
}

bool AlertSubscription::notifyReminder() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_notifyReminder : false;
}

ParamSet AlertSubscription::params() const {
  const AlertSubscriptionData *d = data();
  return d ? d->_params : ParamSet();
}

QVariant AlertSubscriptionData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _pattern;
    case 2:
      return _channelName;
    case 3:
      return _address;
    case 4: {
      QString s;
      if (!_emitMessage.isEmpty())
        s.append(" emitmessage=").append(_emitMessage);
      if (!_cancelMessage.isEmpty())
        s.append(" cancelmessage=").append(_cancelMessage);
      if (!_reminderMessage.isEmpty())
        s.append(" remindermessage=").append(_reminderMessage);
      return s.mid(1);
    }
    case 5: {
      QString s;
      if (!_notifyEmit)
        s.append(" noemitnotify");
      if (!_notifyCancel)
        s.append(" nocancelnotify");
      if (!_notifyReminder)
        s.append(" noremindernotify");
      return s.mid(1);
    }
    case 6:
      return _emitMessage;
    case 7:
      return _cancelMessage;
    case 8:
      return _reminderMessage;
    case 9:
      return _notifyEmit;
    case 10:
      return _notifyCancel;
    case 11:
      return _notifyReminder;
    case 12:
      return _params.toString(false, false);
    }
    break;
  default:
    ;
  }
  return QVariant();
}
