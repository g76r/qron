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
#ifndef CLUSTERSMODEL_H
#define CLUSTERSMODEL_H

#include <QAbstractTableModel>
#include "config/cluster.h"
#include "config/host.h"
#include "config/schedulerconfig.h"
#include "modelview/shareduiitemstreemodel.h"

/** Model holding list of configured clusters, one per root row, along with its
 * configuration attributes, with associated hosts as children. */
class LIBQRONSHARED_EXPORT ClustersModel : public SharedUiItemsTreeModel {
  Q_OBJECT
  Q_DISABLE_COPY(ClustersModel)

public:
  explicit ClustersModel(QObject *parent = 0);
  bool canDropMimeData(
      const QMimeData *data, Qt::DropAction action, int targetRow,
      int targetColumn, const QModelIndex &targetParent) const override;
  bool dropMimeData(
      const QMimeData *data, Qt::DropAction action, int targetRow,
      int targetColumn, const QModelIndex &targetParent) override;
  void changeItem(SharedUiItem newItem, SharedUiItem oldItem,
                  QString idQualifier) override;

protected:
  void determineItemPlaceInTree(SharedUiItem newItem, QModelIndex *parent,
                                int *row) override;
};

#endif // CLUSTERSMODEL_H
