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
#include "host.h"
#include <QSharedData>
#include <QString>
#include <QHash>
#include "pf/pfnode.h"
#include "log/log.h"
#include "configutils.h"
#include "ui/qronuiutils.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Hostname",
  "Resources",
  "Label",
};

class HostData : public SharedUiItemData {
public:
  QString _id, _label, _hostname;
  QHash<QString,qint64> _resources;
  QVariant uiData(int section, int role) const;
  QString id() const { return _id; }
  void setId(QString id) { _id = id; }
  QString idQualifier() const { return "host"; }
};

Host::Host() {
}

Host::Host(const Host &other) : SharedUiItem(other) {
}

Host::Host(PfNode node) {
  HostData *hd = new HostData;
  hd->_id = ConfigUtils::sanitizeId(node.contentAsString(), true);
  hd->_label = node.attribute("label", hd->_id);
  hd->_hostname = ConfigUtils::sanitizeId(node.attribute("hostname", hd->_id),
                                          true);
  QListIterator<QPair<QString,qlonglong> > it(
        node.stringLongPairChildrenByName("resource"));
  while (it.hasNext()) {
    const QPair<QString,qlonglong> &p(it.next());
    if (p.second <= 0)
      Log::warning() << "ignoring resource of kind " << p.first
                     << "with incorrect quantity in host" << node.toString();
    else
      hd->_resources.insert(ConfigUtils::sanitizeId(p.first), p.second);
  }
  setData(hd);
}

Host::~Host() {
}

QString Host::hostname() const {
  return !isNull() ? hd()->_hostname : QString();
}

QHash<QString,qint64> Host::resources() const {
  return !isNull() ? hd()->_resources : QHash<QString,qint64>();
}

QString Host::resourcesAsString() const {
  return !isNull() ? QronUiUtils::resourcesAsString(hd()->_resources)
                   : QString();
}

QString Host::label() const {
  return !isNull() ? hd()->_label : QString();
}

QVariant HostData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _hostname;
    case 2:
      return QronUiUtils::resourcesAsString(_resources);
    case 3:
      return _label;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant Host::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int Host::uiDataCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

HostData *Host::hd() {
  SharedUiItem::detach<HostData>();
  return (HostData*)constData();
}

void Host::detach() {
  SharedUiItem::detach<HostData>();
}