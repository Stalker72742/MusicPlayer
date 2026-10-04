#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QDate>
#include <QList>
#include <QtQml/qqmlregistration.h>

/// @brief A track as a list row shows it.
struct TrackData
{
    /// TrackId value.
    int id = 0;
    QString title;
    QString artist;
    QString album;
    QDate dateAdded;
    int durationSec = 0;
    bool bLiked = false;
    /// Placeholder colour while there is no cover.
    QColor artTint;
    /// Cover image, may be empty.
    QString artUrl;

    /// Found online; a transient one (a search result) is not in the library yet.
    bool bOnline = false;
    bool bInLibrary = true;

    /// An online track with a local copy.
    bool bDownloaded = false;
};

/// @brief A list of tracks for QML views; filled by the view models.
class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by LibraryViewModel")

    Q_PROPERTY(int count READ GetCount NOTIFY countChanged)

public:
    /// @brief Model roles.
    enum Roles
    {
        TrackIdRole = Qt::UserRole + 1,
        NumberRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DateAddedRole,
        DurationRole,
        DurationSecRole,
        LikedRole,
        ArtTintRole,
        ArtUrlRole,
        OnlineRole,
        InLibraryRole,
        DownloadedRole
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Number of rows (the count property).
    int GetCount() const { return static_cast<int>(tracks.size()); }
    const QList<TrackData>& GetTracks() const { return tracks; }

    /// @brief Replaces the rows.
    ///
    /// Updates in place when the rows are the same and inserts when rows were only appended,
    /// so views keep their scroll position; anything else resets the model.
    void SetTracks(const QList<TrackData>& newTracks);

    /// @brief Row of a track, -1 if the track is not in the list.
    int RowOf(int trackId) const;

    /// @brief "3:07", or "1:51:26" for an hour and more.
    static QString FormatDuration(int seconds);

signals:
    void countChanged();

private:
    QList<TrackData> tracks;
};
