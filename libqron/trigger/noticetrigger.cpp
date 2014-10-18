/* Copyright 2013 Hallowyn and others.
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
#include "noticetrigger.h"
#include "trigger_p.h"

class NoticeTriggerData : public TriggerData {
public:
  QString _notice;
  NoticeTriggerData(QString notice = QString()) : _notice(notice) { }
  QString expression() const { return _notice; }
  QString humanReadableExpression() const { return "^"+_notice; }
  bool isValid() const { return !_notice.isEmpty(); }
  QString triggerType() const { return "notice"; }
};

NoticeTrigger::NoticeTrigger() {
}

NoticeTrigger::NoticeTrigger(PfNode node,
                             QHash<QString,Calendar> namedCalendars)
  : Trigger(new NoticeTriggerData(node.contentAsString())){
  loadConfig(node, namedCalendars);
}


NoticeTrigger::NoticeTrigger(const NoticeTrigger &rhs) : Trigger(rhs) {
}

NoticeTrigger &NoticeTrigger::operator=(const NoticeTrigger &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

NoticeTrigger::~NoticeTrigger() {
}
