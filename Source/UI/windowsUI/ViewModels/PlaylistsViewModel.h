#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "PlaylistListModel.h"
#include "TrackListModel.h"

class QJSEngine;
class QQmlEngine;
struct PlaylistRule;

/// @brief Playlists for the UI, backed by PlaylistSubsystem. QML singleton.
///
/// Provides the list, the open one (Navigation::playlistId)
/// and what the smart playlist editor needs.
///
/// A smart definition travels as `{rules: [{field, op, value}], matchAll, sort, limit}`.
class PlaylistsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(PlaylistListModel* playlists READ GetPlaylists CONSTANT)

    Q_PROPERTY(QString currentId READ GetCurrentId NOTIFY currentChanged)
    Q_PROPERTY(QString currentName READ GetCurrentName NOTIFY currentChanged)
    Q_PROPERTY(bool currentSmart READ IsCurrentSmart NOTIFY currentChanged)
    Q_PROPERTY(QString currentSummary READ GetCurrentSummary NOTIFY currentChanged)
    Q_PROPERTY(int currentExcluded READ GetCurrentExcluded NOTIFY currentChanged)
    /// @brief Online tracks of the open playlist without a local copy.
    Q_PROPERTY(int currentDownloadable READ GetCurrentDownloadable NOTIFY currentChanged)
    Q_PROPERTY(TrackListModel* currentTracks READ GetCurrentTracks CONSTANT)

public:
    /// @brief The instance shared by C++ and QML.
    static PlaylistsViewModel& Get();
    /// @brief QML singleton factory; returns Get().
    static PlaylistsViewModel* create(QQmlEngine*, QJSEngine*);

    PlaylistListModel* GetPlaylists() { return &playlistsModel; }
    QString GetCurrentId() const { return currentId; }
    QString GetCurrentName() const { return currentName; }
    bool IsCurrentSmart() const { return bCurrentSmart; }
    QString GetCurrentSummary() const { return currentSummary; }
    int GetCurrentExcluded() const { return currentExcluded; }
    int GetCurrentDownloadable() const { return currentDownloadable; }
    TrackListModel* GetCurrentTracks() { return &currentTracks; }

    /// @brief Creates a static playlist.
    /// @return The new playlist's id.
    Q_INVOKABLE QString createPlaylist(const QString& name, const QVariantList& trackIds = {});
    /// @brief Creates a smart playlist from a definition.
    /// @return The new playlist's id.
    Q_INVOKABLE QString createSmartPlaylist(const QString& name, const QVariantMap& definition);

    /// @brief Renames a playlist.
    Q_INVOKABLE void rename(const QString& id, const QString& name);
    /// @brief Deletes a playlist.
    Q_INVOKABLE void remove(const QString& id);
    /// @brief Opens the playlist screen; see Navigation::openPlaylist().
    Q_INVOKABLE void open(const QString& id);

    /// @brief Adds a track to a static playlist.
    Q_INVOKABLE void addTrack(const QString& id, int trackId);
    /// @brief Removes a track; from a smart playlist it is hidden.
    Q_INVOKABLE void removeTrack(const QString& id, int trackId);
    /// @brief Moves a track within a static playlist.
    Q_INVOKABLE void moveTrack(const QString& id, int from, int to);
    /// @brief Brings back the tracks hidden from a smart playlist.
    Q_INVOKABLE void restoreExcluded(const QString& id);

    /// @brief For "Add to playlist": [{id, name, contains}] of the static playlists.
    Q_INVOKABLE QVariantList staticPlaylistsFor(int trackId) const;

    /// @brief Name of a playlist by id.
    Q_INVOKABLE QString nameOf(const QString& id) const;

    /// @brief The definition of a smart playlist, for the editor.
    Q_INVOKABLE QVariantMap definition(const QString& id) const;
    /// @brief Replaces the definition of a smart playlist.
    Q_INVOKABLE void setDefinition(const QString& id, const QVariantMap& definition);
    /// @brief How many tracks a definition would give now.
    Q_INVOKABLE int previewCount(const QVariantMap& definition) const;

    /// @brief Fields rules can test: [{id, title, type}].
    Q_INVOKABLE QVariantList ruleFields() const;
    /// @brief Operators of a field type: [{id, title}].
    Q_INVOKABLE QVariantList operators(const QString& fieldType) const;
    /// @brief Sort orders: [{id, title}].
    Q_INVOKABLE QVariantList sortOrders() const;
    /// @brief Fixed values of "bool" / "source" fields; empty for free input.
    Q_INVOKABLE QVariantList valueOptions(const QString& fieldType) const;
    /// @brief Type of a field by id.
    Q_INVOKABLE QString fieldType(const QString& field) const;

    /// @brief A readable summary of rules, e.g. "Artist contains “daft” and Liked is yes".
    static QString Summarize(const QList<PlaylistRule>& rules, bool bMatchAll);

signals:
    /// @brief The open playlist or its tracks changed.
    void currentChanged();

private:
    PlaylistsViewModel();

    void Rebuild();
    static QList<PlaylistRule> RulesFromVariant(const QVariantMap& definition);

    PlaylistListModel playlistsModel;
    TrackListModel currentTracks;

    QString currentId;
    QString currentName;
    bool bCurrentSmart = false;
    QString currentSummary;
    int currentExcluded = 0;
    int currentDownloadable = 0;
};
