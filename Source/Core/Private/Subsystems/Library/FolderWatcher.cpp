//
// Created by Stalker7274 on 05.11.2026.
//

#include "FolderWatcher.h"

#include <QDir>

#include <algorithm>

#include "FileScanner.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <list>
#include <vector>

namespace
{
    constexpr DWORD RetryIntervalMs = 5000;
    constexpr size_t BufferDwords = 16 * 1024;
    constexpr qsizetype MaxRoots = MAXIMUM_WAIT_OBJECTS - 1;
    constexpr DWORD NotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE
                                 | FILE_NOTIFY_CHANGE_LAST_WRITE;

    struct WatchedRoot
    {
        QString path;
        HANDLE dir = INVALID_HANDLE_VALUE;
        HANDLE event = nullptr;
        OVERLAPPED overlapped{};
        std::vector<DWORD> buffer = std::vector<DWORD>(BufferDwords);

        bool IsOpen() const { return dir != INVALID_HANDLE_VALUE; }

        QString ExtendedPath() const
        {
            const QString native = QDir::toNativeSeparators(path);
            return native.startsWith(QStringLiteral("\\\\")) ? QStringLiteral("\\\\?\\UNC\\") + native.mid(2)
                                                             : QStringLiteral("\\\\?\\") + native;
        }

        bool PathExists() const
        {
            const QString extended = ExtendedPath();
            return GetFileAttributesW(reinterpret_cast<const wchar_t*>(extended.utf16())) != INVALID_FILE_ATTRIBUTES;
        }

        bool Arm()
        {
            overlapped = {};
            overlapped.hEvent = event;
            ResetEvent(event);
            return ReadDirectoryChangesW(dir, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(DWORD)), TRUE, NotifyFilter,
                nullptr, &overlapped, nullptr);
        }

        bool Open()
        {
            const QString extended = ExtendedPath();
            dir = CreateFileW(reinterpret_cast<const wchar_t*>(extended.utf16()), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
            if (!IsOpen())
                return false;

            if (!Arm())
            {
                Close();
                return false;
            }
            return true;
        }

        void Close()
        {
            if (!IsOpen())
                return;

            CancelIoEx(dir, &overlapped);
            DWORD ignored = 0;
            GetOverlappedResult(dir, &overlapped, &ignored, TRUE);
            CloseHandle(dir);
            dir = INVALID_HANDLE_VALUE;
        }

        QStringList ReadPaths(DWORD bytes) const
        {
            QStringList paths;
            const char* cursor = reinterpret_cast<const char*>(buffer.data());
            const char* end = cursor + bytes;

            while (cursor < end)
            {
                const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
                const QString relative = QString::fromWCharArray(info->FileName, info->FileNameLength / sizeof(WCHAR))
                                             .replace(QLatin1Char('\\'), QLatin1Char('/'));

                if (info->Action != FILE_ACTION_MODIFIED || FileScanner::IsAudioFile(relative))
                    paths.append(path.endsWith(QLatin1Char('/')) ? path + relative : path + QLatin1Char('/') + relative);

                if (info->NextEntryOffset == 0)
                    break;
                cursor += info->NextEntryOffset;
            }
            return paths;
        }
    };
}
#endif

FolderWatcher::FolderWatcher(QObject* parent)
    : QThread(parent)
{
#ifdef Q_OS_WIN
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
#endif
}

FolderWatcher::~FolderWatcher()
{
    Stop();
#ifdef Q_OS_WIN
    CloseHandle(stopEvent);
#endif
}

void FolderWatcher::Stop()
{
#ifdef Q_OS_WIN
    SetEvent(stopEvent);
    wait();
    ResetEvent(stopEvent);
#endif
}

void FolderWatcher::SetRoots(const QStringList& newRoots)
{
#ifdef Q_OS_WIN
    Stop();
    roots = newRoots.first(std::min(newRoots.size(), MaxRoots));
    if (!roots.isEmpty())
        start(QThread::LowPriority);
#else
    Q_UNUSED(newRoots);
#endif
}

void FolderWatcher::run()
{
#ifdef Q_OS_WIN
    std::list<WatchedRoot> watched;
    for (const QString& root : roots)
    {
        WatchedRoot& entry = watched.emplace_back();
        entry.path = root;
        entry.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        entry.Open();
    }

    while (true)
    {
        std::vector<HANDLE> handles{stopEvent};
        std::vector<WatchedRoot*> open;
        for (WatchedRoot& entry : watched)
        {
            if (entry.IsOpen())
            {
                handles.push_back(entry.event);
                open.push_back(&entry);
            }
        }

        const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, RetryIntervalMs);

        if (result == WAIT_OBJECT_0 || result == WAIT_FAILED)
            break;

        if (result == WAIT_TIMEOUT)
        {
            bool bChanged = false;
            for (WatchedRoot& entry : watched)
            {
                if (!entry.IsOpen())
                {
                    bChanged |= entry.Open();
                }
                else if (!entry.PathExists())
                {
                    entry.Close();
                    bChanged = true;
                }
            }
            if (bChanged)
                emit rescanNeeded();
            continue;
        }

        WatchedRoot& entry = *open[result - WAIT_OBJECT_0 - 1];

        DWORD bytes = 0;
        const bool bRead = GetOverlappedResult(entry.dir, &entry.overlapped, &bytes, FALSE);
        const DWORD error = bRead ? ERROR_SUCCESS : GetLastError();

        if (bRead && bytes > 0)
        {
            const QStringList paths = entry.ReadPaths(bytes);
            if (!paths.isEmpty())
                emit pathsChanged(paths);
        }
        else if (bRead || error == ERROR_NOTIFY_ENUM_DIR)
        {
            emit rescanNeeded();
        }

        const bool bBroken = !bRead && error != ERROR_NOTIFY_ENUM_DIR;
        if (bBroken || !entry.Arm())
        {
            CloseHandle(entry.dir);
            entry.dir = INVALID_HANDLE_VALUE;
            emit rescanNeeded();
        }
    }

    for (WatchedRoot& entry : watched)
    {
        entry.Close();
        CloseHandle(entry.event);
    }
#endif
}
