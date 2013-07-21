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
#include "log/qterrorcodes.h"

Executor::Executor() : QObject(0), _isTemporary(false), _thread(new QThread),
  _process(0), _nam(new QNetworkAccessManager(this)), _reply(0) {
  _thread->setObjectName(QString("Executor-%1")
                         .arg((long)_thread, sizeof(long)*2, 16,
                              QLatin1Char('0')));
  connect(_nam, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(replyFinished(QNetworkReply*)));
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  _baseenv = QProcessEnvironment::systemEnvironment();
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
    localMean(request);
  else if (mean == "ssh")
    sshMean(request);
  else if (mean == "http")
    httpMean(request);
  else if (mean == "donothing") {
    request.setSuccess(true);
    request.setReturnCode(0);
    request.setEndDatetime();
    emit taskStarted(request);
    emit taskFinished(request, this);
  } else {
    Log::error(request.task().fqtn(), request.id())
        << "cannot execute task with unknown mean '" << mean << "'";
    request.setSuccess(false);
    request.setReturnCode(-1);
    request.setEndDatetime();
    emit taskFinished(request, this);
  }
}

void Executor::localMean(TaskRequest request) {
  QStringList cmdline;
  cmdline = request.params()
      .splitAndEvaluate(request.command(), &request);
  Log::info(request.task().fqtn(), request.id())
      << "exact command line to be executed (locally): " << cmdline.join(" ");
  QProcessEnvironment sysenv;
  prepareEnv(request, &sysenv);
  request.setAbortable();
  execProcess(request, cmdline, sysenv);
}

void Executor::sshMean(TaskRequest request) {
  QStringList cmdline, sshCmdline;
  cmdline = request.params()
      .splitAndEvaluate(request.command(), &request);
  Log::info(request.task().fqtn(), request.id())
      << "exact command line to be executed (through ssh on host "
      << request.target().hostname() <<  "): " << cmdline.join(" ");
  QHash<QString,QString> setenv;
  QProcessEnvironment sysenv;
  prepareEnv(request, &sysenv, &setenv);
  QString username = request.params().value("ssh.username");
  qlonglong port = request.params().valueAsLong("ssh.port");
  QString ignoreknownhosts = request.params().value("ssh.ignoreknownhosts",
                                                    "true");
  QString identity = request.params().value("ssh.identity");
  QStringList options = request.params().valueAsStrings("ssh.options");
  bool disablepty = request.params().value("ssh.disablepty") == "true";
  sshCmdline << "ssh" << "-oLogLevel=ERROR" << "-oEscapeChar=none"
             << "-oServerAliveInterval=10" << "-oServerAliveCountMax=3"
             << "-oIdentitiesOnly=yes" << "-oKbdInteractiveAuthentication=no"
             << "-oBatchMode=yes" << "-oConnectionAttempts=3"
             << "-oTCPKeepAlive=yes" << "-oPasswordAuthentication=false";
  if (!disablepty) {
    sshCmdline << "-t" << "-t";
    request.setAbortable();
  }
  if (ignoreknownhosts == "true")
    sshCmdline << "-oUserKnownHostsFile=/dev/null"
               << "-oGlobalKnownHostsFile=/dev/null"
               << "-oStrictHostKeyChecking=no";
  if (port > 0 && port < 65536)
    sshCmdline << "-oPort="+QString::number(port);
  if (!identity.isEmpty())
    sshCmdline << "-oIdentityFile=" + identity;
  foreach (QString option, options)
    sshCmdline << "-o" + option;
  if (!username.isEmpty())
    sshCmdline << "-oUser=" + username;
  sshCmdline << request.target().hostname();
  foreach (QString key, setenv.keys())
    if (!request.task().unsetenv().contains(key)) {
      QString value = setenv.value(key);
      value.replace('\'', QString());
      sshCmdline << key+"='"+value+"'";
    }
  sshCmdline << cmdline;
  execProcess(request, sshCmdline, sysenv);
}

void Executor::execProcess(TaskRequest request, QStringList cmdline,
                           QProcessEnvironment sysenv) {
  if (cmdline.isEmpty()) {
    Log::warning(request.task().fqtn(), request.id())
        << "cannot execute task with empty command '"
        << request.task().fqtn() << "'";
    request.setSuccess(false);
    request.setReturnCode(-1);
    request.setEndDatetime();
    emit taskFinished(request, this);
    return;
  }
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
  _process->setProcessEnvironment(sysenv);
  QString program = cmdline.takeFirst();
  Log::debug(_request.task().fqtn(), _request.id())
      << "about to start system process '" << program << "' with args "
      << cmdline << " and environment " << sysenv.toStringList();
  emit taskStarted(request);
  _process->start(program, cmdline);
}

void Executor::processError(QProcess::ProcessError error) {
  //qDebug() << "************ processError" << _request.id() << _process;
  if (!_process)
    return; // LATER add log
  readyReadStandardError();
  readyReadStandardOutput();
  Log::error(_request.task().fqtn(), _request.id())
      << "task error #" << error << " : " << _process->errorString();
  _process->kill();
  processFinished(-1, QProcess::CrashExit);
}

void Executor::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
  //qDebug() << "************ processFinished" << _request.id() << _process;
  if (!_process)
    return; // LATER add log
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
  _process->deleteLater(); // TODO actually delete should only be done when QProcess::finished() is emited, but we get here too when QProcess::error() is emited
  _process = 0;
  _errBuf.clear();
  _request = TaskRequest();
}

void Executor::readyReadStandardError() {
  //qDebug() << "************ readyReadStandardError" << _request.id() << _process;
  // LATER provide a way to define several stderr filter regexps
  // LATER provide a way to choose log level for stderr
  if (!_process)
    return;
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
        QList<QRegExp> filters(_request.task().stderrFilters());
        if (filters.isEmpty() && _request.task().mean() == "ssh")
          filters.append(QRegExp("^Connection to [^ ]* closed\\.$"));
        foreach (QRegExp filter, filters)
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
  //qDebug() << "************ readyReadStandardOutput" << _request.id() << _process;
  if (!_process)
    return;
  _process->setReadChannel(QProcess::StandardOutput);
  while (!_process->read(1024).isEmpty());
  // LATER make it possible to log stdout too (as debug, depending on task cfg)
}

void Executor::httpMean(TaskRequest request) {
  // LATER http mean should support http auth, http proxy auth and ssl
  QString method = request.params().value("method");
  QUrl url;
  int port = request.params().valueAsInt("port", 80);
  QString hostname = request.params()
      .evaluate(request.target().hostname(), &request);
  QString command = request.params()
      .evaluate(request.command(), &request);
  if (command.size() && command.at(0) == '/')
    command = command.mid(1);
  url.setEncodedUrl(QString("http://%1:%2/%3").arg(hostname).arg(port)
                    .arg(command).toUtf8(), QUrl::TolerantMode);
  QNetworkRequest networkRequest(url);
  foreach (QString name, request.setenv().keys()) {
    const QString expr(request.setenv().rawValue(name));
    if (name.endsWith(":")) // ignoring : at end of header name
      name.chop(1);
    name.replace(QRegExp("[^a-zA-Z_0-9\\-]+"), "_");
    const QString value = request.params().evaluate(expr, &request);
    //Log::fatal(request.task().fqtn(), request.id()) << "setheader: " << name << "=" << value << ".";
    networkRequest.setRawHeader(name.toAscii(), value.toUtf8());
  }
  _request = request;
  if (url.isValid()) {
    request.setAbortable();
    emit taskStarted(request);
    if (method.isEmpty() || method.compare("get", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact GET URL to be called: "
          << url.toString(QUrl::RemovePassword);
      _reply = _nam->get(networkRequest);
    } else if (method.compare("put", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact PUT URL to be called: "
          << url.toString(QUrl::RemovePassword);
      QBuffer *buffer = new QBuffer;
      buffer->open(QIODevice::ReadOnly);
      _reply = _nam->put(networkRequest, buffer);
      buffer->setParent(_reply);
    } else if (method.compare("post", Qt::CaseInsensitive) == 0) {
      Log::info(_request.task().fqtn(), _request.id())
          << "exact POST URL to be called: "
          << url.toString(QUrl::RemovePassword);
      QBuffer *buffer = new QBuffer;
      buffer->open(QIODevice::ReadOnly);
      networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
      _reply = _nam->post(networkRequest, buffer);
      buffer->setParent(_reply);
    } else {
      Log::error(_request.task().fqtn(), _request.id())
          << "unsupported HTTP method: " << method;
      _request.setSuccess(false);
      _request.setReturnCode(-1);
      _request.setEndDatetime();
      emit taskFinished(_request, this);
      _request = TaskRequest();
    }
  } else {
    Log::error(_request.task().fqtn(), _request.id())
        << "unsupported HTTP URL: " << url.toString(QUrl::RemovePassword);
    _request.setSuccess(false);
    _request.setReturnCode(-1);
    _request.setEndDatetime();
    emit taskFinished(_request, this);
    _request = TaskRequest();
  }
}

void Executor::replyFinished(QNetworkReply *reply) {
  QString fqtn(_request.task().fqtn());
  if (reply != _reply || !reply)
    Log::warning(fqtn, _request.id())
        << "Executor::replyFinished receive strange pointer";
  int status = reply
      ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QString reason = reply
      ->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
  QNetworkReply::NetworkError error = reply->error();
  bool success = (status < 300 && status >= 100
                  && error == QNetworkReply::NoError);
  _request.setEndDatetime();
  Log::log(success ? Log::Info : Log::Warning, fqtn, _request.id())
      << "task '" << fqtn << "' finished "
      << (success ? "successfully" : "in failure") << " with return code "
      << status << " (" << reason << ") on host '"
      << _request.target().hostname() << "' in " << _request.runningMillis()
      << " ms, with network error '" << networkErrorAsString(error)
      << "' (code " << error << ")";
  // LATER translate network error codes into human readable strings
  _request.setSuccess(success);
  _request.setReturnCode(status);
  emit taskFinished(_request, this);
  _request = TaskRequest();
  reply->deleteLater();
  _reply = 0;
}

void Executor::prepareEnv(TaskRequest request, QProcessEnvironment *sysenv,
                          QHash<QString,QString> *setenv) {
  // first clean system base env from any unset variables
  *sysenv = _baseenv;
  foreach (const QString pattern, request.task().unsetenv()) {
    QRegExp re(pattern, Qt::CaseInsensitive, QRegExp::WildcardUnix);
    foreach (const QString key, sysenv->keys())
      if (re.exactMatch(key))
        sysenv->remove(key);
  }
  // then build setenv evaluated paramset that may be used apart from merging
  // into sysenv
  foreach (QString key, request.setenv().keys()) {
    const QString expr(request.setenv().rawValue(key));
    /*Log::debug(request.task().fqtn(), request.id())
        << "setting environment variable " << key << "="
        << expr << " " << request.params().keys(false).size() << " "
        << request.params().keys(true).size() << " ["
        << request.params().evaluate("%!yyyy %!fqtn %{!fqtn}", &_request)
        << "]";*/
    key.replace(QRegExp("[^a-zA-Z_0-9]+"), "_");
    const QString value = request.params().evaluate(expr, &request);
    if (setenv)
      setenv->insert(key, value);
    sysenv->insert(key, value);
  }
}

void Executor::abort() {
  QMetaObject::invokeMethod(this, "doAbort");
}

void Executor::doAbort() {
  if (_request.isNull()) {
    Log::warning() << "cannot abort task because this executor is not "
                      "currently responsible for any task";
  } else if (!_request.abortable()) {
    if (_request.task().mean() == "ssh")
      Log::warning(_request.task().fqtn(), _request.id())
          << "cannot abort task because ssh tasks are not abortable when "
             "ssh.disablepty is set to true";
    else
      Log::warning(_request.task().fqtn(), _request.id())
          << "cannot abort task because is marked as not abortable";
  } else if (_process) {
    _process->kill();
  } else if (_reply) {
    _reply->abort();
  } else {
    Log::warning(_request.task().fqtn(), _request.id())
        << "cannot abort task because its execution mean is not abortable";
  }
}
