/* Copyright 2012 Hallowyn and others.
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
#include "textviews.h"

class ResourcesAllocationModel : public TextMatrixModel {
  Q_OBJECT
public:
  enum Mode { Configured, Allocated, Free, FreeOverConfigured,
              AllocatedOverConfigured };
private:
  Mode _mode;
  QMap<QString,QMap<QString,qint64> > _configured;

public:
  explicit ResourcesAllocationModel(
      QObject *parent = 0, ResourcesAllocationModel::Mode mode
      = ResourcesAllocationModel::FreeOverConfigured);
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void setResourceAllocationForHost(QString host,
                                    QMap<QString,qint64> resources);
  void setResourceConfiguration(QMap<QString,QMap<QString,qint64> > resources);
};

#endif // RESOURCESALLOCATIONMODEL_H
