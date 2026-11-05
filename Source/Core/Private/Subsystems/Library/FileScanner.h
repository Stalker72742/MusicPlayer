//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <atomic>

/// @brief Fast listing of audio files.
///
/// On Windows whole directories are read at once with GetFileInformationByHandleEx: size, time and file id come
/// with the listing, so no file is opened. Directories are walked in parallel on SSDs and sequentially on HDDs.
/// Symlinks, junctions and hidden items are skipped.
namespace FileScanner
{
    /// @brief An audio file as found on disk.
    struct ScannedFile
    {
        QString path;         ///< Absolute path with forward slashes.
        qint64 size = 0;      ///< Size in bytes.
        qint64 modifiedMs = 0; ///< Modification time, ms since the epoch.
        quint64 fileId = 0;   ///< File id on its volume, survives renames and moves. 0 if unknown.
        quint32 volumeId = 0; ///< Serial number of the volume. 0 if unknown.
    };

    /// @brief Whether this file name has a supported audio extension. Case-insensitive.
    bool IsAudioFile(QStringView name);

    /// @brief Cleans the paths and drops duplicates and folders nested in other folders of the list.
    QStringList CollapseRoots(const QStringList& roots);

    /// @brief Lists the audio files under these folders.
    /// @param roots Folders, see CollapseRoots(). A folder that cannot be read simply gives no files.
    /// @param bStop Set from another thread to give up early; the result is then incomplete.
    QList<ScannedFile> ScanTrees(const QStringList& roots, const std::atomic_bool& bStop);

    /// @brief Looks at single paths: a file gives itself, a folder its whole tree, a missing path nothing.
    /// @param paths Absolute paths, without paths nested in each other.
    /// @param bStop See ScanTrees().
    QList<ScannedFile> ScanPaths(const QStringList& paths, const std::atomic_bool& bStop);
}
