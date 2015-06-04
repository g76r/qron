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
#ifndef PARAMETRIZEDUDPSENDER_H
#define PARAMETRIZEDUDPSENDER_H

#include <QUdpSocket>
#include "util/paramset.h"

class ParametrizedUdpSender : public QUdpSocket {
  Q_OBJECT
  Q_DISABLE_COPY(ParametrizedUdpSender)
  QString _host;
  quint16 _port;
  int _connectTimeout, _disconnectTimeout;
  QString  _payloadFromParams, _logTask, _logExecId;
  ParamSet _params;

public:
  /** @param url e.g. "localhost:53", "udp:127.0.0.1:53"
   * @param logTask only used in log, e.g. task id
   * @param logExecId only used in log, e.g. task instance id
   */
  ParametrizedUdpSender(QObject *parent, QString url, ParamSet params,
                        ParamsProvider *paramsEvaluationContext = 0,
                        QString logTask = QString(),
                        quint64 logExecId = 0);
  /** @param url e.g. "localhost:53", "udp:127.0.0.1:53"
   * @param logTask only used in log, e.g. task id
   * @param logExecId only used in log, e.g. task instance id
   */
  ParametrizedUdpSender(QString url, ParamSet params,
                        ParamsProvider *paramsEvaluationContext = 0,
                        QString logTask = QString(),
                        quint64 logExecId = 0);
  /** @param payload if not set, use "payload" parameter content instead
   * @return false if the request cannot be performed, e.g. unknown address */
  bool performRequest(QString payload = QString(),
                      ParamsProvider *payloadEvaluationContext = 0);

private:
  void init(QString url, ParamSet params,
            ParamsProvider *paramsEvaluationContext, QString logTask,
            quint64 logExecId);
};

#endif // PARAMETRIZEDUDPSENDER_H
