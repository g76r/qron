/* Copyright 2014 Hallowyn and others.
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
#ifndef CONFIGMODEL_H
#define CONFIGMODEL_H

#include "modelview/shareduiitemstablemodel.h"
#include "config/schedulerconfig.h"

/** Model holding configs, one config per line, in insertion reverse order. */
class LIBQRONSHARED_EXPORT ConfigsModel : public SharedUiItemsTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(ConfigsModel)
  QString _activeConfigId;

public:
  explicit ConfigsModel(QObject *parent = 0);
  QVariant data(const QModelIndex &index, int role) const override;

public slots:
  void configAdded(QString id, SchedulerConfig config);
  void configRemoved(QString id);
  void configActivated(QString id);
};

#endif // CONFIGMODEL_H
