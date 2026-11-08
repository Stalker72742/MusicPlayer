#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include "Catalog/CatalogSubsystem.h"
#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;

/// @brief Artists, albums or genres of the library for a grid; filled by CatalogViewModel.
class CatalogListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by CatalogViewModel")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    /// @brief Model roles.
    enum Roles
    {
        KeyRole = Qt::UserRole + 1,
        KindRole,       ///< "artist", "album", "genre".
        NameRole,
        SubtitleRole,
        TrackCountRole,
        ImageUrlRole,   ///< Empty while there is no picture; asking for it may start an online lookup.
        InitialsRole,   ///< Placeholder text while there is no picture.
        TintRole        ///< Placeholder colour.
    };

    explicit CatalogListModel(CatalogKind kind, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Takes the current entries of the catalog.
    void Refresh();
    /// @brief Pictures changed.
    void RefreshImages();

signals:
    void countChanged();

private:
    CatalogKind kind;
    QList<CatalogEntry> entries;
};

/// @brief Library grouped by artist, album and genre, and the open group. QML singleton.
///
/// The grids read artists / albums / genres. The open group follows Navigation (page Collection,
/// collectionKind + collectionKey): its name, picture and tracks. Pictures are ArtworkSubsystem files;
/// choosing, removing or finding one online acts on the open group.
class CatalogViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CatalogListModel* artists READ GetArtists CONSTANT)
    Q_PROPERTY(CatalogListModel* albums READ GetAlbums CONSTANT)
    Q_PROPERTY(CatalogListModel* genres READ GetGenres CONSTANT)

    /// @brief The open group; empty strings when it is gone.
    Q_PROPERTY(QString currentKind READ GetCurrentKind NOTIFY currentChanged)
    Q_PROPERTY(QString currentName READ GetCurrentName NOTIFY currentChanged)
    Q_PROPERTY(QString currentSubtitle READ GetCurrentSubtitle NOTIFY currentChanged)
    Q_PROPERTY(QUrl currentImage READ GetCurrentImage NOTIFY currentChanged)
    Q_PROPERTY(QString currentInitials READ GetCurrentInitials NOTIFY currentChanged)
    Q_PROPERTY(QColor currentTint READ GetCurrentTint NOTIFY currentChanged)
    /// @brief An online lookup of the open group's picture is running.
    Q_PROPERTY(bool currentFetching READ IsCurrentFetching NOTIFY currentChanged)
    Q_PROPERTY(TrackListModel* currentTracks READ GetCurrentTracks CONSTANT)

public:
    /// @brief The instance shared by C++ and QML.
    static CatalogViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static CatalogViewModel* create(QQmlEngine*, QJSEngine*);

    CatalogListModel* GetArtists() { return &artistsModel; }
    CatalogListModel* GetAlbums() { return &albumsModel; }
    CatalogListModel* GetGenres() { return &genresModel; }

    QString GetCurrentKind() const;
    QString GetCurrentName() const;
    QString GetCurrentSubtitle() const;
    QUrl GetCurrentImage() const;
    QString GetCurrentInitials() const;
    QColor GetCurrentTint() const;
    bool IsCurrentFetching() const;
    TrackListModel* GetCurrentTracks() { return &currentTracksModel; }

    /// @brief Opens a group's page.
    Q_INVOKABLE void open(const QString& kind, const QString& key);
    /// @brief Opens the page of the track's (first) artist or its album; false if it has none.
    Q_INVOKABLE bool openOfTrack(const QString& kind, int trackId);
    /// @brief Whether the track has an artist / album / genre to open.
    Q_INVOKABLE bool hasOfTrack(const QString& kind, int trackId) const;

    /// @brief Names of a kind, for autocompletion.
    /// @brief The tracks of a group.
    Q_INVOKABLE QVariantList trackIdsOf(const QString& kind, const QString& key) const;
    /// @brief Plays a group from its first track.
    Q_INVOKABLE void playGroup(const QString& kind, const QString& key);
    /// @brief Whether a group has a picture.
    Q_INVOKABLE bool hasImageOf(const QString& kind, const QString& key) const;
    /// @brief Name of a group.
    Q_INVOKABLE QString nameOf(const QString& kind, const QString& key) const;
    /// @brief Copies an image in as the group's picture.
    /// @return Empty on success, otherwise what went wrong.
    Q_INVOKABLE QString setImageOf(const QString& kind, const QString& key, const QUrl& file);
    /// @brief Moves the group's picture to the Recycle Bin.
    Q_INVOKABLE void removeImageOf(const QString& kind, const QString& key);
    /// @brief Looks the group's picture up online, replacing the current one if something is found.
    Q_INVOKABLE void findImageOnlineOf(const QString& kind, const QString& key);
    /// @brief Shows the group's picture, or its folder if there is none, in Explorer.
    Q_INVOKABLE void showImageInFolderOf(const QString& kind, const QString& key);

    Q_INVOKABLE QStringList names(const QString& kind) const;

    /// @brief Uses an image file as the open group's picture. @return Empty, or what went wrong.
    Q_INVOKABLE QString setImage(const QUrl& file);
    /// @brief Removes the open group's picture.
    Q_INVOKABLE void removeImage();
    /// @brief Looks the open group's picture up online, replacing the current one if found.
    Q_INVOKABLE void findImageOnline();
    /// @brief Shows the open group's picture in Explorer, or its subfolder if there is none.
    Q_INVOKABLE void showImageInFolder();
    /// @brief Opens the artwork folder.
    Q_INVOKABLE void openArtworkFolder();

    /// @brief Placeholder text for a name: initials of up to two words.
    static QString Initials(const QString& name);
    /// @brief Placeholder colour for a name.
    static QColor Tint(const QString& name);
    /// @brief One line under a group's name.
    static QString Subtitle(const CatalogEntry& entry);

signals:
    void currentChanged();

private:
    CatalogViewModel();

    const CatalogEntry* FindCurrent() const;
    const CatalogEntry* FindEntry(const QString& kind, const QString& key) const;
    void RefreshCurrent();

    CatalogListModel artistsModel{CatalogKind::Artist};
    CatalogListModel albumsModel{CatalogKind::Album};
    CatalogListModel genresModel{CatalogKind::Genre};
    TrackListModel currentTracksModel;
};
