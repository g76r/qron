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
#ifndef PARAMETRIZEDHTTPREQUEST_H
#define PARAMETRIZEDHTTPREQUEST_H

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include "util/paramset.h"
#include "httpd/httprequest.h"

/** Class extending QNetworkRequest to give easy ways to parametrize the
 * request using ParamSet parameters.
 * Supported parameters:
 * - "method" to set HTTP method (default: GET)
 * - "user" and "password" to set HTTP basic authentication
 * - "proto" to set network protocol (default: http)
 * - "port" to set TCP port number (overrinding the one specified in the url)
 * - "payload" to set the reuqest payload/body
 * - "content-type" to set payload (and header) content type
 */
class ParametrizedNetworkRequest : public QNetworkRequest {
  QString _logTask, _logExecId;
  HttpRequest::HttpRequestMethod _method;
  QString _payloadFromParams;
  ParamSet _params;

public:
  /**
   * @param logTask only used in log, e.g. task id
   * @param logExecId only used in log, e.g. task instance id
   */
  ParametrizedNetworkRequest(
      QString url, ParamSet params, ParamsProvider *paramsEvaluationContext = 0,
      QString logTask = QString(), quint64 logExecId = 0);
  /** @param payload if not set, use "payload" parameter content instead
   * @return 0 if the request cannot be performed, e.g. unknown method */
  QNetworkReply *performRequest(
      QNetworkAccessManager *nam, QString payload = QString(),
      ParamsProvider *payloadEvaluationContext = 0);
  static HttpRequest::HttpRequestMethod methodFromText(QString name);
};

#endif // PARAMETRIZEDHTTPREQUEST_H
