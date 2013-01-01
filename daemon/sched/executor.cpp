/* Copyright 2012 Hallowyn and others.
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
#include "executor.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>
#include "log/log.h"

Executor::Executor(QObject *threadParent) : QObject(0), _isTemporary(false),
  _thread(new QThread(threadParent)), _process(0) {
  _thread->setObjectName(QString("Executor-%1")
                         .arg((long)_thread, sizeof(long)*8, 16));
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  //qDebug() << "creating new task executor" << this;
}

void Executor::execute(TaskRequest request, Host target) {
  QMetaObject::invokeMethod(this, "doExecute", Q_ARG(TaskRequest, request),
                            Q_ARG(Host, target));
}

void Executor::doExecute(TaskRequest request, Host target) {
  const QString mean = request.task().mean();
  Log::info(request.task().fqtn(), request.id())
      << "executing task '" << request.task().fqtn() << "' through mean '"
      << mean << "'";
  if (mean == "exec")
    execMean(request, target);
  else if (mean == "ssh")
    sshMean(request, target);
  else if (mean == "http")
    httpMean(request, target);
  else {
    Log::error(request.task().fqtn(), request.id())
        << "cannot execute task with unknown mean '" << mean << "'";
    emit taskFinished(request, target, false, 1, this);
  }
  // LATER add other means: https, postevent, setflag, clearflag
}

void Executor::execMean(TaskRequest request, Host target) {
  execProcess(request, target,
              request.params().splitAndEvaluate(request.task().command()));
}

void Executor::sshMean(TaskRequest request, Host target) {
  QStringList cmdline;
  // ssh options are set to avoid any host key check to make the connection
  // successful even if the host is not known or if its key changed
  // LATER make the host key bypass optional (since it's insecure)
  // LATER support ssh options from params, such as port and keys
  // LATER remove warning about known hosts file from stderr log
  cmdline << "ssh" << "-oUserKnownHostsFile=/dev/null"
          << "-oGlobalKnownHostsFile=/dev/null" << "-oStrictHostKeyChecking=no"
          << target.hostname()
          << request.params().evaluate(request.task().command());
  execProcess(request, target, cmdline);
}

void Executor::execProcess(TaskRequest request, Host target,
                           QStringList cmdline) {
  _request = request;
  _target = target;
  _errBuf.clear();
  _process = new QProcess(this);
  _process->setProcessChannelMode(QProcess::SeparateChannels);
  connect(_process, SIGNAL(error(QProcess::ProcessError)),
          this, SLOT(error(QProcess::ProcessError)));
  connect(_process, SIGNAL(finished(int,QProcess::ExitStatus)),
          this, SLOT(finished(int,QProcess::ExitStatus)));
  connect(_process, SIGNAL(readyReadStandardError()),
          this, SLOT(readyReadStandardError()));
  connect(_process, SIGNAL(readyReadStandardOutput()),
          this, SLOT(readyReadStandardOutput()));
   QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
   foreach (const QString key, request.params().keys()) {
     //qDebug() << "setting environment variable" << key << "="
     //         << request.params().value(key);
     env.insert(key, request.params().value(key));
   }
   _process->setProcessEnvironment(env);
   if (!cmdline.isEmpty()) {
     QString program = cmdline.takeFirst();
     Log::debug(_request.task().fqtn(), _request.id())
         << "about to run command '" << program << "' with args " << cmdline
         << " and environment " << env.toStringList();
     _process->start(program, cmdline);
   } else
     Log::warning() << "cannot execute task with empty command '"
                    << request.task().fqtn() << "'";
}

void Executor::error(QProcess::ProcessError error) {
  Log::error(_request.task().fqtn(), _request.id())
      << "task error #" << error << " : " << _process->errorString();
  // LATER log duration and wait time
}

void Executor::finished(int exitCode, QProcess::ExitStatus exitStatus) {
  readyReadStandardError();
  readyReadStandardOutput();
  bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
  Log::info(_request.task().fqtn(), _request.id())
      << "task '" << _request.task().fqtn() << "' finished "
      << (success ? "successfully" : "in failure") << " with return code "
      << exitCode << " on host '" << _target.hostname() << "'";
  // LATER log duration and wait time
  emit taskFinished(_request, _target, success, exitCode, this);
  _process->deleteLater();
  _process = 0;
  _errBuf.clear();
  _target = Host();
  _request = TaskRequest();
}

void Executor::readyReadStandardError() {
  _process->setReadChannel(QProcess::StandardError);
  QByteArray ba;
  while (!(ba = _process->read(1024)).isEmpty()) {
    _errBuf.append(ba);
    int i;
    while (((i = _errBuf.indexOf('\n')) >= 0)) {
      QString line = QString::fromUtf8(_errBuf.mid(0, i));
      _errBuf.remove(0, i+1);
      Log::warning(_request.task().fqtn(), _request.id())
          << "task stderr: " << line.trimmed();
    }
  }
}

void Executor::readyReadStandardOutput() {
  _process->setReadChannel(QProcess::StandardOutput);
  while (!_process->read(1024).isEmpty());
  // LATER make it possible to log stdout too (as debug, depending on task cfg)
}

void Executor::httpMean(TaskRequest request, Host target) {
  // TODO http mean
  emit taskFinished(request, target, false, 42, this);
}
