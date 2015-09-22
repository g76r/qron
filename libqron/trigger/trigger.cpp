/* Copyright 2013-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#include "trigger_p.h"
#include "config/configutils.h"

static QSet<QString> excludedDescendantsForComments { "calendar" };

Trigger::Trigger() {
}

Trigger::Trigger(const Trigger &rhs) : d(rhs.d) {
}

Trigger::Trigger(TriggerData *data) : d(data) {
}

Trigger &Trigger::operator=(const Trigger &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Trigger::~Trigger() {
}

QString Trigger::expression() const {
  return d ? d->expression() : QString();
}

QString Trigger::canonicalExpression() const {
  return d ? d->canonicalExpression() : QString();
}

QString Trigger::humanReadableExpression() const {
  return d ? d->humanReadableExpression() : QString();
}

QString Trigger::humanReadableExpressionWithCalendar() const {
  return (!calendar().isNull())
      ? '['+QString::fromUtf8(calendar().toPfNode(true)
                              .toPf(PfOptions()
                                    .setShouldWriteContentBeforeSubnodes()))+']'
        +humanReadableExpression()
      : humanReadableExpression();
}

bool Trigger::isValid() const {
  return d && d->isValid();
}

TriggerData::~TriggerData() {
}

QString TriggerData::expression() const {
  return QString();
}

QString TriggerData::canonicalExpression() const {
  return expression();
}

QString TriggerData::humanReadableExpression() const {
  return expression();
}

bool TriggerData::isValid() const {
  return false;
}

//void Trigger::setCalendar(Calendar calendar) {
//  if (d)
//    d->_calendar = calendar;
//}

Calendar Trigger::calendar() const {
  return d ? d->_calendar : Calendar();
}

ParamSet Trigger::overridingParams() const {
  return d ? d->_overridingParams : ParamSet();
}

bool Trigger::loadConfig(PfNode node, QHash<QString,Calendar> namedCalendars) {
  ConfigUtils::loadComments(node, &d->_commentsList,
                            excludedDescendantsForComments);
  QList<PfNode> list = node.childrenByName(QStringLiteral("calendar"));
  if (list.size() > 1)
    Log::error() << "ignoring multiple calendar definition: "
                 << node.toPf();
  else if (list.size() == 1) {
    PfNode child = list.first();
    QString content = child.contentAsString();
    if (!content.isEmpty()) {
      Calendar calendar = namedCalendars.value(content);
      if (calendar.isNull())
        Log::error() << "ignoring undefined calendar '" << content
                     << "': " << child.toPf();
      else {
        d->_calendar = calendar;
        // load comments only for named calendars, since their global definition
        // will be taken instead of child node content
        // in the other hand, non-named calendars are loaded as actual calendars
        // and therefore will load their comments by their own
        ConfigUtils::loadComments(child, &d->_commentsList);
      }
    } else {
      Calendar calendar = Calendar(child);
      if (calendar.isNull())
        Log::error() << "ignoring empty calendar: "
                       << child.toPf();
      else
        d->_calendar = calendar;
    }
  }
  ConfigUtils::loadParamSet(node, &d->_overridingParams,
                            QStringLiteral("param"));
  return true;
}

QString TriggerData::triggerType() const {
  return QStringLiteral("unknown");
}

PfNode TriggerData::toPfNode() const {
  PfNode node(triggerType(), expression());
  ConfigUtils::writeComments(&node, _commentsList);
  ConfigUtils::writeParamSet(&node, _overridingParams, QStringLiteral("param"));
  if (!_calendar.isNull())
    node.appendChild(_calendar.toPfNode(true));
  return node;
}

PfNode Trigger::toPfNode() const {
  return d ? d->toPfNode() : PfNode();
}
