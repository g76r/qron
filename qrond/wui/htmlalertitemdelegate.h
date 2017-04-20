/* Copyright 2013-2016 Hallowyn and others.
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
#ifndef HTMLALERTITEMDELEGATE_H
#define HTMLALERTITEMDELEGATE_H

#include "textview/htmlitemdelegate.h"

/** Specific item delegate for alerts (both raised alerts and emited alerts). */
class HtmlAlertItemDelegate : public HtmlItemDelegate {
  Q_OBJECT
  Q_DISABLE_COPY(HtmlAlertItemDelegate)
  bool _canRaiseAndCancel;

public:
  explicit HtmlAlertItemDelegate(QObject *parent, bool canRaiseAndCancel);
  QString text(const QModelIndex &index) const;
};

#endif // HTMLALERTITEMDELEGATE_H
