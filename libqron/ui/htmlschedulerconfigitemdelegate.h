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
#ifndef HTMLSCHEDULERCONFIGITEMDELEGATE_H
#define HTMLSCHEDULERCONFIGITEMDELEGATE_H

#include "libqron_global.h"
#include "textview/htmlitemdelegate.h"
#include <QSet>
#include "config/schedulerconfig.h"

/** Specific item delegate for scheduler config. */
class LIBQRONSHARED_EXPORT HtmlSchedulerConfigItemDelegate
    : public HtmlItemDelegate {
  Q_OBJECT
  Q_DISABLE_COPY(HtmlSchedulerConfigItemDelegate)
  QString _activeConfigId;
  QSet<QString> _configIds;
  int _idColumn, _isActiveColumn, _actionsColumn;

public:
  explicit HtmlSchedulerConfigItemDelegate(
      int idColumn, int isActiveColumn, int actionsColumn, QObject *parent = 0);
  QString text(const QModelIndex &index) const;

public slots:
  void configActivated(SchedulerConfig config);
  void configAdded(QString id);
  void configRemoved(QString id);
};

#endif // HTMLSCHEDULERCONFIGITEMDELEGATE_H
