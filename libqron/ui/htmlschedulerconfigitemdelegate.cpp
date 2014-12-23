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
#include "htmlschedulerconfigitemdelegate.h"

HtmlSchedulerConfigItemDelegate::HtmlSchedulerConfigItemDelegate(
    QObject *parent) : HtmlItemDelegate(parent) {
}

QString HtmlSchedulerConfigItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  QString id = index.model()->index(index.row(), 0, index.parent())
      .data().toString();
  switch (index.column()) {
  case 0:
    text.prepend("<a href=\"../rest/pf/config/v1?configid="+id+"\">");
    text.append("</a>");
    break;
  case 2:
    text = (id == _activeConfigId)
        ? "<i class=\"icon-play\"></i>&nbsp;active" : "-";
    break;
  case 3:
    text.prepend("<span class=\"label label-info\" title=\"Download\">"
                 "<a href=\"../rest/pf/config/v1?configid="+id
                +"\"><i class=\"icon-file\"></i></a></span> ");
    if (id == _activeConfigId)
      text.prepend("<span class=\"label label-default\" title=\""
                   "Cannot remove active config\">"
                   "<i class=\"icon-trash\"></i></span> "
                    "<span class=\"label label-default\" title=\""
                   "Cannot activate active config\">"
                    "<i class=\"icon-play\"></i></span> ");
    else
      text.prepend("<span class=\"label label-important\" title=\"Remove\">"
                   "<a href=\"confirm?event=removeConfig&configid="+id
                   +"\"><i class=\"icon-trash\"></i></a></span> "
                    "<span class=\"label label-important\" title=\"Activate\">"
                    "<a href=\"confirm?event=activateConfig&configid="+id
                   +"\"><i class=\"icon-play\"></i></a></span> ");
    break;
  }
  return text;
}

void HtmlSchedulerConfigItemDelegate::setActiveConfig(QString configId) {
  _activeConfigId = configId;
}
