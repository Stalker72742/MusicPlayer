#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;
struct Track;

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

    /// @brief A track as list rows show it.
    static TrackData ToTrackData(const Track& track);

signals:
    /// @brief The track lists were rebuilt.
    void libraryChanged();
    void scanStateChanged();

private:
    LibraryViewModel();

    void Rebuild();

    int albumCount = 0;

    TrackListModel tracksModel;
    TrackListModel favoritesModel;
    TrackListModel recentModel;
    TrackListModel onlineModel;
};
