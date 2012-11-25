/* Copyright 2012 Hallowyn and others.
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
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef FILELOGGER_H
#define FILELOGGER_H

#include <QObject>
#include "log.h"

class QIODevice;
class QThread;

class FileLogger : public QObject {
  Q_OBJECT
  QIODevice *_device;
  QThread *_thread;
  Log::Severity _minSeverity;

public:
  /** Takes ownership of the device (= will delete it);
    * @param threadParent will be used as QThread parent
    */
  explicit FileLogger(QIODevice *device, Log::Severity minSeverity = Log::Info,
                      QObject *threadParent = 0);
  /** @param threadParent will be used as QThread parent
    */
  explicit FileLogger(const QString path, Log::Severity minSeverity = Log::Info,
                      QObject *threadParent = 0);
  ~FileLogger();
  
public slots:
  /** This method is thread-safe.
    */
  void log(Log::Severity severity, const QString line);

private:
  Q_INVOKABLE void doLog(const QString line);
};

#endif // FILELOGGER_H
