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
#include "gridboard.h"
#include "util/paramset.h"
#include "config/configutils.h"
#include "util/regularexpressionmatchparamsprovider.h"

#define DEFAULT_WARNING_DELAY 600000 /* 600" = 10' */

namespace {

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Label",
  "Pattern",
  "Dimensions",
  "Params",
  "Warning Delay" // 5
};

enum GridStatus { Unknown, Ok, Long, Error };

inline QString statusToHumanReadableString(GridStatus status) {
  switch (status) {
  case Ok:
    return QStringLiteral("ok");
  case Long:
    return QStringLiteral("ok (long)");
  case Error:
    return QStringLiteral("ERROR");
  case Unknown:
    ;
  }
  return QStringLiteral("unknown");
}

class TreeItem {
public:
  QString _name;
  GridStatus _status;
  QDateTime _timestamp;
  QHash<QString,TreeItem*> _children;
  TreeItem(QString name) : _name(name), _status(Unknown) { }
  void setDataFromAlert(Alert alert) {
    _timestamp = QDateTime::currentDateTime();
    switch (alert.status()) {
    case Alert::Raised:
    case Alert::Rising:
      _status = Error;
      break;
    case Alert::MayRise:
    case Alert::Dropping:
    case Alert::Canceled:
      _status = Ok;
      break;
    default:
      _status = Unknown;
    }
  }
  ~TreeItem() { qDeleteAll(_children.values()); }
};

class DimensionData : public SharedUiItemData {
  friend class Dimension;
  QString _id, _key, _label;
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("gridboarddimension"); }
};

class Dimension : public SharedUiItem {
public:
  Dimension(PfNode node) {
    DimensionData *d = new DimensionData;
    d->_id = ConfigUtils::sanitizeId(node.contentAsString(),
                                     ConfigUtils::LocalId);
    if (d->_id.isEmpty()) {
      delete d;
      return;
    }
    d->_key = node.attribute(QStringLiteral("key"));
    if (d->_key.isEmpty())
      d->_key = "%{"+d->_id+"}";
    d->_label = node.attribute(QStringLiteral("label"));
  }
  PfNode toPfNode() const {
    const DimensionData *d = data();
    if (!d)
      return PfNode();
    PfNode node(QStringLiteral("dimension"));
    if (d->_key != "%{"+d->_id+"}" && d->_key != "%"+d->_id)
      node.setAttribute(QStringLiteral("key"), d->_key);
    if (!d->_label.isEmpty())
      node.setAttribute(QStringLiteral("label"), d->_label);
    return node;
  }
  const DimensionData *data() const {
    return (const DimensionData *)SharedUiItem::data();
  }
  QString key() const {
    const DimensionData *d = data();
    return d ? d->_key : QString();
  }
  QString label() const {
    const DimensionData *d = data();
    return d ? (d->_label.isEmpty() ? d->_id : d->_label) : QString();
  }
};

} // unnamed namespace

class GridboardData : public SharedUiItemData {
public:
  QString _id, _label, _pattern;
  QRegularExpression _patternRegexp;
  QList<Dimension> _dimensions;
  ParamSet _params;
  qint64 _warningDelay;
  TreeItem _dataTree;
  QStringList _commentsList;
  GridboardData() : _warningDelay(DEFAULT_WARNING_DELAY),
    _dataTree(QStringLiteral("root")) { }
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("gridboard"); }
  int uiSectionCount() const {
    return sizeof _uiHeaderNames / sizeof *_uiHeaderNames; }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const {
    return role == Qt::DisplayRole && section >= 0
        && (unsigned)section < sizeof _uiHeaderNames
        ? _uiHeaderNames[section] : QVariant();
  }
};

Gridboard::Gridboard(PfNode node, Gridboard oldGridboard) {
  GridboardData *d = new GridboardData;
  d->_id = node.contentAsString();
  if (d->_id.isEmpty()) {
    // FIXME warn
    delete d;
    return;
  }
  d->_label = node.attribute(QStringLiteral("label"));
  d->_pattern = node.attribute(QStringLiteral("pattern"));
  d->_patternRegexp = QRegularExpression(d->_pattern);
  // FIXME warn if invalid
  foreach (const PfNode &child, node.childrenByName("dimension")) {
    Dimension dimension(child);
    if (dimension.isNull()) {
      // FIXME warn
      continue;
    } else {
      if (d->_dimensions.contains(dimension)) {
        // FIXME warn
        continue;
      }
      d->_dimensions.append(dimension);
    }
  }
  d->_warningDelay = node.doubleAttribute(
        QStringLiteral("gridboard.warningdelay"),
        DEFAULT_WARNING_DELAY/1e3)*1e3;
  ConfigUtils::loadParamSet(node, QStringLiteral("param"));
  ConfigUtils::loadComments(node, &d->_commentsList);
  // FIXME load old state
  // FIXME load initvalues
  setData(d);
}

PfNode Gridboard::toPfNode() const {
  const GridboardData *d = data();
  if (!d)
    return PfNode();
  PfNode node(QStringLiteral("gridboard"), d->_id);
  ConfigUtils::writeComments(&node, d->_commentsList);
  ConfigUtils::writeParamSet(&node, d->_params, QStringLiteral("param"));
  if (d->_warningDelay != DEFAULT_WARNING_DELAY)
    node.setAttribute(QStringLiteral("gridboard.warningdelay"),
                      d->_warningDelay/1e3);
  if (!d->_label.isEmpty())
    node.setAttribute(QStringLiteral("label"), d->_label);
  node.setAttribute(QStringLiteral("pattern"), d->_pattern);
  foreach (const Dimension &dimension, d->_dimensions)
    node.appendChild(dimension.toPfNode());
  // FIXME initvalues
  return node;
}

GridboardData *Gridboard::data() {
  detach<GridboardData>();
  return (GridboardData*)SharedUiItem::data();
}

QRegularExpression Gridboard::patternRegexp() const {
  const GridboardData *d = data();
  return d ? d->_patternRegexp : QRegularExpression();
}

void Gridboard::update(QRegularExpressionMatch match, Alert alert) {
  GridboardData *d = data();
  if (!d)
    return;
  TreeItem *treeItem = &d->_dataTree;
  RegularExpressionMatchParamsProvider rempp(match);
  foreach (const Dimension &dimension, d->_dimensions) {
    QString dimensionValue =
        rempp.paramValue(dimension.key(), QStringLiteral("(none)")).toString();
    TreeItem *child = treeItem->_children.value(dimensionValue);
    if (!child) {
      child = new TreeItem(dimensionValue);
      treeItem->_children.insert(dimensionValue, child);
    }
    treeItem = child;
  }
  treeItem->setDataFromAlert(alert);
}

static QString oneDimensionColumnName = "status";

QString Gridboard::toHtmlTable() const {
  const GridboardData *d = data();
  if (!d)
    return QString();
  QDateTime now = QDateTime::currentDateTime();
  QHash<QString,QHash<QString, TreeItem*> > matrix;
  QStringList rows, columns;
  switch (d->_dimensions.size()) {
  case 1:
    foreach (TreeItem *treeItem1, d->_dataTree._children) {
      matrix[treeItem1->_name][oneDimensionColumnName] = treeItem1;
      rows.append(treeItem1->_name);
    }
    qSort(rows);
    columns.append(oneDimensionColumnName);
    break;
  case 2: {
    QSet<QString> columnsSet;
    foreach (TreeItem *treeItem1, d->_dataTree._children) {
      rows.append(treeItem1->_name);
      foreach (TreeItem *treeItem2, treeItem1->_children) {
        matrix[treeItem1->_name][treeItem2->_name] = treeItem2;
        columnsSet.insert(treeItem2->_name);
      }
    }
    columns = columnsSet.toList();
    qSort(rows);
    qSort(columns);
    break;
  }
  default:
    // LATER support more than 2 dimensions
    return "Gridboard do not yet support more than 2 dimensions.";
  }
  QString tableClass = d->_params.value(
        QStringLiteral("table.class"),
        QStringLiteral("table table-condensed table-hover"));
  QString tdClassOk = d->_params.value(QStringLiteral("td.class.ok"));
  QString tdClassWarning = d->_params.value(
        QStringLiteral("td.class.warning"), QStringLiteral("warning"));
  QString tdClassError = d->_params.value(
        QStringLiteral("td.class.error"), QStringLiteral("danger"));
  QString tdClassUnknown = d->_params.value(QStringLiteral("td.class.warning"));
  QString s = "<table class=\""+tableClass+"\" id=\"gridboard."+d->_id
      +"\"><tr><th>&nbsp;</th>";
  foreach (const QString &column, columns)
    s = s+"<th>"+column+"</th>";
  foreach (const QString &row, rows) {
    s += "</tr><tr>";
    foreach (const QString &column, columns) {
      TreeItem *item = matrix[row][column];
      GridStatus status = item->_status;
      if (status == Ok && item->_timestamp.msecsTo(now) > d->_warningDelay)
        status = Long;
      QString tdClass;
      switch (status) {
      case Ok:
        tdClass = tdClassOk;
        break;
      case Long:
        tdClass = tdClassWarning;
        break;
      case Error:
        tdClass = tdClassError;
        break;
      case Unknown:
        tdClass = tdClassUnknown;
        break;
      }
      s = s+"<td class=\""+tdClass+"\">"+statusToHumanReadableString(status)+"<br/>"
          +item->_timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"))
          +"</td>";
    }
  }
  s += "</tr></table>";
  return s;
}

QVariant GridboardData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _label;
    case 2:
      return _pattern;
    case 3: {
      // LATER optimize
      QStringList list;
      foreach (const Dimension &dimension, _dimensions) {
        list << dimension.id()+"="+dimension.key();
      }
      return list.join(' ');
    }
    case 4:
      return _params.toString(false, false);
    case 5:
      return _warningDelay/1e3;
    }
    break;
  default:
    ;
  }
  return QVariant();
}
