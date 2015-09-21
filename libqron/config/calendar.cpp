/* Copyright 2013-2015 Hallowyn and others.
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
#include <QList>
#include <QDate>
#include <QStringList>
#include "log/log.h"
#include "configutils.h"
#include <QAtomicInteger>

static QAtomicInteger<qint32> sequence = 1;

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Name",
  "Date Rules"
};

class CalendarData : public SharedUiItemData {
public:
  struct Rule {
    QDate _begin; // QDate() means -inf
    QDate _end; // QDate() means +inf
    bool _include;
    Rule(QDate begin, QDate end, bool include)
      : _begin(begin), _end(end), _include(include) { }
    bool isIncludeAll() const {
      return _include && _begin.isNull() && _end.isNull(); }
    bool isExcludeAll() const {
      return !_include && _begin.isNull() && _end.isNull(); }
  };
  QString _id;
  QString _name;
  QList<Rule> _rules;
  QStringList _commentsList;
  CalendarData(QString name = QString())
    : _id(QString::number(sequence.fetchAndAddOrdered(1))),
      _name(name.isEmpty() ? QString() : name) { }
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("calendar"); }
  int uiSectionCount() const {
    return sizeof _uiHeaderNames / sizeof *_uiHeaderNames; }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const {
    return role == Qt::DisplayRole && section >= 0
        && (unsigned)section < sizeof _uiHeaderNames
        ? _uiHeaderNames[section] : QVariant();
  }
  QString toCommaSeparatedRulesString() const;
  inline CalendarData &append(QDate begin, QDate end, bool include);
};

Q_DECLARE_TYPEINFO(CalendarData::Rule, Q_MOVABLE_TYPE);

CalendarData &CalendarData::append(QDate begin, QDate end, bool include) {
  _rules.append(Rule(begin, end, include));
  return *this;
}

static QRegExp reDate("(([0-9]+)-([0-9]+)-([0-9]+))?"
                      "(..)?(([0-9]+)-([0-9]+)-([0-9]+))?");

Calendar::Calendar(PfNode node) : SharedUiItem() {
  CalendarData *d = new CalendarData(node.contentAsString());
  bool atLessOneExclude = false;
  //qDebug() << "*** Calendar(PfNode): " << node.toPf();
  foreach(PfNode child, node.children()) {
    if (child.isComment())
      continue;
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
      d->append(QDate(), QDate(), include);
    } else
      foreach (QString s, dates) {
        QRegExp re(reDate);
        if (re.exactMatch(s)) {
          QDate begin(re.cap(2).toInt(), re.cap(3).toInt(), re.cap(4).toInt());
          QDate end(re.cap(7).toInt(), re.cap(8).toInt(), re.cap(9).toInt());
          //qDebug() << "calendar date spec:" << begin << ".." << end;
          d->append(begin, re.cap(5).isEmpty() ? begin : end, include);
        } else {
          Log::error() << "incorrect calendar date specification: "
                       << node.toPf();
        }
      }
  }
  if (!d->_rules.isEmpty() && !d->_rules.last().isIncludeAll()
      && !atLessOneExclude) {
    // if calendar only declares "include" rules, append a final "exclude" rule
    d->append(QDate(), QDate(), false);
  }
  ConfigUtils::loadComments(node, &d->_commentsList);
  setData(d);
}

Calendar &Calendar::append(QDate begin, QDate end, bool include) {
  CalendarData *d = data();
  d->append(begin, end, include);
  return *this;
}

bool Calendar::isIncluded(QDate date) const {
  const CalendarData *d = data();
  if (!d)
    return false;
  foreach (const CalendarData::Rule &r, d->_rules) {
    if ((!r._begin.isValid() || r._begin <= date)
        && (!r._end.isValid() || r._end <= date))
      return r._include;
  }
  return d->_rules.isEmpty();
}

PfNode Calendar::toPfNode(bool useNameOnlyIfSet) const {
  const CalendarData *d = data();
  if (!d)
    return PfNode();
  PfNode node("calendar", d->_name);
  if (!d->_name.isEmpty() && useNameOnlyIfSet)
    return node;
  ConfigUtils::writeComments(&node, d->_commentsList);
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
  const CalendarData *d = data();
  return d ? d->toCommaSeparatedRulesString() : QString();
}

QString CalendarData::toCommaSeparatedRulesString() const {
  QString s;
  foreach (const CalendarData::Rule &r, _rules) {
    s.append(r._include ? "include" : "exclude");
    if (!r._begin.isNull() || !r._end.isNull())
      s.append(" ");
    if (!r._begin.isNull())
      s.append(r._begin.toString(QStringLiteral("yyyy-MM-dd")));
    if (r._begin != r._end) {
      s.append("..");
      if (!r._end.isNull())
        s.append(r._end.toString(QStringLiteral("yyyy-MM-dd")));
    }
    s.append(", ");
  }
  if (_rules.isEmpty())
    return QStringLiteral("include");
  else {
    s.chop(2);
    return s;
  }
}

QString Calendar::name() const {
  const CalendarData *d = data();
  return d ? d->_name : QString();
}

CalendarData *Calendar::data() {
  detach<CalendarData>();
  return (CalendarData*)SharedUiItem::data();
}

QVariant CalendarData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
  case SharedUiItem::ExternalDataRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _name;
    case 2:
      return toCommaSeparatedRulesString();
    default:
      ;
    }
  }
  return QVariant();
}
