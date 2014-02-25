/* Copyright 2012-2014 Hallowyn and others.
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
#ifndef HOSTSLISTMODEL_H
#define HOSTSLISTMODEL_H

#include <QAbstractTableModel>
#include "config/cluster.h"
#include "config/host.h"
#include "config/schedulerconfig.h"

// TODO rename to HostsModel
/** Model holding list of configured hosts, one per line, along with its
 * configuration attributes. */
class HostsListModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(HostsListModel)
  QList<Host> _hosts;

public:
  explicit HostsListModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void configChanged(SchedulerConfig config);
};

#endif // HOSTSLISTMODEL_H
