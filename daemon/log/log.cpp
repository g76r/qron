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
 * along with qron. If not, see <http://www.gnu.org/licenses/>.
 */
#include "log.h"
#include "filelogger.h"
#include <QList>
#include <QString>
#include <QDateTime>
#include <QRegExp>
#include <QThread>
#include <QMutex>
#include <QFile>

static QList<FileLogger*> _loggers;
static QMutex _loggersMutex;

void Log::addConsoleLogger() {
  QFile *console = new QFile;
  console->open(1, QIODevice::WriteOnly|QIODevice::Unbuffered);
  FileLogger *logger = new FileLogger(console, Log::Debug);
  Log::addLogger(logger);
}

void Log::addLogger(FileLogger *logger) {
  QMutexLocker locker(&_loggersMutex);
  if (logger) {
    if (_loggers.isEmpty())
      qInstallMsgHandler(Log::logMessageHandler);
    _loggers.append(logger);
  }
}

void Log::clearLoggers() {
  QMutexLocker locker(&_loggersMutex);
  foreach(FileLogger *logger, _loggers)
    logger->deleteLater();
  _loggers.clear();
}

void Log::replaceLoggers(FileLogger *newLogger) {
  QMutexLocker locker(&_loggersMutex);
  foreach(FileLogger *logger, _loggers)
    logger->deleteLater();
  _loggers.clear();
  if (newLogger)
    _loggers.append(newLogger);
  else
    qInstallMsgHandler(0);
}

void Log::log(const QString message, Severity severity, const QString task,
              const QString execId, const QString sourceCode) {
  QDateTime now = QDateTime::currentDateTime();
  QString realTask(task);
  if (realTask.isNull())
    realTask = QThread::currentThread()->objectName();
  QString line = QString("%1 %2/%3 %4 %5 %6")
      .arg(now.toString(Qt::ISODate))
      .arg(sanitize(realTask.isEmpty() ? "?" : realTask))
      .arg(execId.isEmpty() ? "0" : sanitize(execId))
      .arg(sourceCode.isEmpty() ? ":" : sanitize(sourceCode))
      .arg(severityToString(severity)).arg(message);
  //qDebug() << "***log" << line;
  QMutexLocker locker(&_loggersMutex);
  foreach (FileLogger *logger, _loggers)
    logger->log(severity, line);
}

const QString Log::severityToString(Severity severity) {
  switch (severity) {
  case Debug:
    return "DEBUG";
  case Info:
    return "INFO";
  case Warning:
    return "WARNING";
  case Error:
    return "ERROR";
  case Fatal:
    return "FATAL";
  }
  return "UNKNOWN";
}

Log::Severity Log::severityFromString(const QString string) {
  if (!string.isEmpty())
    switch (string.at(0).toAscii()) {
    case 'I':
    case 'i':
      return Info;
    case 'W':
    case 'w':
      return Warning;
    case 'E':
    case 'e':
      return Error;
    case 'F':
    case 'f':
      return Fatal;
    }
  return Debug;
}

QString Log::sanitize(const QString string) {
  QString s(string);
  // LATER optimize: avoid using a regexp 3 times per log line
  s.replace(QRegExp("\\s"), "_");
  return s;
}

void Log::logMessageHandler(QtMsgType type, const char *msg) {
  switch (type) {
  case QtDebugMsg:
    Log::log(msg, Log::Debug);
    break;
  case QtWarningMsg:
    Log::log(msg, Log::Warning);
    break;
  case QtCriticalMsg:
    Log::log(msg, Log::Error);
    break;
  case QtFatalMsg:
    Log::log(msg, Log::Fatal);
    // LATER shutdown process because default Qt message handler does shutdown
  }
}
