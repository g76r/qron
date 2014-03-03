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
#include <QRegExp>

class EventSubscription;
class Scheduler;

/** Miscellaneous tools for handling configuration */
class ConfigUtils {
public:
  static inline void loadParamSet(PfNode parentnode, ParamSet *params) {
    return loadGenericParamSet(parentnode, params, "param"); }
  static inline void loadSetenv(PfNode parentnode, ParamSet *params) {
    return loadGenericParamSet(parentnode, params, "setenv"); }
  static void loadUnsetenv(PfNode parentnode, ParamSet *unsetenv);
  /** For identifier, with or without dot. Cannot contain ParamSet interpreted
   * expressions such as %!yyyy. */
  static QString sanitizeId(QString string, bool allowDot = false);
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

private:
  static void loadGenericParamSet(PfNode parentnode, ParamSet *params,
                                  QString attrname);
  ConfigUtils();
  /** Convert patterns like "some.path.**" "some.*.path" "some.**.path" or
   * "some.path.with.\*.star.and.\\.backslash" into regular expressions. */
  static QRegExp convertDotHierarchicalFilterToRegexp(
      QString pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive);
};

#endif // CONFIGUTILS_H
