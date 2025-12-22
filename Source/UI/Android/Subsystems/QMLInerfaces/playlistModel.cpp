//
// Created by Stalker7274 on 07.11.2025.
//

#include "playlistModel.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include "FileManager/FileManager.h"

PlaylistModel::PlaylistModel(std::shared_ptr<FileManager> fs, QObject* parent)
    : QAbstractListModel(parent)
{
    refreshPlaylists();
}

int PlaylistModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return static_cast<int>(m_playlistPaths.size());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_playlistPaths.size()))
        return QVariant();

    const QString& path = m_playlistPaths[index.row()];

    switch (role) {
    case PathRole:
        return path;
    case NameRole:
        return QFileInfo(path).baseName();
    case TrackCountRole: {
        QJsonObject json = FileManager::ReadPlaylistJson(path);
        return json.size();
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PathRole] = "playlistPath";
    roles[NameRole] = "playlistName";
    roles[TrackCountRole] = "trackCount";
    return roles;
}

void PlaylistModel::refreshPlaylists() {
    beginResetModel();
    m_playlistPaths.clear();
    FileManager::GetAllPlaylists(m_playlistPaths);
    endResetModel();

    emit playlistsLoaded();
}

QVariantList PlaylistModel::getTracksByPlaylistName(const QString& name) const {
    return FileManager::GetPlaylistTracks(name);
}
