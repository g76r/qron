/* Copyright 2013-2014 Hallowyn and others.
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
#include "httpaction.h"
#include "action_p.h"
#include <QUrl>
#include <QNetworkAccessManager>

class HttpActionData : public ActionData {
public:
  QString _url;
  ParamSet _params;
  HttpActionData(QString url = QString(), ParamSet params = ParamSet())
    : _url(url), _params(params) { }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance instance) const {
    Q_UNUSED(subscription)
    Q_UNUSED(eventContext)
    Q_UNUSED(instance)
    // LATER implement http action
    Log::error() << "HttpAction not implemented";
  }
  QString toString() const {
    return "http{"+_url+"}";
  }
  QString actionType() const {
    return "http";
  }
};

HttpAction::HttpAction(QString url, ParamSet params)
  : Action(new HttpActionData(url, params)) {
}

HttpAction::HttpAction(const HttpAction &rhs) : Action(rhs) {
}

HttpAction::~HttpAction() {
}
