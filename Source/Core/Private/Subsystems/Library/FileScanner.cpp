//
// Created by Stalker7274 on 05.11.2026.
//

#include "FileScanner.h"

#include <QDir>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <winioctl.h>
#else
#include <QDirIterator>
#include <QFileInfo>
#endif

namespace
{
    constexpr std::array<QStringView, 9> AudioExtensions{u"mp3", u"flac", u"wav", u"ogg", u"opus", u"webm", u"m4a", u"aac", u"wma"};

    QString JoinPath(const QString& dir, QStringView name)
    {
        QString path;
        path.reserve(dir.size() + name.size() + 1);
        path += dir;
        if (!dir.endsWith(QLatin1Char('/')))
            path += QLatin1Char('/');
        path += name;
        return path;
    }

#ifdef Q_OS_WIN
    constexpr int SsdThreads = 4;
    constexpr size_t ListingBufferSize = 64 * 1024;
    constexpr qint64 EpochDifference100ns = 116444736000000000LL;

    qint64 ToEpochMs(qint64 fileTime)
    {
        return fileTime > EpochDifference100ns ? (fileTime - EpochDifference100ns) / 10000 : 0;
    }

    qint64 ToEpochMs(const FILETIME& time)
    {
        return ToEpochMs((static_cast<qint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime);
    }

    const wchar_t* ToWide(const QString& text)
    {
        return reinterpret_cast<const wchar_t*>(text.utf16());
    }

    QString ToExtendedPath(const QString& path)
    {
        const QString native = QDir::toNativeSeparators(path);
        if (native.startsWith(QStringLiteral("\\\\")))
            return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
        return QStringLiteral("\\\\?\\") + native;
    }

    bool IsLink(DWORD attributes, DWORD reparseTag)
    {
        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
            && (reparseTag == IO_REPARSE_TAG_SYMLINK || reparseTag == IO_REPARSE_TAG_MOUNT_POINT);
    }

    void AddEntry(const QString& dir, QStringView name, DWORD attributes, DWORD reparseTag, qint64 size, qint64 modifiedMs,
        quint64 fileId, quint32 volumeId, QList<FileScanner::ScannedFile>& files, QStringList& subdirs)
    {
        if (attributes & FILE_ATTRIBUTE_HIDDEN)
            return;

        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (name != u"." && name != u".." && !IsLink(attributes, reparseTag))
                subdirs.append(JoinPath(dir, name));
            return;
        }

        if (FileScanner::IsAudioFile(name))
            files.append({JoinPath(dir, name), size, modifiedMs, fileId, volumeId});
    }

    void ListWithFindFirstFile(const QString& dir, quint32 volumeId, QList<FileScanner::ScannedFile>& files, QStringList& subdirs)
    {
        const QString pattern = ToExtendedPath(dir) + (dir.endsWith(QLatin1Char('/')) ? QStringLiteral("*") : QStringLiteral("\\*"));

        WIN32_FIND_DATAW data;
        const HANDLE find = FindFirstFileExW(ToWide(pattern), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr,
            FIND_FIRST_EX_LARGE_FETCH);
        if (find == INVALID_HANDLE_VALUE)
            return;

        do
        {
            const qint64 size = (static_cast<qint64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            AddEntry(dir, QStringView(reinterpret_cast<const char16_t*>(data.cFileName)), data.dwFileAttributes, data.dwReserved0,
                size, ToEpochMs(data.ftLastWriteTime), 0, volumeId, files, subdirs);
        } while (FindNextFileW(find, &data));

        FindClose(find);
    }

    void ListDirectory(const QString& dir, quint32 volumeId, std::vector<char>& buffer, QList<FileScanner::ScannedFile>& files,
        QStringList& subdirs)
    {
        const HANDLE handle = CreateFileW(ToWide(ToExtendedPath(dir)), FILE_LIST_DIRECTORY | SYNCHRONIZE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return;

        bool bAnyListing = false;
        while (GetFileInformationByHandleEx(handle, FileIdBothDirectoryInfo, buffer.data(), static_cast<DWORD>(buffer.size())))
        {
            bAnyListing = true;

            const char* cursor = buffer.data();
            while (true)
            {
                const auto* info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(cursor);
                const QStringView name(reinterpret_cast<const char16_t*>(info->FileName), info->FileNameLength / sizeof(wchar_t));

                AddEntry(dir, name, info->FileAttributes, info->EaSize, info->EndOfFile.QuadPart,
                    ToEpochMs(info->LastWriteTime.QuadPart), static_cast<quint64>(info->FileId.QuadPart), volumeId, files, subdirs);

                if (info->NextEntryOffset == 0)
                    break;
                cursor += info->NextEntryOffset;
            }
        }

        const DWORD error = GetLastError();
        CloseHandle(handle);

        if (!bAnyListing && error != ERROR_NO_MORE_FILES && error != ERROR_ACCESS_DENIED)
            ListWithFindFirstFile(dir, volumeId, files, subdirs);
    }

    struct PathInfo
    {
        DWORD attributes = 0;
        DWORD reparseTag = 0;
        qint64 size = 0;
        qint64 modifiedMs = 0;
        quint64 fileId = 0;
        quint32 volumeId = 0;
    };

    bool GetPathInfo(const QString& path, PathInfo& out)
    {
        const HANDLE handle = CreateFileW(ToWide(ToExtendedPath(path)), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;

        BY_HANDLE_FILE_INFORMATION info;
        const bool bOk = GetFileInformationByHandle(handle, &info);
        if (bOk)
        {
            out.attributes = info.dwFileAttributes;
            out.size = (static_cast<qint64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
            out.modifiedMs = ToEpochMs(info.ftLastWriteTime);
            out.fileId = (static_cast<quint64>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
            out.volumeId = info.dwVolumeSerialNumber;

            FILE_ATTRIBUTE_TAG_INFO tag;
            if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)))
                out.reparseTag = tag.ReparseTag;
        }

        CloseHandle(handle);
        return bOk;
    }

    bool HasSeekPenalty(const QString& path)
    {
        if (path.size() < 2 || path[1] != QLatin1Char(':'))
            return false;

        const QString volume = QStringLiteral("\\\\.\\") + path.left(2);
        const HANDLE handle = CreateFileW(ToWide(volume), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;

        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceSeekPenaltyProperty;
        query.QueryType = PropertyStandardQuery;

        DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor{};
        DWORD returned = 0;
        const bool bOk = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor,
            sizeof(descriptor), &returned, nullptr);

        CloseHandle(handle);
        return bOk && descriptor.IncursSeekPenalty;
    }

    QList<FileScanner::ScannedFile> ScanTree(const QString& root, quint32 volumeId, const std::atomic_bool& bStop)
    {
        struct Walk
        {
            std::mutex mutex;
            std::condition_variable wake;
            QStringList pending;
            int busy = 0;
        } walk;
        walk.pending.append(root);

        const int threadCount = HasSeekPenalty(root) ? 1 : SsdThreads;
        std::vector<QList<FileScanner::ScannedFile>> results(threadCount);

        const auto work = [&](QList<FileScanner::ScannedFile>& files) {
            std::vector<char> buffer(ListingBufferSize);
            QStringList subdirs;

            std::unique_lock lock(walk.mutex);
            while (true)
            {
                walk.wake.wait(lock, [&] { return !walk.pending.isEmpty() || walk.busy == 0 || bStop; });
                if (walk.pending.isEmpty() || bStop)
                    break;

                const QString dir = walk.pending.takeLast();
                ++walk.busy;
                lock.unlock();

                subdirs.clear();
                ListDirectory(dir, volumeId, buffer, files, subdirs);

                lock.lock();
                --walk.busy;
                walk.pending.append(subdirs);
                walk.wake.notify_all();
            }
            walk.wake.notify_all();
        };

        std::vector<std::thread> threads;
        for (int i = 1; i < threadCount; ++i)
            threads.emplace_back(work, std::ref(results[i]));
        work(results[0]);
        for (std::thread& thread : threads)
            thread.join();

        for (int i = 1; i < threadCount; ++i)
            results[0].append(results[i]);
        return std::move(results[0]);
    }

    QString WithLongFileName(const QString& path)
    {
        const qsizetype nameStart = path.lastIndexOf(QLatin1Char('/')) + 1;
        if (path.indexOf(QLatin1Char('~'), nameStart) < 0)
            return path;

        std::vector<wchar_t> buffer(32768);
        const DWORD length = GetLongPathNameW(ToWide(ToExtendedPath(path)), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return path;

        const QString longPath = QString::fromWCharArray(buffer.data(), length);
        return path.left(nameStart) + longPath.sliced(longPath.lastIndexOf(QLatin1Char('\\')) + 1);
    }
#else
    QList<FileScanner::ScannedFile> ScanTree(const QString& root, quint32 volumeId, const std::atomic_bool& bStop)
    {
        QList<FileScanner::ScannedFile> files;
        QDirIterator it(root, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext() && !bStop)
        {
            const QFileInfo info = it.nextFileInfo();
            if (FileScanner::IsAudioFile(info.fileName()))
                files.append({info.filePath(), info.size(), info.lastModified().toMSecsSinceEpoch(), 0, volumeId});
        }
        return files;
    }
#endif
}

bool FileScanner::IsAudioFile(QStringView name)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return false;

    const QStringView extension = name.sliced(dot + 1);
    return std::any_of(AudioExtensions.cbegin(), AudioExtensions.cend(), [extension](QStringView known) {
        return extension.compare(known, Qt::CaseInsensitive) == 0;
    });
}

QStringList FileScanner::CollapseRoots(const QStringList& roots)
{
    QStringList cleaned;
    for (const QString& root : roots)
    {
        if (!root.isEmpty())
            cleaned.append(QDir::cleanPath(root));
    }

    std::sort(cleaned.begin(), cleaned.end(), [](const QString& a, const QString& b) { return a.size() < b.size(); });

    QStringList result;
    for (const QString& root : cleaned)
    {
        const bool bCovered = std::any_of(result.cbegin(), result.cend(), [&root](const QString& outer) {
            if (!root.startsWith(outer, Qt::CaseInsensitive))
                return false;
            return root.size() == outer.size() || outer.endsWith(QLatin1Char('/')) || root[outer.size()] == QLatin1Char('/');
        });
        if (!bCovered)
            result.append(root);
    }
    return result;
}

QList<FileScanner::ScannedFile> FileScanner::ScanTrees(const QStringList& roots, const std::atomic_bool& bStop)
{
    std::vector<QList<ScannedFile>> results(roots.size());
    std::vector<std::thread> threads;

    for (qsizetype i = 0; i < roots.size(); ++i)
    {
        threads.emplace_back([&results, &roots, &bStop, i] {
#ifdef Q_OS_WIN
            PathInfo info;
            if (GetPathInfo(roots[i], info) && (info.attributes & FILE_ATTRIBUTE_DIRECTORY))
                results[i] = ScanTree(roots[i], info.volumeId, bStop);
#else
            results[i] = ScanTree(roots[i], 0, bStop);
#endif
        });
    }
    for (std::thread& thread : threads)
        thread.join();

    QList<ScannedFile> files;
    for (QList<ScannedFile>& result : results)
        files.append(std::move(result));
    return files;
}

QList<FileScanner::ScannedFile> FileScanner::ScanPaths(const QStringList& paths, const std::atomic_bool& bStop)
{
    QList<ScannedFile> files;

#ifdef Q_OS_WIN
    for (const QString& path : paths)
    {
        if (bStop)
            break;

        PathInfo info;
        if (!GetPathInfo(path, info) || (info.attributes & FILE_ATTRIBUTE_HIDDEN) || IsLink(info.attributes, info.reparseTag))
            continue;

        if (info.attributes & FILE_ATTRIBUTE_DIRECTORY)
            files.append(ScanTree(path, info.volumeId, bStop));
        else if (IsAudioFile(path))
            files.append({WithLongFileName(path), info.size, info.modifiedMs, info.fileId, info.volumeId});
    }
#else
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        if (info.isDir())
            files.append(ScanTree(path, 0, bStop));
        else if (info.exists() && IsAudioFile(info.fileName()))
            files.append({path, info.size(), info.lastModified().toMSecsSinceEpoch(), 0, 0});
    }
#endif

    return files;
}
