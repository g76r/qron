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
#include "schedulereventsmodel.h"

#define COLUMNS 7

SchedulerEventsModel::SchedulerEventsModel(QObject *parent)
  : QAbstractListModel(parent) {
}

int SchedulerEventsModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : 1;
}

int SchedulerEventsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant SchedulerEventsModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::DisplayRole && index.isValid())
    switch(index.column()) {
    case 0:
      return Event::toStringList(_onschedulerstart).join(" ");
    case 1:
      return Event::toStringList(_onconfigload).join(" ");
    case 2:
      return Event::toStringList(_onnotice).join(" ");
    case 3:
      return Event::toStringList(_onlog).join(" ");
    case 4:
      return Event::toStringList(_onstart).join(" ");
    case 5:
      return Event::toStringList(_onsuccess).join(" ");
    case 6:
      return Event::toStringList(_onfailure).join(" ");
    }
  return QVariant();
}

QVariant SchedulerEventsModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    switch(section) {
    case 0:
      return "On scheduler start";
    case 1:
      return "On config load";
    case 2:
      return "On notice";
    case 3:
      return "On log";
    case 4:
      return "On start";
    case 5:
      return "On success";
    case 6:
      return "On failure";
    }
  return QVariant();
}

void SchedulerEventsModel::eventsConfigurationReset(
    QList<Event> onstart, QList<Event> onsuccess, QList<Event> onfailure,
    QList<Event> onlog, QList<Event> onnotice, QList<Event> onschedulerstart,
    QList<Event> onconfigload) {
  _onstart = onstart;
  _onsuccess = onsuccess;
  _onfailure = onfailure;
  _onlog = onlog;
  _onnotice = onnotice;
  _onschedulerstart = onschedulerstart;
  _onconfigload = onconfigload;
  emit dataChanged(createIndex(0, 0), createIndex(0, COLUMNS-1));
}
