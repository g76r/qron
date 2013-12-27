/* Copyright 2012-2013 Hallowyn and others.
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
#ifndef RESOURCESALLOCATIONMODEL_H
#define RESOURCESALLOCATIONMODEL_H

#include "textview/textmatrixmodel.h"

/** Model holding resources allocation matrix, one resource kind per column and
 * one host per line.
 * Can display either configured qunatities or allocated or free or free /
 * configured or allocated / configured. */
class ResourcesAllocationModel : public TextMatrixModel {
  Q_OBJECT
  Q_DISABLE_COPY(ResourcesAllocationModel)
public:
  enum Mode { Configured, Allocated, Free, FreeOverConfigured,
              AllocatedOverConfigured, LowWaterMark, LwmOverConfigured };
private:
  Mode _mode;
  QHash<QString,QHash<QString,qint64> > _configured, _lwm;

public:
  explicit ResourcesAllocationModel(
      QObject *parent = 0, ResourcesAllocationModel::Mode mode
      = ResourcesAllocationModel::FreeOverConfigured);

public slots:
  // LATER rename misleading signal and slot ("allocation" -> "available")
  void setResourceAllocationForHost(QString host,
                                    QHash<QString,qint64> resources);
  void setResourceConfiguration(
      QHash<QString,QHash<QString,qint64> > resources);
};

#endif // RESOURCESALLOCATIONMODEL_H
