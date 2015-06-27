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
#ifndef GRIDBOARD_H
#define GRIDBOARD_H

#include "modelview/shareduiitem.h"
#include "alert.h"
#include <QRegularExpression>
#include "pf/pfnode.h"

class GridboardData;

// FIXME doc
class Gridboard : public SharedUiItem {
public:
  Gridboard() { }
  Gridboard(const Gridboard &other) : SharedUiItem(other) { }
  Gridboard(PfNode node, Gridboard oldGridboard);
  QRegularExpression patternRegexp() const;
  void update(QRegularExpressionMatch match, Alert alert);
  QString toHtmlTable() const;
  // LATER QString toCsvTable() const;
  PfNode toPfNode() const;

private:
  const GridboardData *data() const {
    return (const GridboardData*)SharedUiItem::data(); }
  GridboardData *data();
};

#endif // GRIDBOARD_H
