/* Copyright 2013 Hallowyn and others.
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
#include "alertrulesmodel.h"
#include "alert/alertchannel.h"
#include "textviews.h"

#define COLUMNS 6

AlertRulesModel::AlertRulesModel(QObject *parent) : QAbstractTableModel(parent) {
}

int AlertRulesModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _rules.size();
}

int AlertRulesModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant AlertRulesModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _rules.size()) {
    AlertRule rule = _rules.at(index.row());
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return rule.pattern();
      case 1:
        return rule.channelName();
      case 2:
        return rule.address();
      case 3:
        return rule.rawMessage();
      case 4:
        return rule.rawCancelMessage();
      case 5: {
        QString s;
        if (!rule.notifyCancel())
          s.append("nonotifycancel ");
        if (!rule.notifyReminder())
          s.append("nonotifyreminder");
        return s;
      }
      }
      break;
    case TextViews::HtmlPrefixRole:
       // LATER move Twitter Bootstrap specific icons to WebConsole
      switch(index.column()) {
      case 0:
        return "<i class=\"icon-filter\"></i> ";
      case 1:
        return rule.stop() ? "<i class=\"icon-stop\"></i> " : QVariant();
      }
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant AlertRulesModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Pattern";
    case 1:
      return "Channel";
    case 2:
      return "Address";
    case 3:
      return "Message";
    case 4:
      return "Cancel message";
    case 5:
      return "Parameters";
    }
  }
  return QVariant();
}

void AlertRulesModel::rulesChanged(QList<AlertRule> rules) {
  beginResetModel();
  _rules = rules;
  endResetModel();
}
