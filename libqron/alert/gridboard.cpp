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
  "Warning Delay", // 5
  "Updates Counter",
  "Renders Counter",
  "Current Components Count",
  "Current Items Count"
};

enum GridStatus { Unknown, Ok, Long, Error };

inline QString statusToHumanReadableString(GridStatus status) {
  switch (status) {
  case Ok:
    return QStringLiteral("ok");
  case Long:
    return QStringLiteral("OK BUT LONG");
  case Error:
    return QStringLiteral("ERROR");
  case Unknown:
    ;
  }
  return QStringLiteral("unknown");
}

inline QString statusToHtmlHumanReadableString(GridStatus status) {
  switch (status) {
  case Ok:
    return QStringLiteral("ok");
  case Long:
    return QStringLiteral("<stong>OK BUT LONG</stong>");
  case Error:
    return QStringLiteral("<stong>ERROR</stong>");
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
    case Alert::Nonexistent:
      _status = Ok;
      break;
    default:
      _status = Unknown;
    }
  }
  ~TreeItem() {
    qDebug() << "~TreeItem" << _name << _children.size();
    //char *n = 0;
    //*n = 42;
    foreach (TreeItem *item, _children)
      delete item;
  }
  void merge(const TreeItem *source) {
    foreach (const QString &dimensionValue, source->_children.keys()) {
      if (!_children.contains(dimensionValue)) {
        _children.insert(dimensionValue, new TreeItem(dimensionValue));
      }
      _children[dimensionValue]->merge(source->_children[dimensionValue]);
    }
    _status = source->_status;
    _timestamp = source->_timestamp;
  }
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
    setData(d);
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

static void mergeDataRoots(
    TreeItem *target, TreeItem *source,
    QList<TreeItem*> *roots, QList<QHash<QString,TreeItem*>> *indexes) {
  // recursive merge tree
  target->merge(source);
  // replace references in indexes
  for (int i = 0; i < indexes->size(); ++i) {
    QHash<QString,TreeItem*> &index = (*indexes)[i];
    foreach (const QString &dimensionValue, index.keys())
      if (index.value(dimensionValue) == source)
        index.insert(dimensionValue, target);
  }
  // delete source
  roots->removeOne(source);
  delete source;
}

class GridboardData : public SharedUiItemData {
public:
  QString _id, _label, _pattern;
  QRegularExpression _patternRegexp;
  QList<Dimension> _dimensions;
  ParamSet _params;
  qint64 _warningDelay;
  QList<TreeItem*> _dataRoots;
  QList<QHash<QString,TreeItem*>> _dataIndexesByDimension;
  QStringList _commentsList;
  qint64 _updatesCounter, _currentComponentsCount, _currentItemsCount;
  mutable QAtomicInteger<int> _rendersCounter;
  GridboardData() : _warningDelay(DEFAULT_WARNING_DELAY),
    _updatesCounter(0), _currentComponentsCount(0), _currentItemsCount(0) { }
  ~GridboardData() { } // FIXME delete roots when no longer shared by any copied instance
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

Gridboard::Gridboard(PfNode node, Gridboard oldGridboard,
                     ParamSet parentParams) {
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
  for (int i = 0; i < d->_dimensions.size(); ++i)
    d->_dataIndexesByDimension.append(QHash<QString,TreeItem*>());
  d->_warningDelay = node.doubleAttribute(
        QStringLiteral("warningdelay"),
        DEFAULT_WARNING_DELAY/1e3)*1e3;
  d->_params.setParent(parentParams);
  ConfigUtils::loadParamSet(node, &d->_params, QStringLiteral("param"));
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
  //qDebug() << "Gridboard::update" << alert.id();
  GridboardData *d = data();
  if (!d || d->_dimensions.isEmpty())
    return;
  ++d->_updatesCounter;
  // compute dimension values and identify possible roots
  QSet<TreeItem*> rootsSet;
  RegularExpressionMatchParamsProvider rempp(match);
  QStringList dimensionValues;
  for (int i = 0; i < d->_dimensions.size(); ++i) {
    QString dimensionValue =
        d->_params.evaluate(d->_dimensions[i].key(), &rempp);
    if (dimensionValue.isEmpty())
      dimensionValue = QStringLiteral("(none)");
    //qDebug() << "rempp.paramValue" << d->_dimensions[i].key() << "->"
    //         << dimensionValue << match.capturedTexts()
    //         << match.regularExpression().namedCaptureGroups();
    dimensionValues.append(dimensionValue);
    TreeItem *root = d->_dataIndexesByDimension[i].value(dimensionValue);
    if (root)
      rootsSet.insert(root);
  }
  QList<TreeItem*> roots = rootsSet.toList();
  // merge roots when needed
  while (roots.size() > 1) {
    TreeItem *source = roots.takeFirst();
    //qDebug() << "merging root:" << source << source->_name << "into"
    //         << roots.first() << roots.first()->_name;
    mergeDataRoots(roots.first(), source, &d->_dataRoots,
                   &d->_dataIndexesByDimension);
    --d->_currentComponentsCount;
  }
  TreeItem *root = roots.size() > 0 ? roots.first() : 0;
  // create new root if needed
  if (!root) {
    root = new TreeItem(QString());
    d->_dataRoots.append(root);
    d->_dataIndexesByDimension[0].insert(dimensionValues[0], root);
    ++d->_currentComponentsCount;
    //qDebug() << "creating new root" << dimensionValues[0]
    //         << d->_dataRoots.size() << d->_dataIndexesByDimension.size()
    //         << d->_dataIndexesByDimension[0].size();
  }
  TreeItem *item = root;
  //qDebug() << "found root" << root << root->_name << root->_children.size()
  //         << "for" << alert.id();
  // walk through dimensions to create or update item
  for (int i = 0; i < d->_dimensions.size(); ++i) {
    //qDebug() << "****a" << i << d->_dimensions.size() << dimensionValues.size()
    //         << item << root;
    TreeItem *child = item->_children.value(dimensionValues[i]);
    //qDebug() << "  " << child << (child ? child->_name : "null")
    //         << (child ? child->_children.size() : 0);
    if (!child) {
      child = new TreeItem(dimensionValues[i]);
      if (i == d->_dimensions.size()-1)
        ++d->_currentItemsCount;
      d->_dataIndexesByDimension[i].insert(dimensionValues[i], root);
      item->_children.insert(dimensionValues[i], child);
    }
    item = child;
  }
  item->setDataFromAlert(alert);
  //qDebug() << "  ->" << item << item->_name << item->_children.size();
}

QString Gridboard::toHtml() const {
  const GridboardData *d = data();
  if (!d)
    return QString("Null gridboard");
  d->_rendersCounter.fetchAndAddOrdered(1);
  if (d->_dataRoots.isEmpty())
    return QString("No data so far");
  QString tableClass = d->_params.value(
        QStringLiteral("gridboard.tableclass"),
        QStringLiteral("table table-condensed table-hover"));
  QString divClass = d->_params.value(
        QStringLiteral("gridboard.divclass"),
        QStringLiteral("row gridboard-status"));
  QString componentClass = d->_params.value(
        QStringLiteral("gridboard.componentclass"),
        QStringLiteral("gridboard-component"));
  QString tdClassOk = d->_params.value(QStringLiteral("gridboard.tdclass.ok"));
  QString tdClassWarning = d->_params.value(
        QStringLiteral("gridboard.tdclass.warning"), QStringLiteral("warning"));
  QString tdClassError = d->_params.value(
        QStringLiteral("gridboard.tdclass.error"), QStringLiteral("danger"));
  QString tdClassUnknown = d->_params.value(
        QStringLiteral("gridboard.tdclass.warning"));
  QString s;
  QList<TreeItem*> dataRoots;
  TreeItem *pseudoRoot = 0;
  if (d->_dimensions.size() == 1) {
    // with 1 dimension merge roots by replacing actual roots by a fake one
    pseudoRoot = new TreeItem(QString());
    foreach (TreeItem *root, d->_dataRoots)
      foreach (TreeItem *child, root->_children)
        pseudoRoot->_children.insert(child->_name, child);
    dataRoots.append(pseudoRoot);
  } else {
    dataRoots = d->_dataRoots;
  }
  s = s+"<div class=\""+divClass+"\">\n";
  // FIXME sort dataRoot by their first (in alpha order) child alpha order, or replace QHash<> children with QMap<>
  foreach (TreeItem *dataRoot, dataRoots) {
    QDateTime now = QDateTime::currentDateTime();
    QHash<QString,QHash<QString, TreeItem*> > matrix;
    QStringList rows, columns;
    switch (d->_dimensions.size()) {
    case 1: {
      // with 1 dimension convert to 2 dimensions by adding a fake column
      QString fakeDimensionName = d->_params.value(
            QStringLiteral("gridboard.fakedimensionname"),
            QStringLiteral("status"));
      foreach (TreeItem *treeItem1, dataRoot->_children) {
        matrix[treeItem1->_name][fakeDimensionName] = treeItem1;
        rows.append(treeItem1->_name);
        //qDebug() << "**b" << treeItem1 << treeItem1->_name
        //         << treeItem1->_children.size() << treeItem1->_timestamp
        //         << statusToHumanReadableString(treeItem1->_status);
        //if (treeItem1->_children.size()) {
        //  qDebug() << "  " << treeItem1->_children[0];
        //  qDebug() << "  " << treeItem1->_children[0]->_name;
        //  qDebug() << "  " << treeItem1->_children[0]->_timestamp;
        //  qDebug() << "  " << statusToHumanReadableString(treeItem1->_children[0]->_status);
        //}
      }
      qSort(rows);
      columns.append(fakeDimensionName);
      break;
    }
    case 2: {
      QSet<QString> columnsSet;
      foreach (TreeItem *treeItem1, dataRoot->_children) {
        //qDebug() << "**c row:" << treeItem1 << treeItem1->_name
        //         << treeItem1->_children.size();
        rows.append(treeItem1->_name);
        foreach (TreeItem *treeItem2, treeItem1->_children) {
          //qDebug() << "**c col:" << treeItem2 << treeItem2->_name
          //         << treeItem2->_children.size();
          matrix[treeItem1->_name][treeItem2->_name] = treeItem2;
          columnsSet.insert(treeItem2->_name);
        }
      }
      columns = columnsSet.toList();
      qSort(rows);
      qSort(columns);
      break;
    }
    case 0:
      return "Gridboard with 0 dimensions.";
    default:
      // LATER support more than 2 dimensions
      return "Gridboard do not yet support more than 2 dimensions.";
    }
    s = s+"<div class=\""+componentClass+"\"><table class=\""+tableClass
        +"\" id=\"gridboard."+d->_id+"\"><tr><th>&nbsp;</th>";
    foreach (const QString &column, columns)
      s = s+"<th>"+column+"</th>";
    foreach (const QString &row, rows) {
      s = s+"</tr><tr><th>"+row+"</th>";
      foreach (const QString &column, columns) {
        TreeItem *item = matrix[row][column];
        GridStatus status = item ? item->_status : Unknown;
        if (status == Ok && item &&
            item->_timestamp.msecsTo(now) > d->_warningDelay)
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
        s = s+"<td class=\""+tdClass+"\">"
            +(item ? statusToHtmlHumanReadableString(status)
                     +"<br/>"+item->_timestamp.toString(
                       QStringLiteral("yyyy-MM-dd hh:mm:ss,zzz"))
                   : "")+"</td>";
      }
    }
    s += "</tr></table></div>\n";
    //s = s+"<p>rows:"+rows.join(',')+" columns:"+columns.join(',')
    //    +" dimensions#:"+QString::number(d->_dimensions.size())
    //    +" roots#:"+QString::number(dataRoots.size())
    //    +" indexes:";
    //for (int i = 0; i < d->_dataIndexesByDimension.size(); ++i)
    //  s = s+QString::number(d->_dataIndexesByDimension[i].size())+" ";
  }
  s += "</div>";
  if (pseudoRoot) {
    pseudoRoot->_children.clear(); // prevent deleting children
    delete pseudoRoot;
  }
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
    case 6:
      return _updatesCounter;
    case 7:
      return _rendersCounter.load();
    case 8:
      return _currentComponentsCount;
    case 9:
      return _currentItemsCount;
    }
    break;
  default:
    ;
  }
  return QVariant();
}
