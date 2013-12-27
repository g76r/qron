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
#ifndef RESOURCESCONSUMPTIONMODEL_H
#define RESOURCESCONSUMPTIONMODEL_H

#include "textview/textmatrixmodel.h"
#include <QHash>
#include <QList>
#include <QString>
#include "config/taskgroup.h"
#include "config/task.h"
#include "config/host.h"
#include "config/cluster.h"

/** Model holding maximum possible resources consumption per host, one host per
 * column and one task per line, plus total line on first line. */
class ResourcesConsumptionModel : public TextMatrixModel {
  Q_OBJECT
  Q_DISABLE_COPY(ResourcesConsumptionModel)
  QList<Task> _tasks;
  QHash<QString,Cluster> _clusters;
  QList<Host> _hosts;
  QHash<QString,QHash<QString,qint64> > _resources;

public:
  ResourcesConsumptionModel(QObject *parent = 0);

public slots:
  void tasksConfigurationReset(QHash<QString,TaskGroup> tasksGroups,
                               QHash<QString,Task> tasks);
  void targetsConfigurationReset(QHash<QString,Cluster> clusters,
                                 QHash<QString,Host> hosts);
  void hostResourceConfigurationChanged(
      QHash<QString,QHash<QString,qint64> > resources);
  void configReloaded();
};

#endif // RESOURCESCONSUMPTIONMODEL_H
