/* Copyright 2013-2022 Hallowyn and others.
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
#include "qrond.h"
#include <QCoreApplication>
#include <QFile>
#include <QtDebug>
#include "log/log.h"
#include "log/filelogger.h"
#include <QThread>
#include <unistd.h>
#include "httpd/pipelinehttphandler.h"
#include <stdlib.h>
#include <time.h>
#include "io/unixsignalmanager.h"

static QMutex *_instanceMutex = new QMutex;
static Qrond *_instance = nullptr;

Qrond *Qrond::instance() {
  QMutexLocker locker(_instanceMutex);
  if (!_instance)
    _instance = new Qrond;
  return _instance;
}

Qrond::Qrond(QObject *parent) : QObject(parent),
  _webconsoleAddress(QHostAddress::Any), _webconsolePort(8086),
  _scheduler(new Scheduler),
  _httpd(new HttpServer(32, 128)), // LATER should be configurable
  _httpAuthRealm("qron"),
  _authorizer(new InMemoryRulesAuthorizer(this)),
  _configRepository(new LocalConfigRepository(this, _scheduler)),
  _webconsole(new WebConsole) {
  _webconsole->setScheduler(_scheduler);
  _webconsole->setConfigRepository(_configRepository);
  PipelineHttpHandler *pipeline = new PipelineHttpHandler;
  _httpAuthHandler = new BasicAuthHttpHandler;
  _httpAuthHandler->setAuthenticator(_scheduler->authenticator());
  _authorizer->setUsersDatabase(_scheduler->usersDatabase());
  _httpAuthHandler->setAuthorizer(_authorizer);
  _webconsole->setAuthorizer(_authorizer);
  pipeline->appendHandler(_httpAuthHandler);
  pipeline->appendHandler(_webconsole);
  _httpd->appendHandler(pipeline);
  connect(_configRepository, &LocalConfigRepository::configActivated,
          _scheduler, &Scheduler::activateConfig);
  connect(UnixSignalManager::instance(), &UnixSignalManager::signalCaught,
          this, &Qrond::signalCaught);
#ifdef Q_OS_UNIX
  UnixSignalManager::addToCatchList({ SIGINT, SIGTERM, SIGHUP });
#endif
}

Qrond::~Qrond() {
}

void Qrond::startup(QStringList args) {
  int n = args.size();
  for (int i =0; i < n; ++i) {
    const QString &arg = args[i];
    if (i < n-1 && (arg == "--http-address")) {
      _webconsoleAddress.setAddress(args[++i]);
    } else if (i < n-1 && (arg == "--http-port")) {
      int p = QString(args[++i]).toInt();
      if (p >= 1 && p <= 65535)
        _webconsolePort = p;
      else
        Log::error() << "bad port number: " << arg
                     << " using default instead (8086)";
    } else if (i < n-1 && (arg == "--config-file")) {
       _configFilePath = args[++i];
    } else if (i < n-1 && (arg == "--config-repository")) {
       _configRepoPath = args[++i];
    } else if (i < n-1 && arg == "--http-auth-realm") {
       _httpAuthRealm = args[++i];
       _httpAuthRealm.replace("\"", "'");
    } else {
      Log::warning() << "unknown command line parameter: " << arg;
    }
  }
  _webconsole->setConfigPaths(_configFilePath, _configRepoPath);
  _httpAuthHandler->setRealm(_httpAuthRealm);
  if (!_httpd->listen(_webconsoleAddress, _webconsolePort))
    Log::error() << "cannot start webconsole on "
                 << _webconsoleAddress.toString() << ":" << _webconsolePort
                 << ": " << _httpd->errorString();
  if (!_configRepoPath.isEmpty())
    _configRepository->openRepository(_configRepoPath);
  if (!_configFilePath.isEmpty())
    systemTriggeredLoadConfig("startup");
  if (_configRepository->activeConfigId().isNull()) {
    Log::fatal() << "cannot load configuration";
    Log::fatal() << "qrond is aborting startup sequence";
    return;
  }
}

bool Qrond::systemTriggeredLoadConfig(QString actor) {
  bool result = loadConfig();
  Log::info() << "AUDIT action: 'reload_config_file' "
              << (result ? "result: success" : "result: failure")
              << " actor: '" << actor << "'";
  return result;
}

bool Qrond::loadConfig() {
  bool result = false;
  if (this->thread() == QThread::currentThread())
    result = doLoadConfig();
  else
    QMetaObject::invokeMethod(this, [this,&result]() {
      result = doLoadConfig();
    }, Qt::BlockingQueuedConnection);
  return result;
}

bool Qrond::doLoadConfig() {
  if (_configFilePath.isEmpty())
    return false;
  Log::info() << "loading configuration from file: " << _configFilePath;
  // LATER support a config directory like /etc/qron.d rather only one file
  QFile file(_configFilePath);
  SchedulerConfig config = _configRepository->parseConfig(&file, true);
  if (config.isNull()) {
    Log::error() << "cannot load configuration from file: "
                 << _configFilePath;
    return false;
  }
  if (ParamsProvider::environment()
      ->paramValue("DISABLE_TASKS_ON_CREATION").toBool()) {
    for (Task &t : config.tasks().values()) {
      t.setEnabled(false);
      //emit itemChanged(t, t, QStringLiteral("task"));
    }
  }
  _configRepository->addAndActivate(config);
  return true;
}

void Qrond::systemTriggeredShutdown(int returnCode, QString actor) {
  Log::info() << "AUDIT action: 'shutdown' actor: '"<< actor << "'";
  shutdown(returnCode);
}

void Qrond::shutdown(int returnCode) {
  if (this->thread() == QThread::currentThread())
    doShutdown(returnCode);
  else
    asyncShutdown(returnCode);
}

void Qrond::asyncShutdown(int returnCode) {
  QMetaObject::invokeMethod(this, [this,returnCode]() {
    doShutdown(returnCode);
  }, Qt::QueuedConnection);
}

void Qrond::doShutdown(int returnCode) {
  if (_shutingDown)
    return;
  _shutingDown = true;
  Log::info() << "qrond is shuting down";
  // wait for running tasks while starting new ones is disabled
  _scheduler->shutdown();
  // delete HttpServer and Scheduler
  // WebConsole will be deleted since HttpServer connects its deleteLater()
  _httpd->deleteLater(); // cannot be a child because it lives it its own thread
  // give a chance to WebConsole to fully shutdown before Scheduler deletion
  ::usleep(100000); // TODO replace with lambda connected on destroyed() ?
  _scheduler->deleteLater(); // cant be a child cause it lives it its own thread
  // give a chance for last main loop events, incl. QThread::deleteLater() for
  // HttpServer, Scheduler and children
  ::usleep(100000); // TODO replace with lambda connected on destroyed() ?
  // shutdown main thread and stop QCoreApplication::exec() in main()
  QCoreApplication::exit(returnCode);
  // Qrond instance will be deleted by Q_GLOBAL_STATIC
}

void Qrond::signalCaught(int signal_number) {
  if (_shutingDown)
    return;
  switch (signal_number) {
#ifdef Q_OS_UNIX
    case SIGTERM:
    case SIGINT:
      systemTriggeredShutdown(0, "signal");
      break;
    case SIGHUP:
      systemTriggeredLoadConfig("signal");
      break;
#endif
    default:
      Log::warning() << "Qrond received unexpected unix signal: "
                     << signal_number;
  }
}

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  Log::wrapQtLogToSamePattern();
  Log::addConsoleLogger(Log::Debug, true);
  QThread::currentThread()->setObjectName("MainThread");
  QStringList args;
  for (int i = 1; i < argc; ++i)
    args << QString::fromLocal8Bit(argv[i]);
  Qrond::instance()->startup(args);
  int rc = a.exec();
  Log::info() << "qrond exiting with status " << rc;
  QThread::usleep(100'000); // let last log entries be written
  Log::shutdown();
  QThread::usleep(100'000); // let last log entries be written
  qInfo() << "qrond exiting with status" << rc;
  return rc;
}
