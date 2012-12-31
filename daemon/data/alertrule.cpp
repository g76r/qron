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
#include "alertrule.h"
#include <QSharedData>
#include "alert/alertchannel.h"
#include "pf/pfnode.h"
#include "data/paramset.h"
#include "alert.h"

class AlertRuleData : public QSharedData {
public:
  QString _pattern;
  QRegExp _patterRegExp;
  QWeakPointer<AlertChannel> _channel;
  QString _address, _message;
  bool _stop;
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

AlertRule::AlertRule(const PfNode node, const QString pattern,
                     QWeakPointer<AlertChannel> channel, bool stop)
  : d(new AlertRuleData) {
  d->_pattern = pattern;
  d->_patterRegExp = compilePattern(pattern);
  d->_channel = channel;
  d->_address = node.attribute("address"); // LATER check uniqueness
  d->_message = node.attribute("message"); // LATER check uniqueness
  d->_stop = stop;
}

QRegExp AlertRule::compilePattern(const QString pattern) {
  QString re;
  for (int i = 0; i < pattern.size(); ++i) {
    QChar c = pattern.at(i);
    switch (c.toAscii()) {
    case '*':
      if (i >= pattern.size()-1 || pattern.at(i+1) != '*')
        re.append("[^.]*");
      else {
        re.append(".*");
        ++i;
      }
      break;
    case '\\':
      if (i < pattern.size()-1) {
        c = pattern.at(++i);
        switch (c.toAscii()) {
        case '*':
        case '\\':
          re.append('\\').append(c);
          break;
        default:
          re.append("\\\\").append(c);
        }
      }
      break;
    case '.':
    case '[':
    case ']':
    case '(':
    case ')':
    case '+':
    case '?':
    case '^':
    case '$':
    case 0: // actual 0 or non-ascii
      // TODO fix regexp conversion, it is erroneous with some special chars
      re.append('\\').append(c);
      break;
    default:
      re.append(c);
    }
  }
  return QRegExp(re);
}

QString AlertRule::pattern() const {
  return d ? d->_pattern : QString();
}

QRegExp AlertRule::patternRegExp() const {
  return d ? d->_patterRegExp : QRegExp();
}

QWeakPointer<AlertChannel> AlertRule::channel() const {
  return d ? d->_channel : QWeakPointer<AlertChannel>();
}

QString AlertRule::address() const {
  return d ? d->_address : QString();

}

QString AlertRule::message(Alert alert) const {
  QString rawMessage = d ? d->_message : QString();
  if (rawMessage.isEmpty())
    rawMessage = "alert "+alert.id();
  // LATER give alert context to evaluation method
  return ParamSet().evaluate(rawMessage);
}

bool AlertRule::stop() const {
  return d ? d->_stop : false;
}

bool AlertRule::isNull() const {
  return !d;
}
