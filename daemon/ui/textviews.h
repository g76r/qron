/* Copyright 2012 Hallowyn and others.
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
#ifndef TEXTVIEWS_H
#define TEXTVIEWS_H

#include "log/logmodel.h"

// TODO remove these constants and those in logmodel.h and replace w/ kind of TextViewDelegate

/** Common constants among console text views. */
namespace TextViews {
  static const int HtmlPrefixRole = LogModel::HtmlPrefixRole;
  static const int TrClassRole = LogModel::TrClassRole;
  static const int LinkRole = Qt::UserRole+20;
  static const int HtmlSuffixRole = Qt::UserRole+21;
}

#endif // TEXTVIEWS_H
