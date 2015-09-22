/* Copyright 2012-2015 Hallowyn and others.
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
#include "host.h"
#include <QSharedData>
#include <QString>
#include <QHash>
#include "pf/pfnode.h"
#include "log/log.h"
#include "configutils.h"
#include "ui/qronuiutils.h"
#include "modelview/shareduiitemdocumentmanager.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Hostname",
  "Resources",
  "Label",
};

class HostData : public SharedUiItemData {
public:
  QString _id, _label, _hostname;
  QHash<QString,qint64> _resources; // configured max resources available
  QStringList _commentsList;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  void setId(QString id) { _id = id; }
  QString idQualifier() const { return "host"; }
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 SharedUiItemDocumentTransaction *transaction, int role);
  Qt::ItemFlags uiFlags(int section) const;
};

Host::Host() {
}

Host::Host(const Host &other) : SharedUiItem(other) {
}

Host::Host(PfNode node) {
  HostData *d = new HostData;
  d->_id = ConfigUtils::sanitizeId(node.contentAsString(),
                                    ConfigUtils::FullyQualifiedId);
  d->_label = node.attribute("label");
  d->_hostname = ConfigUtils::sanitizeId(node.attribute("hostname"),
                                          ConfigUtils::Hostname);
  ConfigUtils::loadResourcesSet(node, &d->_resources, "resource");
  ConfigUtils::loadComments(node, &d->_commentsList);
  setData(d);
}

Host::~Host() {
}

QString Host::hostname() const {
  if (isNull())
    return QString();
  const HostData *d = data();
  return d->_hostname.isEmpty() ? d->_id : d->_hostname;
}

QHash<QString,qint64> Host::resources() const {
  return !isNull() ? data()->_resources : QHash<QString,qint64>();
}

QVariant HostData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      if (role == Qt::EditRole)
        return _hostname == _id ? QVariant() : _hostname;
      return _hostname.isEmpty() ? _id : _hostname;
    case 2:
      return QronUiUtils::resourcesAsString(_resources);
    case 3:
      if (role == Qt::EditRole)
        return _label == _id ? QVariant() : _label;
      return _label.isEmpty() ? _id : _label;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

bool Host::setUiData(int section, const QVariant &value, QString *errorString,
                     SharedUiItemDocumentTransaction *transaction, int role) {
  if (isNull())
    return false;
  detach();
  return ((HostData*)data())
      ->setUiData(section, value, errorString, transaction, role);
}

bool HostData::setUiData(
    int section, const QVariant &value, QString *errorString,
    SharedUiItemDocumentTransaction *transaction, int role) {
  Q_ASSERT(transaction != 0);
  Q_ASSERT(errorString != 0);
  QString s = value.toString().trimmed();
  switch(section) {
  case 0:
    s = ConfigUtils::sanitizeId(s, ConfigUtils::FullyQualifiedId);
    _id = s;
    return true;
  case 1:
    _hostname = ConfigUtils::sanitizeId(s, ConfigUtils::Hostname);
    return true;
  case 2: {
    QHash<QString,qint64> resources;
    if (QronUiUtils::resourcesFromString(value.toString(), &resources,
                                         errorString)) {
      _resources = resources;
      return true;
    }
    return false;
  }
  case 3:
    _label = s.trimmed();
    return true;
  }
  return SharedUiItemData::setUiData(section, value, errorString, transaction,
                                     role);
}

Qt::ItemFlags HostData::uiFlags(int section) const {
  Qt::ItemFlags flags = SharedUiItemData::uiFlags(section);
  switch(section) {
  case 0:
  case 1:
  case 2:
  case 3:
    flags |= Qt::ItemIsEditable;
  }
  return flags;
}

QVariant HostData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int HostData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

HostData *Host::data() {
  SharedUiItem::detach<HostData>();
  return (HostData*)SharedUiItem::data();
}

void Host::detach() {
  SharedUiItem::detach<HostData>();
}

PfNode Host::toPfNode() const {
  const HostData *d = this->data();
  if (!d)
    return PfNode();
  PfNode node("host", d->_id);
  ConfigUtils::writeComments(&node, d->_commentsList);
  if (!d->_label.isEmpty() && d->_label != d->_id)
    node.appendChild(PfNode("label", d->_label));
  if (!d->_hostname.isEmpty() && d->_hostname != d->_id)
    node.appendChild(PfNode("hostname", d->_hostname));
  foreach (const QString &key, d->_resources.keys())
    node.appendChild(
          PfNode("resource",
                 key+" "+QString::number(d->_resources.value(key))));
  return node;
}
