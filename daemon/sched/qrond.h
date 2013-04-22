/* Copyright 2013 Hallowyn and others.
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
#ifndef QROND_H
#define QROND_H

#include <QStringList>
#include "sched/scheduler.h"
#include "httpd/httpserver.h"
#include "ui/webconsole.h"

class Qrond : public QObject {
  Q_OBJECT
  QHostAddress _webconsoleAddress;
  quint16 _webconsolePort;
  Scheduler *_scheduler;
  HttpServer *_httpd;
  WebConsole *_webconsole;
  QString _configPath;

public:
  explicit Qrond(QObject *parent = 0);
  ~Qrond();
  static Qrond *instance();
  Q_INVOKABLE void startup(QStringList args);
  Q_INVOKABLE void reload();
  Q_INVOKABLE void shutdown(int returnCode);
};

#endif // QROND_H