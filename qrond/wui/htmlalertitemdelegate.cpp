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
#include "htmlalertitemdelegate.h"
#include "alert/alert.h"
#include <QRegularExpression>

#define ACTION_COLUMN 5

HtmlAlertItemDelegate::HtmlAlertItemDelegate(
    QObject *parent, bool canRaiseAndCancel)
  : HtmlItemDelegate(parent), _canRaiseAndCancel(canRaiseAndCancel) {
}

static QRegularExpression taskIdInAlertRE{"^task\\.[^\\.]+\\.(.*)$"};

QString HtmlAlertItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  if (index.column() == ACTION_COLUMN) {
    QString alertId = index.model()->index(index.row(), 0, index.parent())
        .data().toString();
    QString status = index.model()->index(index.row(), 1, index.parent())
        .data().toString();
    if (_canRaiseAndCancel)
      text.prepend(/* immediate cancel */
                   "<span class=\"label label-danger\">"
                   // TODO add !pathtoroot
                   "<a title=\"Cancel alert immediatly\"href=\"../do/v1/alerts/"
                   "cancel_immediately/"+alertId+"\"><i class=\"fa-solid fa-check\">"
                    "</i></a></span> ");
    if (_canRaiseAndCancel
        && (status == Alert::statusAsString(Alert::Rising)
            || status == Alert::statusAsString(Alert::MayRise)
            || status == Alert::statusAsString(Alert::Dropping)))
      text.prepend(/* immediate raise */
                   "<span class=\"label label-danger\">"
                   // TODO add !pathtoroot
                   "<a title=\"Raise alert immediately\"href=\"../do/v1/alerts/"
                   "raise_immediately/"+alertId+"\"><i class=\"fa-solid fa-bell\">"
                    "</i></a></span> ");
    QRegularExpressionMatch match = taskIdInAlertRE.match(alertId);
    if (match.hasMatch()) {
      text.append(
            /* related task log */
            " <span class=\"label label-info\" title=\"Related tasks log\">"
            "<a target=\"_blank\" href=\"../rest/v1/logs/entries.txt?"
            "regexp=^[^ ]* "+match.captured(1)+"[/:]\">"
            "<i class=\"fa-solid fa-file-lines\"></i></a></span>"
            /* detail page */
            " <span class=\"label label-info\" title=\"Detailed task info\">"
            "<a href=\"tasks/"+match.captured(1)+"\">"
            "<i class=\"fa-solid fa-gear\"></i></a></span>");
    }
  }
  return text;
}
