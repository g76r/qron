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
#ifndef LASTOCCUREDTEXTEVENTSMODEL_H
#define LASTOCCUREDTEXTEVENTSMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>

class LastOccuredTextEventsModel : public QAbstractListModel {
  Q_OBJECT
  class OccuredEvent {
  public:
    QString _event;
    QDateTime _datetime;
    OccuredEvent(QString event) : _event(event),
      _datetime(QDateTime::currentDateTime()) { }
  };
  QList<OccuredEvent> _occuredEvents;
  int _maxsize;
  QString _eventName;

public:
  // LATER limit last emited alerts by age
  explicit LastOccuredTextEventsModel(QObject *parent = 0, int maxsize = 100);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  void setEventName(const QString eventName) { _eventName = eventName; }

public slots:
  void eventOccured(QString event);

};

#endif // LASTOCCUREDTEXTEVENTSMODEL_H
