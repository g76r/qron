/* Copyright 2014 Hallowyn and others.
 * This file is part of libqtssu, see <https://github.com/g76r/libqtssu>.
 * Libqtssu is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Libqtssu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with libqtssu.  If not, see <http://www.gnu.org/licenses/>.
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
      text.prepend("<i class=\"icon-cog\"></i> "); // was: icon-puzzle-piece
    else if (kind == "and")
      text.prepend("<i class=\"icon-chevron-up\"></i> ");
      //text.prepend("<span class=\"fa-stack\"><i class=\"fa fa-square fa-stack\"></i><i class=\"fa fa-chevron-up fa-stack fa-inverse\"></i></span> ");
    else if (kind == "or")
      text.prepend("<i class=\"icon-chevron-down\"></i> ");
      //text.prepend("<span class=\"fa-stack\"><i class=\"fa fa-circle fa-stack\"></i><i class=\"fa fa-chevron-down fa-stack fa-inverse\"></i></span> ");
    else if (kind == "start")
      text.prepend("<i class=\"icon-circle\"></i> ");
    else if (kind == "end")
      text.prepend("<i class=\"icon-dot-circled\"></i> ");
    else
      text.prepend("<i class=\"icon-help-circled\"></i> ");
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
      text.prepend("<i class=\"icon-check\"></i> ");
      break;
    case 7:
      text.prepend("<i class=\"icon-play\"></i> ");
      break;
    case 8:
      text.prepend("<i class=\"icon-check\"></i> ");
      break;
    case 9:
      text.prepend("<i class=\"icon-minus-circled\"></i> ");
      break;
    }
  }
  return text;
}
