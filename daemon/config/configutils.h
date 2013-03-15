/* Copyright 2013 Hallowyn and others.
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
#ifndef CONFIGUTILS_H
#define CONFIGUTILS_H

#include "pf/pfnode.h"
#include "util/paramset.h"

class ConfigUtils {
public:
  static bool loadParamSet(PfNode parentnode, ParamSet &params,
                           QString &errorString);
  static inline bool loadParamSet(PfNode parentnode, ParamSet &params) {
    QString errorString;
    return loadParamSet(parentnode, params, errorString); }

private:
  ConfigUtils();
};

#endif // CONFIGUTILS_H
