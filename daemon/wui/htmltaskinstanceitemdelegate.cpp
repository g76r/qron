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
#include "htmltaskinstanceitemdelegate.h"

HtmlTaskInstanceItemDelegate::HtmlTaskInstanceItemDelegate(QObject *parent)
  : HtmlItemDelegate(parent) {
  QHash<QString,QString> instancesStatusIcons;
  instancesStatusIcons.insert("queued", "<i class=\"icon-inbox\"></i>&nbsp;");
  instancesStatusIcons.insert("running", "<i class=\"icon-play\"></i>&nbsp;");
  instancesStatusIcons.insert("failure", "<i class=\"icon-minus-circled\"></i>&nbsp;");
  instancesStatusIcons.insert("canceled", "<i class=\"icon-cancel\"></i>&nbsp;");
  setPrefixForColumn(1, "<i class=\"icon-cog\"></i>&nbsp;"
                     "<a href=\"tasks/%1\">", 1);
  setSuffixForColumn(1, "</a>");
  setPrefixForColumn(2, "%1", 2, instancesStatusIcons);
}

QString HtmlTaskInstanceItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 8: {
    QString taskInstanceId = index.model()->index(
          index.row(), 0, index.parent()).data().toString();
    QString taskId = index.model()->index(index.row(), 1, index.parent()).data()
        .toString();
    QString status = index.model()->index(index.row(), 2, index.parent()).data()
        .toString();
    bool abortable = index.model()->index(index.row(), 9, index.parent()).data()
        .toBool();
    text = index.data().toString(); // disable truncating and HTML encoding
    text.prepend(/* log */
                 "<span class=\"label label-info\" title=\"Log\">"
                 "<a target=\"_blank\" href=\"../rest/v1/logs/entries.txt?"
                 "filter=/"+taskInstanceId+" \">"
                 "<i class=\"icon-file-text\"></i></a></span> "
                 /* detail page */
                 "<span class=\"label label-info\" title=\""
                 "Detailed task info\"><a href=\"tasks/"
                 +taskId+"\"><i class=\"icon-cog\"></i></a></span> ");
    if (status == "queued")
      text.prepend(/* cancel */
                   "<span class=\"label label-danger\" title=\"Cancel "
                   "request\"><a href=\"confirm/do/v1/taskinstances/cancel/"
                   +taskInstanceId+"?confirm_message=cancel task request "
                   +taskInstanceId
                   +"\"><i class=\"icon-cancel\"></i></a></span> ");
    else if (status == "running") {
      if (abortable)
        text.prepend(/* abort */
                     "<span class=\"label label-danger\" title=\"Abort "
                     "task\"><a href=\"confirm/do/v1/taskinstances/abort/"
                     +taskInstanceId+"?confirm_message=abort task instance "
                     +taskInstanceId+
                     "\"><i class=\"icon-fire\"></i></a></span> ");
      else
        text.prepend(/* cannot abort */
                     "<span class=\"label\" title=\"Cannot abort task\">"
                     "<i class=\"icon-fire\"></i></span> ");
    } else
      text.prepend(/* reexec */
                   "<span class=\"label label-danger\" "
                   "title=\"Request execution of same task\"><a href=\""
                   "tasks/request/"+taskId+"\">"
                   "<i class=\"icon-repeat\"></i></a></span> ");
    break;
  }
  }
  return text;
}
