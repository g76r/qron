/* Copyright 2013 Hallowyn and others.
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
#include "htmltaskinstanceitemdelegate.h"

HtmlTaskInstanceItemDelegate::HtmlTaskInstanceItemDelegate(QObject *parent)
  : HtmlItemDelegate(parent) {
  QHash<QString,QString> instancesStatusIcons;
  instancesStatusIcons.insert("queued", "<i class=\"fa fa-inbox\"></i> ");
  instancesStatusIcons.insert("running", "<i class=\"fa fa-play\"></i> ");
  instancesStatusIcons.insert("failure", "<i class=\"fa fa-minus-circle\"></i> ");
  instancesStatusIcons.insert("canceled", "<i class=\"fa fa-times\"></i> ");
  setPrefixForColumn(1, "<i class=\"fa fa-cog\"></i> "
                     "<a href=\"taskdoc.html?fqtn=%1\">", 1);
  setSuffixForColumn(1, "</a>");
  setPrefixForColumn(2, "%1", 2, instancesStatusIcons);
}

QString HtmlTaskInstanceItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 8: {
    QString id = index.model()->index(index.row(), 0, index.parent()).data()
        .toString();
    QString fqtn = index.model()->index(index.row(), 1, index.parent()).data()
        .toString();
    QString status = index.model()->index(index.row(), 2, index.parent()).data()
        .toString();
    bool abortable = index.model()->index(index.row(), 9, index.parent()).data()
        .toBool();
    text.prepend(/* log */
                 "<span class=\"label label-info\" title=\"Log\">"
                 "<a target=\"_blank\" href=\"../rest/txt/log/all/v1?"
                 "filter=/"+id+"\"><i class=\"fa fa-fw fa-align-left\"></i></a>"
                 "</span> "
                 /* taskdoc */
                 "<span class=\"label label-info\" title=\""
                 "Detailed task info\"><a href=\"taskdoc.html?fqtn="
                 +fqtn+"\"><i class=\"fa fa-fw fa-cog\"></i></a></span> ");
    if (status == "queued")
      text.prepend(/* cancel */
                   "<span class=\"label label-important\" title=\"Cancel "
                   "request\"><a href=\"confirm?event=cancelRequest&id="
                   +id+"\"><i class=\"fa fa-fw fa-times\"></i></a></span> ");
    else if (status == "running") {
      if (abortable)
        text.prepend(/* abort */
                     "<span class=\"label label-important\" title=\"Abort "
                     "task\"><a href=\"confirm?event=abortTask&id="+id+"\">"
                     "<i class=\"fa fa-fw fa-fire\"></i></a></span> ");
      else
        text.prepend(/* cannot abort */
                     "<span class=\"label\" title=\"Cannot abort task\">"
                     "<i class=\"fa fa-fw fa-fire\"></i></span> ");
    } else
      text.prepend(/* reexec */
                   "<span class=\"label label-important\" "
                   "title=\"Request execution of same task\"><a href=\""
                   "requestform?fqtn="+fqtn+"\">"
                   "<i class=\"fa fa-fw fa-repeat\"></i></a></span> ");
    break;
  }
  }
  return text;
}
