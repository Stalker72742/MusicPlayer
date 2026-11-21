//
// Created by Stalker7274 on 08.11.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <array>
#include <optional>

#include "Catalog/CatalogSubsystem.h"

class QFileSystemWatcher;
class QNetworkAccessManager;
class QNetworkReply;

/// @brief Pictures of artists, albums and genres: plain files in one folder the user owns.
///
/// The folder (setting `library.artworkFolder`, `<AppLocalData>/artwork` by default) has `Artists`, `Albums`
/// and `Genres` subfolders; a picture is `<imageName>.<jpg|jpeg|png|webp|svg|gif|bmp>` where imageName comes
/// from CatalogEntry ("Artist", "Artist - Album", "Genre"). Whatever is there is used as is: dropping,
/// replacing or deleting files by hand works, the folder is watched. Nothing else is kept next to them.
///
/// A missing picture is looked up on Deezer (public API, no key) when `library.fetchArtwork` is on: artist
/// photos, album covers, genre pictures. The result is saved into the same folder as `.jpg`. A name Deezer
/// does not know is not asked again for 30 days (`<cache>/artwork-lookups.json`); requests go one at a time.
class ArtworkSubsystem : public Subsystem<ArtworkSubsystem>
{
    Q_OBJECT
    friend class Subsystem<ArtworkSubsystem>;

public:
    /// @brief The artwork folder from the settings.
    QString GetFolder() const;
    /// @brief The subfolder of a kind; created on demand.
    QString GetKindFolder(CatalogKind kind) const;

    /// @brief Path of the picture, empty if there is none.
    QString FindImage(CatalogKind kind, const QString& imageName) const;
    /// @brief URL of the picture for QML, changing whenever the file does; empty if there is none.
    QUrl GetImageUrl(CatalogKind kind, const QString& imageName) const;

    /// @brief Looks the picture up online if it is missing, allowed and not looked up recently.
    void Request(const CatalogEntry& entry);
    /// @brief Looks the picture up online now, replacing the current one if something is found.
    void FindOnline(const CatalogEntry& entry);
    /// @brief Whether a lookup for the picture is queued or running.
    bool IsFetching(CatalogKind kind, const QString& imageName) const;

    /// @brief Copies an image file in as the picture, replacing the current one.
    /// @return Empty on success, otherwise what went wrong.
    QString SetImage(CatalogKind kind, const QString& imageName, const QString& sourcePath);
    /// @brief Moves the picture to the Recycle Bin (deleted if there is none). Is not looked up again
    /// automatically for 30 days.
    void RemoveImage(CatalogKind kind, const QString& imageName);

    /// @brief Re-reads the folder setting; called when it changes.
    void ApplyFolderSetting();

signals:
    /// @brief Pictures appeared, changed or went away.
    void imagesChanged();
    /// @brief A lookup ended; found is false when nothing was found or the request failed.
    void lookupFinished(CatalogKind kind, const QString& imageName, bool bFound);

private:
    ArtworkSubsystem();

    void Deinitialize() override;

    struct Job
    {
        CatalogKind kind = CatalogKind::Artist;
        QString imageName;
        QString name;
        QString artist;
        bool bForced = false;
    };

    static QString LookupKey(CatalogKind kind, const QString& imageName);

    void Enqueue(const CatalogEntry& entry, bool bForced);
    void StartNext();
    void FinishJob(bool bFound, bool bNetworkError);
    void SearchDeezer();
    void DownloadImage(const QUrl& url);
    QNetworkReply* SendGet(const QUrl& url);

    void RescanFolder(CatalogKind kind);
    void RescanAll();
    void WatchFolders();

    void LoadLookups();
    void SaveLookups() const;

    std::array<QHash<QString, QString>, 3> files; ///< Case-folded imageName -> path, by CatalogKind.
    QFileSystemWatcher* watcher = nullptr;
    QTimer rescanTimer;

    QNetworkAccessManager* network = nullptr;
    QList<Job> queue;
    std::optional<Job> current;
    QNetworkReply* currentReply = nullptr;
    QTimer spacingTimer;

    QHash<QString, qint64> notFoundAt; ///< LookupKey -> ms since the epoch.
    QSet<QString> failedThisRun;       ///< Network errors: tried again on the next start.
    QHash<QString, QUrl> deezerGenres; ///< Normalized genre name -> picture; loaded once.
    bool bGenresLoaded = false;
    QTimer saveTimer;
};
