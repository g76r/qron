/* Copyright 2012-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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

#include "modelview/textmatrixmodel.h"
#include <QHash>
#include <QList>
#include <QString>
#include "config/taskgroup.h"
#include "config/task.h"
#include "config/host.h"
#include "config/cluster.h"
#include "config/schedulerconfig.h"

/** Model holding maximum possible resources consumption per host, one host per
 * column and one task per line, plus total line on first line.
 * Hosts and tasks are sorted in alphabetical order of their ids. */
class LIBQRONSHARED_EXPORT ResourcesConsumptionModel : public TextMatrixModel {
  Q_OBJECT
  Q_DISABLE_COPY(ResourcesConsumptionModel)

public:
  ResourcesConsumptionModel(QObject *parent = 0);

public slots:
  void configActivated(SchedulerConfig config);
};

#endif // RESOURCESCONSUMPTIONMODEL_H
