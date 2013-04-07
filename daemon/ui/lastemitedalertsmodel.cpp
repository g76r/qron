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
#include "lastemitedalertsmodel.h"
#include <QRegExp>

LastEmitedAlertsModel::LastEmitedAlertsModel(QObject *parent, int maxsize)
  : LastOccuredTextEventsModel(parent, maxsize) {
}

int LastEmitedAlertsModel::columnCount(const QModelIndex &parent) const {
  return LastOccuredTextEventsModel::columnCount(parent)+1;
}

QVariant LastEmitedAlertsModel::data(const QModelIndex &index, int role) const {
  QVariant v = LastOccuredTextEventsModel::data(index, role);
  if (v.isValid())
    return v;
  if (role == _prefixRole && index.column()
      == LastOccuredTextEventsModel::columnCount(QModelIndex())) {
    v = LastOccuredTextEventsModel::data(this->index(index.row(), 1,
                                                     index.parent()),
                                         Qt::DisplayRole);
    QRegExp re("task\\.[^\\.]+\\.(.*)");
    if (re.exactMatch(v.toString()))
      return " <span class=\"label label-info\" title=\"Related alerts log\">"
          "<a target=\"_blank\" href=\"../rest/txt/log/all/v1?filter=%20"
          +re.cap(1)
          +"/\"><i class=\"icon-th-list icon-white\"></i></a></span>";
  }
  return QVariant();
}

QVariant LastEmitedAlertsModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole &&
      orientation == Qt::Horizontal
      && section == LastOccuredTextEventsModel::columnCount(QModelIndex()))
    return "Actions";
  else
    return LastOccuredTextEventsModel::headerData(section, orientation, role);
}
