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
#include "configsmodel.h"

ConfigsModel::ConfigsModel(QObject *parent)
: SharedUiItemsTableModel(parent) {
  setHeaderDataFromTemplate(SchedulerConfig(PfNode("qrontab"), 0, false));
}

void ConfigsModel::configAdded(QString id, SchedulerConfig config) {
  Q_UNUSED(id)
  configRemoved(id);
  insertItemAt(0, config);
}

void ConfigsModel::configRemoved(QString id) {
  int rowCount = this->rowCount();
  for (int i = 0; i < rowCount; ) {
    if (itemAt(index(i, 0)).id() == id) {
      removeItems(i, i);
      --rowCount;
    } else
      ++ i;
  }
}
