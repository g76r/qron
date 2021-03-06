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
#ifndef QROND_H
#define QROND_H

#include <QStringList>
#include "sched/scheduler.h"
#include "httpd/httpserver.h"
#include "wui/webconsole.h"
#include "httpd/basicauthhttphandler.h"
#include "configmgt/localconfigrepository.h"
#include "auth/inmemoryrulesauthorizer.h"

/** Operating system interface class.
  Mainly responsible for starting, shutting down and reloading the scheduler. */
class Qrond : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(Qrond)
  QHostAddress _webconsoleAddress;
  quint16 _webconsolePort;
  Scheduler *_scheduler;
  HttpServer *_httpd;
  QString _configRepoPath, _configFilePath, _httpAuthRealm;
  BasicAuthHttpHandler *_httpAuthHandler;
  InMemoryRulesAuthorizer *_authorizer;
  LocalConfigRepository *_configRepository;
  WebConsole *_webconsole;

public:
  explicit Qrond(QObject *parent = 0);
  ~Qrond();
  static Qrond *instance();
  Q_INVOKABLE void startup(QStringList args);
  /** This method is thread-safe */
  bool loadConfig();
  /** This method is thread-safe */
  void shutdown(int returnCode);

private:
  Q_INVOKABLE bool doLoadConfig();
  Q_INVOKABLE void doShutdown(int returnCode);
  Q_INVOKABLE bool systemTriggeredLoadConfig(QString actor);
  Q_INVOKABLE void systemTriggeredShutdown(int returnCode, QString actor);
};

#endif // QROND_H
