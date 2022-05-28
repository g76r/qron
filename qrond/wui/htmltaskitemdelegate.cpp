/* Copyright 2013-2021 Hallowyn and others.
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
#include "htmltaskitemdelegate.h"
#include <QDateTime>

// 300,000 ms = 5'
// should stay above TasksModel's PERIODIC_REFRESH_INTERVAL
#define SOON_EXECUTION_MILLIS 300000

HtmlTaskItemDelegate::HtmlTaskItemDelegate(QObject *parent)
  : HtmlItemDelegate(parent) {
}

QString HtmlTaskItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 0:
  case 11: {
    QString mean = index.model()->index(index.row(), 3, index.parent()).data()
        .toString();
    text.prepend("<a href=\"tasks/"
                 +index.model()->index(index.row(), 11, index.parent()).data()
                 .toString()+
                 "\">");
    text.prepend("<i class=\"fa-solid fa-gear\"></i>&nbsp;");
    text.append("</a>");
    break;
  }
  case 1:
    text.prepend("<i class=\"fa-solid fa-gears\"></i>&nbsp;");
    break;
  case 6:
  case 28: {
    bool noTrigger = text.isEmpty();
    if (index.model()->index(index.row(), 30, index.parent()).data().toBool())
      text.prepend("<i class=\"fa-solid fa-calendar-days\"></i>&nbsp;");
    if (noTrigger)
      text.prepend("<i class=\"fa-regular fa-circle\"></i>&nbsp;no trigger ");
    if (!index.model()->index(index.row(), 29, index.parent()).data().toBool())
      text.prepend("<i class=\"fa-solid fa-ban\"></i>&nbsp;disabled ");
    break;
  }
  case 10: {
    QDateTime dt = index.data().toDateTime();
    if (!dt.isNull()
        && dt.toMSecsSinceEpoch()-QDateTime::currentMSecsSinceEpoch()
        < SOON_EXECUTION_MILLIS)
      text.prepend("<i class=\"fa-solid fa-clock\"></i>&nbsp;");
    break;
  }
  case 13:
  case 17:
    if (index.model()->index(index.row(), 13, index.parent()).data().toInt() > 0)
      text.prepend("<i class=\"fa-solid fa-play\"></i>&nbsp;");
    break;
  case 18: {
    bool enabled = index.model()->index(index.row(), 29, index.parent()).data()
        .toBool();
    QString taskId = index.model()->index(index.row(), 11, index.parent())
        .data().toString();
    text = index.data().toString(); // disable truncating and HTML encoding
    text.prepend(/* request task */ QString() +
                 "<span class=\"label label-danger\" "
                 "title=\"Request execution\"><a href=\"tasks/request/"
                 +taskId+"\"><i class=\"fa-solid fa-play\"></i></a></span> "
                 /* {enable,disable} task */
                 "<span class=\"label label-"+(enabled?"danger":"warning")
                 +"\" title=\""+(enabled?"Disable":"Enable")+"\">"
                 // TODO add !pathtoroot, which probably needs context
                 "<a href=\"../do/v1/tasks/"+(enabled?"disable":"enable")+"/"
                 +taskId+"\"><i class=\"fa-solid fa-ban\"></i></a></span> "
                 /* log */
                 "<span class=\"label label-info\" title=\"Log\">"
                 "<a target=\"_blank\" href=\"../rest/v1/logs/entries.txt?"
                 "regexp=^[^ ]* "+taskId+"[/:]\"><i class=\"fa-solid fa-file-lines\">"
                 "</i></a></span> "
                 /* detail page */
                 "<span class=\"label label-info\" "
                 "title=\"Detailed task info\"><a href=\"tasks/"
                 +taskId+"\"><i class=\"fa-solid fa-gear\"></i></a></span> ");
    break;
  }
  case 19:
    if (index.data().toString().contains("failure"))
      text.prepend("<i class=\"fa-solid fa-circle-minus\"></i>&nbsp;");
    break;
  case 27: {
    if (!text.isEmpty())
      text.prepend("<i class=\"fa-solid fa-skull-crossbones\"></i>&nbsp;");
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
      text.prepend("<i class=\"fa-solid fa-crosshairs\"></i>&nbsp;");
      break;
    case 6:
    case 28:
      text.prepend("<i class=\"fa-solid fa-bolt-lightning\"></i>&nbsp;");
      break;
    case 8:
      text.prepend("<i class=\"fa-solid fa-wheat-awn\"></i>&nbsp;");
      break;
    case 14:
      text.prepend("<i class=\"fa-solid fa-play\"></i>&nbsp;");
      break;
    case 15:
      text.prepend("<i class=\"fa-solid fa-check\"></i>&nbsp;");
      break;
    case 16:
      text.prepend("<i class=\"fa-solid fa-circle-minus\"></i>&nbsp;");
      break;
    case 27:
      text.prepend("<i class=\"fa-solid fa-skull-crossbones\"></i>&nbsp;");
    }
  }
  return text;
}
