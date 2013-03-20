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

bool ConfigUtils::loadGenericParamSet(PfNode parentnode, ParamSet &params,
                                      QString attrname, QString &errorString) {
  Q_UNUSED(errorString);
  /*QStringList list;
  foreach (PfNode node, parentnode.children())
    list << node.name();
  Log::debug() << "loadGenericParamSet " << parentnode.name() << " "
               << attrname << " " << parentnode.childrenByName(attrname).size()
               << " " << parentnode.children().size() << " " << list.join(",");*/
  foreach (PfNode node, parentnode.childrenByName(attrname)) {
    // use key and value subnodes if present
    QString key = node.attribute("key");
    QString value = node.attribute("value");
    // otherwise use node content, key being delimited by the first whitespace
    if (key.isEmpty() || value.isEmpty()) {
      QString content = node.contentAsString().trimmed();
      int i = content.indexOf(QRegExp("\\s"));
      if (i == -1) {
        if (content.isEmpty()) {
          Log::warning() << "invalid param " << node.toPf();
          continue;
        }
        key = content;
        value = "";
      } else {
        key = content.left(i);
        value = content.mid(i+1).trimmed();
      }
    }
    params.setValue(key, value);
    //Log::debug() << "setting value " << attrname << " " << key << "=" << value;
  }
  return true;
}

bool ConfigUtils::loadUnsetenv(PfNode parentnode, QSet<QString> &unsetenv,
                               QString &errorString) {
  Q_UNUSED(errorString);
  foreach (PfNode node, parentnode.childrenByName("unsetenv")) {
    QStringList names = node.contentAsString().split(QRegExp("\\s"));
    foreach (const QString name, names)
      unsetenv.insert(name);
  }
  return true;
}
