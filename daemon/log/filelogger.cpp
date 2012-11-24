#include "filelogger.h"
#include <QMetaObject>
#include <QFile>
#include <QtDebug>
#include <QThread>

FileLogger::FileLogger(QIODevice *device, Log::Severity minSeverity,
                       QObject *threadParent)
  : QObject(0), _device(0), _thread(new QThread(threadParent)),
    _minSeverity(minSeverity) {
  qDebug() << "creating FileLogger from device" << device;
  if (_device)
    delete _device;
  _device = device;
  _device->setParent(this);
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  if (!_device->isOpen()) {
    if (!_device->open(QIODevice::WriteOnly|QIODevice::Append
                       |QIODevice::Unbuffered)) {
      qWarning() << "cannot open log device" << _device << ":"
                 << _device->errorString();
      delete _device;
      _device = 0;
    }
  }
}

FileLogger::FileLogger(const QString path, Log::Severity minSeverity,
                       QObject *threadParent)
  : QObject(0), _device(0), _thread(new QThread(threadParent)),
    _minSeverity(minSeverity) {
  qDebug() << "creating FileLogger from path" << path;
  // TODO parameters substitution (to allow date/time in path)
  if (_device)
    delete _device;
  _device = new QFile(path, this);
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  // LATER open in another thread (maybe during reopen)
  if (!_device->open(QIODevice::WriteOnly|QIODevice::Append
                     |QIODevice::Unbuffered)) {
    qWarning() << "cannot open log file" << path << ":"
               << _device->errorString();
    _device->deleteLater();
    _device = 0;
  } else
    qDebug() << "opened log file" << path;
}

FileLogger::~FileLogger() {
  if (_device)
    delete _device;
}

void FileLogger::log(Log::Severity severity, QString line) {
  // force asynchronous call to protect against i/o latency: slow disk,
  // NFS stall (for those fool enough to write logs over NFS), etc.
  //qDebug() << "***log" << line;
  if (severity >= _minSeverity)
    QMetaObject::invokeMethod(this, "doLog", Qt::QueuedConnection,
                              Q_ARG(QString, line));
}

void FileLogger::doLog(const QString line) {
  // TODO file reopen (to allow log rotation and date/time in path)
  //qDebug() << "***doLog" << line;
  if (_device) {
    QByteArray ba = line.toUtf8();
    ba.append("\n");
    if (_device->write(ba) != ba.size()) {
      qWarning() << "error while writing log:" << _device
                 << _device->errorString();
      qWarning() << line;
    }
  } else {
    qWarning() << "error while writing log: null log device";
    qWarning() << line;
  }
}
