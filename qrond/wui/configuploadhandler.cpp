/* Copyright 2014 Hallowyn and others.
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
#include "configuploadhandler.h"

ConfigUploadHandler::ConfigUploadHandler(
    QString urlPathPrefix, int maxSimultaneousUploads, QObject *parent)
  : UploadHttpHandler(urlPathPrefix, maxSimultaneousUploads, parent) {
}

ConfigRepository *ConfigUploadHandler::configRepository() const {
  return _configRepository;
}

void ConfigUploadHandler::setConfigRepository(
    ConfigRepository *configRepository) {
  _configRepository = configRepository;
}

void ConfigUploadHandler::processUploadedFile(
    HttpRequest req, HttpResponse res, ParamsProviderMerger *processingContext,
    QFile *file) {
  Q_UNUSED(req)
  Q_UNUSED(processingContext)
  if (!_configRepository) {
    Log::error() << "ConfigUploadHandler::processUploadedFile called with null "
                    "ConfigRepository";
    res.set_status(HttpResponse::HTTP_Internal_Server_Error);
    return;
  }
  SchedulerConfig config = _configRepository->parseConfig(file, false);
  if (config.isNull()) {
    Log::error() << "cannot process uploaded configuration";
    res.set_status(HttpResponse::HTTP_Unsupported_Media_Type);
    return;
  }
  _configRepository->addConfig(config);
  res.set_header("X-Qron-ConfigId", config.id());
  res.output()->write(QString("(id %1)\n").arg(config.id()).toUtf8());
}
