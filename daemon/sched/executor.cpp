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
#include "executor.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>
#include "log/log.h"
#include <QNetworkAccessManager>
#include <QUrl>
#include <QBuffer>
#include <QNetworkReply>

Executor::Executor() : QObject(0), _isTemporary(false), _thread(new QThread),
  _process(0), _nam(new QNetworkAccessManager(this)) {
  _thread->setObjectName(QString("Executor-%1")
                         .arg((long)_thread, sizeof(long)*8, 16));
  connect(_nam, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(replyFinished(QNetworkReply*)));
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
  Log::debug(request.task().fqtn(), request.id())
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
  QStringList cmdline;
  cmdline = request.params().splitAndEvaluate(request.task().command());
  Log::info(_request.task().fqtn(), _request.id())
      << "exact command line to be executed (locally): " << cmdline.join(" ");
  execProcess(request, target, cmdline);
}

void Executor::sshMean(TaskRequest request, Host target) {
  QStringList cmdline, sshCmdline;
  cmdline = request.params().splitAndEvaluate(request.task().command());
  Log::info(_request.task().fqtn(), _request.id())
      << "exact command line to be executed (through ssh on host "
      << target.hostname() <<  "): " << cmdline.join(" ");
  // ssh options are set to avoid any host key check to make the connection
  // successful even if the host is not known or if its key changed
  // LATER make the host key bypass optional (since it's insecure)
  // LATER support ssh options from params, such as port and keys
  // LATER remove warning about known hosts file from stderr log
  // LATER provide a way to set ssh username
  sshCmdline << "ssh" << "-oUserKnownHostsFile=/dev/null"
             << "-oGlobalKnownHostsFile=/dev/null"
             << "-oStrictHostKeyChecking=no"
             << "-oServerAliveInterval=10" << "-oServerAliveCountMax=3"
             << target.hostname()
             << cmdline;
  execProcess(request, target, sshCmdline);
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
        << "about to start system process '" << program << "' with args "
        << cmdline << " and environment " << env.toStringList();
    _process->start(program, cmdline);
  } else {
    delete _process;
    _process = 0;
    _target = Host();
    _request = TaskRequest();
    Log::warning(_request.task().fqtn(), _request.id())
        << "cannot execute task with empty command '"
        << request.task().fqtn() << "'";
  }
}

void Executor::error(QProcess::ProcessError error) {
  readyReadStandardError();
  readyReadStandardOutput();
  Log::error(_request.task().fqtn(), _request.id())
      << "task error #" << error << " : " << _process->errorString();
  // LATER log duration and wait time
  _process->deleteLater();
  _process = 0;
  _errBuf.clear();
  _target = Host();
  _request = TaskRequest();
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
  // LATER http mean should support http auth, http proxy auth and ssl
  QString method = request.params().value("method");
  QUrl url;
  int port = request.params().value("port", "80").toInt();
  url.setScheme("http");
  url.setHost(target.hostname());
  url.setPort(port);
  url.setPath(request.task().command());
  if (url.isValid()) {
    _request = request;
    _target = target;
    if (method.isEmpty() || method.compare("get", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact GET URL to be called: "
          << url.toString(QUrl::RemovePassword);
      _nam->get(QNetworkRequest(url));
    } else if (method.compare("put", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact PUT URL to be called: "
          << url.toString(QUrl::RemovePassword);
      QBuffer *buffer = new QBuffer(this);
      buffer->open(QIODevice::ReadOnly);
      _nam->put(QNetworkRequest(url), buffer);
    } else if (method.compare("post", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact POST URL to be called: "
          << url.toString(QUrl::RemovePassword);
      QBuffer *buffer = new QBuffer(this);
      buffer->open(QIODevice::ReadOnly);
      _nam->post(QNetworkRequest(url), buffer);
    } else {
      Log::error(_request.task().fqtn(), _request.id())
          << "unsupported HTTP method: " << method;
      _target = Host();
      _request = TaskRequest();
    }
  } else {
    Log::error(_request.task().fqtn(), _request.id())
        << "unsupported HTTP URL: " << url.toString(QUrl::RemovePassword);
  }
}

void Executor::replyFinished(QNetworkReply *reply) {
  int status = reply
      ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QString reason = reply
      ->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
  bool success = (status < 300);
  Log::info(_request.task().fqtn(), _request.id())
      << "task '" << _request.task().fqtn() << "' finished "
      << (success ? "successfully" : "in failure") << " with return code "
      << status << " (" << reason << ") on host '" << _target.hostname() << "'";
  // LATER log duration and wait time
  emit taskFinished(_request, _target, success, status, this);
  _target = Host();
  _request = TaskRequest();
  reply->deleteLater();
}
