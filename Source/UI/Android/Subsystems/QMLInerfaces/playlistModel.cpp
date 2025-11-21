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
    return static_cast<int>(m_playlists.size());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_playlists.size()))
        return QVariant();

    const auto& playlist = m_playlists[index.row()];

    switch (role) {
    case PathRole:
        return playlist.path;
    case NameRole:
        return playlist.name;
    case CoverArtRole:
        return playlist.coverArt;
    case TrackCountRole:
        return playlist.trackCount;
    case RawDataRole:
        return playlist.rawData;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PathRole] = "playlistPath";
    roles[NameRole] = "playlistName";
    roles[CoverArtRole] = "coverArt";
    roles[TrackCountRole] = "trackCount";
    roles[RawDataRole] = "rawData";
    return roles;
}

void PlaylistModel::refreshPlaylists() {

    QStringList jsonFiles;
    FileManager::GetAllPlaylists(jsonFiles);

    beginResetModel();
    m_playlists.clear();
    parsePlaylistFiles(jsonFiles);
    endResetModel();

    emit playlistsLoaded();
}

void PlaylistModel::parsePlaylistFiles(const QStringList& files) {
    for (const auto& filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Не могу открыть ебучий файл:" << filePath;
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull()) {
            qWarning() << "JSON хуйня в файле:" << filePath;
            continue;
        }

        PlaylistData data;
        data.path = filePath;
        data.rawData = doc.object();

        // Парсим нужные поля из JSON
        data.name = QFileInfo(filePath).baseName();
        data.coverArt = data.rawData.value("cover").toString();

        // Считаем треки если есть массив
        QJsonArray tracks = data.rawData.value("tracks").toArray();
        data.trackCount = tracks.size();

        m_playlists.push_back(std::move(data));
    }
}

QJsonObject PlaylistModel::getPlaylistAt(int index) const {
    if (index >= 0 && index < static_cast<int>(m_playlists.size())) {
        return m_playlists[index].rawData;
    }
    return QJsonObject();
}

void PlaylistModel::loadPlaylist(const QString& path) {
    // Тут можешь сделать загрузку конкретного плейлиста
    // и эмитнуть сигнал или что тебе нужно
}
