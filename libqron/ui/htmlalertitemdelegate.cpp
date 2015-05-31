/* Copyright 2013-2015 Hallowyn and others.
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
#include "alert/alert.h"

HtmlAlertItemDelegate::HtmlAlertItemDelegate(
    QObject *parent, int actionsColumn, bool canRaiseAndCancel)
  : HtmlItemDelegate(parent), _actionsColumn(actionsColumn),
    _canRaiseAndCancel(canRaiseAndCancel) {
}

QString HtmlAlertItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  if (index.column() == _actionsColumn) {
    static QRegExp taskIdInAlert("task\\.[^\\.]+\\.(.*)");
    QRegExp re = taskIdInAlert;
    QString alertId = index.model()->index(index.row(), 0, index.parent())
        .data().toString();
    QString status = index.model()->index(index.row(), 1, index.parent())
        .data().toString();
    if (_canRaiseAndCancel)
      text.prepend(/* immediate cancel */
                   "<span class=\"label label-important\">"
                   "<a title=\"Cancel alert immediatly\"href=\"do?"
                   "event=cancelAlert&alert="+alertId
                   +"&immediately=true\"><i class=\"icon-check\">"
                    "</i></a></span> ");
    if (_canRaiseAndCancel
        && (status == Alert::statusToString(Alert::Rising)
            || status == Alert::statusToString(Alert::MayRise)
            || status == Alert::statusToString(Alert::Dropping)))
      text.prepend(/* immediate raise */
                   "<span class=\"label label-important\">"
                   "<a title=\"Raise alert immediately\"href=\"do?"
                   "event=raiseAlert&alert="+alertId
                   +"&immediately=true\"><i class=\"icon-bell\">"
                    "</i></a></span> ");
    if (re.exactMatch(alertId)) {
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
