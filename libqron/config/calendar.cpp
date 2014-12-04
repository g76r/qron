/* Copyright 2013-2014 Hallowyn and others.
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
#include "calendar.h"
#include <QSharedData>
#include <QList>
#include <QDate>
#include <QStringList>
#include "log/log.h"

class CalendarData : public QSharedData {
public:
  class Rule {
  public:
    QDate _begin; // QDate() means -inf
    QDate _end; // QDate() means +inf
    bool _include;
    Rule(QDate begin, QDate end, bool include)
      : _begin(begin), _end(end), _include(include) { }
  };
  QList<Rule> _rules;
  QString _name;
  CalendarData(QString name = QString())
    : _name(name.isEmpty() ? QString() : name) { }
  inline CalendarData &append(QDate begin, QDate end, bool include);
};

Q_DECLARE_TYPEINFO(CalendarData::Rule, Q_MOVABLE_TYPE);

CalendarData &CalendarData::append(QDate begin, QDate end, bool include) {
  _rules.append(Rule(begin, end, include));
  return *this;
}

Calendar::Calendar() : d(new CalendarData) {
}

Calendar::Calendar(const Calendar &rhs) : d(rhs.d) {
}

Calendar::Calendar(PfNode node)
  : d(new CalendarData(node.contentAsString())) {
  static QRegExp reDate("(([0-9]+)-([0-9]+)-([0-9]+))?"
                        "(..)?(([0-9]+)-([0-9]+)-([0-9]+))?");
  bool atLessOneExclude = false;
  //qDebug() << "*** Calendar(PfNode): " << node.toPf();
  foreach(PfNode child, node.children()) {
    bool include;
    //qDebug() << child.name() << ": " << child.contentAsString();
    if (child.name() == "include")
      include = true;
    else if (child.name() == "exclude") {
      include = false;
      atLessOneExclude = true;
    } else {
      Log::error() << "unsupported calendar rule '" << child.name() << "': "
                   << node.toPf();
      continue;
    }
    QStringList dates = child.contentAsStringList();
    if (dates.isEmpty()) {
      //qDebug() << "calendar date spec empty";
      append(include);
    } else
      foreach (QString s, dates) {
        QRegExp re(reDate);
        if (re.exactMatch(s)) {
          QDate begin(re.cap(2).toInt(), re.cap(3).toInt(), re.cap(4).toInt());
          QDate end(re.cap(7).toInt(), re.cap(8).toInt(), re.cap(9).toInt());
          //qDebug() << "calendar date spec:" << begin << ".." << end;
          if (re.cap(5).isEmpty())
            append(begin, include);
          else
            append(begin, end, include);
        } else {
          Log::error() << "incorrect calendar date specification: "
                       << node.toPf();
        }
      }
  }
  if (isNull()) {
    Log::warning() << "meaningless calendar (every day belongs to it): "
                   << node.toPf();
  } else if (!atLessOneExclude) {
    // if calendar only declares "include" rules, append a final "exclude" rule
    append(false);
  }
}

Calendar::~Calendar() {
}

Calendar &Calendar::operator=(const Calendar &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Calendar &Calendar::append(QDate begin, QDate end, bool include) {
  d->append(begin, end, include);
  return *this;
}

bool Calendar::isIncluded(QDate date) const {
  foreach (const CalendarData::Rule &r, d->_rules) {
    if ((!r._begin.isValid() || r._begin <= date)
        && (!r._end.isValid() || r._end <= date))
      return r._include;
  }
  return d->_rules.isEmpty();
}

bool Calendar::isNull() const {
  return d->_rules.isEmpty();
}

void Calendar::clear() {
  d->_rules.clear();
}

PfNode Calendar::toPfNode(bool useNameOnlyIfSet) const {
  QString name(this->name());
  PfNode node("calendar", name);
  if (!name.isEmpty() && useNameOnlyIfSet)
    return node;
  foreach (const CalendarData::Rule &r, d->_rules) {
    QString s;
    s.append(r._begin.isNull() ? "" : r._begin.toString("yyyy-MM-dd"));
    if (r._begin != r._end)
      s.append("..")
          .append(r._end.isNull() ? "" : r._end.toString("yyyy-MM-dd"));
    node.appendChild(PfNode(r._include ? "include" : "exclude", s));
  }
  return node;
}

QString Calendar::toCommaSeparatedRulesString() const {
  QString s;
  foreach (const CalendarData::Rule &r, d->_rules) {
    s.append(r._include ? "include" : "exclude");
    if (!r._begin.isNull() || !r._end.isNull())
      s.append(" ");
    s.append(r._begin.isNull() ? "" : r._begin.toString("yyyy-MM-dd"));
    if (r._begin != r._end)
      s.append("..")
          .append(r._end.isNull() ? "" : r._end.toString("yyyy-MM-dd"));
    s.append(", ");
  }
  if (!d->_rules.isEmpty())
    s.chop(2);
  return s;
}

QString Calendar::name() const {
  return d->_name;
}
