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
#ifndef RAISEDALERTSMODEL_H
#define RAISEDALERTSMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QDateTime>

/** Model holding raised alerts along with their raise and scheduled
 * cancellation dates, one alert per line, in reverse order of raising. */
class RaisedAlertsModel : public QAbstractListModel {
  Q_OBJECT
  Q_DISABLE_COPY(RaisedAlertsModel)
  class RaisedAlert {
  public:
    QString _alert;
    QDateTime _raiseTime, _scheduledCancellationTime;
    RaisedAlert(QString alert) : _alert(alert),
      _raiseTime(QDateTime::currentDateTime()) { }
    RaisedAlert(const RaisedAlert &o) : _alert(o._alert),
      _raiseTime(o._raiseTime),
      _scheduledCancellationTime(o._scheduledCancellationTime) { }
  };
  QList<RaisedAlert> _raisedAlerts;
  QString _prefix;
  int _prefixRole;

public:
  explicit RaisedAlertsModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  void setPrefix(QString prefix, int prefixRole) {
    _prefix = prefix;
    _prefixRole = prefixRole;
  }

public slots:
  void alertRaised(QString alert);
  void alertCanceled(QString alert);
  void alertCancellationScheduled(QString alert, QDateTime scheduledTime);
  void alertCancellationUnscheduled(QString alert);
};

#endif // RAISEDALERTSMODEL_H
