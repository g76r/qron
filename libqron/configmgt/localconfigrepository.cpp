#include "localconfigrepository.h"
#include <QtDebug>
#include "log/log.h"
#include <QSaveFile>

LocalConfigRepository::LocalConfigRepository(
    QObject *parent, Scheduler *scheduler, QString basePath)
  : ConfigRepository(parent, scheduler) {
  if (!basePath.isEmpty())
    openRepository(basePath);
}

void LocalConfigRepository::openRepository(QString basePath) {
  _basePath = QString();
  _activeConfigId = QString();
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
  QFile activeFile(basePath+"/active");
  QString activeConfigId;
  if (activeFile.open(QIODevice::ReadOnly)) {
    if (activeFile.size() < 1024) { // arbitrary limit
      QByteArray hash = activeFile.readAll().trimmed();
      activeConfigId = QString::fromUtf8(hash);
    }
    activeFile.close();
  }
  if (activeConfigId.isEmpty()) {
    if (activeFile.exists()) {
      Log::error() << "error reading active config: " << activeFile.fileName()
                   << ((activeFile.error() == QFileDevice::NoError)
                       ? "" : " : "+activeFile.errorString());
    } else {
      Log::warning() << "config repository lacking active config: "
                     << activeFile.fileName();
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
      //qDebug() << "***************** openRepository" << configId;
      emit configAdded(configId, config);
    } else {
      Log::error() << "cannot open file in config repository: "
                   << fi.filePath() << ": " << f.errorString();
    }
  }
  qDebug() << "***************** contains" << activeConfigId
           << _configs.contains(activeConfigId) << _configs.size();
  // FIXME fix active if needed (e.g. fixed target)
  // FIXME load history
  _basePath = basePath;
  if (_configs.contains(activeConfigId)) {
    _activeConfigId = activeConfigId;
    emit configActivated(_activeConfigId, _configs.value(_activeConfigId));
  } else {
    Log::error() << "active configuration do not exist: " << activeConfigId;
  }
}

QStringList LocalConfigRepository::availlableConfigIds() {
  return _configs.keys();
}

QString LocalConfigRepository::activeConfigId() {
  return _activeConfigId;
}

SchedulerConfig LocalConfigRepository::config(QString id) {
  return _configs.value(id);
}

QString LocalConfigRepository::addConfig(SchedulerConfig config) {
  QString id = config.hash();
  if (!id.isNull()) {
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
    //qDebug() << "***************** addConfig" << id;
    emit configAdded(id, config);
  }
  return id;
}

bool LocalConfigRepository::activateConfig(QString id) {
  // FIXME threadsafe
  SchedulerConfig config = _configs.value(id);
  if (config.isNull()) {
    Log::error() << "cannote activate config since it is not found in "
                    "repository: " << id;
    return false;
  }
  // LATER should avoid activating for real when already active ?
  //if (id != _activeConfigId) {
  if (!_basePath.isEmpty()) {
    QSaveFile f(_basePath+"/active");
    if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)
        || f.write(id.toUtf8().append('\n')) < 0
        || !f.commit()) {
      Log::error() << "error writing config in repository: " << f.fileName()
                   << ((f.error() == QFileDevice::NoError)
                       ? "" : " : "+f.errorString());
      return false; // FIXME should activate even on write error ?
    }
    // FIXME write history in repo
  }
  _activeConfigId = id;
  emit configActivated(_activeConfigId, config);
  //}
  return true;
}

bool LocalConfigRepository::removeConfig(QString id) {
  // FIXME threadsafe
  SchedulerConfig config = _configs.value(id);
  if (config.isNull()) {
    Log::error() << "cannote remove config since it is not found in "
                    "repository: " << id;
    return false;
  }
  if (id == _activeConfigId) {
    Log::error() << "cannote remove currently active config: " << id;
    return false;
  }
  if (!_basePath.isEmpty()) {
    QFile f(_basePath+"/configs/"+id);
    if (f.exists() && !f.remove()) {
      Log::error() << "error removing config from repository: " << f.fileName()
                   << ((f.error() == QFileDevice::NoError)
                       ? "" : " : "+f.errorString());
      return false; // FIXME should remove even on write error ?
    }
    // FIXME write history in repo
  }
  _configs.remove(id);
  emit configRemoved(id);
  return true;
}
