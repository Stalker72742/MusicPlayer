//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QStringList>
#include <QThread>

/// @brief Watches whole folder trees for changes, with one ReadDirectoryChangesW per root.
///
/// Runs in its own thread; signals arrive in the owner's thread. A root that cannot be opened (a missing folder,
/// an unplugged drive) is retried every few seconds. The open handle follows a root that gets renamed without any
/// notification, so its path is checked at the same pace. Does nothing on other systems.
///
/// Folders report a modification whenever something inside them changes, so only modified audio files are passed on.
/// A notification without data or with ERROR_NOTIFY_ENUM_DIR means the buffer overflowed: see rescanNeeded().
///
/// The open handle of a root on a removable drive makes Windows refuse to eject the drive while watching.
class FolderWatcher : public QThread
{
    Q_OBJECT

public:
    explicit FolderWatcher(QObject* parent = nullptr);
    ~FolderWatcher() override;

    /// @brief Starts watching these folders instead of the previous ones. An empty list stops the thread.
    /// @param roots Folders, see FileScanner::CollapseRoots(). At most 63 are watched.
    void SetRoots(const QStringList& roots);

signals:
    /// @brief Something happened to these paths: a file or a folder appeared, went away, was renamed or a file changed.
    ///
    /// Only a look at the disk tells which: a path may be gone again, and a rename reports both of its names.
    void pathsChanged(const QStringList& paths);

    /// @brief Events were lost, or a root appeared or went away: only a full scan can tell what changed.
    void rescanNeeded();

protected:
    void run() override;

private:
    void Stop();

    QStringList roots;
    void* stopEvent = nullptr; ///< HANDLE.
};
