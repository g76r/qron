/* Copyright 2015 Hallowyn and others.
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
#ifndef NOTICEPSEUDOPARAMSPROVIDER_H
#define NOTICEPSEUDOPARAMSPROVIDER_H

#include "util/paramset.h"

/** ParamsProvider wrapper for pseudo params.
 * Not currently used. Maybe used later if actions get a way to receive a
 * ParamsProvider (not only ParamSet or TaskInstance). */
class NoticePseudoParamsProvider : public ParamsProvider {
  QString _notice;
  ParamSet _params;

public:
  NoticePseudoParamsProvider(QString notice, ParamSet params = ParamSet())
    : _notice(notice), _params(params) { }
  QVariant paramValue(QString key, QVariant defaultValue = QVariant(),
                      QSet<QString> alreadyEvaluated = QSet<QString>()) const;
};

#endif // NOTICEPSEUDOPARAMSPROVIDER_H
