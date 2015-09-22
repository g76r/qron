/* Copyright 2015 Hallowyn and others.
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
#ifndef GRIDBOARD_H
#define GRIDBOARD_H

#include "modelview/shareduiitem.h"
#include "alert.h"
#include <QRegularExpression>
#include "pf/pfnode.h"
#include "util/paramset.h"

class GridboardData;

/** Class for building a grid-like HTML dashboard of statuses from alert
 * patterns conventions such as host.down.http.$host.$port.$path (think of
 * host.down.http.www_google_com.80.index_html).
 *
 * Works by first mapping an alert id string pattern to a multi-dimensionnal
 * matrix of statuses, and then mapping this matrix to a set of connected
 * components submatrixes displayable in HTML.
 *
 * Configuration examples:
 * (gridboard ping
 *   (label Hosts Ping Statuses)
 *   (pattern "^host\.down\.ping\.(?<host>[^\.]+)")
 *   (dimension host)
 *   (initvalue (dimension host localhost))
 *   (warningdelay 30)
 *   (param gridboard.fakedimensionname current status)
 * )
 * (gridboard service_x_instance
 *   (pattern "^host\.down\.http\.(?<host>[^\.]+)\.(?<port>[0-9]+)\.(?<path>.+)$")
 *   (dimension service (key "%path"))
 *   (dimension instance (key "%host:%port"))
 *   (warningdelay 30)
 * )
 * (gridboard tasks
 *   (label Tasks Alerts)
 *   (pattern "^task\.(?<status>[^\.]+)\.(?<taskid>.+)$")
 *   (dimension taskid)
 *   (dimension status)
 *   (warningdelay 2000000000)
 *   (param gridboard.rowformat '<a href="taskdoc.html?taskid=%1">%1</a>')
 * )
 */
class LIBQRONSHARED_EXPORT Gridboard : public SharedUiItem {
public:
  Gridboard() { }
  Gridboard(const Gridboard &other) : SharedUiItem(other) { }
  Gridboard(PfNode node, Gridboard oldGridboard, ParamSet parentParams);
  QRegularExpression patternRegexp() const;
  PfNode toPfNode() const;
  void update(QRegularExpressionMatch match, Alert alert);
  void clear();
  QString toHtml() const;

private:
  const GridboardData *data() const {
    return (const GridboardData*)SharedUiItem::data(); }
  GridboardData *data();
};

#endif // GRIDBOARD_H
