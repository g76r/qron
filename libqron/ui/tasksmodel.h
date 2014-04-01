/* Copyright 2013-2014 Hallowyn and others.
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
#ifndef TASKSMODEL_H
#define TASKSMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QHash>
#include "config/task.h"
#include "config/taskgroup.h"
#include "sched/taskinstance.h"
#include "config/schedulerconfig.h"

/** Model holding tasks along with their attributes, one task per line, in
 * fqtn alphabetical order. */
class LIBQRONSHARED_EXPORT TasksModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(TasksModel)
  QList<Task> _tasks;
  QString _customActions;

public:
  explicit TasksModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  /** Way to add custom html at the end of "Actions" column. Will be evaluated
   * through Task.params(). */
  void setCustomActions(QString customActions) {
    _customActions = customActions; }

public slots:
  void configChanged(SchedulerConfig config);
  void taskChanged(Task task);

private slots:
  void forceTimeRelatedDataRefresh();
};

#endif // TASKSMODEL_H
