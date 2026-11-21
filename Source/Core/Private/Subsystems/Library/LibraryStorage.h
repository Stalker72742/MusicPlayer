//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QList>
#include <QString>

#include "Library/Track.h"

/// @brief Files of the track registry.
///
/// `library.bin`: the magic, the format version and then the tracks field by field (see LibraryStorage.cpp).
/// A new field means a new format version with its own branch in the reader, so older files stay readable.
namespace LibraryStorage
{
    /// @brief Result of reading the registry file.
    enum class LoadResult
    {
        Loaded,   ///< The file was read.
        NoFile,   ///< There is no file yet.
        Corrupted ///< The file is not readable; the tracks read so far are dropped.
    };

    /// @brief Reads the binary registry.
    LoadResult Load(const QString& path, QList<Track>& tracks);

    /// @brief Writes the binary registry atomically: tracks that are not to be kept should be left out by the caller.
    /// @return Whether the file was written.
    bool Save(const QString& path, const QList<const Track*>& tracks);

    /// @brief Reads the JSON cache of older versions (`library.json`). The file itself stays as it is.
    /// @return Whether a file of a known version was read.
    bool ImportJson(const QString& path, QList<Track>& tracks);
}
