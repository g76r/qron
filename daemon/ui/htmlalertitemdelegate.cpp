#include "htmlalertitemdelegate.h"

HtmlAlertItemDelegate::HtmlAlertItemDelegate(QObject *parent, int alertColumn,
                                             int actionsColumn, bool canCancel)
  : HtmlItemDelegate(parent), _alertColumn(alertColumn),
    _actionsColumn(actionsColumn), _canCancel(canCancel) {
}

QString HtmlAlertItemDelegate::text(const QModelIndex &index) const {
  QString text = HtmlItemDelegate::text(index);
  if (index.column() == _actionsColumn) {
    static QRegExp fqtnInTaskAlert("task\\.[^\\.]+\\.(.*)");
    QRegExp re(fqtnInTaskAlert);
    QString alert = index.model()
        ->index(index.row(), _alertColumn, index.parent()).data().toString();
    if (_canCancel)
      text.prepend(/* cancel immediate */
                   "<span class=\"label label-important\">"
                   "<a title=\"Cancel alert\"href=\"do?event=cancelAlert&alert="
                   +alert+"&immediately=true\"><i class=\"fa fa-check\">"
                   "</i></a></span> ");
    if (re.exactMatch(alert)) {
      text.append(
            /* related task log */
            " <span class=\"label label-info\" title=\"Related tasks log\">"
            "<a target=\"_blank\" href=\"../rest/txt/log/all/v1?filter= "
            +re.cap(1)+"/\"><i class=\"fa fa-list\"></i></a></span>"
            /* related task taskdoc */
            " <span class=\"label label-info\" title=\"Detailed task info\">"
            "<a href=\"taskdoc.html?fqtn="+re.cap(1)+"\">"
            "<i class=\"fa fa-cog\"></i></a></span>");
    }
  }
  return text;
}
