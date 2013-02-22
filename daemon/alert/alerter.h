/* Copyright 2012-2013 Hallowyn and others.
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
#ifndef ALERTER_H
#define ALERTER_H

#include <QObject>
#include "util/paramset.h"
#include "config/alertrule.h"
#include "config/alert.h"
#include "alertchannel.h"
#include <QHash>
#include <QString>
#include <QDateTime>

class QThread;
class PfNode;

class Alerter : public QObject {
  Q_OBJECT
  QThread *_thread;
  ParamSet _params;
  QHash<QString,AlertChannel*> _channels;
  QList<AlertRule> _rules;
  QHash<QString,QDateTime> _raisedAlerts; // alert + raise time
  QHash<QString,QDateTime> _soonCanceledAlerts; // alert + scheduled cancel time
  int _cancelDelay;

public:
  explicit Alerter();
  ~Alerter();
  bool loadConfiguration(PfNode root, QString &errorString);
  /** This method is threadsafe. */
  void emitAlert(QString alert);
  /** This method is threadsafe. */
  void raiseAlert(QString alert);
  /** This method is threadsafe. */
  void cancelAlert(QString alert);
  /** This method is threadsafe. */
  ParamSet params() const { return _params; }

signals:
  void alertRaised(QString alert);
  void alertCanceled(QString alert);
  void alertCancellationScheduled(QString alert, QDateTime scheduledTime);
  void alertEmited(QString alert);
  void alertCancellationEmited(QString alert);
  void paramsChanged(ParamSet params);
  void rulesChanged(QList<AlertRule> rules);

private slots:
  void processCancellation();

private:
  Q_INVOKABLE void doEmitAlert(QString alert);
  void doEmitAlertCancellation(QString alert);
  Q_INVOKABLE void doRaiseAlert(QString alert);
  Q_INVOKABLE void doCancelAlert(QString alert);
  void sendMessage(Alert alert, bool cancellation);
};

#endif // ALERTER_H
