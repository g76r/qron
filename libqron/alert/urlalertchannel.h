/* Copyright 2014-2015 Hallowyn and others.
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
#ifndef URLALERTCHANNEL_H
#define URLALERTCHANNEL_H

#include "alertchannel.h"

class QNetworkAccessManager;
class QNetworkReply;

/** Alert channel that send alerts through URL requests. */
class LIBQRONSHARED_EXPORT UrlAlertChannel : public AlertChannel {
  Q_OBJECT
  Q_DISABLE_COPY(UrlAlertChannel)
  QNetworkAccessManager *_nam;

public:
  explicit UrlAlertChannel(QObject *parent, QPointer<Alerter> alerter);
  void doNotifyAlert(Alert alert);

private slots:
  void replyFinished(QNetworkReply *reply);
};

#endif // URLALERTCHANNEL_H
