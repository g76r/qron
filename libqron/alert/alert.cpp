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
#include "alert.h"
#include "config/alertrule.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Status",
  "Rise Date",
  "Due Date",
  "Visibility Date",
  "Cancellation Date", // 5
  "Actions"
};

class AlertData : public SharedUiItemData {
public:
  QString _id;
  Alert::AlertStatus _status;
  QDateTime _riseDate, _dueDate, _lastReminderDate;
  AlertRule _rule;
  AlertData(const QString id = QString(),
            QDateTime riseDate = QDateTime::currentDateTime())
    : _id(id), _status(Alert::Nonexistent), _riseDate(riseDate ) { }
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("alert"); }
  int uiSectionCount() const;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
};

int AlertData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

QVariant AlertData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

QVariant AlertData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return Alert::statusToString(_status);
    case 2:
      return _riseDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"));
    case 3:
      return _dueDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"));
    case 4:
      return _status == Alert::Rising
          ? _dueDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"))
          : QVariant();
    case 5:
      return _status == Alert::MayRise || _status == Alert::Dropping
          ? _dueDate.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"))
          : QVariant();
    case 6:
      break; // actions
    }
    break;
  default:
    ;
  }
  return QVariant();
}

Alert::Alert() : SharedUiItem() {
}

Alert::Alert(QString id, QDateTime datetime)
  : SharedUiItem(new AlertData(id, datetime)) {
}

Alert::Alert(const Alert &other) : SharedUiItem(other) {
}

QDateTime Alert::riseDate() const {
  const AlertData *d = data();
  return d ? d->_riseDate : QDateTime();
}

QDateTime Alert::dueDate() const {
  const AlertData *d = data();
  return d ? d->_dueDate : QDateTime();
}

void Alert::setDueDate(QDateTime dueDate) {
  AlertData *d = data();
  if (d)
    d->_dueDate = dueDate;
}

Alert::AlertStatus Alert::status() const {
  const AlertData *d = data();
  return d ? d->_status : Alert::Nonexistent;
}

void Alert::setStatus(Alert::AlertStatus status) {
  AlertData *d = data();
  if (d)
    d->_status = status;
}

QDateTime Alert::lastRemindedDate() const {
  const AlertData *d = data();
  return d ? d->_lastReminderDate : QDateTime();
}

void Alert::setLastRemindedDate(QDateTime lastRemindedDate) {
  AlertData *d = data();
  if (d)
    d->_lastReminderDate = lastRemindedDate;
}

AlertRule Alert::rule() const {
  const AlertData *d = data();
  return d ? d->_rule : AlertRule();
}

void Alert::setRule(AlertRule rule) {
  AlertData *d = data();
  if (d)
    d->_rule = rule;
}

AlertData *Alert::data() {
  detach<AlertData>();
  return (AlertData*)SharedUiItem::data();
}

static QString nonexistentStatus("nonexistent");
static QString risingStatus("rising");
static QString mayRiseStatus("may_rise");
static QString raisedStatus("raised");
static QString droppingStatus("dropping");
static QString canceledStatus("canceled");

Alert::AlertStatus Alert::statusFromString(QString string) {
  if (string == risingStatus)
    return Alert::Rising;
  if (string == mayRiseStatus)
    return Alert::MayRise;
  if (string == raisedStatus)
    return Alert::Raised;
  if (string == droppingStatus)
    return Alert::Dropping;
  if (string == canceledStatus)
    return Alert::Canceled;
  return Alert::Nonexistent;
}

QString Alert::statusToString(Alert::AlertStatus status) {
  switch(status) {
  case Alert::Rising:
    return risingStatus;
  case Alert::MayRise:
    return mayRiseStatus;
  case Alert::Raised:
    return raisedStatus;
  case Alert::Dropping:
    return droppingStatus;
  case Alert::Canceled:
    return canceledStatus;
  case Alert::Nonexistent:
    ;
  }
  return nonexistentStatus;
}

QVariant AlertPseudoParamsProvider::paramValue(
    QString key, QVariant defaultValue, QSet<QString> alreadyEvaluated) const {
  Q_UNUSED(alreadyEvaluated)
  if (key.at(0) == '!') {
    if (key == "!alertid") {
      return _alert.id();
    } else if (key == "!alertdate") {
      // FIXME make this support !date formating
      return _alert.riseDate().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"));
    }
    // MAYDO guess !taskid from "task.{failure,toolong...}.%!taskid" alerts
  }
  return _alert.rule().params().paramValue(key, defaultValue, alreadyEvaluated);
}

