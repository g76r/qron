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
                         .arg((long)_thread, sizeof(long)*2, 16,
                              QLatin1Char('0')));
  connect(_nam, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(replyFinished(QNetworkReply*)));
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  //qDebug() << "creating new task executor" << this;
}

void Executor::execute(TaskRequest request) {
  QMetaObject::invokeMethod(this, "doExecute", Q_ARG(TaskRequest, request));
}

void Executor::doExecute(TaskRequest request) {
  const QString mean = request.task().mean();
  Log::debug(request.task().fqtn(), request.id())
      << "starting task '" << request.task().fqtn() << "' through mean '"
      << mean << "' after " << request.queuedMillis() << " ms in queue";
  if (mean == "local")
    execMean(request);
  else if (mean == "ssh")
    sshMean(request);
  else if (mean == "http")
    httpMean(request);
  else if (mean == "donothing") {
    request.setSuccess(true);
    request.setReturnCode(0);
    emit taskFinished(request, this);
  } else {
    Log::error(request.task().fqtn(), request.id())
        << "cannot execute task with unknown mean '" << mean << "'";
    request.setSuccess(false);
    request.setReturnCode(-1);
    emit taskFinished(request, this);
  }
  // LATER add other means: https, postevent, setflag, clearflag
}

void Executor::execMean(TaskRequest request) {
  QStringList cmdline;
  cmdline = request.params()
      .splitAndEvaluate(request.task().command(), &request);
  Log::info(request.task().fqtn(), request.id())
      << "exact command line to be executed (locally): " << cmdline.join(" ");
  execProcess(request, cmdline);
}

void Executor::sshMean(TaskRequest request) {
  QStringList cmdline, sshCmdline;
  cmdline = request.params()
      .splitAndEvaluate(request.task().command(), &request);
  Log::info(request.task().fqtn(), request.id())
      << "exact command line to be executed (through ssh on host "
      << request.target().hostname() <<  "): " << cmdline.join(" ");
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
             << request.target().hostname()
             << cmdline;
  execProcess(request, sshCmdline);
}

void Executor::execProcess(TaskRequest request, QStringList cmdline) {
  _request = request;
  _errBuf.clear();
  _process = new QProcess(this);
  _process->setProcessChannelMode(QProcess::SeparateChannels);
  connect(_process, SIGNAL(error(QProcess::ProcessError)),
          this, SLOT(processError(QProcess::ProcessError)));
  connect(_process, SIGNAL(finished(int,QProcess::ExitStatus)),
          this, SLOT(processFinished(int,QProcess::ExitStatus)));
  connect(_process, SIGNAL(readyReadStandardError()),
          this, SLOT(readyReadStandardError()));
  connect(_process, SIGNAL(readyReadStandardOutput()),
          this, SLOT(readyReadStandardOutput()));
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  foreach (const QString key, request.params().keys()) {
    //qDebug() << "setting environment variable" << key << "="
    //         << request.params().value(key);
    env.insert(key, request.params().value(key, &request));
  }
  _process->setProcessEnvironment(env);
  if (!cmdline.isEmpty()) {
    QString program = cmdline.takeFirst();
    Log::debug(_request.task().fqtn(), _request.id())
        << "about to start system process '" << program << "' with args "
        << cmdline << " and environment " << env.toStringList();
    _process->start(program, cmdline);
  } else {
    Log::warning(_request.task().fqtn(), _request.id())
        << "cannot execute task with empty command '"
        << request.task().fqtn() << "'";
    _request.setSuccess(false);
    _request.setReturnCode(-1);
    emit taskFinished(_request, this);
    _process->deleteLater();
    _process = 0;
    _request = TaskRequest();
  }
}

void Executor::processError(QProcess::ProcessError error) {
  readyReadStandardError();
  readyReadStandardOutput();
  Log::error(_request.task().fqtn(), _request.id())
      << "task error #" << error << " : " << _process->errorString();
  _request.setSuccess(false);
  _request.setReturnCode(-1);
  emit taskFinished(_request, this);
  _process->deleteLater();
  _process = 0;
  _errBuf.clear();
  _request = TaskRequest();
}

void Executor::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
  readyReadStandardError();
  readyReadStandardOutput();
  bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
  _request.setEndDatetime();
  Log::log(success ? Log::Info : Log::Warning, _request.task().fqtn(),
           _request.id())
      << "task '" << _request.task().fqtn() << "' finished "
      << (success ? "successfully" : "in failure") << " with return code "
      << exitCode << " on host '" << _request.target().hostname() << "' in "
      << _request.runningMillis() << " ms";
  _request.setSuccess(success);
  _request.setReturnCode(exitCode);
  emit taskFinished(_request, this);
  _process->deleteLater();
  _process = 0;
  _errBuf.clear();
  _request = TaskRequest();
}

void Executor::readyReadStandardError() {
  // LATER provide a way to define several stderr filter regexps
  // LATER provide a way to choose log level for stderr
  _process->setReadChannel(QProcess::StandardError);
  QByteArray ba;
  while (!(ba = _process->read(1024)).isEmpty()) {
    _errBuf.append(ba);
    int i;
    while (((i = _errBuf.indexOf('\n')) >= 0)) {
      QString line;
      if (i > 0 && _errBuf.at(i-1) == '\r')
        line = QString::fromUtf8(_errBuf.mid(0, i-1));
      else
        line = QString::fromUtf8(_errBuf.mid(0, i));
      _errBuf.remove(0, i+1);
      if (!line.isEmpty())
      if (!line.isEmpty()) {
        foreach (QRegExp filter, _request.task().stderrFilters())
          if (filter.indexIn(line) >= 0)
            goto line_filtered;
        Log::warning(_request.task().fqtn(), _request.id())
            << "task stderr: " << line.trimmed();
line_filtered:;
      }
    }
  }
}

void Executor::readyReadStandardOutput() {
  _process->setReadChannel(QProcess::StandardOutput);
  while (!_process->read(1024).isEmpty());
  // LATER make it possible to log stdout too (as debug, depending on task cfg)
}

void Executor::httpMean(TaskRequest request) {
  // LATER http mean should support http auth, http proxy auth and ssl
  QString method = request.params().value("method");
  QUrl url;
  int port = request.params().value("port", "80").toInt();
  QString hostname = request.params()
      .evaluate(request.target().hostname(), &request);
  QString command = request.params()
      .evaluate(request.task().command(), &request);
  if (command.size() && command.at(0) == '/')
    command = command.mid(1);
  url.setEncodedUrl(QString("http://%1:%2/%3").arg(hostname).arg(port)
                    .arg(command).toUtf8(), QUrl::TolerantMode);
  if (url.isValid()) {
    _request = request;
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
      _request.setSuccess(false);
      _request.setReturnCode(-1);
      emit taskFinished(_request, this);
      _request = TaskRequest();
    }
  } else {
    Log::error(_request.task().fqtn(), _request.id())
        << "unsupported HTTP URL: " << url.toString(QUrl::RemovePassword);
    _request.setSuccess(false);
    _request.setReturnCode(-1);
    emit taskFinished(_request, this);
  }
}

void Executor::replyFinished(QNetworkReply *reply) {
  int status = reply
      ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QString reason = reply
      ->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
  bool success = (status < 300);
  _request.setEndDatetime();
  Log::info(_request.task().fqtn(), _request.id())
      << "task '" << _request.task().fqtn() << "' finished "
      << (success ? "successfully" : "in failure") << " with return code "
      << status << " (" << reason << ") on host '"
      << _request.target().hostname() << "' in " << _request.runningMillis()
      << " ms";
  _request.setSuccess(success);
  _request.setReturnCode(status);
  emit taskFinished(_request, this);
  _request = TaskRequest();
  reply->deleteLater();
}
