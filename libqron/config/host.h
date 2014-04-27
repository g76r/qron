/* Copyright 2012-2014 Hallowyn and others.
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
#ifndef HOST_H
#define HOST_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include <QHash>
#include "modelview/shareduiitem.h"

class HostData;
class PfNode;

/** A host is a single execution target.
 * @see Cluster */
class LIBQRONSHARED_EXPORT Host : public SharedUiItem {
public:
  Host();
  Host(PfNode node);
  Host(const Host &other);
  ~Host();
  Host &operator=(const Host &other) {
    SharedUiItem::operator=(other); return *this; }
  QString hostname() const;
  /** Resources available. */
  QHash<QString, qint64> resources() const;
  QString resourcesAsString() const;
  QString label() const;
  void detach();
  QVariant uiHeaderData(int section, int role) const;
  int uiDataCount() const;

private:
  HostData *hd();
  const HostData *hd() const { return (const HostData*)constData(); }
};

#endif // HOST_H
