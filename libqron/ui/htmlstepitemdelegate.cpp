/* Copyright 2014 Hallowyn and others.
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
#include "htmlstepitemdelegate.h"
#include <QString>

HtmlStepItemDelegate::HtmlStepItemDelegate(QObject *parent)
  : HtmlItemDelegate(parent) {
}

QString HtmlStepItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 0:
  case 1: {
    QString kind = index.model()->index(index.row(), 2, index.parent()).data()
        .toString();
    if (kind == "task")
      text.prepend("<i class=\"icon-cog\"></i>&nbsp;"); // was: icon-puzzle-piece
    else if (kind == "and")
      text.prepend("<i class=\"icon-chevron-up\"></i>&nbsp;");
    else if (kind == "or")
      text.prepend("<i class=\"icon-chevron-down\"></i>&nbsp;");
    else if (kind == "start")
      text.prepend("<i class=\"icon-circle\"></i>&nbsp;");
    else if (kind == "end")
      text.prepend("<i class=\"icon-dot-circled\"></i>&nbsp;");
    else
      text.prepend("<i class=\"icon-help-circled\"></i>&nbsp;");
    break;
  }
  case 3:
    text.prepend("<a href=\"taskdoc.html?taskid="+text+"\">");
    text.append("</a>");
    break;
  case 4:
    if (!text.isEmpty()) {
      text.prepend("<a href=\"taskdoc.html?taskid="+text+"\">");
      text.append("</a>");
    }
    break;
  }
  return text;
}

QString HtmlStepItemDelegate::headerText(
    int section, Qt::Orientation orientation,
    const QAbstractItemModel *model) const {
  QString text = HtmlItemDelegate::headerText(section, orientation, model);
  if (orientation == Qt::Horizontal) {
    switch (section) {
    case 6:
      text.prepend("<i class=\"icon-check\"></i>&nbsp;");
      break;
    case 7:
      text.prepend("<i class=\"icon-play\"></i>&nbsp;");
      break;
    case 8:
      text.prepend("<i class=\"icon-check\"></i>&nbsp;");
      break;
    case 9:
      text.prepend("<i class=\"icon-minus-circled\"></i>&nbsp;");
      break;
    }
  }
  return text;
}
