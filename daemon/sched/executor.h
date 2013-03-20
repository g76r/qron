/* Copyright 2012-2013 Hallowyn and others.
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
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <QObject>
#include "config/taskrequest.h"
#include "config/host.h"
#include <QWeakPointer>
#include <QProcess>

class QThread;
class QNetworkAccessManager;
class QNetworkReply;

class Executor : public QObject {
  Q_OBJECT
  bool _isTemporary;
  QThread *_thread;
  QProcess *_process;
  QByteArray _errBuf;
  TaskRequest _request;
  QNetworkAccessManager *_nam;
  QProcessEnvironment _baseenv;

public:
  explicit Executor();
  void setTemporary(bool temporary = true) { _isTemporary = temporary; }
  bool isTemporary() const { return _isTemporary; }
  
public slots:
  /** Execute a request now. This method is thread-safe. */
  void execute(TaskRequest request);

signals:
  /** Signal emited whenever a task is no longer running or queued:
    * when finished on failure, finished on success, or cannot be started
    * because of a failure on start. */
  void taskFinished(TaskRequest request, QWeakPointer<Executor> executor);

private slots:
  void processError(QProcess::ProcessError error);
  void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void readyReadStandardError();
  void readyReadStandardOutput();
  void replyFinished(QNetworkReply *reply);

private:
  Q_INVOKABLE void doExecute(TaskRequest request);
  void localMean(TaskRequest request);
  void sshMean(TaskRequest request);
  void httpMean(TaskRequest request);
  void execProcess(TaskRequest request, QStringList cmdline,
                   QProcessEnvironment sysenv);
  inline void prepareEnv(TaskRequest request, QProcessEnvironment *sysenv,
                         QHash<QString, QString> *setenv = 0);
  Q_DISABLE_COPY(Executor)
};

#endif // EXECUTOR_H
