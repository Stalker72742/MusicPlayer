#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QList>
#include <QtQml/qqmlregistration.h>

/// @brief A playlist as a card shows it.
struct PlaylistData
{
    QString id;
    QString name;
    bool bSmart = false;
    int trackCount = 0;
    /// Placeholder colour while there is no cover.
    QColor artTint;

    /// Cover of the first track, if it has one.
    QString artUrl;
};

/// @brief The list of playlists for QML views; filled by PlaylistsViewModel.
class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by PlaylistsViewModel")

    Q_PROPERTY(int count READ GetCount NOTIFY countChanged)

public:
    /// @brief Model roles.
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SmartRole,
        TrackCountRole,
        ArtTintRole,
        ArtUrlRole
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Number of rows (the count property).
    int GetCount() const { return static_cast<int>(playlists.size()); }
    const QList<PlaylistData>& GetPlaylists() const { return playlists; }

    /// @brief Replaces the rows.
    ///
    /// Same playlists in the same order: updated in place, so views keep their state.
    void SetPlaylists(const QList<PlaylistData>& newPlaylists);

signals:
    void countChanged();

private:
    QList<PlaylistData> playlists;
};
