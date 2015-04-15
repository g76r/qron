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
#include "htmlalertitemdelegate.h"

HtmlAlertItemDelegate::HtmlAlertItemDelegate(QObject *parent, int alertColumn,
                                             int actionsColumn, bool canCancel)
  : HtmlItemDelegate(parent), _alertColumn(alertColumn),
    _actionsColumn(actionsColumn), _canCancel(canCancel) {
}

QString HtmlAlertItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  if (index.column() == _actionsColumn) {
    static QRegExp taskIdInAlert("task\\.[^\\.]+\\.(.*)");
    QRegExp re = taskIdInAlert;
    QString alert = index.model()
        ->index(index.row(), _alertColumn, index.parent()).data().toString();
    if (_canCancel)
      text.prepend(/* cancel immediate */
                   "<span class=\"label label-important\">"
                   "<a title=\"Cancel alert\"href=\"do?event=cancelAlert&alert="
                   +alert+"&immediately=true\"><i class=\"icon-check\">"
                   "</i></a></span> ");
    if (re.exactMatch(alert)) {
      text.append(
            /* related task log */
            " <span class=\"label label-info\" title=\"Related tasks log\">"
            "<a target=\"_blank\" href=\"../rest/txt/log/all/v1?regexp=^[^ ]* "
            +re.cap(1)+"[/:]\"><i class=\"icon-file-text\"></i></a></span>"
            /* related task taskdoc */
            " <span class=\"label label-info\" title=\"Detailed task info\">"
            "<a href=\"taskdoc.html?taskid="+re.cap(1)+"\">"
            "<i class=\"icon-cog\"></i></a></span>");
    }
  }
  return text;
}
