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

#include <QAbstractTableModel>
#include <QDateTime>
#include <QString>

// LATER move to libqtssu
/** Model holding generic text events along with the date their occured, one
 * event per line, in reverse order of occurrence. */
class LastOccuredTextEventsModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(LastOccuredTextEventsModel)

protected:
  class OccuredEvent {
  public:
    QString _event;
    int _type;
    QDateTime _datetime;
    OccuredEvent() : _type(0) { }
    OccuredEvent(QString event, int type) : _event(event), _type(type),
      _datetime(QDateTime::currentDateTime()) { }
    OccuredEvent(const OccuredEvent &o) : _event(o._event), _type(o._type),
      _datetime(o._datetime) { }
  };

  QList<OccuredEvent> _occuredEvents;
  int _maxrows;
  QString _eventName;
  QList<QString> _additionnalColumnsHeaders;

public:
  // LATER limit last emited alerts by age
  explicit LastOccuredTextEventsModel(QObject *parent = 0, int maxrows = 100);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  LastOccuredTextEventsModel *setEventName(QString eventName) {
    _eventName = eventName; return this; }
  LastOccuredTextEventsModel *setMaxrows(int maxrows) {
    _maxrows = maxrows; return this; }
  int maxrows() const { return _maxrows; }
  LastOccuredTextEventsModel *addEmptyColumn(QString header) {
    _additionnalColumnsHeaders.append(header); return this; }
  LastOccuredTextEventsModel *removeEmptyColumns() {
    _additionnalColumnsHeaders.clear(); return this; }

public slots:
  void eventOccured(QString event);
  void eventOccured(QString event, int type);
};

#endif // LASTOCCUREDTEXTEVENTSMODEL_H
