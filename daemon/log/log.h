#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QtDebug>
#include <QVariant>
#include <QStringList>

class FileLogger;
class LogHelper;

// LATER handle default task/id

/** This class provides a server-side log facility with common server-side
  * severities (whereas QtDebug does not) and write timestamped log files.
  */
class Log {
public:
  enum Severity { Debug, Info, Warning, Error, Fatal };
  static void addLogger(FileLogger *logger);
  static void clearLoggers();
  static void log(const QString message, Severity severity = Info,
                  const QString task = "?", const QString execId = "?",
                  const QString sourceCode = ":");
  static const QString severityToString(Severity severity);
  static void logMessageHandler(QtMsgType type, const char *msg);
  static inline LogHelper debug(const QString task = "?",
                                const QString execId = "?",
                                const QString sourceCode = ":");
  static inline LogHelper info(const QString task = "?",
                               const QString execId = "?",
                               const QString sourceCode = ":");
  static inline LogHelper warning(const QString task = "?",
                                  const QString execId = "?",
                                  const QString sourceCode = ":");
  static inline LogHelper error(const QString task = "?",
                                const QString execId = "?",
                                const QString sourceCode = ":");
  static inline LogHelper fatal(const QString task = "?",
                                const QString execId = "?",
                                const QString sourceCode = ":");
  static inline LogHelper debug(const QString task, quint64 execId,
                                const QString sourceCode = ":");
  static inline LogHelper info(const QString task, quint64 execId,
                               const QString sourceCode = ":");
  static inline LogHelper warning(const QString task, quint64 execId,
                                  const QString sourceCode = ":");
  static inline LogHelper error(const QString task, quint64 execId,
                                const QString sourceCode = ":");
  static inline LogHelper fatal(const QString task, quint64 execId,
                                const QString sourceCode = ":");

private:
  static inline QString sanitize(const QString string);
};

class LogHelper {
  Log::Severity _severity;
  QString _message, _task, _execId, _sourceCode;
public:
  inline LogHelper(Log::Severity severity, const QString task,
                   const QString execId, const QString sourceCode)
    : _severity(severity), _task(task), _execId(execId),
      _sourceCode(sourceCode) { }
  inline ~LogHelper() {
    Log::log(_message, _severity, _task, _execId, _sourceCode); }
  inline LogHelper &operator <<(const QString &o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(const QLatin1String &o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(const QStringRef &o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(const QByteArray &o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(const QChar &o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(const char *o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(char o) {
    _message += o; return *this; }
  inline LogHelper &operator <<(qint64 o) {
    _message += QString::number(o); return *this; }
  inline LogHelper &operator <<(quint64 o) {
    _message += QString::number(o); return *this; }
  inline LogHelper &operator <<(qint32 o) {
    _message += QString::number(o); return *this; }
  inline LogHelper &operator <<(quint32 o) {
    _message += QString::number(o); return *this; }
  inline LogHelper &operator <<(double o) {
    _message += QString::number(o); return *this; }
  inline LogHelper &operator <<(const QVariant &o) {
    _message += o.toString(); return *this; }
  inline LogHelper &operator <<(const QStringList &o) {
    _message += "{ ";
    foreach (const QString &s, o) {
      _message += "\"";
      _message += s;
      _message += "\" ";
    }
    _message += "}";
    return *this; }
  inline LogHelper &operator <<(const QObject *o) {
    if (o) {
      _message += o->objectName();
      _message += "(0x";
      _message += QString::number((quint64)o, 16);
      _message += ")";
    } else
      _message += "null";
    return *this; }
  inline LogHelper &operator <<(const QObject &o) {
    return operator <<(&o); }
  inline LogHelper &operator <<(const void *o) {
    _message += "0x";
    _message += QString::number((quint64)o, 16);
    return *this; }
};

LogHelper Log::debug(const QString task, const QString execId,
                     const QString sourceCode) {
  return LogHelper(Log::Debug, task, execId, sourceCode);
}

LogHelper Log::info(const QString task, const QString execId,
                     const QString sourceCode) {
  return LogHelper(Log::Info, task, execId, sourceCode);
}

LogHelper Log::warning(const QString task, const QString execId,
                     const QString sourceCode) {
  return LogHelper(Log::Warning, task, execId, sourceCode);
}

LogHelper Log::error(const QString task, const QString execId,
                     const QString sourceCode) {
  return LogHelper(Log::Error, task, execId, sourceCode);
}

LogHelper Log::fatal(const QString task, const QString execId,
                     const QString sourceCode) {
  return LogHelper(Log::Fatal, task, execId, sourceCode);
}

LogHelper Log::debug(const QString task, quint64 execId,
                     const QString sourceCode) {
  return debug(task, QString::number(execId), sourceCode);
}

LogHelper Log::info(const QString task, quint64 execId,
                    const QString sourceCode) {
  return info(task, QString::number(execId), sourceCode);
}

LogHelper Log::warning(const QString task, quint64 execId,
                       const QString sourceCode) {
  return warning(task, QString::number(execId), sourceCode);
}

LogHelper Log::error(const QString task, quint64 execId,
                     const QString sourceCode) {
  return error(task, QString::number(execId), sourceCode);
}

LogHelper Log::fatal(const QString task, quint64 execId,
                     const QString sourceCode) {
  return fatal(task, QString::number(execId), sourceCode);
}

/*
#define LOG_TASK_LOG(message, task, execId, severity) \
  Log::log(message, severity, task, execId, \
  QString(__FILE__":%1").arg(__LINE__))
#define LOG_TASK_DEBUG(message, task, execId) \
  LOG_TASK_LOG(message, task, execId, Log::Debug)
#define LOG_TASK_INFO(message, task, execId) \
  LOG_TASK_LOG(message, task, execId, Log::Info)
#define LOG_TASK_WARNING(message, task, execId) \
  LOG_TASK_LOG(message, task, execId, Log::Warning)
#define LOG_TASK_ERROR(message, task, execId) \
  LOG_TASK_LOG(message, task, execId, Log::Error)
#define LOG_TASK_FATAL(message, task, execId) \
  LOG_TASK_LOG(message, task, execId, Log::Fatal)
#define LOG_LOG(message, severity)  Log::log(message, severity)
#define LOG_DEBUG(message) LOG_LOG(message, Log::Debug)
#define LOG_INFO(message) LOG_LOG(message, Log::Info)
#define LOG_WARNING(message) LOG_LOG(message, Log::Warning)
#define LOG_ERROR(message) LOG_LOG(message, Log::Error)
#define LOG_FATAL(message) LOG_LOG(message, Log::Fatal)
*/

#endif // LOG_H
