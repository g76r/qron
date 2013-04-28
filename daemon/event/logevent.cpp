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
#include "logevent.h"
#include "event_p.h"
#include "util/paramset.h"

class LogEventData : public EventData {
public:
  QString _message;
  Log::Severity _severity;
  LogEventData(const QString logMessage = QString(),
               Log::Severity severity = Log::Info)
    : _message(logMessage), _severity(severity) { }
  QString toString() const {
    return "log{"+Log::severityToString(_severity)+" "+_message+"}";
  }
  void trigger(const ParamsProvider *context) const {
    QString fqtn = context
        ? context->paramValue("!fqtn").toString() : QString();
    QString id = context
        ? context->paramValue("!taskrequestid").toString() : QString();
    Log::log(_severity, fqtn, id.toLongLong())
        << ParamSet().evaluate(_message, context);
  }
};

LogEvent::LogEvent(Log::Severity severity, const QString message)
  : Event(new LogEventData(message, severity)) {
}

LogEvent::LogEvent(const LogEvent &rhs) : Event(rhs) {
}

LogEvent::~LogEvent() {
}
