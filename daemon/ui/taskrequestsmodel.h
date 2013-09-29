/* Copyright 2013 Hallowyn and others.
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
#ifndef TASKREQUESTSMODEL_H
#define TASKREQUESTSMODEL_H

#include <QAbstractTableModel>
#include <QString>
#include <QDateTime>
#include "config/taskrequest.h"
#include "config/host.h"
#include <QList>
#include "textviews.h"

/** Model holding tasks requests along with their attributes, one request per
 * line, in reverse request order. */
class TaskRequestsModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(TaskRequestsModel)
  QList<TaskRequest> _requests;
  int _maxrows;
  bool _keepFinished;
  QString _customActions;

public:
  explicit TaskRequestsModel(QObject *parent = 0, int maxrows = 100,
                             bool keepFinished = true);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  inline void setMaxrows(int maxrows) { _maxrows = maxrows; }
  inline int maxrows() const { return _maxrows; }
  /** Way to add custom html at the end of "Actions" column. Will be evaluated
   * through TaskRequest.params(). */
  void setCustomActions(QString customActions) {
    _customActions = customActions; }

public slots:
  void taskChanged(TaskRequest request);
};

#endif // TASKREQUESTSMODEL_H
