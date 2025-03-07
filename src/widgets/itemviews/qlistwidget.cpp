// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlistwidget.h"

#include <private/qlistview_p.h>
#include <private/qwidgetitemdata_p.h>
#include <private/qlistwidget_p.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

class QListWidgetMimeData : public QMimeData
{
    Q_OBJECT
public:
    QList<QListWidgetItem*> items;
};

QT_BEGIN_INCLUDE_NAMESPACE
#include "qlistwidget.moc"
QT_END_INCLUDE_NAMESPACE

QListModel::QListModel(QListWidget *parent)
    : QAbstractListModel(parent)
{
}

QListModel::~QListModel()
{
    clear();
}

void QListModel::clear()
{
    beginResetModel();
    for (int i = 0; i < items.size(); ++i) {
        if (items.at(i)) {
            items.at(i)->d->theid = -1;
            items.at(i)->view = nullptr;
            delete items.at(i);
        }
    }
    items.clear();
    endResetModel();
}

QListWidgetItem *QListModel::at(int row) const
{
    return items.value(row);
}

void QListModel::remove(QListWidgetItem *item)
{
    if (!item)
        return;
    int row = items.indexOf(item); // ### use index(item) - it's faster
    Q_ASSERT(row != -1);
    beginRemoveRows(QModelIndex(), row, row);
    items.at(row)->d->theid = -1;
    items.at(row)->view = nullptr;
    items.removeAt(row);
    endRemoveRows();
}

void QListModel::insert(int row, QListWidgetItem *item)
{
    if (!item)
        return;

    item->view = qobject_cast<QListWidget*>(QObject::parent());
    if (item->view && item->view->isSortingEnabled()) {
        // sorted insertion
        QList<QListWidgetItem*>::iterator it;
        it = sortedInsertionIterator(items.begin(), items.end(),
                                     item->view->sortOrder(), item);
        row = qMax<qsizetype>(it - items.begin(), 0);
    } else {
        if (row < 0)
            row = 0;
        else if (row > items.size())
            row = items.size();
    }
    beginInsertRows(QModelIndex(), row, row);
    items.insert(row, item);
    item->d->theid = row;
    endInsertRows();
}

void QListModel::insert(int row, const QStringList &labels)
{
    const int count = labels.size();
    if (count <= 0)
        return;
    QListWidget *view = qobject_cast<QListWidget*>(QObject::parent());
    if (view && view->isSortingEnabled()) {
        // sorted insertion
        for (int i = 0; i < count; ++i) {
            QListWidgetItem *item = new QListWidgetItem(labels.at(i));
            insert(row, item);
        }
    } else {
        if (row < 0)
            row = 0;
        else if (row > items.size())
            row = items.size();
        beginInsertRows(QModelIndex(), row, row + count - 1);
        for (int i = 0; i < count; ++i) {
            QListWidgetItem *item = new QListWidgetItem(labels.at(i));
            item->d->theid = row;
            item->view = qobject_cast<QListWidget*>(QObject::parent());
            items.insert(row++, item);
        }
        endInsertRows();
    }
}

QListWidgetItem *QListModel::take(int row)
{
    if (row < 0 || row >= items.size())
        return nullptr;

    beginRemoveRows(QModelIndex(), row, row);
    items.at(row)->d->theid = -1;
    items.at(row)->view = nullptr;
    QListWidgetItem *item = items.takeAt(row);
    endRemoveRows();
    return item;
}

void QListModel::move(int srcRow, int dstRow)
{
    if (srcRow == dstRow
        || srcRow < 0 || srcRow >= items.size()
        || dstRow < 0 || dstRow > items.size())
        return;

    if (!beginMoveRows(QModelIndex(), srcRow, srcRow, QModelIndex(), dstRow))
        return;
    if (srcRow < dstRow)
        --dstRow;
    items.move(srcRow, dstRow);
    endMoveRows();
}

int QListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : items.size();
}

QModelIndex QListModel::index(const QListWidgetItem *item_) const
{
    QListWidgetItem *item = const_cast<QListWidgetItem *>(item_);
    if (!item || !item->view || static_cast<const QListModel *>(item->view->model()) != this
        || items.isEmpty())
        return QModelIndex();
    int row;
    const int theid = item->d->theid;
    if (theid >= 0 && theid < items.size() && items.at(theid) == item) {
        row = theid;
    } else { // we need to search for the item
        row = items.lastIndexOf(item);  // lastIndexOf is an optimization in favor of indexOf
        if (row == -1) // not found
            return QModelIndex();
        item->d->theid = row;
    }
    return createIndex(row, 0, item);
}

QModelIndex QListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return createIndex(row, column, items.at(row));
    return QModelIndex();
}

QVariant QListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size())
        return QVariant();
    return items.at(index.row())->data(role);
}

bool QListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= items.size())
        return false;
    items.at(index.row())->setData(role, value);
    return true;
}

bool QListModel::clearItemData(const QModelIndex &index)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return false;
    QListWidgetItem *item = items.at(index.row());
    const auto beginIter = item->d->values.cbegin();
    const auto endIter = item->d->values.cend();
    if (std::all_of(beginIter, endIter, [](const QWidgetItemData& data) -> bool { return !data.value.isValid(); }))
        return true; //it's already cleared
    item->d->values.clear();
    emit dataChanged(index, index, QList<int> {});
    return true;
}

QMap<int, QVariant> QListModel::itemData(const QModelIndex &index) const
{
    QMap<int, QVariant> roles;
    if (!index.isValid() || index.row() >= items.size())
        return roles;
    QListWidgetItem *itm = items.at(index.row());
    for (int i = 0; i < itm->d->values.size(); ++i) {
        roles.insert(itm->d->values.at(i).role,
                     itm->d->values.at(i).value);
    }
    return roles;
}

bool QListModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > rowCount() || parent.isValid())
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);
    QListWidget *view = qobject_cast<QListWidget*>(QObject::parent());
    QListWidgetItem *itm = nullptr;

    for (int r = row; r < row + count; ++r) {
        itm = new QListWidgetItem;
        itm->view = view;
        itm->d->theid = r;
        items.insert(r, itm);
    }

    endInsertRows();
    return true;
}

bool QListModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || (row + count) > rowCount() || parent.isValid())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    QListWidgetItem *itm = nullptr;
    for (int r = row; r < row + count; ++r) {
        itm = items.takeAt(row);
        itm->view = nullptr;
        itm->d->theid = -1;
        delete itm;
    }
    endRemoveRows();
    return true;
}

/*!
    \since 5.13
    \reimp
*/
bool QListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceRow < 0
        || sourceRow + count - 1 >= rowCount(sourceParent)
        || destinationChild < 0
        || destinationChild > rowCount(destinationParent)
        || sourceRow == destinationChild
        || sourceRow == destinationChild - 1
        || count <= 0
        || sourceParent.isValid()
        || destinationParent.isValid()) {
        return false;
    }
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild))
        return false;

    int fromRow = sourceRow;
    if (destinationChild < sourceRow)
        fromRow += count - 1;
    else
        destinationChild--;
    while (count--)
        items.move(fromRow, destinationChild);
    endMoveRows();
    return true;
}

Qt::ItemFlags QListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= items.size() || index.model() != this)
        return Qt::ItemIsDropEnabled; // we allow drops outside the items
    return items.at(index.row())->flags();
}

void QListModel::sort(int column, Qt::SortOrder order)
{
    if (column != 0)
        return;

    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);

    QList<QPair<QListWidgetItem *, int>> sorting(items.size());
    for (int i = 0; i < items.size(); ++i) {
        QListWidgetItem *item = items.at(i);
        sorting[i].first = item;
        sorting[i].second = i;
    }

    const auto compare = (order == Qt::AscendingOrder ? &itemLessThan : &itemGreaterThan);
    std::stable_sort(sorting.begin(), sorting.end(), compare);
    QModelIndexList fromIndexes;
    QModelIndexList toIndexes;
    const int sortingCount = sorting.size();
    fromIndexes.reserve(sortingCount);
    toIndexes.reserve(sortingCount);
    for (int r = 0; r < sortingCount; ++r) {
        QListWidgetItem *item = sorting.at(r).first;
        toIndexes.append(createIndex(r, 0, item));
        fromIndexes.append(createIndex(sorting.at(r).second, 0, sorting.at(r).first));
        items[r] = sorting.at(r).first;
    }
    changePersistentIndexList(fromIndexes, toIndexes);

    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

/**
 * This function assumes that all items in the model except the items that are between
 * (inclusive) start and end are sorted.
 */
void QListModel::ensureSorted(int column, Qt::SortOrder order, int start, int end)
{
    if (column != 0)
        return;

    const auto compareLt = [](const QListWidgetItem *left, const QListWidgetItem *right) -> bool {
        return *left < *right;
    };

    const auto compareGt = [](const QListWidgetItem *left, const QListWidgetItem *right) -> bool {
        return *right < *left;
    };

    /** Check if range [start,end] is already in sorted position in list.
     *  Take for this the assumption, that outside [start,end] the list
     *  is already sorted. Therefore the sorted check has to be extended
     *  to the first element that is known to be sorted before the range
     *  [start, end], which is (start-1) and the first element after the
     *  range [start, end], which is (end+2) due to end being included.
    */
    const auto beginChangedIterator = items.constBegin() + qMax(start - 1, 0);
    const auto endChangedIterator = items.constBegin() + qMin(end + 2, items.size());
    const bool needsSorting = !std::is_sorted(beginChangedIterator, endChangedIterator,
                                              order == Qt::AscendingOrder ? compareLt : compareGt);

    if (needsSorting)
        sort(column, order);
}

bool QListModel::itemLessThan(const QPair<QListWidgetItem*,int> &left,
                              const QPair<QListWidgetItem*,int> &right)
{
    return (*left.first) < (*right.first);
}

bool QListModel::itemGreaterThan(const QPair<QListWidgetItem*,int> &left,
                                 const QPair<QListWidgetItem*,int> &right)
{
    return (*right.first) < (*left.first);
}

QList<QListWidgetItem*>::iterator QListModel::sortedInsertionIterator(
    const QList<QListWidgetItem*>::iterator &begin,
    const QList<QListWidgetItem*>::iterator &end,
    Qt::SortOrder order, QListWidgetItem *item)
{
    if (order == Qt::AscendingOrder)
        return std::lower_bound(begin, end, item, QListModelLessThan());
    return std::lower_bound(begin, end, item, QListModelGreaterThan());
}

void QListModel::itemChanged(QListWidgetItem *item, const QList<int> &roles)
{
    const QModelIndex idx = index(item);
    emit dataChanged(idx, idx, roles);
}

QStringList QListModel::mimeTypes() const
{
    const QListWidget *view = qobject_cast<const QListWidget*>(QObject::parent());
    if (view)
        return view->mimeTypes();
    return {};
}

QMimeData *QListModel::internalMimeData()  const
{
    return QAbstractItemModel::mimeData(cachedIndexes);
}

QMimeData *QListModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QListWidgetItem*> itemlist;
    const int indexesCount = indexes.size();
    itemlist.reserve(indexesCount);
    for (int i = 0; i < indexesCount; ++i)
        itemlist << at(indexes.at(i).row());
    const QListWidget *view = qobject_cast<const QListWidget*>(QObject::parent());

    cachedIndexes = indexes;
    QMimeData *mimeData = view->mimeData(itemlist);
    cachedIndexes.clear();
    return mimeData;
}

#if QT_CONFIG(draganddrop)
bool QListModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column, const QModelIndex &index)
{
    Q_UNUSED(column);
    QListWidget *view = qobject_cast<QListWidget*>(QObject::parent());
    if (index.isValid())
        row = index.row();
    else if (row == -1)
        row = items.size();

    return view->dropMimeData(row, data, action);
}

Qt::DropActions QListModel::supportedDropActions() const
{
    const QListWidget *view = qobject_cast<const QListWidget*>(QObject::parent());
    return view->supportedDropActions();
}
#endif // QT_CONFIG(draganddrop)

/*!
    \class QListWidgetItem
    \brief The QListWidgetItem class provides an item for use with the
    QListWidget item view class.

    \ingroup model-view
    \inmodule QtWidgets

    A QListWidgetItem represents a single item in a QListWidget. Each item can
    hold several pieces of information, and will display them appropriately.

    The item view convenience classes use a classic item-based interface rather
    than a pure model/view approach. For a more flexible list view widget,
    consider using the QListView class with a standard model.

    List items can be inserted automatically into a list, when they are
    constructed, by specifying the list widget:

    \snippet qlistwidget-using/mainwindow.cpp 2

    Alternatively, list items can also be created without a parent widget, and
    later inserted into a list using QListWidget::insertItem().

    List items are typically used to display text() and an icon(). These are
    set with the setText() and setIcon() functions. The appearance of the text
    can be customized with setFont(), setForeground(), and setBackground().
    Text in list items can be aligned using the setTextAlignment() function.
    Tooltips, status tips and "What's This?" help can be added to list items
    with setToolTip(), setStatusTip(), and setWhatsThis().

    By default, items are enabled, selectable, checkable, and can be the source
    of drag and drop operations.

    Each item's flags can be changed by calling setFlags() with the appropriate
    value (see Qt::ItemFlags). Checkable items can be checked, unchecked and
    partially checked with the setCheckState() function. The corresponding
    checkState() function indicates the item's current check state.

    The isHidden() function can be used to determine whether the item is
    hidden. To hide an item, use setHidden().


    \section1 Subclassing

    When subclassing QListWidgetItem to provide custom items, it is possible to
    define new types for them enabling them to be distinguished from standard
    items. For subclasses that require this feature, ensure that you call the
    base class constructor with a new type value equal to or greater than
    \l UserType, within \e your constructor.

    \sa QListWidget, {Model/View Programming}, QTreeWidgetItem, QTableWidgetItem
*/

/*!
    \enum QListWidgetItem::ItemType

    This enum describes the types that are used to describe list widget items.

    \value Type     The default type for list widget items.
    \value UserType The minimum value for custom types. Values below UserType are
                    reserved by Qt.

    You can define new user types in QListWidgetItem subclasses to ensure that
    custom items are treated specially.

    \sa type()
*/

/*!
    \fn int QListWidgetItem::type() const

    Returns the type passed to the QListWidgetItem constructor.
*/

/*!
    \fn QListWidget *QListWidgetItem::listWidget() const

    Returns the list widget containing the item.
*/

/*!
    \fn void QListWidgetItem::setHidden(bool hide)

    Hides the item if \a hide is true; otherwise shows the item.

    \sa isHidden()
*/

/*!
    \fn bool QListWidgetItem::isHidden() const

    Returns \c true if the item is hidden; otherwise returns \c false.

    \sa setHidden()
*/

/*!
    \fn QListWidgetItem::QListWidgetItem(QListWidget *parent, int type)

    Constructs an empty list widget item of the specified \a type with the
    given \a parent. If \a parent is not specified, the item will need to be
    inserted into a list widget with QListWidget::insertItem().

    This constructor inserts the item into the model of the parent that is
    passed to the constructor. If the model is sorted then the behavior of the
    insert is undetermined since the model will call the \c '<' operator method
    on the item which, at this point, is not yet constructed. To avoid the
    undetermined behavior, we recommend not to specify the parent and use
    QListWidget::insertItem() instead.

    \sa type()
*/
QListWidgetItem::QListWidgetItem(QListWidget *listview, int type)
    : rtti(type), view(listview), d(new QListWidgetItemPrivate(this)),
      itemFlags(Qt::ItemIsSelectable
                |Qt::ItemIsUserCheckable
                |Qt::ItemIsEnabled
                |Qt::ItemIsDragEnabled)
{
    if (QListModel *model = listModel())
        model->insert(model->rowCount(), this);
}

/*!
    \fn QListWidgetItem::QListWidgetItem(const QString &text, QListWidget *parent, int type)

    Constructs an empty list widget item of the specified \a type with the
    given \a text and \a parent. If the parent is not specified, the item will
    need to be inserted into a list widget with QListWidget::insertItem().

    This constructor inserts the item into the model of the parent that is
    passed to the constructor. If the model is sorted then the behavior of the
    insert is undetermined since the model will call the \c '<' operator method
    on the item which, at this point, is not yet constructed. To avoid the
    undetermined behavior, we recommend not to specify the parent and use
    QListWidget::insertItem() instead.

    \sa type()
*/
QListWidgetItem::QListWidgetItem(const QString &text, QListWidget *listview, int type)
    : rtti(type), view(listview), d(new QListWidgetItemPrivate(this)),
      itemFlags(Qt::ItemIsSelectable
                |Qt::ItemIsUserCheckable
                |Qt::ItemIsEnabled
                |Qt::ItemIsDragEnabled)
{
    QListModel *model = listModel();
    {
        QSignalBlocker b(view);
        QSignalBlocker bm(model);
        setData(Qt::DisplayRole, text);
    }
    if (model)
        model->insert(model->rowCount(), this);
}

/*!
    \fn QListWidgetItem::QListWidgetItem(const QIcon &icon, const QString &text, QListWidget *parent, int type)

    Constructs an empty list widget item of the specified \a type with the
    given \a icon, \a text and \a parent. If the parent is not specified, the
    item will need to be inserted into a list widget with
    QListWidget::insertItem().

    This constructor inserts the item into the model of the parent that is
    passed to the constructor. If the model is sorted then the behavior of the
    insert is undetermined since the model will call the \c '<' operator method
    on the item which, at this point, is not yet constructed. To avoid the
    undetermined behavior, we recommend not to specify the parent and use
    QListWidget::insertItem() instead.

    \sa type()
*/
QListWidgetItem::QListWidgetItem(const QIcon &icon,const QString &text,
                                 QListWidget *listview, int type)
    : rtti(type), view(listview), d(new QListWidgetItemPrivate(this)),
      itemFlags(Qt::ItemIsSelectable
                |Qt::ItemIsUserCheckable
                |Qt::ItemIsEnabled
                |Qt::ItemIsDragEnabled)
{
    QListModel *model = listModel();
    {
        QSignalBlocker b(view);
        QSignalBlocker bm(model);
        setData(Qt::DisplayRole, text);
        setData(Qt::DecorationRole, icon);
    }
    if (model)
        model->insert(model->rowCount(), this);
}

/*!
    Destroys the list item.
*/
QListWidgetItem::~QListWidgetItem()
{
    if (QListModel *model = listModel())
        model->remove(this);
    delete d;
}

/*!
    Creates an exact copy of the item.
*/
QListWidgetItem *QListWidgetItem::clone() const
{
    return new QListWidgetItem(*this);
}

/*!
    Sets the data for a given \a role to the given \a value. Reimplement this
    function if you need extra roles or special behavior for certain roles.

    \note The default implementation treats Qt::EditRole and Qt::DisplayRole as
    referring to the same data.

    \sa Qt::ItemDataRole, data()
*/
void QListWidgetItem::setData(int role, const QVariant &value)
{
    bool found = false;
    role = (role == Qt::EditRole ? Qt::DisplayRole : role);
    for (int i = 0; i < d->values.size(); ++i) {
        if (d->values.at(i).role == role) {
            if (d->values.at(i).value == value)
                return;
            d->values[i].value = value;
            found = true;
            break;
        }
    }
    if (!found)
        d->values.append(QWidgetItemData(role, value));
    if (QListModel *model = listModel()) {
        const QList<int> roles((role == Qt::DisplayRole)
                                       ? QList<int>({ Qt::DisplayRole, Qt::EditRole })
                                       : QList<int>({ role }));
        model->itemChanged(this, roles);
    }
}

/*!
    Returns the item's data for a given \a role. Reimplement this function if
    you need extra roles or special behavior for certain roles.

    \sa Qt::ItemDataRole, setData()
*/
QVariant QListWidgetItem::data(int role) const
{
    role = (role == Qt::EditRole ? Qt::DisplayRole : role);
    for (int i = 0; i < d->values.size(); ++i)
        if (d->values.at(i).role == role)
            return d->values.at(i).value;
    return QVariant();
}

/*!
    Returns \c true if this item's text is less then \a other item's text;
    otherwise returns \c false.
*/
bool QListWidgetItem::operator<(const QListWidgetItem &other) const
{
    const QVariant v1 = data(Qt::DisplayRole), v2 = other.data(Qt::DisplayRole);
    return QAbstractItemModelPrivate::variantLessThan(v1, v2);
}

#ifndef QT_NO_DATASTREAM

/*!
    Reads the item from stream \a in.

    \sa write()
*/
void QListWidgetItem::read(QDataStream &in)
{
    in >> d->values;
}

/*!
    Writes the item to stream \a out.

    \sa read()
*/
void QListWidgetItem::write(QDataStream &out) const
{
    out << d->values;
}
#endif // QT_NO_DATASTREAM

/*!
    Constructs a copy of \a other. Note that type() and listWidget() are not
    copied.

    This function is useful when reimplementing clone().

    \sa data(), flags()
*/
QListWidgetItem::QListWidgetItem(const QListWidgetItem &other)
    : rtti(Type), view(nullptr),
      d(new QListWidgetItemPrivate(this)),
      itemFlags(other.itemFlags)
{
    d->values = other.d->values;
}

/*!
    Assigns \a other's data and flags to this item. Note that type() and
    listWidget() are not copied.

    This function is useful when reimplementing clone().

    \sa data(), flags()
*/
QListWidgetItem &QListWidgetItem::operator=(const QListWidgetItem &other)
{
    d->values = other.d->values;
    itemFlags = other.itemFlags;
    return *this;
}

/*!
   \internal
   returns the QListModel if a view is set
 */
QListModel *QListWidgetItem::listModel() const
{
    return (view ? qobject_cast<QListModel*>(view->model()) : nullptr);
}

#ifndef QT_NO_DATASTREAM

/*!
    \relates QListWidgetItem

    Writes the list widget item \a item to stream \a out.

    This operator uses QListWidgetItem::write().

    \sa {Serializing Qt Data Types}
*/
QDataStream &operator<<(QDataStream &out, const QListWidgetItem &item)
{
    item.write(out);
    return out;
}

/*!
    \relates QListWidgetItem

    Reads a list widget item from stream \a in into \a item.

    This operator uses QListWidgetItem::read().

    \sa {Serializing Qt Data Types}
*/
QDataStream &operator>>(QDataStream &in, QListWidgetItem &item)
{
    item.read(in);
    return in;
}

#endif // QT_NO_DATASTREAM

/*!
    \fn Qt::ItemFlags QListWidgetItem::flags() const

    Returns the item flags for this item (see \l{Qt::ItemFlags}).
*/

/*!
    \fn QString QListWidgetItem::text() const

    Returns the list item's text.

    \sa setText()
*/

/*!
    \fn QIcon QListWidgetItem::icon() const

    Returns the list item's icon.

    \sa setIcon(), {QAbstractItemView::iconSize}{iconSize}
*/

/*!
    \fn QString QListWidgetItem::statusTip() const

    Returns the list item's status tip.

    \sa setStatusTip()
*/

/*!
    \fn QString QListWidgetItem::toolTip() const

    Returns the list item's tooltip.

    \sa setToolTip(), statusTip(), whatsThis()
*/

/*!
    \fn QString QListWidgetItem::whatsThis() const

    Returns the list item's "What's This?" help text.

    \sa setWhatsThis(), statusTip(), toolTip()
*/

/*!
    \fn QFont QListWidgetItem::font() const

    Returns the font used to display this list item's text.
*/

/*!
    \if defined(qt7)

    \fn Qt::Alignment QListWidgetItem::textAlignment() const

    Returns the text alignment for the list item.

    \else

    \fn int QListWidgetItem::textAlignment() const

    Returns the text alignment for the list item.

    \note This function returns an int for historical reasons. It will
    be corrected to return Qt::Alignment in Qt 7.

    \sa Qt::Alignment

    \endif
*/

/*!
    \fn QBrush QListWidgetItem::background() const

    Returns the brush used to display the list item's background.

    \sa setBackground(), foreground()
*/

/*!
    \fn QBrush QListWidgetItem::foreground() const

    Returns the brush used to display the list item's foreground (e.g. text).

    \sa setForeground(), background()
*/

/*!
    \fn Qt::CheckState QListWidgetItem::checkState() const

    Returns the checked state of the list item (see \l{Qt::CheckState}).

    \sa flags()
*/

/*!
    \fn QSize QListWidgetItem::sizeHint() const

    Returns the size hint set for the list item.
*/

/*!
    \fn void QListWidgetItem::setSizeHint(const QSize &size)

    Sets the size hint for the list item to be \a size.
    If no size hint is set or \a size is invalid, the item
    delegate will compute the size hint based on the item data.
*/

/*!
    \fn void QListWidgetItem::setSelected(bool select)

    Sets the selected state of the item to \a select.

    \sa isSelected()
*/
void QListWidgetItem::setSelected(bool select)
{
    const QListModel *model = listModel();
    if (!model || !view->selectionModel())
        return;
    const QAbstractItemView::SelectionMode selectionMode = view->selectionMode();
    if (selectionMode == QAbstractItemView::NoSelection)
        return;
    const QModelIndex index = model->index(this);
    if (selectionMode == QAbstractItemView::SingleSelection)
        view->selectionModel()->select(index, select
                                       ? QItemSelectionModel::ClearAndSelect
                                       : QItemSelectionModel::Deselect);
    else
        view->selectionModel()->select(index, select
                                       ? QItemSelectionModel::Select
                                       : QItemSelectionModel::Deselect);
}

/*!
    \fn bool QListWidgetItem::isSelected() const

    Returns \c true if the item is selected; otherwise returns \c false.

    \sa setSelected()
*/
bool QListWidgetItem::isSelected() const
{
    const QListModel *model = listModel();
    if (!model || !view->selectionModel())
        return false;
    const QModelIndex index = model->index(this);
    return view->selectionModel()->isSelected(index);
}

/*!
    \fn void QListWidgetItem::setFlags(Qt::ItemFlags flags)

    Sets the item flags for the list item to \a flags.

    \sa Qt::ItemFlags
*/
void QListWidgetItem::setFlags(Qt::ItemFlags aflags)
{
    itemFlags = aflags;
    if (QListModel *model = listModel())
        model->itemChanged(this);
}


/*!
    \fn void QListWidgetItem::setText(const QString &text)

    Sets the text for the list widget item's to the given \a text.

    \sa text()
*/

/*!
    \fn void QListWidgetItem::setIcon(const QIcon &icon)

    Sets the icon for the list item to the given \a icon.

    \sa icon(), text(), {QAbstractItemView::iconSize}{iconSize}
*/

/*!
    \fn void QListWidgetItem::setStatusTip(const QString &statusTip)

    Sets the status tip for the list item to the text specified by
    \a statusTip. QListWidget mouseTracking needs to be enabled for this
    feature to work.

    \sa statusTip(), setToolTip(), setWhatsThis(), QWidget::setMouseTracking()
*/

/*!
    \fn void QListWidgetItem::setToolTip(const QString &toolTip)

    Sets the tooltip for the list item to the text specified by \a toolTip.

    \sa toolTip(), setStatusTip(), setWhatsThis()
*/

/*!
    \fn void QListWidgetItem::setWhatsThis(const QString &whatsThis)

    Sets the "What's This?" help for the list item to the text specified by
    \a whatsThis.

    \sa whatsThis(), setStatusTip(), setToolTip()
*/

/*!
    \fn void QListWidgetItem::setFont(const QFont &font)

    Sets the font used when painting the item to the given \a font.
*/

/*!
    \obsolete [6.4] Use the overload that takes a Qt::Alignment argument.

    \fn void QListWidgetItem::setTextAlignment(int alignment)

    Sets the list item's text alignment to \a alignment.

    \sa Qt::Alignment
*/

/*!
    \since 6.4

    \fn void QListWidgetItem::setTextAlignment(Qt::Alignment alignment)

    Sets the list item's text alignment to \a alignment.
*/

/*!
    \fn void QListWidgetItem::setTextAlignment(Qt::AlignmentFlag alignment)
    \internal
*/

/*!
    \fn void QListWidgetItem::setBackground(const QBrush &brush)

    Sets the background brush of the list item to the given \a brush.
    Setting a default-constructed brush will let the view use the
    default color from the style.

    \sa background(), setForeground()
*/

/*!
    \fn void QListWidgetItem::setForeground(const QBrush &brush)

    Sets the foreground brush of the list item to the given \a brush.
    Setting a default-constructed brush will let the view use the
    default color from the style.

    \sa foreground(), setBackground()
*/

/*!
    \fn void QListWidgetItem::setCheckState(Qt::CheckState state)

    Sets the check state of the list item to \a state.

    \sa checkState()
*/

void QListWidgetPrivate::setup()
{
    Q_Q(QListWidget);
    q->QListView::setModel(new QListModel(q));
    // view signals
    connections = {
        QObjectPrivate::connect(q, &QListWidget::pressed,
                                this, &QListWidgetPrivate::emitItemPressed),
        QObjectPrivate::connect(q, &QListWidget::clicked,
                                this, &QListWidgetPrivate::emitItemClicked),
        QObjectPrivate::connect(q, &QListWidget::doubleClicked,
                                this, &QListWidgetPrivate::emitItemDoubleClicked),
        QObjectPrivate::connect(q, &QListWidget::activated,
                                this, &QListWidgetPrivate::emitItemActivated),
        QObjectPrivate::connect(q, &QListWidget::entered,
                                this, &QListWidgetPrivate::emitItemEntered),
        QObjectPrivate::connect(model, &QAbstractItemModel::dataChanged,
                                this, &QListWidgetPrivate::emitItemChanged),
        QObjectPrivate::connect(model, &QAbstractItemModel::dataChanged,
                                this, &QListWidgetPrivate::dataChanged),
        QObjectPrivate::connect(model, &QAbstractItemModel::columnsRemoved,
                                this, &QListWidgetPrivate::sort)
    };
}

void QListWidgetPrivate::clearConnections()
{
    for (const QMetaObject::Connection &connection : connections)
        QObject::disconnect(connection);
    for (const QMetaObject::Connection &connection : selectionModelConnections)
        QObject::disconnect(connection);
}

void QListWidgetPrivate::emitItemPressed(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemPressed(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitItemClicked(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemClicked(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitItemDoubleClicked(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemDoubleClicked(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitItemActivated(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemActivated(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitItemEntered(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemEntered(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitItemChanged(const QModelIndex &index)
{
    Q_Q(QListWidget);
    emit q->itemChanged(listModel()->at(index.row()));
}

void QListWidgetPrivate::emitCurrentItemChanged(const QModelIndex &current,
                                                const QModelIndex &previous)
{
    Q_Q(QListWidget);
    QPersistentModelIndex persistentCurrent = current;
    QListWidgetItem *currentItem = listModel()->at(persistentCurrent.row());
    emit q->currentItemChanged(currentItem, listModel()->at(previous.row()));

    //persistentCurrent is invalid if something changed the model in response
    //to the currentItemChanged signal emission and the item was removed
    if (!persistentCurrent.isValid()) {
        currentItem = nullptr;
    }

    emit q->currentTextChanged(currentItem ? currentItem->text() : QString());
    emit q->currentRowChanged(persistentCurrent.row());
}

void QListWidgetPrivate::sort()
{
    if (sortingEnabled)
        model->sort(0, sortOrder);
}

void QListWidgetPrivate::dataChanged(const QModelIndex &topLeft,
                                     const QModelIndex &bottomRight)
{
    if (sortingEnabled && topLeft.isValid() && bottomRight.isValid())
        listModel()->ensureSorted(topLeft.column(), sortOrder,
                              topLeft.row(), bottomRight.row());
}

/*!
    \class QListWidget
    \brief The QListWidget class provides an item-based list widget.

    \ingroup model-view
    \inmodule QtWidgets

    \image windows-listview.png

    QListWidget is a convenience class that provides a list view similar to the
    one supplied by QListView, but with a classic item-based interface for
    adding and removing items. QListWidget uses an internal model to manage
    each QListWidgetItem in the list.

    For a more flexible list view widget, use the QListView class with a
    standard model.

    List widgets are constructed in the same way as other widgets:

    \snippet qlistwidget-using/mainwindow.cpp 0

    The selectionMode() of a list widget determines how many of the items in
    the list can be selected at the same time, and whether complex selections
    of items can be created. This can be set with the setSelectionMode()
    function.

    There are two ways to add items to the list: they can be constructed with
    the list widget as their parent widget, or they can be constructed with no
    parent widget and added to the list later. If a list widget already exists
    when the items are constructed, the first method is easier to use:

    \snippet qlistwidget-using/mainwindow.cpp 1

    If you need to insert a new item into the list at a particular position,
    then it should be constructed without a parent widget. The insertItem()
    function should then be used to place it within the list. The list widget
    will take ownership of the item.

    \snippet qlistwidget-using/mainwindow.cpp 6
    \snippet qlistwidget-using/mainwindow.cpp 7

    For multiple items, insertItems() can be used instead. The number of items
    in the list is found with the count() function. To remove items from the
    list, use takeItem().

    The current item in the list can be found with currentItem(), and changed
    with setCurrentItem(). The user can also change the current item by
    navigating with the keyboard or clicking on a different item. When the
    current item changes, the currentItemChanged() signal is emitted with the
    new current item and the item that was previously current.

    \sa QListWidgetItem, QListView, QTreeView, {Model/View Programming},
        {Tab Dialog Example}
*/

/*!
    \fn void QListWidget::addItem(QListWidgetItem *item)

    Inserts the \a item at the end of the list widget.

    \warning A QListWidgetItem can only be added to a QListWidget once. Adding
    the same QListWidgetItem multiple times to a QListWidget will result in
    undefined behavior.

    \sa insertItem()
*/

/*!
    \fn void QListWidget::addItem(const QString &label)

    Inserts an item with the text \a label at the end of the list widget.
*/

/*!
    \fn void QListWidget::addItems(const QStringList &labels)

    Inserts items with the text \a labels at the end of the list widget.

    \sa insertItems()
*/

/*!
    \fn void QListWidget::itemPressed(QListWidgetItem *item)

    This signal is emitted with the specified \a item when a mouse button is
    pressed on an item in the widget.

    \sa itemClicked(), itemDoubleClicked()
*/

/*!
    \fn void QListWidget::itemClicked(QListWidgetItem *item)

    This signal is emitted with the specified \a item when a mouse button is
    clicked on an item in the widget.

    \sa itemPressed(), itemDoubleClicked()
*/

/*!
    \fn void QListWidget::itemDoubleClicked(QListWidgetItem *item)

    This signal is emitted with the specified \a item when a mouse button is
    double clicked on an item in the widget.

    \sa itemClicked(), itemPressed()
*/

/*!
    \fn void QListWidget::itemActivated(QListWidgetItem *item)

    This signal is emitted when the \a item is activated. The \a item is
    activated when the user clicks or double clicks on it, depending on the
    system configuration. It is also activated when the user presses the
    activation key (on Windows and X11 this is the \uicontrol Return key, on Mac OS
    X it is \uicontrol{Command+O}).
*/

/*!
    \fn void QListWidget::itemEntered(QListWidgetItem *item)

    This signal is emitted when the mouse cursor enters an item. The \a item is
    the item entered. This signal is only emitted when mouseTracking is turned
    on, or when a mouse button is pressed while moving into an item.

    \sa QWidget::setMouseTracking()
*/

/*!
    \fn void QListWidget::itemChanged(QListWidgetItem *item)

    This signal is emitted whenever the data of \a item has changed.
*/

/*!
    \fn void QListWidget::currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)

    This signal is emitted whenever the current item changes.

    \a previous is the item that previously had the focus; \a current is the
    new current item.
*/

/*!
    \fn void QListWidget::currentTextChanged(const QString &currentText)

    This signal is emitted whenever the current item changes.

    \a currentText is the text data in the current item. If there is no current
    item, the \a currentText is invalid.
*/

/*!
    \fn void QListWidget::currentRowChanged(int currentRow)

    This signal is emitted whenever the current item changes.

    \a currentRow is the row of the current item. If there is no current item,
    the \a currentRow is -1.
*/

/*!
    \fn void QListWidget::itemSelectionChanged()

    This signal is emitted whenever the selection changes.

    \sa selectedItems(), QListWidgetItem::isSelected(), currentItemChanged()
*/

/*!
    \fn void QListWidget::removeItemWidget(QListWidgetItem *item)

    Removes the widget set on the given \a item.

    To remove an item (row) from the list entirely, either delete the item or
    use takeItem().

    \sa itemWidget(), setItemWidget()
*/

/*!
    Constructs an empty QListWidget with the given \a parent.
*/

QListWidget::QListWidget(QWidget *parent)
    : QListView(*new QListWidgetPrivate(), parent)
{
    Q_D(QListWidget);
    d->setup();
}

/*!
    Destroys the list widget and all its items.
*/

QListWidget::~QListWidget()
{
    Q_D(QListWidget);
    d->clearConnections();
}

/*!
    \reimp
*/

void QListWidget::setSelectionModel(QItemSelectionModel *selectionModel)
{
    Q_D(QListWidget);

    for (const QMetaObject::Connection &connection : d->selectionModelConnections)
        disconnect(connection);

    QListView::setSelectionModel(selectionModel);

    if (d->selectionModel) {
        d->selectionModelConnections = {
            QObjectPrivate::connect(d->selectionModel, &QItemSelectionModel::currentChanged,
                                    d, &QListWidgetPrivate::emitCurrentItemChanged),
            QObject::connect(d->selectionModel, &QItemSelectionModel::selectionChanged,
                             this, &QListWidget::itemSelectionChanged)
        };
    }
}

/*!
    Returns the item that occupies the given \a row in the list if one has been
    set; otherwise returns \nullptr.

    \sa row()
*/

QListWidgetItem *QListWidget::item(int row) const
{
    Q_D(const QListWidget);
    if (row < 0 || row >= d->model->rowCount())
        return nullptr;
    return d->listModel()->at(row);
}

/*!
    Returns the row containing the given \a item.

    \sa item()
*/

int QListWidget::row(const QListWidgetItem *item) const
{
    Q_D(const QListWidget);
    return d->listModel()->index(const_cast<QListWidgetItem*>(item)).row();
}


/*!
    Inserts the \a item at the position in the list given by \a row.

    \sa addItem()
*/

void QListWidget::insertItem(int row, QListWidgetItem *item)
{
    Q_D(QListWidget);
    if (item && !item->view)
        d->listModel()->insert(row, item);
}

/*!
    Inserts an item with the text \a label in the list widget at the position
    given by \a row.

    \sa addItem()
*/

void QListWidget::insertItem(int row, const QString &label)
{
    Q_D(QListWidget);
    d->listModel()->insert(row, new QListWidgetItem(label));
}

/*!
    Inserts items from the list of \a labels into the list, starting at the
    given \a row.

    \sa insertItem(), addItem()
*/

void QListWidget::insertItems(int row, const QStringList &labels)
{
    Q_D(QListWidget);
    d->listModel()->insert(row, labels);
}

/*!
    Removes and returns the item from the given \a row in the list widget;
    otherwise returns \nullptr.

    Items removed from a list widget will not be managed by Qt, and will need
    to be deleted manually.

    \sa insertItem(), addItem()
*/

QListWidgetItem *QListWidget::takeItem(int row)
{
    Q_D(QListWidget);
    if (row < 0 || row >= d->model->rowCount())
        return nullptr;
    return d->listModel()->take(row);
}

/*!
    \property QListWidget::count
    \brief the number of items in the list including any hidden items.
*/

int QListWidget::count() const
{
    Q_D(const QListWidget);
    return d->model->rowCount();
}

/*!
    Returns the current item.
*/
QListWidgetItem *QListWidget::currentItem() const
{
    Q_D(const QListWidget);
    return d->listModel()->at(currentIndex().row());
}


/*!
    Sets the current item to \a item.

    Unless the selection mode is \l{QAbstractItemView::}{NoSelection},
    the item is also selected.
*/
void QListWidget::setCurrentItem(QListWidgetItem *item)
{
    setCurrentRow(row(item));
}

/*!
    Set the current item to \a item, using the given \a command.
*/
void QListWidget::setCurrentItem(QListWidgetItem *item, QItemSelectionModel::SelectionFlags command)
{
    setCurrentRow(row(item), command);
}

/*!
    \property QListWidget::currentRow
    \brief the row of the current item.

    Depending on the current selection mode, the row may also be selected.
*/

int QListWidget::currentRow() const
{
    return currentIndex().row();
}

void QListWidget::setCurrentRow(int row)
{
    Q_D(QListWidget);
    QModelIndex index = d->listModel()->index(row);
    if (d->selectionMode == SingleSelection)
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    else if (d->selectionMode == NoSelection)
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    else
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);
}

/*!
    Sets the current row to be the given \a row, using the given \a command,
*/
void QListWidget::setCurrentRow(int row, QItemSelectionModel::SelectionFlags command)
{
    Q_D(QListWidget);
    d->selectionModel->setCurrentIndex(d->listModel()->index(row), command);
}

/*!
    Returns a pointer to the item at the coordinates \a p. The coordinates
    are relative to the list widget's \l{QAbstractScrollArea::}{viewport()}.

*/
QListWidgetItem *QListWidget::itemAt(const QPoint &p) const
{
    Q_D(const QListWidget);
    return d->listModel()->at(indexAt(p).row());

}

/*!
    \fn QListWidgetItem *QListWidget::itemAt(int x, int y) const
    \overload

    Returns a pointer to the item at the coordinates (\a x, \a y).
    The coordinates are relative to the list widget's
    \l{QAbstractScrollArea::}{viewport()}.

*/


/*!
    Returns the rectangle on the viewport occupied by the item at \a item.
*/
QRect QListWidget::visualItemRect(const QListWidgetItem *item) const
{
    Q_D(const QListWidget);
    QModelIndex index = d->listModel()->index(const_cast<QListWidgetItem*>(item));
    return visualRect(index);
}

/*!
    Sorts all the items in the list widget according to the specified \a order.
*/
void QListWidget::sortItems(Qt::SortOrder order)
{
    Q_D(QListWidget);
    d->sortOrder = order;
    d->listModel()->sort(0, order);
}

/*!
    \property QListWidget::sortingEnabled
    \brief whether sorting is enabled

    If this property is \c true, sorting is enabled for the list; if the property
    is false, sorting is not enabled.

    The default value is false.
*/
void QListWidget::setSortingEnabled(bool enable)
{
    Q_D(QListWidget);
    d->sortingEnabled = enable;
}

bool QListWidget::isSortingEnabled() const
{
    Q_D(const QListWidget);
    return d->sortingEnabled;
}

/*!
    \internal
*/
Qt::SortOrder QListWidget::sortOrder() const
{
    Q_D(const QListWidget);
    return d->sortOrder;
}

/*!
    Starts editing the \a item if it is editable.
*/

void QListWidget::editItem(QListWidgetItem *item)
{
    Q_D(QListWidget);
    edit(d->listModel()->index(item));
}

/*!
    Opens an editor for the given \a item. The editor remains open after
    editing.

    \sa closePersistentEditor(), isPersistentEditorOpen()
*/
void QListWidget::openPersistentEditor(QListWidgetItem *item)
{
    Q_D(QListWidget);
    QModelIndex index = d->listModel()->index(item);
    QAbstractItemView::openPersistentEditor(index);
}

/*!
    Closes the persistent editor for the given \a item.

    \sa openPersistentEditor(), isPersistentEditorOpen()
*/
void QListWidget::closePersistentEditor(QListWidgetItem *item)
{
    Q_D(QListWidget);
    QModelIndex index = d->listModel()->index(item);
    QAbstractItemView::closePersistentEditor(index);
}

/*!
    \since 5.10

    Returns whether a persistent editor is open for item \a item.

    \sa openPersistentEditor(), closePersistentEditor()
*/
bool QListWidget::isPersistentEditorOpen(QListWidgetItem *item) const
{
    Q_D(const QListWidget);
    const QModelIndex index = d->listModel()->index(item);
    return QAbstractItemView::isPersistentEditorOpen(index);
}

/*!
    Returns the widget displayed in the given \a item.

    \sa setItemWidget(), removeItemWidget()
*/
QWidget *QListWidget::itemWidget(QListWidgetItem *item) const
{
    Q_D(const QListWidget);
    QModelIndex index = d->listModel()->index(item);
    return QAbstractItemView::indexWidget(index);
}

/*!
    Sets the \a widget to be displayed in the given \a item.

    This function should only be used to display static content in the place of
    a list widget item. If you want to display custom dynamic content or
    implement a custom editor widget, use QListView and subclass QStyledItemDelegate
    instead.

    \note The list takes ownership of the \a widget.

    \sa itemWidget(), removeItemWidget(), {Delegate Classes}
*/
void QListWidget::setItemWidget(QListWidgetItem *item, QWidget *widget)
{
    Q_D(QListWidget);
    QModelIndex index = d->listModel()->index(item);
    QAbstractItemView::setIndexWidget(index, widget);
}

/*!
    Returns a list of all selected items in the list widget.
*/

QList<QListWidgetItem*> QListWidget::selectedItems() const
{
    Q_D(const QListWidget);
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    QList<QListWidgetItem*> items;
    const int numIndexes = indexes.size();
    items.reserve(numIndexes);
    for (int i = 0; i < numIndexes; ++i)
        items.append(d->listModel()->at(indexes.at(i).row()));
    return items;
}

/*!
    Finds items with the text that matches the string \a text using the given
    \a flags.
*/

QList<QListWidgetItem*> QListWidget::findItems(const QString &text, Qt::MatchFlags flags) const
{
    Q_D(const QListWidget);
    QModelIndexList indexes = d->listModel()->match(model()->index(0, 0, QModelIndex()),
                                                Qt::DisplayRole, text, -1, flags);
    QList<QListWidgetItem*> items;
    const int indexesSize = indexes.size();
    items.reserve(indexesSize);
    for (int i = 0; i < indexesSize; ++i)
        items.append(d->listModel()->at(indexes.at(i).row()));
    return items;
}

/*!
    Scrolls the view if necessary to ensure that the \a item is visible.

    \a hint specifies where the \a item should be located after the operation.
*/

void QListWidget::scrollToItem(const QListWidgetItem *item, QAbstractItemView::ScrollHint hint)
{
    Q_D(QListWidget);
    QModelIndex index = d->listModel()->index(const_cast<QListWidgetItem*>(item));
    QListView::scrollTo(index, hint);
}

/*!
    Removes all items and selections in the view.

    \warning All items will be permanently deleted.
*/
void QListWidget::clear()
{
    Q_D(QListWidget);
    selectionModel()->clear();
    d->listModel()->clear();
}

/*!
    Returns a list of MIME types that can be used to describe a list of
    listwidget items.

    \sa mimeData()
*/
QStringList QListWidget::mimeTypes() const
{
    return d_func()->listModel()->QAbstractListModel::mimeTypes();
}

/*!
    Returns an object that contains a serialized description of the specified
    \a items. The format used to describe the items is obtained from the
    mimeTypes() function.

    If the list of items is empty, \nullptr is returned instead of a
    serialized empty list.
*/
QMimeData *QListWidget::mimeData(const QList<QListWidgetItem *> &items) const
{
    Q_D(const QListWidget);

    QModelIndexList &cachedIndexes = d->listModel()->cachedIndexes;

    // if non empty, it's called from the model's own mimeData
    if (cachedIndexes.isEmpty()) {
        cachedIndexes.reserve(items.size());
        for (QListWidgetItem *item : items)
            cachedIndexes << indexFromItem(item);

        QMimeData *result = d->listModel()->internalMimeData();

        cachedIndexes.clear();
        return result;
    }

    return d->listModel()->internalMimeData();
}

#if QT_CONFIG(draganddrop)
/*!
    Handles \a data supplied by an external drag and drop operation that ended
    with the given \a action in the given \a index. Returns \c true if \a data and
    \a action can be handled by the model; otherwise returns \c false.

    \sa supportedDropActions()
*/
bool QListWidget::dropMimeData(int index, const QMimeData *data, Qt::DropAction action)
{
    QModelIndex idx;
    int row = index;
    int column = 0;
    if (dropIndicatorPosition() == QAbstractItemView::OnItem) {
        // QAbstractListModel::dropMimeData will overwrite on the index if row == -1 and column == -1
        idx = model()->index(row, column);
        row = -1;
        column = -1;
    }
    return d_func()->listModel()->QAbstractListModel::dropMimeData(data, action , row, column, idx);
}

/*! \reimp */
void QListWidget::dropEvent(QDropEvent *event)
{
    QListView::dropEvent(event);
}

/*!
    Returns the drop actions supported by this view.

    \sa Qt::DropActions
*/
Qt::DropActions QListWidget::supportedDropActions() const
{
    Q_D(const QListWidget);
    return d->listModel()->QAbstractListModel::supportedDropActions() | Qt::MoveAction;
}
#endif // QT_CONFIG(draganddrop)

/*!
    Returns a list of pointers to the items contained in the \a data object. If
    the object was not created by a QListWidget in the same process, the list
    is empty.
*/
QList<QListWidgetItem*> QListWidget::items(const QMimeData *data) const
{
    const QListWidgetMimeData *lwd = qobject_cast<const QListWidgetMimeData*>(data);
    if (lwd)
        return lwd->items;
    return QList<QListWidgetItem*>();
}

/*!
    Returns the QModelIndex associated with the given \a item.

    \note In Qt versions prior to 5.10, this function took a non-\c{const} \a item.
*/

QModelIndex QListWidget::indexFromItem(const QListWidgetItem *item) const
{
    Q_D(const QListWidget);
    return d->listModel()->index(item);
}

/*!
    Returns a pointer to the QListWidgetItem associated with the given \a index.
*/

QListWidgetItem *QListWidget::itemFromIndex(const QModelIndex &index) const
{
    Q_D(const QListWidget);
    if (d->isIndexValid(index))
        return d->listModel()->at(index.row());
    return nullptr;
}

/*!
    \internal
*/
void QListWidget::setModel(QAbstractItemModel * /*model*/)
{
    Q_ASSERT(!"QListWidget::setModel() - Changing the model of the QListWidget is not allowed.");
}

/*!
    \reimp
*/
bool QListWidget::event(QEvent *e)
{
    return QListView::event(e);
}

QT_END_NAMESPACE

#include "moc_qlistwidget.cpp"
#include "moc_qlistwidget_p.cpp"
