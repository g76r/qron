/* Copyright 2013-2014 Hallowyn and others.
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
#include "htmltaskitemdelegate.h"
#include <QDateTime>

// 300,000 ms = 5'
// should stay above TasksModel's FULL_REFRESH_INTERVAL
#define SOON_EXECUTION_MILLIS 300000

HtmlTaskItemDelegate::HtmlTaskItemDelegate(QObject *parent)
  : HtmlItemDelegate(parent) {
}

QString HtmlTaskItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  bool isSubtask = !index.model()->index(index.row(), 31, index.parent())
      .data().toString().isEmpty();
  switch (index.column()) {
  case 0:
  case 11: {
    QString mean = index.model()->index(index.row(), 3, index.parent()).data()
        .toString();
    text.prepend("<a href=\"taskdoc.html?fqtn="
                 +index.model()->index(index.row(), 11, index.parent()).data()
                 .toString()+
                 "\">");
    if (mean == "workflow")
      text.prepend("<i class=\"fa fa-sitemap\"></i> ");
    else if (isSubtask)
      text.prepend("<i class=\"fa fa-puzzle-piece\"></i> ");
    else
      text.prepend("<i class=\"fa fa-cog\"></i> ");
    text.append("</a>");
    break;
  }
  case 1:
    text.prepend("<i class=\"fa fa-cogs\"></i> ");
    break;
  case 6:
  case 28: {
    bool noTrigger = text.isEmpty();
    if (index.model()->index(index.row(), 30, index.parent()).data().toBool())
      text.prepend("<i class=\"fa fa-calendar\"></i> ");
    if (noTrigger && !isSubtask)
      text.prepend("<i class=\"fa fa-circle-o\"></i> no trigger ");
    if (!index.model()->index(index.row(), 29, index.parent()).data().toBool())
      text.prepend("<i class=\"fa fa-ban\"></i> disabled ");
    break;
  }
  case 10: {
    QDateTime dt = index.data().toDateTime();
    if (!dt.isNull()
        && dt.toMSecsSinceEpoch()-QDateTime::currentMSecsSinceEpoch()
        < SOON_EXECUTION_MILLIS)
      text.prepend("<i class=\"fa fa-clock-o\"></i> ");
    break;
  }
  case 13:
  case 17:
    if (index.model()->index(index.row(), 13, index.parent()).data() > 0)
      text.prepend("<i class=\"fa fa-play\"></i> ");
    break;
  case 18: {
    bool enabled = index.model()->index(index.row(), 29, index.parent()).data()
        .toBool();
    QString fqtn = index.model()->index(index.row(), 11, index.parent()).data()
        .toString();
    text = index.data().toString(); // disable truncating and HTML encoding
    text.prepend(/* requestTask */ QString() +
                 "<span class=\"label label-important\" "
                 "title=\"Request execution\"><a href=\"requestform?"
                 "fqtn="+fqtn+"\"><i class=\"fa fa-fw fa-play\"></i></a>"
                 "</span> "
                 /* {enable,disable}Task */
                 "<span class=\"label label-"+(enabled?"important":"warning")
                 +"\" title=\""+(enabled?"Disable":"Enable")+"\">"
                 "<a href=\"do?event=enableTask&fqtn="+fqtn+"&enable="
                 +(enabled?"false":"true")+"\"><i class=\"fa fa-fw fa-ban\">"
                 "</i></a></span> "
                 /* log */
                 "<span class=\"label label-info\" title=\"Log\">"
                 "<a target=\"_blank\" href=\"../rest/txt/log/all/v1?"
                 "filter= "+fqtn+"/\"><i class=\"fa fa-fw fa-align-left\">"
                 "</i></a></span> "
                 /* taskdoc */
                 "<span class=\"label label-info\" "
                 "title=\"Detailed task info\"><a href=\"taskdoc.html?fqtn="
                 +fqtn+"\"><i class=\"fa fa-fw fa-cog\"></i></a></span> ");
    break;
  }
  case 19:
    if (index.data().toString().contains("failure"))
      text.prepend("<i class=\"fa fa-minus-circle\"></i> ");
    break;
  case 27: {
    if (!text.isEmpty())
      text.prepend("<i class=\"fa fa-fire\"></i> ");
    break;
  }
  }
  return text;
}

QString HtmlTaskItemDelegate::headerText(
    int section, Qt::Orientation orientation,
    const QAbstractItemModel *model) const {
  QString text = HtmlItemDelegate::headerText(section, orientation, model);
  //qDebug() << "***" << model << orientation << section;
  if (orientation == Qt::Horizontal) {
    switch (section) {
    case 5:
      text.prepend("<i class=\"fa fa-crosshairs\"></i> ");
      break;
    case 6:
    case 28:
      text.prepend("<i class=\"fa fa-bolt\"></i> ");
      break;
    case 8:
      text.prepend("<i class=\"fa fa-beer\"></i> ");
      break;
    case 14:
      text.prepend("<i class=\"fa fa-rocket\"></i> ");
      break;
    case 15:
      text.prepend("<i class=\"fa fa-flag-checkered\"></i> ");
      break;
    case 16:
      text.prepend("<i class=\"fa fa-minus-circle\"></i> ");
      break;
    }
  }
  return text;
}
