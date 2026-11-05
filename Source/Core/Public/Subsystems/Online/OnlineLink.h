//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QString>

/// @brief A YouTube link reduced to what it points at.
///
/// Parse() drops everything else from the link (`list`, `index`, `pp`, `si`, `t`, `feature`...), so the same video
/// always gives the same URL, which is the identity of an online track. Anything that is not recognised
/// is not a link: its text never reaches yt-dlp.
struct OnlineLink
{
    /// @brief What the link points at.
    enum class Kind
    {
        None,     ///< Not a YouTube link, or a private list (Watch later, Liked videos, a mix).
        Video,    ///< One video.
        Playlist, ///< A public playlist.
        Channel   ///< The uploads of a channel.
    };

    Kind kind = Kind::None;
    QString url; ///< Canonical URL; empty for Kind::None.

    /// @brief The public playlist a video was opened from (`watch?v=...&list=...`), kept apart from the video.
    QString playlistUrl;

    /// @brief Whether the text was a recognised link.
    bool IsValid() const { return kind != Kind::None; }

    /// @brief Recognises a link in pasted or dropped text.
    /// @param text A URL with or without the scheme; of a text/uri-list, the first entry counts.
    static OnlineLink Parse(const QString& text);
};
