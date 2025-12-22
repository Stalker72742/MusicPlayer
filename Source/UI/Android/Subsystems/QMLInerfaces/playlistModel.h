//
// Created by Stalker7274 on 07.11.2025.
//
#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <QStringList>
#include <memory>

class FileManager;

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum PlaylistRoles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        TrackCountRole
    };

    explicit PlaylistModel(std::shared_ptr<FileManager> fs, QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Методы для работы с плейлистами (читают с диска)
    Q_INVOKABLE void refreshPlaylists();
    Q_INVOKABLE QVariantList getTracksByPlaylistName(const QString& name) const;

signals:
    void playlistsLoaded();
    void errorOccurred(const QString& error);

private:
    QStringList m_playlistPaths;
};
