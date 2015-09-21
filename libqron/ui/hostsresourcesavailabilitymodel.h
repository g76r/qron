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
#ifndef HOSTSRESOURCESAVAILABILITYMODEL_H
#define HOSTSRESOURCESAVAILABILITYMODEL_H

#include "modelview/textmatrixmodel.h"
#include "config/schedulerconfig.h"

/** Model holding resources allocation matrix, one resource kind per column and
 * one host per line.
 * Can display either configured qunatities or allocated or free or free /
 * configured or allocated / configured. */
class LIBQRONSHARED_EXPORT HostsResourcesAvailabilityModel
    : public TextMatrixModel {
  Q_OBJECT
  Q_DISABLE_COPY(HostsResourcesAvailabilityModel)
public:
  enum Mode { Configured, Allocated, Free, FreeOverConfigured,
              AllocatedOverConfigured, LowWaterMark, LwmOverConfigured };
private:
  Mode _mode;
  QHash<QString,QHash<QString,qint64> > _configured, _lwm;

public:
  explicit HostsResourcesAvailabilityModel(
      QObject *parent = 0, HostsResourcesAvailabilityModel::Mode mode
      = HostsResourcesAvailabilityModel::FreeOverConfigured);
  void changeItem(
      SharedUiItem newItem, SharedUiItem oldItem, QString idQualifier);
  void hostsResourcesAvailabilityChanged(
      QString host, QHash<QString,qint64> resources);
};

#endif // HOSTSRESOURCESAVAILABILITYMODEL_H
