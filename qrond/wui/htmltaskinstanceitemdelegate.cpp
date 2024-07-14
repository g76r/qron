/* Copyright 2013-2024 Hallowyn and others.
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

HtmlTaskInstanceItemDelegate::HtmlTaskInstanceItemDelegate(
  QObject *parent, bool decorateHerdId)
  : HtmlItemDelegate(parent), _decorateHerdId(decorateHerdId) {
  QHash<QString,QString> instancesStatusIcons;
  instancesStatusIcons.insert("planned", "<i class=\"fa-solid fa-table-cells-large\"></i>&nbsp;");
  instancesStatusIcons.insert("queued", "<i class=\"fa-solid fa-inbox\"></i>&nbsp;");
  instancesStatusIcons.insert("running", "<i class=\"fa-solid fa-play\"></i>&nbsp;");
  instancesStatusIcons.insert("waiting", "<i class=\"fa-solid fa-hourglass-half\"></i>&nbsp;");
  instancesStatusIcons.insert("failure", "<i class=\"fa-solid fa-circle-minus\"></i>&nbsp;");
  instancesStatusIcons.insert("canceled", "<i class=\"fa-solid fa-xmark\"></i>&nbsp;");
  setPrefixForColumn(1, "<i class=\"fa-solid fa-gear\"></i>&nbsp;"
                     "<a href=\"tasks/%1\">", 1);
  setSuffixForColumn(1, "</a>");
  setPrefixForColumn(2, "%1", 2, instancesStatusIcons);
}

QString HtmlTaskInstanceItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 10: // herdid
    if (!_decorateHerdId)
      break;
    [[fallthrough]];
  case 0: // taskinstanceid
    text = "<a target=\"_blank\" href=\"taskinstances/"+text+"\">"+text+"</a>";
    break;
  case 8: { // actions
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
          "<span class=\"label label-info\" title=\"Log\"><a target=\"_blank\" "
          "href=\"../rest/v1/logs/entries.txt?filter=/"+taskInstanceId
          +"\"><i class=\"fa-solid fa-file-lines\"></i></a></span>");
    if (status == "queued" || status == "planned")
      text.prepend(/* cancel */
                   "<span class=\"label label-danger\" title=\"Cancel "
                   "request\"><a href=\"confirm/do/v1/taskinstances/cancel/"
                   +taskInstanceId+"?confirm_message=cancel task request "
                   +taskInstanceId
                   +"\"><i class=\"fa-solid fa-xmark\"></i></a></span> ");
    else if (status == "running" || status == "waiting") {
      if (abortable)
        text.prepend(/* abort */
                     "<span class=\"label label-danger\" title=\"Abort "
                     "task\"><a href=\"confirm/do/v1/taskinstances/abort/"
                     +taskInstanceId+"?confirm_message=abort task instance "
                     +taskInstanceId+
                     "\"><i class=\"fa-solid fa-skull-crossbones\"></i></a></span> ");
      else
        text.prepend(/* cannot abort */
                     "<span class=\"label\" title=\"Cannot abort task\">"
                     "<i class=\"fa-solid fa-skull-crossbones\"></i></span> ");
    } else
      text.prepend(/* reexec */
                   "<span class=\"label label-danger\" "
                   "title=\"Request execution of same task\"><a href=\""
                   "tasks/request/"+taskId+"\">"
                   "<i class=\"fa-solid fa-arrow-rotate-right\"></i></a></span> ");
    break;
  }
  case 11: { // herded task instances
      QStringList idSlashId = text.split(' '), html, tiis;
      for (auto task: idSlashId) {
        QStringList ids = task.split('/');
        auto taskid = ids.value(0);
        auto tii = ids.value(1);
        html += "<a href=\"tasks/"+taskid+"\">"+taskid+"</a>/"
                +"<a target=\"_blank\" "
                  "href=\"../rest/v1/logs/entries.txt?filter=/"
                +tii+"\">"+tii+"</a>";
        if (!tii.isEmpty()) [[likely]]
          tiis << tii;
      }
      text = html.join(' ');
      if (!tiis.isEmpty()) {
        text += " <br/><a target=\"_blank\" "
                "href=\"../rest/v1/logs/entries.txt?regexp=/("
                + tiis.join('|') +")\">full herd log</a>";
      }
      break;
  }
  }
  return text;
}
