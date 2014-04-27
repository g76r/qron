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
#ifndef HOSTSMODEL_H
#define HOSTSMODEL_H

#include <QAbstractTableModel>
#include "config/cluster.h"
#include "config/host.h"
#include "config/schedulerconfig.h"
#include "modelview/shareduiitemstablemodel.h"

/** Model holding list of configured hosts, one per line, along with its
 * configuration attributes. */
class LIBQRONSHARED_EXPORT HostsModel : public SharedUiItemsTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(HostsModel)

public:
  explicit HostsModel(QObject *parent = 0);

public slots:
  void configReset(SchedulerConfig config);
};

#endif // HOSTSMODEL_H
