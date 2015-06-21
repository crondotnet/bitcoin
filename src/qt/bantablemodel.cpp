// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bantablemodel.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"

#include "sync.h"
#include "utiltime.h"

#include <QDebug>
#include <QList>
#include <QTimer>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>

// private implementation
class BanTablePriv
{
public:
    /** Local cache of peer information */
    QList<CCombinedBan> cachedBanlist;
    /** Column to sort nodes by */
    int sortColumn;
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;

    /** Pull a full list of banned nodes from CNode into our cache */
    void refreshBanlist()
    {
        std::map<CSubNet, int64_t> banMap;
        CNode::GetBanned(banMap);

        cachedBanlist.clear();
#if QT_VERSION >= 0x040700
        cachedBanlist.reserve(banMap.size());
#endif
        std::map<CSubNet, int64_t>::iterator iter;
        foreach (const PAIRTYPE(CSubNet, int64_t)& banentry, banMap)
        {
            CCombinedBan banEntry;
            banEntry.subnet = banentry.first;
            banEntry.bantil = banentry.second;
            cachedBanlist.append(banEntry);
        }
    }

    int size() const
    {
        return cachedBanlist.size();
    }

    CCombinedBan *index(int idx)
    {
        if(idx >= 0 && idx < cachedBanlist.size()) {
            return &cachedBanlist[idx];
        } else {
            return 0;
        }
    }
};

BanTableModel::BanTableModel(ClientModel *parent) :
    QAbstractTableModel(parent),
    clientModel(parent),
    timer(0)
{
    columns << tr("IP/Netmask") << tr("Banned Until");
    priv = new BanTablePriv();
    // default to unsorted
    priv->sortColumn = -1;

    // set up timer for auto refresh
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), SLOT(refresh()));
    timer->setInterval(MODEL_UPDATE_DELAY);

    // load initial data
    refresh();
}

void BanTableModel::startAutoRefresh()
{
    timer->start();
}

void BanTableModel::stopAutoRefresh()
{
    timer->stop();
}

int BanTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int BanTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();;
}

QVariant BanTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CCombinedBan *rec = static_cast<CCombinedBan*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        switch(index.column())
        {
        case Address:
            return QString::fromStdString(rec->subnet.ToString());
        case Bantime:
            QDateTime date = QDateTime::fromMSecsSinceEpoch(0);
            date = date.addSecs(rec->bantil);
            return date.toString(Qt::SystemLocaleLongDate);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == Bantime)
            return (int)(Qt::AlignRight | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant BanTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags BanTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex BanTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    CCombinedBan *data = priv->index(row);

    if (data)
    {
        return createIndex(row, column, data);
    }
    else
    {
        return QModelIndex();
    }
}

void BanTableModel::refresh()
{
    emit layoutAboutToBeChanged();
    priv->refreshBanlist();
    emit layoutChanged();
}

void BanTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}

bool BanTableModel::shouldShow()
{
    if (priv->size() > 0)
        return true;
    return false;
}