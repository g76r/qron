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
#ifndef FLAGSSETMODEL_H
#define FLAGSSETMODEL_H

#include <QAbstractListModel>
#include <QDateTime>

/** Model holding currently set flags, one per line, along with the time it
  * was set on. */
class FlagsSetModel : public QAbstractListModel {
  Q_OBJECT
  Q_DISABLE_COPY(FlagsSetModel)
  class SetFlag {
  public:
    QString _flag;
    QDateTime _setTime;
    SetFlag(QString flag) : _flag(flag),
      _setTime(QDateTime::currentDateTime()) { }
    SetFlag(const SetFlag &o) : _flag(o._flag), _setTime(o._setTime) { }
  };
  QList<SetFlag> _setFlags;
  QString _prefix;
  int _prefixRole;

public:
  explicit FlagsSetModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  void setPrefix(QString prefix, int prefixRole) {
    _prefix = prefix;
    _prefixRole = prefixRole;
  }

public slots:
  void setFlag(QString flag);
  void clearFlag(QString alert);
};

#endif // FLAGSSETMODEL_H
