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
#ifndef LOGFILE_H
#define LOGFILE_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "log/log.h"
#include "pf/pfnode.h"

class LogFileData;

/** Log file definition. */
class LIBQRONSHARED_EXPORT LogFile {
  QSharedDataPointer<LogFileData> d;

public:
  /** @param pathPattern interpreted using Paramset::evaluate() hence can
   * contain placeholders like %!yyyy%!mm%!dd
   * @param buffered allow write buffering (both user space and system) */
  LogFile(QString pathPattern = QString(),
          Log::Severity minimumSeverity = Log::Debug, bool buffered = true);
  LogFile(const LogFile &);
  LogFile &operator=(const LogFile &);
  ~LogFile();
  QString pathPattern() const;
  Log::Severity minimumSeverity() const;
  bool buffered() const;
  PfNode toPfNode() const;
};

#endif // LOGFILE_H
