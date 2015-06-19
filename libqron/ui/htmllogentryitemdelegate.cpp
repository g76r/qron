/* Copyright 2014-2015 Hallowyn and others.
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
#include "htmllogentryitemdelegate.h"
#include "log/log.h"

HtmlLogEntryItemDelegate::HtmlLogEntryItemDelegate(QObject *parent) :
  HtmlItemDelegate(parent) {
}


QString HtmlLogEntryItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  switch (index.column()) {
  case 1:
    if (text.contains('.')) {
      text.prepend("<a href=\"taskdoc.html?taskid="+text+"\">");
      text.append("</a>");
    }
    break;
  case 5: {
    QString severity = index.model()->index(index.row(), 4, index.parent())
        .data().toString();
    if (severity == Log::severityToString(Log::Warning))
      text.prepend("<i class=\"icon-warning\"></i>&nbsp;");
    else if (severity == Log::severityToString(Log::Error)
             || severity == Log::severityToString(Log::Fatal))
      text.prepend("<i class=\"icon-minus-circled\"></i>&nbsp;");
    break;
  }
  }
  return text;
}
