/* Copyright 2014 Hallowyn and others.
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

#include <QAbstractTableModel>
#include "config/task.h"
#include "config/taskgroup.h"
#include "config/step.h"
#include "config/schedulerconfig.h"

/** Model holding steps along with their attributes, one step per line, in
 * fqsn alphabetical order. */
class LIBQRONSHARED_EXPORT StepsModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(StepsModel)
  class StepWrapper;
  QList<StepWrapper> _steps;

public:
  explicit StepsModel(QObject *parent = 0);
  ~StepsModel();
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void configChanged(SchedulerConfig config);
};

#endif // STEPSMODEL_H
