/* Copyright 2014 Hallowyn and others.
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
#include "parametrizedudpsender.h"
#include <QUrl>

ParametrizedUdpSender::ParametrizedUdpSender(
    QObject *parent, QString url, ParamSet params,
    ParamsProvider *paramsEvaluationContext, QString logTask, quint64 logExecId)
  : QUdpSocket(parent) {
  init(url, params, paramsEvaluationContext, logTask, logExecId);
}

ParametrizedUdpSender::ParametrizedUdpSender(
    QString url, ParamSet params, ParamsProvider *paramsEvaluationContext,
    QString logTask, quint64 logExecId)
  : QUdpSocket() {
  init(url, params, paramsEvaluationContext, logTask, logExecId);
}

void ParametrizedUdpSender::init(
    QString url, ParamSet params, ParamsProvider *paramsEvaluationContext,
    QString logTask, quint64 logExecId) {
  QUrl qurl(params.evaluate(url, paramsEvaluationContext));
  if (qurl.isValid()) {
    _host = qurl.host();
    _port = (quint16)qurl.port(0);
  } else {
    _port = 0;
  }
  _connectTimeout = params.valueAsDouble("connecttimeout", 2.0,
                                         paramsEvaluationContext)*1000;
  _disconnectTimeout = params.valueAsDouble("disconnecttimeout", .2,
                                            paramsEvaluationContext)*1000;
  _payloadFromParams = params.rawValue("payload");
  _logTask = logTask;
  _logExecId = logExecId ? QString::number(logExecId) : QString();
  _params = params;
}

/** @param payload if not set, use "payload" parameter content instead
 * @return false if the request cannot be performed, e.g. unknown address */
bool ParametrizedUdpSender::performRequest(
    QString payload, ParamsProvider *payloadEvaluationContext) {
  qint64 rc = -1;
  if (payload.isNull())
    payload = _payloadFromParams;
  payload = _params.evaluate(payload, payloadEvaluationContext);
  if (_host.isEmpty() || !_port)
    return false;
  connectToHost(_host, _port, QIODevice::WriteOnly);
  if (waitForConnected(_connectTimeout)) {
    rc = write(payload.toUtf8());
    if (rc < 0)
      Log::warning(_logTask, _logExecId)
          << "error when emiting UDP alert: " << error() << " "
          << errorString();
    //else
    //  Log::debug(_logTask, _logExecId)
    //      << "UDP packet sent on " << _host << ":" << _port << " for "
    //      << rc << " bytes: " << payload.size();
  } else {
    Log::warning(_logTask, _logExecId)
        << "timeout when emiting UDP alert: " << error() << " "
        << errorString();
  }
  disconnectFromHost();
  while(state() != QAbstractSocket::UnconnectedState
        && waitForBytesWritten(_disconnectTimeout))
    ;
  return rc >= 0;
}
