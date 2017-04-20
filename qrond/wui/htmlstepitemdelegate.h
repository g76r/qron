/* Copyright 2014-2016 Hallowyn and others.
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
#ifndef HTMLSTEPITEMDELEGATE_H
#define HTMLSTEPITEMDELEGATE_H

#include "textview/htmlitemdelegate.h"

class HtmlStepItemDelegate : public HtmlItemDelegate {
  Q_OBJECT
  Q_DISABLE_COPY(HtmlStepItemDelegate)
public:
  explicit HtmlStepItemDelegate(QObject *parent = 0);
  QString text(const QModelIndex &index) const;
  QString headerText(int section, Qt::Orientation orientation,
                     const QAbstractItemModel *model) const;
};

#endif // HTMLSTEPITEMDELEGATE_H
