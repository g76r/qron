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
#include "httpevent.h"
#include "event_p.h"
#include <QUrl>
#include <QNetworkAccessManager>

class HttpEventData : public EventData {
public:
  QString _url;
  ParamSet _params;
  HttpEventData(const QString url = QString(),
                const ParamSet params = ParamSet())
    : _url(url), _params(params) { }
  void trigger(const ParamsProvider *context) const {
    QUrl url(_params.evaluate(_url, context));
    // LATER implement http event
  }
  QString toString() const {
    return "http{"+_url+"}";
  }
};

HttpEvent::HttpEvent(const QString url, const ParamSet params)
  : Event(new HttpEventData(url, params)) {
}

HttpEvent::HttpEvent(const HttpEvent &rhs) : Event(rhs) {
}

HttpEvent::~HttpEvent() {
}
