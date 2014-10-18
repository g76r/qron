#include "localconfigrepository.h"
#include <QtDebug>
#include "log/log.h"
#include <QSaveFile>

LocalConfigRepository::LocalConfigRepository(Scheduler *scheduler,
                                             QString basePath)
  : ConfigRepository(scheduler) {
  if (!basePath.isEmpty())
    openRepository(basePath);
}

void LocalConfigRepository::openRepository(QString basePath) {
  _basePath = QString();
  _currentId = QString();
  _configs.clear();
  QDir configsDir(basePath+"/configs");
  QDir errorsDir(basePath+"/errors");
  if (!QDir().mkpath(configsDir.path()))
    Log::error() << "cannot access or create directory " << configsDir.path();
  if (!QDir().mkpath(errorsDir.path()))
    Log::error() << "cannot access or create directory " << errorsDir.path();
  QFileInfoList fileInfos =
      configsDir.entryInfoList(QDir::Files|QDir::NoSymLinks,
                               QDir::Time|QDir::Reversed);
  QFile currentFile(basePath+"/current");
  QString currentId;
  if (currentFile.open(QIODevice::ReadOnly)) {
    if (currentFile.size() < 1024) { // arbitrary limit
      QByteArray hash = currentFile.readAll();
      currentId = QString::fromUtf8(hash);
    }
    currentFile.close();
  }
  if (currentId.isEmpty()) {
    if (currentFile.exists()) {
      Log::error() << "error reading current config: " << currentFile.fileName()
                   << ((currentFile.error() == QFileDevice::NoError)
                       ? "" : " : "+currentFile.errorString());
    } else {
      Log::warning() << "config repository lacking current config: "
                     << currentFile.fileName();
    }
  }
  foreach (const QFileInfo &fi, fileInfos) {
    QFile f(fi.filePath());
    qDebug() << "LocalConfigRepository::openRepository" << fi.filePath();
    if (f.open(QIODevice::ReadOnly)) {
      SchedulerConfig config = parseConfig(&f);
      if (config.isNull()) {
        Log::warning() << "moving incorect config file to 'errors' subdir: "
                       << fi.fileName();
        if (!f.rename(errorsDir.path()+"/"+fi.fileName()))
          Log::error() << "cannot move incorect config file to 'errors' "
                          "subdir: " << fi.fileName();
        continue; // ignore incorrect config files
      }
      QString configId = config.hash();
      if (_configs.contains(configId)) {
        Log::warning() << "moving duplicated config file to 'errors' subdir: "
                       << fi.fileName();
        if (!f.rename(errorsDir.path()+"/"+fi.fileName()))
          Log::error() << "cannot move duplicated config file to 'errors' "
                          "subdir: " << fi.fileName();
        continue; // ignore duplicated config file
      }
      // FIXME fix repo if hash and filename do not match: rename
      _configs.insert(configId, config);
      emit configAdded(configId, config);
    } else {
      Log::error() << "cannot open file in config repository: "
                   << fi.filePath() << ": " << f.errorString();
    }
  }
  if (_configs.contains(currentId))
    _currentId = currentId;
  else {
    Log::error() << "current configuration do not exist: " << currentId;
  }
  // FIXME fix current if needed (e.g. fixed target)
  // FIXME load history
  _basePath = basePath;
  emit currentConfigChanged(_currentId, _configs.value(_currentId));
}

QStringList LocalConfigRepository::availlableConfigIds() {
  return _configs.keys();
}

QString LocalConfigRepository::currentConfigId() {
  return _currentId;
}

SchedulerConfig LocalConfigRepository::config(QString id) {
  return _configs.value(id);
}

QString LocalConfigRepository::addConfig(SchedulerConfig config) {
  QString id = config.hash();
  if (!id.isNull() && !_configs.contains(id)) {
    if (!_basePath.isEmpty()) {
      QSaveFile f(_basePath+"/configs/"+id);
      if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)
          || config.writeAsPf(&f) < 0
          || !f.commit()) {
        Log::error() << "error writing config in repository: " << f.fileName()
                     << ((f.error() == QFileDevice::NoError)
                         ? "" : " : "+f.errorString());
        return QString();
      }
    }
    _configs.insert(id, config);
    emit configAdded(id, config);
  }
  return id;
}

void LocalConfigRepository::setCurrent(QString id) {
  SchedulerConfig config = _configs.value(id);
  id = config.isNull() ? QString() : id;
  if (id != _currentId || true) { // FIXME remove || true when config id is really implemented
    if (!_basePath.isEmpty()) {
      QSaveFile f(_basePath+"/current");
      if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)
          || f.write(id.toUtf8().append('\n')) < 0
          || !f.commit()) {
        Log::error() << "error writing config in repository: " << f.fileName()
                     << ((f.error() == QFileDevice::NoError)
                         ? "" : " : "+f.errorString());
        return; // FIXME right ?
      }
      // FIXME write history in repo
    }
    _currentId = id;
    emit currentConfigChanged(_currentId, config);
  }
}
