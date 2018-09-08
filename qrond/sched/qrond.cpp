/* Copyright 2013-2016 Hallowyn and others.
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
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

Q_GLOBAL_STATIC(Qrond, qrondInstance)

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
  //connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
  //        _httpd, SLOT(deleteLater()));
  //connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
  //        _scheduler, SLOT(deleteLater()));
}

Qrond::~Qrond() {
}

Qrond *Qrond::instance() {
  return qrondInstance();
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
    return; // TODO clean up the whole shutdown sequence
    //QMetaObject::invokeMethod(qrondInstance(), "shutdown",
    //                          Qt::QueuedConnection, Q_ARG(int, 1));
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
    QMetaObject::invokeMethod(this, "doLoadConfig",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result));
  return result;
}

bool Qrond::doLoadConfig() {
  bool success = false;
  if (!_configFilePath.isEmpty()) {
    Log::info() << "loading configuration from file: " << _configFilePath;
    // LATER support a config directory like /etc/qron.d rather only one file
    QFile file(_configFilePath);
    SchedulerConfig config = _configRepository->parseConfig(&file, true);
    if (config.isNull()) {
      Log::error() << "cannot load configuration from file: "
                   << _configFilePath;
    } else {
      _configRepository->addAndActivate(config);
      success = true;
    }
  }
  return success;
}

void Qrond::systemTriggeredShutdown(int returnCode, QString actor) {
  Log::info() << "AUDIT action: 'shutdown' actor: '"<< actor << "'";
  shutdown(returnCode);
}

void Qrond::shutdown(int returnCode) {
  if (this->thread() == QThread::currentThread())
    doShutdown(returnCode);
  else
    QMetaObject::invokeMethod(this, "doShutdown",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(int, returnCode));
}

void Qrond::doShutdown(int returnCode) {
  Log::info() << "qrond is shuting down";
  // TODO wait for running tasks while starting new ones is disabled
  // delete HttpServer and Scheduler
  // WebConsole will be deleted since HttpServer connects its deleteLater()
  _httpd->deleteLater(); // cannot be a child because it lives it its own thread
  // give a chance to WebConsole to fully shutdown before Scheduler deletion
  ::usleep(100000); // TODO remove
  _scheduler->deleteLater(); // cant be a child cause it lives it its own thread
  // give a chance for last main loop events, incl. QThread::deleteLater() for
  // HttpServer, Scheduler and children
  ::usleep(100000); // TODO remove
  // shutdown main thread and stop QCoreApplication::exec() in main()
  QCoreApplication::exit(returnCode);
  // Qrond instance will be deleted by Q_GLOBAL_STATIC
}

#ifdef Q_OS_UNIX
static QMutex signalHandlerMutex;
static bool shutingDown(false);

static void signal_handler(int signal_number) {
  //qDebug() << "signal" << signal_number;
  QMutexLocker ml(&signalHandlerMutex);
  if (shutingDown)
    return;
  switch (signal_number) {
  case SIGHUP:    
    QMetaObject::invokeMethod(qrondInstance(), "systemTriggeredLoadConfig",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "signal"));
    break;
  case SIGTERM:
  case SIGINT:
    shutingDown = true;
    QMetaObject::invokeMethod(qrondInstance(), "systemTriggeredShutdown",
                              Qt::QueuedConnection,
                              Q_ARG(int, 0),
                              Q_ARG(QString, "signal"));
    break;
  }
}
#endif

int main(int argc, char *argv[]) {
  qsrand(time(0));
  QCoreApplication a(argc, argv);
  QThread::currentThread()->setObjectName("MainThread");
  Log::wrapQtLogToSamePattern();
  Log::addConsoleLogger(Log::Info, true);
  QStringList args;
  for (int i = 1; i < argc; ++i)
    args << QString::fromUtf8(argv[i]);
  qrondInstance()->startup(args);
#ifdef Q_OS_UNIX
  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = signal_handler;
  sigaction(SIGHUP, &action, 0);
  sigaction(SIGTERM, &action, 0);
  sigaction(SIGINT, &action, 0);
#endif
  // LATER truly daemonize on Unix (pidfile...)
  // LATER servicize on Windows
  int rc = a.exec();
#ifdef Q_OS_UNIX
  action.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &action, 0);
  sigaction(SIGTERM, &action, 0);
  sigaction(SIGINT, &action, 0);
#endif
  // TODO may we turn log synchronous rather than call usleep ?
  ::usleep(100000); // give a chance for last asynchronous log writing
  // LATER removeLoggers should not be called but rather managed as a singleton in libqtssu
  // anyway some loggers are not deleted by removeLoggers
  //Log::removeLoggers();
  return rc;
}
