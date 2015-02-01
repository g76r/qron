/* Copyright 2013-2014 Hallowyn and others.
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
#include "action/action.h"

ConfigUtils::ConfigUtils() {
}

void ConfigUtils::loadParamSet(PfNode parentnode, ParamSet *params,
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

void ConfigUtils::loadFlagSet(PfNode parentnode, ParamSet *unsetenv,
                              QString attrname) {
  if (!unsetenv)
    return;
  foreach (QString content, parentnode.stringChildrenByName(attrname)) {
    QStringList names = content.split(QRegExp("\\s"));
    foreach (const QString name, names)
      unsetenv->setValue(name, QString());
  }
}

void ConfigUtils::writeParamSet(PfNode *parentnode, ParamSet params,
                                QString attrname, bool inherit) {
  if (!parentnode)
    return;
  QStringList list = params.keys(inherit).values();
  qSort(list);
  foreach (const QString &key, list)
    parentnode->appendChild(PfNode(attrname, key+" "+params.rawValue(key)));
}

void ConfigUtils::writeFlagSet(PfNode *parentnode, QSet<QString> set,
                               QString attrname) {
  if (!parentnode || set.isEmpty())
    return;
  QStringList list = set.values();
  qSort(list);
  parentnode->appendChild(PfNode(attrname, list.join(' ')));
}

void ConfigUtils::writeEventSubscriptions(PfNode *parentnode,
                                          QList<EventSubscription> list,
                                          QStringList exclusionList) {
  foreach (EventSubscription es, list)
    if (!exclusionList.contains(es.eventName())
        && !es.actions().isEmpty())
      parentnode->appendChild(es.toPfNode());
}

QString ConfigUtils::sanitizeId(QString string, IdType idType) {
  static QRegExp unallowedCharsForTask("[^a-zA-Z0-9_\\-]+");
  static QRegExp unallowedCharsForGroup("[^a-zA-Z0-9_\\-\\.]+");
  static QRegExp unallowedCharsForSubTask("[^a-zA-Z0-9_\\-\\:]+");
  static QString placeholder("_");
  QRegExp re;
  switch (idType) {
  case TaskId:
    re = unallowedCharsForTask;
    break;
  case GroupId:
    re = unallowedCharsForGroup;
    break;
  case SubTaskId:
    re = unallowedCharsForSubTask;
    break;
  }
  return string.replace(re, placeholder);
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
