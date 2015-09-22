/* Copyright 2013-2014 Hallowyn and others.
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
#include "logfile.h"
#include <QSharedData>

class LogFileData : public QSharedData {
public:
  QString _pathPattern;
  Log::Severity _minimumSeverity;
  bool _buffered;
  LogFileData(QString pathPattern = QString(),
              Log::Severity minimumSeverity = Log::Debug,
              bool buffered = true)
    : _pathPattern(pathPattern), _minimumSeverity(minimumSeverity),
      _buffered(buffered) { }
};

LogFile::LogFile(QString pathPattern, Log::Severity minimumSeverity,
                 bool buffered)
  : d(new LogFileData(pathPattern, minimumSeverity, buffered)) {
}

LogFile::LogFile(const LogFile &rhs) : d(rhs.d) {
}

LogFile &LogFile::operator=(const LogFile &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

LogFile::~LogFile() {
}

QString LogFile::pathPattern() const {
  return d->_pathPattern;
}

Log::Severity LogFile::minimumSeverity() const {
  return d->_minimumSeverity;
}

bool LogFile::buffered() const {
  return d->_buffered;
}

PfNode LogFile::toPfNode() const {
  if (!d)
    return PfNode();
  PfNode node("log");
  node.appendChild(PfNode("file", d->_pathPattern));
  node.appendChild(PfNode("level", Log::severityToString(d->_minimumSeverity)));
  if (!d->_buffered)
    node.appendChild(PfNode("unbuffered"));
  return node;
}
