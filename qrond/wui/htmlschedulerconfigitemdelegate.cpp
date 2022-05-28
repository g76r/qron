/* Copyright 2014 Hallowyn and others.
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
#include "htmlschedulerconfigitemdelegate.h"

HtmlSchedulerConfigItemDelegate::HtmlSchedulerConfigItemDelegate(
    int idColumn, int isActiveColumn, int actionsColumn, QObject *parent)
  : HtmlItemDelegate(parent), _idColumn(idColumn),
    _isActiveColumn(isActiveColumn), _actionsColumn(actionsColumn) {
}

QString HtmlSchedulerConfigItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  QString id = index.model()->index(index.row(), _idColumn, index.parent())
      .data().toString();
  int column = index.column();
  if (column == _idColumn) {
    if (_configIds.contains(id)) {
      text.prepend("<a href=\"../rest/v1/configs/"+id+".pf\">");
      text.append("</a>");
    }
  } else if (column == _isActiveColumn) {
    text = (id == _activeConfigId)
        ? "<i class=\"fa-solid fa-play\"></i>&nbsp;active" : "-";
  } else if (column == _actionsColumn) {
    if (_configIds.contains(id)) {
      text.prepend("<span class=\"label label-info\" title=\"Download\">"
                   "<a href=\"../rest/v1/configs/"+id
                   +".pf\"><i class=\"fa-solid fa-file\"></i></a></span> ");
      if (id == _activeConfigId)
        text.prepend("<span class=\"label label-default\" title=\""
                     "Cannot remove active config\">"
                     "<i class=\"fa-solid fa-trash-can\"></i></span> "
                     "<span class=\"label label-default\" title=\""
                     "Cannot activate active config\">"
                     "<i class=\"fa-solid fa-play\"></i></span> ");
      else
        text.prepend("<span class=\"label label-danger\" title=\"Remove\">"
                     "<a href=\"confirm/do/v1/configs/remove/"+id
                     +"?confirm_message=remove configuration "+id
                     +"\"><i class=\"fa-solid fa-trash-can\"></i></a></span> "
                      "<span class=\"label label-danger\" title=\"Activate\">"
                      "<a href=\"confirm/do/v1/configs/activate/"+id
                     +"?confirm_message=activate configuration "+id
                     +"\"><i class=\"fa-solid fa-play\"></i></a></span> ");
    }
  }
  return text;
}

void HtmlSchedulerConfigItemDelegate::configActivated(SchedulerConfig config) {
  _activeConfigId = config.id();
}

void HtmlSchedulerConfigItemDelegate::configAdded(QString id) {
  _configIds.insert(id);
}

void HtmlSchedulerConfigItemDelegate::configRemoved(QString id) {
  _configIds.remove(id);
}
