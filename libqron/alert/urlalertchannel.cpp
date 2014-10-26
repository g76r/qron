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
#include "urlalertchannel.h"
#include "sysutil/parametrizednetworkrequest.h"
#include "sysutil/parametrizedudpsender.h"
#include "alerter.h"
#include "config/alerterconfig.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>

UrlAlertChannel::UrlAlertChannel(QObject *parent, QPointer<Alerter> alerter) :
  AlertChannel(parent, alerter), _nam(0) {
  _thread->setObjectName("UrlAlertChannelThread");
}

void UrlAlertChannel::doSendMessage(Alert alert, MessageType type) {
  if (!_nam) {
    // must be created here since the constructor is called by another thread
    // and one cannot create an QObject which parent lies in another thread
    _nam = new QNetworkAccessManager(this);
    connect(_nam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));
  }
  QString address = alert.rule().address(alert), message;
  ParamSet params = alert.rule().params();
  switch(type) {
  case Emit:
  case Raise:
    if (!alert.rule().notifyEmit())
      return;
    if (params.contains("emitaddress"))
      address = params.rawValue("emitaddress");
    if (params.contains("emitmethod"))
      params.setValue("method", params.rawValue("emitmethod"));
    message = alert.rule().emitMessage(alert);

    break;
  case Cancel:
    if (!alert.rule().notifyCancel())
      return;
    if (params.contains("canceladdress"))
      address = params.rawValue("canceladdress");
    if (params.contains("cancelmethod"))
      params.setValue("method", params.rawValue("cancelmethod"));
    message = alert.rule().cancelMessage(alert);
    break;
  }
  // LATER support for binary messages
  if (address.startsWith("udp:", Qt::CaseInsensitive)) {
    ParametrizedUdpSender sender(address, params, &alert);
    sender.performRequest(message, &alert);
  } else {
    ParametrizedNetworkRequest request(address, params, &alert);
    request.performRequest(_nam, message, &alert);
    /*if (reply) {
    QObject::connect(reply, (void (QNetworkReply::*)(QNetworkReply::NetworkError))&QNetworkReply::error,
                     [=](QNetworkReply::NetworkError error){
      qDebug() << "network reply error for alert:" << (long)reply << address << error;
    });
    QObject::connect(reply, &QNetworkReply::finished, [=](){
      qDebug() << "network reply finished for alert:" << (long)reply << address;
    });
  }*/
  }
}

void UrlAlertChannel::replyFinished(QNetworkReply *reply) {
  if (reply)
    reply->deleteLater();
}
