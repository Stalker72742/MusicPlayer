//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include "SubsystemBase.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>

#include "Library/Track.h"

/// @brief One condition of a smart playlist, e.g. `{"artist", "contains", "daft"}`.
///
/// Fields, operators and values are strings so they store and edit as they are;
/// PlaylistSubsystem::GetRuleFields() lists what is understood.
struct PlaylistRule
{
    QString field; ///< A RuleField id.
    QString op;    ///< An operator id from PlaylistSubsystem::GetOperators().
    QString value; ///< Compared value, as typed.
};

/// @brief A static or smart playlist.
struct Playlist
{
    /// @brief How the tracks of the playlist are chosen.
    enum class Kind
    {
        Static, ///< Tracks added and removed by hand, in the user's order.
        Smart   ///< Tracks found by rules, minus the ones the user removed.
    };

    QString id;
    QString name;
    Kind kind = Kind::Static;
    QDateTime created;

    /// @brief Static: the tracks in the user's order.
    QList<TrackId> tracks;

    /// @brief Smart: the conditions.
    QList<PlaylistRule> rules;
    bool bMatchAll = true;                ///< Smart: all rules must match, otherwise any.
    QString sort = QStringLiteral("added"); ///< Smart: a sort order id from PlaylistSubsystem::GetSortOrders().
    int limit = 0;                        ///< Smart: maximum number of tracks; 0 for no limit.
    QList<TrackId> excluded;              ///< Smart: tracks the user removed.

    bool IsSmart() const { return kind == Kind::Smart; }
};

/// @brief Playlists, kept in `<AppLocalData>/playlists.json`.
///
/// They hold TrackIds, so a track keeps its playlists whatever happens to its audio source (e.g. an online
/// track that got a local copy). Adding an online search result to a playlist saves it to the library.
class PlaylistSubsystem : public Subsystem<PlaylistSubsystem>
{
    Q_OBJECT
    friend class Subsystem<PlaylistSubsystem>;

public:
    /// @brief A track property smart playlist rules can test.
    struct RuleField
    {
        QString id;
        QString title;

        /// @brief "text", "number", "bool", "source" or "date": decides the operators and the value.
        QString type;
    };
    /// @brief Every field rules understand.
    static const QList<RuleField>& GetRuleFields();

    /// @brief Operators for a field type, as {id, title} pairs.
    static QList<std::pair<QString, QString>> GetOperators(const QString& fieldType);
    /// @brief Sort orders of smart playlists, as {id, title} pairs.
    static QList<std::pair<QString, QString>> GetSortOrders();

    const QList<Playlist>& GetPlaylists() const { return playlists; }
    /// @brief The playlist with this id, or nullptr.
    const Playlist* Find(const QString& id) const;

    /// @brief Creates a static playlist.
    /// @return Id of the new playlist.
    QString CreateStatic(const QString& name, const QList<TrackId>& tracks = {});
    /// @brief Creates a smart playlist; see Playlist for the meaning of the arguments.
    /// @return Id of the new playlist.
    QString CreateSmart(const QString& name, const QList<PlaylistRule>& rules, bool bMatchAll, const QString& sort, int limit);
    void Rename(const QString& id, const QString& name);
    void Delete(const QString& id);

    /// @brief Adds tracks to a static playlist; tracks already there are skipped.
    void AddTracks(const QString& id, const QList<TrackId>& tracks);
    /// @brief Moves a track of a static playlist from one position to another.
    void MoveTrack(const QString& id, int from, int to);

    /// @brief Removes tracks: from a static playlist they go away; from a smart one they are excluded until restored.
    void RemoveTracks(const QString& id, const QList<TrackId>& tracks);
    /// @brief Brings back the tracks excluded from a smart playlist.
    void RestoreExcluded(const QString& id);

    /// @brief Replaces the definition of a smart playlist; see Playlist for the meaning of the arguments.
    void SetSmartDefinition(const QString& id, const QList<PlaylistRule>& rules, bool bMatchAll, const QString& sort, int limit);

    /// @brief The track left the library: drops it from every playlist.
    void ForgetTrack(TrackId track);

    /// @brief The tracks to show and play, in order.
    ///
    /// Smart playlists are evaluated against the library now. Tracks that left the library are skipped
    /// but not forgotten: a rescan may bring them back.
    QList<TrackId> Resolve(const QString& id) const;

    /// @brief What a smart definition would give, for previews while editing.
    static QList<TrackId> Evaluate(const QList<PlaylistRule>& rules, bool bMatchAll, const QString& sort, int limit,
        const QList<TrackId>& excluded = {});

signals:
    /// @brief Names, membership or definitions changed (not the library the smart ones depend on).
    void playlistsChanged();

private:
    PlaylistSubsystem();

    void Deinitialize() override;

    Playlist* FindMutable(const QString& id);
    void Changed();

    static bool Matches(const Track& track, const PlaylistRule& rule);

    QString GetStorePath() const;
    void Load();
    void Save();

    QList<Playlist> playlists;
    QTimer saveTimer;
};
