#include "log.h"
#include "filelogger.h"
#include <QList>
#include <QString>
#include <QDateTime>
#include <QRegExp>

static QList<FileLogger*> _loggers;

void Log::addLogger(FileLogger *logger) {
  _loggers.append(logger);
}

void Log::clearLoggers() {
  _loggers.clear();
}

void Log::log(const QString message, Severity severity, const QString task,
              const QString execId, const QString sourceCode) {
  QDateTime now = QDateTime::currentDateTime();
  QString line = QString("%1 %2/%3 %4 %5 %6")
      .arg(now.toString(Qt::ISODate)).arg(task).arg(execId).arg(sourceCode)
      .arg(severityToString(severity)).arg(message);
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

QString Log::sanitize(const QString string) {
  QString s(string);
  s.replace(QRegExp("\\s"), "_");
  return s;
}

void Log::logMessageHandler(QtMsgType type, const char *msg) {
  switch (type) {
  case QtDebugMsg:
    Log::log(msg, Log::Debug, "?", "?");
    break;
  case QtWarningMsg:
    Log::log(msg, Log::Warning, "?", "?");
    break;
  case QtCriticalMsg:
    Log::log(msg, Log::Error, "?", "?");
    break;
  case QtFatalMsg:
    Log::log(msg, Log::Fatal, "?", "?");
  }
}
