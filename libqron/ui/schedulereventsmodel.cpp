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
#include "schedulereventsmodel.h"

#define COLUMNS 7

SchedulerEventsModel::SchedulerEventsModel(QObject *parent)
  : QAbstractTableModel(parent) {
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
      return EventSubscription::toStringList(_onschedulerstart).join("\n");
    case 1:
      return EventSubscription::toStringList(_onconfigload).join("\n");
    case 2:
      return EventSubscription::toStringList(_onnotice).join("\n");
    case 3:
      return EventSubscription::toStringList(_onlog).join("\n");
    case 4:
      return EventSubscription::toStringList(_onstart).join("\n");
    case 5:
      return EventSubscription::toStringList(_onsuccess).join("\n");
    case 6:
      return EventSubscription::toStringList(_onfailure).join("\n");
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

void SchedulerEventsModel::configChanged(SchedulerConfig config) {
  _onstart = config.onstart();
  _onsuccess = config.onsuccess();
  _onfailure = config.onfailure();
  _onlog = config.onlog();
  _onnotice = config.onnotice();
  _onschedulerstart = config.onschedulerstart();
  _onconfigload = config.onconfigload();
  emit dataChanged(createIndex(0, 0), createIndex(0, COLUMNS-1));
}
