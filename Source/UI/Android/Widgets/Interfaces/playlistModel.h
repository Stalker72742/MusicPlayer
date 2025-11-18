//
// Created by Stalker7274 on 07.11.2025.
//
#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <memory>
#include <vector>


class FileManager;

struct PlaylistData {
    QString path;
    QString name;
    QString coverArt;
    int trackCount;
    QJsonObject rawData;
};

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum PlaylistRoles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        CoverArtRole,
        TrackCountRole,
        RawDataRole
    };

    explicit PlaylistModel(std::shared_ptr<FileManager> fs, QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Методы для работы с плейлистами
    Q_INVOKABLE void refreshPlaylists();
    Q_INVOKABLE QJsonObject getPlaylistAt(int index) const;
    Q_INVOKABLE void loadPlaylist(const QString& path);

signals:
    void playlistsLoaded();
    void errorOccurred(const QString& error);

private:
    std::vector<PlaylistData> m_playlists;

    void parsePlaylistFiles(const QStringList& files);
};
