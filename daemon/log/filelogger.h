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
