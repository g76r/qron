/* Copyright 2013-2015 Hallowyn and others.
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

#include "libqron_global.h"
#include "pf/pfnode.h"
#include "util/paramset.h"
#include <QRegExp>

class EventSubscription;
class Scheduler;

/** Miscellaneous tools for handling configuration */
class LIBQRONSHARED_EXPORT ConfigUtils {
public:
  enum IdType {
    TaskId, // used for tasks and resources; only letters, digits, - and _
    GroupId, // used for groups, hosts and clusters; allows dots
    SubTaskId // used for
  };
  static void loadParamSet(PfNode parentnode, ParamSet *params,
                           QString attrname);
  inline static ParamSet loadParamSet(PfNode parentnode, QString attrname) {
    ParamSet params;
    loadParamSet(parentnode, &params, attrname);
    return params; }
  static void loadFlagSet(PfNode parentnode, ParamSet *unsetenv,
                          QString attrname);
  static void loadResourcesSet(
      PfNode parentnode, QHash<QString,qint64> *resources, QString attrname);
  inline static QHash<QString,qint64> loadResourcesSet(
      PfNode parentnode, QString attrname) {
    QHash<QString,qint64> resources;
    loadResourcesSet(parentnode, &resources, attrname);
    return resources; }
  /** For identifier, with or without dot. Cannot contain ParamSet interpreted
   * expressions such as %!yyyy. */
  static QString sanitizeId(QString string, IdType idType);
  /** Interpret s as regexp if it starts and ends with a /, else as raw text */
  static QRegExp readRawOrRegexpFilter(
      QString s, Qt::CaseSensitivity cs = Qt::CaseSensitive);
  /** Interpret s as regexp if it starts and ends with a /, else as
   * dot-hierarchical globing with * meaning [^.]* and ** meaning .* */
  static QRegExp readDotHierarchicalFilter(
      QString s, Qt::CaseSensitivity cs = Qt::CaseSensitive);
  static void loadEventSubscription(
      PfNode parentNode, QString childName, QString subscriberId,
      QList<EventSubscription> *list, Scheduler *scheduler);
  static void writeParamSet(PfNode *parentnode, ParamSet params,
                            QString attrname, bool inherit = false);
  static void writeFlagSet(PfNode *parentnode, QSet<QString> set,
                           QString attrname);
  static void writeFlagSet(PfNode *parentnode, ParamSet params,
                           QString attrname, bool inherit = false) {
    writeFlagSet(parentnode, params.keys(inherit), attrname);
  }
  /** @param exclusionList may be used e.g. to avoid double "onfinish" which
   * are both in onsuccess and onfailure event subscriptions */
  static void writeEventSubscriptions(
      PfNode *parentnode, QList<EventSubscription> list,
      QStringList exclusionList = QStringList());

private:
  ConfigUtils();
  /** Convert patterns like "some.path.**" "some.*.path" "some.**.path" or
   * "some.path.with.\*.star.and.\\.backslash" into regular expressions. */
  static QRegExp convertDotHierarchicalFilterToRegexp(
      QString pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive);
};

#endif // CONFIGUTILS_H
