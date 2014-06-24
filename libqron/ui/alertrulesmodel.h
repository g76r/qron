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
#ifndef ALERTRULESMODEL_H
#define ALERTRULESMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "config/alertrule.h"
#include "config/alerterconfig.h"

/** Model holding alert rules configuration, one rule per line, in their
 * evaluation order. */
class LIBQRONSHARED_EXPORT AlertRulesModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(AlertRulesModel)
  QList<AlertRule> _rules;

public:
  explicit AlertRulesModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  
public slots:
  void configChanged(AlerterConfig config);
};

#endif // ALERTRULESMODEL_H

