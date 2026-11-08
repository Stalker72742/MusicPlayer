#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "Library/Track.h"
#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;

/// @brief Library data for the UI, built from LibrarySubsystem. QML singleton.
class LibraryViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(TrackListModel* tracks READ GetTracks CONSTANT)
    Q_PROPERTY(TrackListModel* favorites READ GetFavorites CONSTANT)
    Q_PROPERTY(TrackListModel* recent READ GetRecent CONSTANT)
    Q_PROPERTY(TrackListModel* online READ GetOnline CONSTANT)
    Q_PROPERTY(int albumCount READ GetAlbumCount NOTIFY libraryChanged)
    Q_PROPERTY(bool scanning READ IsScanning NOTIFY scanStateChanged)
    Q_PROPERTY(int pendingMetadata READ GetPendingMetadata NOTIFY scanStateChanged)

public:
    /// @brief The instance shared by C++ and QML.
    static LibraryViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static LibraryViewModel* create(QQmlEngine*, QJSEngine*);

    /// @brief Library files ordered by path, then saved online tracks by date added.
    TrackListModel* GetTracks() { return &tracksModel; }
    /// @brief Liked tracks.
    TrackListModel* GetFavorites() { return &favoritesModel; }
    /// @brief Played tracks, most recent first.
    TrackListModel* GetRecent() { return &recentModel; }
    /// @brief Tracks saved from YouTube, newest first.
    TrackListModel* GetOnline() { return &onlineModel; }
    /// @brief Number of albums among local tracks.
    int GetAlbumCount() const { return albumCount; }
    bool IsScanning() const;
    int GetPendingMetadata() const;

    /// @brief Likes or unlikes a track; see LibrarySubsystem::SetLiked().
    Q_INVOKABLE void toggleLiked(int trackId);
    /// @brief Scans the music folders again.
    Q_INVOKABLE void rescan();

    /// @brief Online tracks: adds a search result to the library or takes a saved one out (and out of every playlist).
    Q_INVOKABLE void setInLibrary(int trackId, bool bValue);

    /// @brief The user's tags of a track as one comma-separated line.
    Q_INVOKABLE QString labelsText(int trackId) const;
    /// @brief Sets the user's tags of a track from a comma-separated line.
    Q_INVOKABLE void setLabelsText(int trackId, const QString& text);

    /// @brief Opens the web page of an online track or the folder of a local one.
    Q_INVOKABLE void openSource(int trackId);
    /// @brief Copies the page URL of an online track, or the file path of a local one, to the clipboard.
    Q_INVOKABLE void copyLink(int trackId);
    /// @brief Shows the file of a track (local, or the downloaded copy of an online one) in Explorer.
    Q_INVOKABLE void showInFolder(int trackId);
    /// @brief Copies the path of a track's file to the clipboard.
    Q_INVOKABLE void copyFilePath(int trackId);

    /// @brief The file name of a track without the folder and extension; empty without a file.
    Q_INVOKABLE QString fileBaseName(int trackId) const;
    /// @brief Renames the file of a track in its folder; the extension stays.
    ///
    /// The playing track is closed for the moment of renaming and goes on where it was.
    /// @return Empty on success, otherwise what went wrong, for the user.
    Q_INVOKABLE QString renameFile(int trackId, const QString& baseName);

    /// @brief Opens the properties window for these tracks.
    Q_INVOKABLE void openProperties(const QVariantList& trackIds);
    /// @brief What the properties window and the track menu show about a track.
    /// @return `id` (0 if there is no such track), `title`, `artist`, `album`, `genre`, `year`, `track`, `tags`,
    /// `fileName`, `path`, `hasFile`, `duration`, `artUrl`, `liked`, `online`, `inLibrary`, `downloaded`.
    /// `year` and `track` are text, empty when unknown.
    Q_INVOKABLE QVariantMap trackProperties(int trackId) const;
    /// @brief Sets the app's own values of a track's tags; see LibrarySubsystem::SetTagEdits().
    Q_INVOKABLE void setTrackTags(int trackId, const QVariantMap& tags);
    /// @brief Puts text on the clipboard.
    Q_INVOKABLE void copyText(const QString& text);

    /// @brief What removeTracks() would do with these tracks.
    /// @return `files` (local files to the Recycle Bin), `downloads` (downloaded copies deleted, the tracks
    /// stream again), `saved` (online tracks without a copy taken out of the library), `downloadable` (online
    /// tracks without a copy), `liked` (all of them liked), `total`.
    Q_INVOKABLE QVariantMap describeTracks(const QVariantList& trackIds) const;
    /// @brief Removes tracks the way that fits each: local files go to the Recycle Bin, downloaded copies are
    /// deleted, other online tracks leave the library. A file that cannot go to the Recycle Bin is not deleted.
    /// @return `removed`, `failed` and `error` (the first failure, for the user).
    Q_INVOKABLE QVariantMap removeTracks(const QVariantList& trackIds);
    /// @brief Likes the tracks, or unlikes them if all of them are liked already.
    Q_INVOKABLE void toggleLikedTracks(const QVariantList& trackIds);

    /// @brief A track as list rows show it.
    static TrackData ToTrackData(const Track& track);

signals:
    /// @brief The track lists were rebuilt.
    void libraryChanged();
    void scanStateChanged();
    /// @brief The properties window should open for these tracks.
    void propertiesRequested(const QVariantList& trackIds);

private:
    LibraryViewModel();

    void Rebuild();
    /// Sorting by path is the slow part of a rebuild, and likes or play counts do not change the order.
    void SortTracks();

    int albumCount = 0;
    QList<TrackId> sortedIds;
    quint64 sortedRevision = ~quint64{0};

    TrackListModel tracksModel;
    TrackListModel favoritesModel;
    TrackListModel recentModel;
    TrackListModel onlineModel;
};
