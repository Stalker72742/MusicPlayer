//
// Created by Stalker7274 on 04.10.2026.
//

#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

/// @brief A search result: what a search gives without opening the page.
struct OnlineEntry
{
    QString pageUrl;       ///< Page of the video; becomes StreamComponent::pageUrl.
    QString title;         ///< Video title.
    QString channel;       ///< Channel name.
    QString channelUrl;    ///< Channel page; may be empty.
    QString thumbnailUrl;  ///< Thumbnail image.
    qint64 durationMs = 0; ///< 0 if unknown (live streams, premieres).
};

/// @brief One line of a listing: a playlist, a channel or a results page, as it arrives.
struct OnlineListItem
{
    /// @brief What the line is.
    enum class Kind
    {
        Video,
        Playlist,
        Channel
    };

    Kind kind = Kind::Video;
    OnlineEntry entry;    ///< pageUrl is the page of the video, playlist or channel.
    qint64 followers = 0; ///< Subscribers of a channel in the results; 0 if unknown.

    QString listTitle;      ///< Title of the list this line belongs to.
    QString listChannel;    ///< Owner of the list.
    QString listChannelUrl; ///< Channel page of the owner; may be empty.
};

/// @brief Why an online backend failed; decides whether the next backend is tried.
struct OnlineError
{
    /// @brief Kind of failure.
    enum Kind
    {
        None,        ///< No error.
        Unavailable, ///< Not usable right now (not installed, not connected): try the next one, no penalty.
        Blocked,     ///< Bot check, 403/429, sign-in required: try the next one, rest this one for a while.
        NotFound,    ///< Removed, private: every backend would say the same.
        Failed       ///< Anything else, timeouts included: try the next one, rest this one for a while.
    };

    Kind kind = None;
    QString message; ///< Human-readable reason.

    /// @brief Whether there was no error.
    bool IsOk() const { return kind == None; }

    /// @brief Builds an error of the given kind.
    static OnlineError Make(Kind kind, const QString& message) { return {kind, message}; }
};
