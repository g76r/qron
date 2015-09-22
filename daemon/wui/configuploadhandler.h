/* Copyright 2014-2015 Hallowyn and others.
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
#ifndef CONFIGUPLOADHANDLER_H
#define CONFIGUPLOADHANDLER_H

#include "httpd/uploadhttphandler.h"
#include "configmgt/configrepository.h"

class ConfigUploadHandler : public UploadHttpHandler {
  Q_OBJECT
  Q_DISABLE_COPY(ConfigUploadHandler)
  ConfigRepository *_configRepository;

public:
  explicit ConfigUploadHandler(QString urlPathPrefix,
                               int maxSimultaneousUploads, QObject *parent);
  void processUploadedFile(HttpRequest req, HttpResponse res,
                           ParamsProviderMerger *processingContext,
                           QFile *file);
  ConfigRepository *configRepository() const;
  void setConfigRepository(ConfigRepository *configRepository);
};

#endif // CONFIGUPLOADHANDLER_H
