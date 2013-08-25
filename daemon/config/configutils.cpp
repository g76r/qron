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
#include "configutils.h"

ConfigUtils::ConfigUtils() {
}

void ConfigUtils::loadGenericParamSet(PfNode parentnode, ParamSet &params,
                                      QString attrname) {
  QListIterator<QPair<QString,QString> > it(
        parentnode.stringsPairChildrenByName(attrname));
  while (it.hasNext()) {
    const QPair<QString,QString> &p(it.next());
    if (p.first.isEmpty())
      Log::warning() << "invalid empty param in " << parentnode.toPf();
    else {
      QString value = p.second;
      params.setValue(p.first, value.isNull() ? QString("") : value);
    }
  }
}

void ConfigUtils::loadUnsetenv(PfNode parentnode, QSet<QString> &unsetenv) {
  foreach (QString content, parentnode.stringChildrenByName("unsetenv")) {
    QStringList names = content.split(QRegExp("\\s"));
    foreach (const QString name, names)
      unsetenv.insert(name);
  }
}

QString ConfigUtils::sanitizeId(QString string, bool allowDot) {
  static QRegExp unallowedChars("[^a-zA-Z0-9_\\-]+");
  static QRegExp unallowedCharsWithDot("[^a-zA-Z0-9_\\-\\.]+");
  static QString placeholder("_");
  return string.replace(allowDot ? unallowedCharsWithDot : unallowedChars,
                        placeholder);
}
