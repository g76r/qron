/* Copyright 2013-2015 Hallowyn and others.
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
#ifndef TASKINSTANCESMODEL_H
#define TASKINSTANCESMODEL_H

#include <QAbstractTableModel>
#include <QString>
#include <QDateTime>
#include "sched/taskinstance.h"
#include "config/host.h"
#include <QList>
#include "modelview/shareduiitemstablemodel.h"

/** Model holding tasks instances along with their attributes, one instance per
 * line, in reverse request order. */
class LIBQRONSHARED_EXPORT TaskInstancesModel : public SharedUiItemsTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(TaskInstancesModel)
  QList<TaskInstance> _instances;
  bool _keepFinished;
  QString _customActions;

public:
  explicit TaskInstancesModel(QObject *parent = 0, int maxrows = 100,
                             bool keepFinished = true);
  QVariant data(const QModelIndex &index, int role) const override;
  /** Way to add custom html at the end of "Actions" column. Will be evaluated
   * through TaskInstance.params(). */
  void setCustomActions(QString customActions) {
    _customActions = customActions; }
  void changeItem(SharedUiItem newItem, SharedUiItem oldItem,
                  QString idQualifier) override;
};

#endif // TASKINSTANCESMODEL_H
