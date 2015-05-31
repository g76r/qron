/* Copyright 2013-2015 Hallowyn and others.
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
#ifndef RAISEDALERTSMODEL_H
#define RAISEDALERTSMODEL_H

#include "libqron_global.h"
#include "modelview/shareduiitemstablemodel.h"

// FIXME remove the whole class
/** Model holding raised alerts along with their status, rise, due, visibility
 * and cancellation dates, one alert per line, in reverse order of raising. */
class LIBQRONSHARED_EXPORT RaisedAlertsModel : public SharedUiItemsTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(RaisedAlertsModel)

public:
  explicit RaisedAlertsModel(QObject *parent = 0);
  //void changeItem(SharedUiItem newItem, SharedUiItem oldItem);
};

#endif // RAISEDALERTSMODEL_H
