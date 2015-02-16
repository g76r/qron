/* Copyright 2014-2015 Hallowyn and others.
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
#ifndef STEPSMODEL_H
#define STEPSMODEL_H

#include "modelview/shareduiitemstablemodel.h"
#include "config/task.h"
#include "config/taskgroup.h"
#include "config/step.h"
#include "config/schedulerconfig.h"

/** Model holding steps along with their attributes, one step per line, in
 * step id alphabetical order. */
class LIBQRONSHARED_EXPORT StepsModel : public SharedUiItemsTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(StepsModel)

public:
  explicit StepsModel(QObject *parent = 0);

public slots:
  void configChanged(SchedulerConfig config);
};

#endif // STEPSMODEL_H
