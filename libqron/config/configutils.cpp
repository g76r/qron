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
#include "eventsubscription.h"

ConfigUtils::ConfigUtils() {
}

void ConfigUtils::loadGenericParamSet(PfNode parentnode, ParamSet *params,
                                      QString attrname) {
  if (!params)
    return;
  QListIterator<QPair<QString,QString> > it(
        parentnode.stringsPairChildrenByName(attrname));
  while (it.hasNext()) {
    const QPair<QString,QString> &p(it.next());
    if (p.first.isEmpty())
      Log::warning() << "invalid empty param in " << parentnode.toPf();
    else {
      QString value = p.second;
      params->setValue(p.first, value.isNull() ? QString("") : value);
    }
  }
}

void ConfigUtils::loadUnsetenv(PfNode parentnode, ParamSet *unsetenv) {
  if (!unsetenv)
    return;
  foreach (QString content, parentnode.stringChildrenByName("unsetenv")) {
    QStringList names = content.split(QRegExp("\\s"));
    foreach (const QString name, names)
      unsetenv->setValue(name, QString());
  }
}

QString ConfigUtils::sanitizeId(QString string, bool allowDot) {
  static QRegExp unallowedChars("[^a-zA-Z0-9_\\-]+");
  static QRegExp unallowedCharsWithDot("[^a-zA-Z0-9_\\-\\.]+");
  static QString placeholder("_");
  return string.replace(allowDot ? unallowedCharsWithDot : unallowedChars,
                        placeholder);
}

QRegExp ConfigUtils::readRawOrRegexpFilter(
    QString s, Qt::CaseSensitivity cs) {
  if (s.size() > 1 && s[0] == '/' && s[s.size()-1] == '/' )
    return QRegExp(s.mid(1, s.size()-2), cs);
  return QRegExp(s, cs, QRegExp::FixedString);
}

QRegExp ConfigUtils::readDotHierarchicalFilter(
    QString s, Qt::CaseSensitivity cs) {
  if (s.size() > 1 && s[0] == '/' && s[s.size()-1] == '/' )
    return QRegExp(s.mid(1, s.size()-2), cs);
  return convertDotHierarchicalFilterToRegexp(s, cs);
}

QRegExp ConfigUtils::convertDotHierarchicalFilterToRegexp(
    QString pattern, Qt::CaseSensitivity cs) {
  QString re;
  for (int i = 0; i < pattern.size(); ++i) {
    QChar c = pattern.at(i);
    switch (c.toLatin1()) {
    case '*':
      if (i >= pattern.size()-1 || pattern.at(i+1) != '*')
        re.append("[^.]*");
      else {
        re.append(".*");
        ++i;
      }
      break;
    case '\\':
      if (i < pattern.size()-1) {
        c = pattern.at(++i);
        switch (c.toLatin1()) {
        case '*':
        case '\\':
          re.append('\\').append(c);
          break;
        default:
          re.append("\\\\").append(c);
        }
      }
      break;
    case '.':
    case '[':
    case ']':
    case '(':
    case ')':
    case '+':
    case '?':
    case '^':
    case '$':
    case 0: // actual 0 or non-ascii
      // LATER fix regexp conversion, it is erroneous with some special chars
      re.append('\\').append(c);
      break;
    default:
      re.append(c);
    }
  }
  return QRegExp(re, cs);
}

void ConfigUtils::loadEventSubscription(
    PfNode parentNode, QString childName, QString subscriberId,
    QList<EventSubscription> *list, Scheduler *scheduler) {
  if (!list)
    return;
  foreach (PfNode listnode, parentNode.childrenByName(childName))
    list->append(EventSubscription(subscriberId, listnode, scheduler));
}
