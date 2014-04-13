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
#include "qronuiutils.h"

QString QronUiUtils::resourcesAsString(QHash<QString,qint64> resources) {
  QString s;
  bool first = true;
  foreach(QString key, resources.keys()) {
    if (first)
      first = false;
    else
      s.append(' ');
    s.append(key).append('=')
        .append(QString::number(resources.value(key)));
  }
  return s;
}

QString QronUiUtils::sysenvAsString(ParamSet setenv, ParamSet unsetenv) {
  QString s;
  foreach(const QString key, setenv.keys(false))
    s.append(key).append('=').append(setenv.rawValue(key)).append(' ');
  foreach(const QString key, unsetenv.keys(false))
    s.append('-').append(key).append(' ');
  s.chop(1);
  return s;
}

QString QronUiUtils::paramsAsString(ParamSet params, bool inherit) {
  QString s;
  foreach(const QString key, params.keys(inherit))
    s.append(key).append('=').append(params.rawValue(key)).append(' ');
  s.chop(1);
  return s;
}

QString QronUiUtils::paramsKeysAsString(ParamSet params, bool inherit) {
  QString s;
  foreach(const QString key, params.keys(inherit))
    s.append(key).append(' ');
  s.chop(1);
  return s;
}
