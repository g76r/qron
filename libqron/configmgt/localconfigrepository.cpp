/* Copyright 2014 Hallowyn and others.
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
#include "localconfigrepository.h"
#include <QtDebug>
#include "log/log.h"
#include <QSaveFile>
#include "csv/csvfile.h"

LocalConfigRepository::LocalConfigRepository(
    QObject *parent, Scheduler *scheduler, QString basePath)
  : ConfigRepository(parent, scheduler), _historyLog(new CsvFile(this)) {
  _historyLog->setFieldSeparator(' ');
  if (!basePath.isEmpty())
    openRepository(basePath);
}

void LocalConfigRepository::openRepository(QString basePath) {
  QMutexLocker locker(&_mutex);
  _basePath = QString();
  _activeConfigId = QString();
  _configs.clear();
  // create repository subdirectories if needed
  QDir configsDir(basePath+"/configs");
  QDir errorsDir(basePath+"/errors");
  if (!QDir().mkpath(configsDir.path()))
    Log::error() << "cannot access or create directory " << configsDir.path();
  if (!QDir().mkpath(errorsDir.path()))
    Log::error() << "cannot access or create directory " << errorsDir.path();
  // read active config id
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
  // read and load configs
  QFileInfoList fileInfos =
      configsDir.entryInfoList(QDir::Files|QDir::NoSymLinks,
                               QDir::Time|QDir::Reversed);
  // TODO limit number of loaded config and/or purge oldest ones and/or move oldest into an 'archive' subdir
  foreach (const QFileInfo &fi, fileInfos) {
    QFile f(fi.filePath());
    //qDebug() << "LocalConfigRepository::openRepository" << fi.filePath();
    if (f.open(QIODevice::ReadOnly)) {
      SchedulerConfig config = parseConfig(&f, false);
      if (config.isNull()) {
        Log::warning() << "moving incorect config file to 'errors' subdir: "
                       << fi.fileName();
        QFile(errorsDir.path()+"/"+fi.fileName()).remove();
        if (!f.rename(errorsDir.path()+"/"+fi.fileName()))
          Log::error() << "cannot move incorect config file to 'errors' "
                          "subdir: " << fi.fileName();
        continue; // ignore incorrect config files
      }
      QString configId = config.hash();
      if (_configs.contains(configId)) {
        Log::warning() << "moving duplicated config file to 'errors' subdir: "
                       << fi.fileName();
        QFile(errorsDir.path()+"/"+fi.fileName()).remove();
        if (!f.rename(errorsDir.path()+"/"+fi.fileName()))
          Log::error() << "cannot move duplicated config file to 'errors' "
                          "subdir: " << fi.fileName();
        continue; // ignore duplicated config file
      }
      if (configId != fi.baseName()) {
        QString rightPath = fi.path()+"/"+configId;
        Log::warning() << "renaming config file because id mismatch: "
                       << fi.fileName() << " to " << rightPath;
        if (!f.rename(rightPath)) {
          // this is only a warning since the file can already exist and be a
          // duplicated (in this case the duplicate will be moved to error and
          // the renaming will succeed next time the repo will be opened)
          Log::warning() << "cannot rename file: " << fi.fileName() << " to "
                         << rightPath;
        }
      }
      _configs.insert(configId, config);
      //qDebug() << "***************** openRepository" << configId;
      emit configAdded(configId, config);
    } else {
      Log::error() << "cannot open file in config repository: "
                   << fi.filePath() << ": " << f.errorString();
    }
  }
  //qDebug() << "***************** contains" << activeConfigId
  //         << _configs.contains(activeConfigId) << _configs.size();
  // read and load history
  if (_historyLog->open(basePath+"/history", QIODevice::ReadWrite)) {
    if (_historyLog->header(0) != "Timestamp") {
      QStringList headers;
      headers << "Timestamp" << "Event" << "ConfigId";
      _historyLog->setHeaders(headers);
    }
    int rowCount = _historyLog->rowCount();
    int rowCountMax = 1024; // TODO parametrize
    if (rowCount > rowCountMax) { // shorten history when too long
      _historyLog->removeRows(0, rowCount-rowCountMax-1);
      rowCount = _historyLog->rowCount();
    }
    QList<ConfigHistoryEntry> history;
    for (int i = 0; i < rowCount; ++i) {
      const QStringList row = _historyLog->row(i);
      if (row.size() < 3) {
        Log::error() << "ignoring invalid config history row #" << i;
        continue;
      }
      QDateTime ts = QDateTime::fromString(row[0], Qt::ISODate);
      history.append(ConfigHistoryEntry(QString::number(i), ts, row[1],
                     row[2]));
    }
    emit historyReset(history);
  } else {
    Log::error() << "cannot open history log: " << basePath << "/history";
  }
  _basePath = basePath;
  // activate active config
  if (_configs.contains(activeConfigId)) {
    SchedulerConfig config = _configs.value(_activeConfigId = activeConfigId);
    config.applyLogConfig();
    emit configActivated(_activeConfigId, config);
  } else {
    Log::error() << "active configuration do not exist: " << activeConfigId;
  }
}

QStringList LocalConfigRepository::availlableConfigIds() {
  QMutexLocker locker(&_mutex);
  return _configs.keys();
}

QString LocalConfigRepository::activeConfigId() {
  QMutexLocker locker(&_mutex);
  return _activeConfigId;
}

SchedulerConfig LocalConfigRepository::config(QString id) {
  QMutexLocker locker(&_mutex);
  return _configs.value(id);
}

QString LocalConfigRepository::addConfig(SchedulerConfig config) {
  QMutexLocker locker(&_mutex);
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
    recordInHistory("addConfig", id);
    _configs.insert(id, config);
    //qDebug() << "***************** addConfig" << id;
    emit configAdded(id, config);
  }
  return id;
}

bool LocalConfigRepository::activateConfig(QString id) {
  QMutexLocker locker(&_mutex);
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
      return false;
    }
    recordInHistory("activateConfig", id);
  }
  _activeConfigId = id;
  config.applyLogConfig();
  emit configActivated(_activeConfigId, config);
  //}
  return true;
}

bool LocalConfigRepository::removeConfig(QString id) {
  QMutexLocker locker(&_mutex);
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
      return false;
    }
    recordInHistory("removeConfig", id);
  }
  _configs.remove(id);
  emit configRemoved(id);
  return true;
}

void LocalConfigRepository::recordInHistory(
    QString event, QString configId) {
  QDateTime now = QDateTime::currentDateTime();
  ConfigHistoryEntry entry(QString::number(_historyLog->rowCount()), now, event,
                           configId);
  QStringList row(now.toString(Qt::ISODate));
  row.append(event);
  row.append(configId);
  _historyLog->appendRow(row);
  emit historyEntryAppended(entry);
}
