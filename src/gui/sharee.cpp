/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "sharee.h"
#include "ocsshareejob.h"

namespace OCC {

Sharee::Sharee(const QString shareWith,
               const QString displayName,
               const Type type)
: _shareWith(shareWith),
  _displayName(displayName),
  _type(type)
{
}

QString Sharee::format() const
{
    QString formatted = _displayName;

    if (_type == Type::Group) {
        formatted += QLatin1String(" (group)");
    } else if (_type == Type::Federated) {
        formatted += QLatin1String(" (remote)");
    }

    return formatted;
}

QString Sharee::shareWith() const
{
    return _shareWith;
}

QString Sharee::displayName() const
{
    return _displayName;
}

Sharee::Type Sharee::type() const
{
    return _type;
}

ShareeModel::ShareeModel(const AccountPtr &account, const QString &type, QObject *parent)
    : QAbstractListModel(parent), _account(account), _type(type)
{ }

void ShareeModel::fetch(const QString &search, const ShareeSet &blacklist)
{
    _search = search;
    _shareeBlacklist = blacklist;
    OcsShareeJob *job = new OcsShareeJob(_account);
    connect(job, SIGNAL(shareeJobFinished(QVariantMap)), SLOT(shareesFetched(QVariantMap)));
    job->getSharees(_search, _type, 1, 50);
}

void ShareeModel::shareesFetched(const QVariantMap &reply)
{
    auto data = reply.value("ocs").toMap().value("data").toMap();

    QVector<QSharedPointer<Sharee>> newSharees;

    /*
     * Todo properly loop all of this
     */
    auto exact = data.value("exact").toMap();
    {
        auto users = exact.value("users").toList();
        foreach(auto user, users) {
            newSharees.append(parseSharee(user.toMap()));
        }
        auto groups = exact.value("groups").toList();
        foreach(auto group, groups) {
            newSharees.append(parseSharee(group.toMap()));
        }
        auto remotes = exact.value("remotes").toList();
        foreach(auto remote, remotes) {
            newSharees.append(parseSharee(remote.toMap()));
        }
    }

    {
        auto users = data.value("users").toList();
        foreach(auto user, users) {
            newSharees.append(parseSharee(user.toMap()));
        }
    }
    {
        auto groups = data.value("groups").toList();
        foreach(auto group, groups) {
            newSharees.append(parseSharee(group.toMap()));
        }
    }
    {
        auto remotes = data.value("remotes").toList();
        foreach(auto remote, remotes) {
            newSharees.append(parseSharee(remote.toMap()));
        }
    }

    // Filter sharees that we have already shared with
    QVector<QSharedPointer<Sharee>> filteredSharees;
    foreach(const auto &sharee, newSharees) {
        bool found = false;
        foreach(const auto &blacklistSharee, _shareeBlacklist) {
            if (sharee->type() == blacklistSharee->type() &&
                sharee->shareWith() == blacklistSharee->shareWith()) {
                found = true;
                break;
            }
        }

        if (found == false) {
            filteredSharees.append(sharee);
        }
    }

    setNewSharees(filteredSharees);
    shareesReady();
}

QSharedPointer<Sharee> ShareeModel::parseSharee(const QVariantMap &data)
{
    const QString displayName = data.value("label").toString();
    const QString shareWith = data.value("value").toMap().value("shareWith").toString();
    Sharee::Type type = (Sharee::Type)data.value("value").toMap().value("shareType").toInt();

    return QSharedPointer<Sharee>(new Sharee(shareWith, shareWith, type));
}

/* Set the new sharee

    Do that while preserving the model index so the selection stays
*/
void ShareeModel::setNewSharees(const QVector<QSharedPointer<Sharee>>& newSharees)
{
    layoutAboutToBeChanged();
    const auto persistent = persistentIndexList();
    QVector<QSharedPointer<Sharee>> oldPersistantSharee;
    oldPersistantSharee.reserve(persistent.size());

    std::transform(persistent.begin(), persistent.end(), std::back_inserter(oldPersistantSharee),
                   [](const QModelIndex &idx) { return idx.data(Qt::UserRole).value<QSharedPointer<Sharee>>(); });

    _sharees = newSharees;

    QModelIndexList newPersistant;
    newPersistant.reserve(persistent.size());
    foreach(const QSharedPointer<Sharee> &sharee, oldPersistantSharee) {
        auto it = std::find_if(_sharees.constBegin(), _sharees.constEnd(),
                               [&sharee](const QSharedPointer<Sharee> &s2) {
                                    return s2->format() == sharee->format() && s2->displayName() == sharee->format();
                               });
        if (it == _sharees.constEnd()) {
            newPersistant << QModelIndex();
        } else {
            newPersistant << index(it - _sharees.constBegin());
        }
    }

    changePersistentIndexList(persistent, newPersistant);
    layoutChanged();
}


int ShareeModel::rowCount(const QModelIndex &) const
{
    return _sharees.size();
}

QVariant ShareeModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > _sharees.size()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return _sharees.at(index.row())->format();
    } 
    if (role == Qt::UserRole) {
        return QVariant::fromValue(_sharees.at(index.row()));
    }
    
    return QVariant();
}

QSharedPointer<Sharee> ShareeModel::getSharee(int at) {
    if (at < 0 || at > _sharees.size()) {
        return QSharedPointer<Sharee>(NULL);
    }

    return _sharees.at(at);
}

}