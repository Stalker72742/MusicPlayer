//
// Created by Stalker7274 on 05.11.2026.
//

#include "MemoryDump.h"

#include <private/qv4engine_p.h>
#include <private/qv4mm_p.h>

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSet>
#include <QSize>
#include <QStandardPaths>
#include <QUrl>
#include <QVariant>

#include "AppInstance.h"
#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "OnlinePageViewModel.h"
#include "Player/PlayerSubsystem.h"
#include "Playlists/PlaylistSubsystem.h"
#include "PlaylistsViewModel.h"
#include "SearchViewModel.h"

#include <qt_windows.h>

#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <limits>

namespace
{
    constexpr int MaxReports = 50;
    constexpr int MaxHeaps = 64;
    constexpr int TopModules = 25;
    constexpr int TopMappedFiles = 15;
    constexpr int TopQmlTypes = 40;
    constexpr int TopImages = 10;
    constexpr qsizetype StringHeader = 24;

    QHash<QString, qint64> previousValues;

    struct Totals
    {
        qint64 committed = 0;
        qint64 resident = 0;
        qint64 residentPrivate = 0;

        void Add(const Totals& other)
        {
            committed += other.committed;
            resident += other.resident;
            residentPrivate += other.residentPrivate;
        }
    };

    struct AddressSpace
    {
        Totals image;
        Totals mapped;
        Totals privateMemory;
        qint64 reserved = 0;
        QHash<quintptr, Totals> images;
        QHash<quintptr, Totals> mappedFiles;
    };

    struct SizeClass
    {
        const char* name;
        qint64 limit;
    };

    constexpr std::array<SizeClass, 6> SizeClasses{{
        {"up to 256 B", 256},
        {"up to 4 KB", 4 * 1024},
        {"up to 64 KB", 64 * 1024},
        {"up to 1 MB", 1024 * 1024},
        {"up to 16 MB", 16 * 1024 * 1024},
        {"over 16 MB", std::numeric_limits<qint64>::max()},
    }};

    struct HeapTotals
    {
        quintptr handle = 0;
        qint64 committed = 0;
        qint64 uncommitted = 0;
        qint64 live = 0;
        qint64 liveBlocks = 0;
        qint64 overhead = 0;
        qint64 freeBytes = 0;
        qint64 largestFree = 0;
        qint64 outsideRegions = 0;
        bool bWalked = false;
    };

    struct HeapReport
    {
        std::array<HeapTotals, MaxHeaps> heaps{};
        std::array<qint64, SizeClasses.size()> classBytes{};
        std::array<qint64, SizeClasses.size()> classBlocks{};
        int count = 0;
    };

    struct ChildProcess
    {
        QString name;
        DWORD pid = 0;
        qint64 workingSet = 0;
        qint64 privateBytes = 0;
    };

    class Report
    {
    public:
        void Title(const QString& title)
        {
            text += QStringLiteral("\n== %1 ==\n").arg(title);
        }

        void Line(const QString& line = {})
        {
            text += line + QLatin1Char('\n');
        }

        void Bytes(const QString& name, qint64 value)
        {
            Row(name, QString::number(ToMb(value), 'f', 1) + QStringLiteral(" MB"), DiffBytes(name, value));
        }

        void Count(const QString& name, qint64 value)
        {
            Row(name, QString::number(value), DiffCount(name, value));
        }

        void Header3(const QString& name, const QString& a, const QString& b, const QString& c)
        {
            text += QStringLiteral("%1 %2 %3 %4\n").arg(name, -46).arg(a, 11).arg(b, 11).arg(c, 11);
        }

        void Bytes3(const QString& name, qint64 a, qint64 b, qint64 c)
        {
            const auto cell = [](qint64 value) { return QString::number(ToMb(value), 'f', 1); };
            text += QStringLiteral("%1 %2 %3 %4%5\n")
                        .arg(name, -46)
                        .arg(cell(a), 11)
                        .arg(cell(b), 11)
                        .arg(cell(c), 11)
                        .arg(DiffBytes(name, c));
        }

        void Finish()
        {
            previousValues = currentValues;
        }

        QString text;

    private:
        static double ToMb(qint64 value)
        {
            return static_cast<double>(value) / (1024.0 * 1024.0);
        }

        void Row(const QString& name, const QString& value, const QString& diff)
        {
            text += QStringLiteral("%1 %2%3\n").arg(name, -46).arg(value, 14).arg(diff);
        }

        QString DiffBytes(const QString& name, qint64 value)
        {
            currentValues.insert(name, value);
            const auto previous = previousValues.constFind(name);
            if (previous == previousValues.constEnd())
                return {};
            const double diff = ToMb(value - *previous);
            if (qAbs(diff) < 0.05)
                return {};
            return QStringLiteral("  (%1%2)").arg(diff > 0 ? QStringLiteral("+") : QString()).arg(diff, 0, 'f', 1);
        }

        QString DiffCount(const QString& name, qint64 value)
        {
            currentValues.insert(name, value);
            const auto previous = previousValues.constFind(name);
            if (previous == previousValues.constEnd() || *previous == value)
                return {};
            return QStringLiteral("  (%1%2)").arg(value > *previous ? QStringLiteral("+") : QString()).arg(value - *previous);
        }

        QHash<QString, qint64> currentValues;
    };

    AddressSpace ReadAddressSpace()
    {
        AddressSpace space;

        SYSTEM_INFO system;
        GetSystemInfo(&system);
        const qint64 pageSize = system.dwPageSize;

        QList<MEMORY_BASIC_INFORMATION> regions;
        regions.reserve(16384);

        MEMORY_BASIC_INFORMATION info;
        const char* address = nullptr;
        while (VirtualQuery(address, &info, sizeof(info)) == sizeof(info))
        {
            if (info.State == MEM_COMMIT)
                regions.append(info);
            else if (info.State == MEM_RESERVE)
                space.reserved += static_cast<qint64>(info.RegionSize);
            address = static_cast<const char*>(info.BaseAddress) + info.RegionSize;
        }

        QList<PSAPI_WORKING_SET_EX_INFORMATION> pages(16384);
        const HANDLE process = GetCurrentProcess();

        for (const MEMORY_BASIC_INFORMATION& region : regions)
        {
            Totals totals;
            totals.committed = static_cast<qint64>(region.RegionSize);

            const qint64 pageCount = totals.committed / pageSize;
            for (qint64 done = 0; done < pageCount;)
            {
                const qint64 batch = qMin<qint64>(pageCount - done, pages.size());
                for (qint64 i = 0; i < batch; ++i)
                    pages[i].VirtualAddress = static_cast<char*>(region.BaseAddress) + (done + i) * pageSize;

                if (QueryWorkingSetEx(process, pages.data(), static_cast<DWORD>(batch * sizeof(PSAPI_WORKING_SET_EX_INFORMATION))))
                {
                    for (qint64 i = 0; i < batch; ++i)
                    {
                        const PSAPI_WORKING_SET_EX_BLOCK& page = pages[i].VirtualAttributes;
                        if (!page.Valid)
                            continue;
                        totals.resident += pageSize;
                        if (!page.Shared)
                            totals.residentPrivate += pageSize;
                    }
                }
                done += batch;
            }

            const quintptr base = reinterpret_cast<quintptr>(region.AllocationBase);
            if (region.Type == MEM_IMAGE)
            {
                space.image.Add(totals);
                space.images[base].Add(totals);
            }
            else if (region.Type == MEM_MAPPED)
            {
                space.mapped.Add(totals);
                space.mappedFiles[base].Add(totals);
            }
            else
            {
                space.privateMemory.Add(totals);
            }
        }
        return space;
    }

    /// Nothing here may allocate: the heaps are locked and being walked.
    void WalkHeap(HANDLE heap, HeapTotals& totals, HeapReport& report)
    {
        if (!HeapLock(heap))
            return;

        const char* regionFirst = nullptr;
        const char* regionLast = nullptr;

        PROCESS_HEAP_ENTRY entry;
        entry.lpData = nullptr;
        while (HeapWalk(heap, &entry))
        {
            const qint64 size = static_cast<qint64>(entry.cbData);
            if (entry.wFlags & PROCESS_HEAP_REGION)
            {
                totals.committed += static_cast<qint64>(entry.Region.dwCommittedSize);
                totals.uncommitted += static_cast<qint64>(entry.Region.dwUnCommittedSize);
                regionFirst = static_cast<const char*>(entry.Region.lpFirstBlock);
                regionLast = static_cast<const char*>(entry.Region.lpLastBlock);
            }
            else if (entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE)
            {
                continue;
            }
            else if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
            {
                totals.live += size;
                ++totals.liveBlocks;
                totals.overhead += entry.cbOverhead;

                const char* data = static_cast<const char*>(entry.lpData);
                if (data < regionFirst || data >= regionLast)
                    totals.outsideRegions += size + entry.cbOverhead;

                for (size_t i = 0; i < SizeClasses.size(); ++i)
                {
                    if (size <= SizeClasses[i].limit)
                    {
                        report.classBytes[i] += size;
                        ++report.classBlocks[i];
                        break;
                    }
                }
            }
            else
            {
                totals.freeBytes += size;
                totals.largestFree = qMax(totals.largestFree, size);
            }
        }
        totals.bWalked = true;
        HeapUnlock(heap);
    }

    void ReadHeaps(HeapReport& report)
    {
        std::array<HANDLE, MaxHeaps> handles{};
        const DWORD found = GetProcessHeaps(MaxHeaps, handles.data());
        report.count = static_cast<int>(qMin<DWORD>(found, MaxHeaps));
        for (int i = 0; i < report.count; ++i)
        {
            report.heaps[i].handle = reinterpret_cast<quintptr>(handles[i]);
            WalkHeap(handles[i], report.heaps[i], report);
        }
    }

    QString ModuleName(quintptr base)
    {
        wchar_t path[MAX_PATH * 2];
        const DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(base), path, MAX_PATH * 2);
        return length ? QFileInfo(QString::fromWCharArray(path, static_cast<int>(length))).fileName() : QStringLiteral("(unknown)");
    }

    QString MappedName(quintptr base)
    {
        wchar_t path[MAX_PATH * 2];
        const DWORD length = GetMappedFileNameW(GetCurrentProcess(), reinterpret_cast<void*>(base), path, MAX_PATH * 2);
        return length ? QString::fromWCharArray(path, static_cast<int>(length)).section(QLatin1Char('\\'), -1) : QStringLiteral("(no file)");
    }

    void WriteTop(Report& report, const QHash<quintptr, Totals>& items, int limit, const QString& prefix, QString (*name)(quintptr))
    {
        QList<quintptr> order = items.keys();
        std::sort(order.begin(), order.end(), [&](quintptr a, quintptr b) { return items[a].resident > items[b].resident; });

        report.Header3(QString(), QStringLiteral("committed"), QStringLiteral("in RAM"), QStringLiteral("private"));

        Totals rest;
        for (int i = 0; i < order.size(); ++i)
        {
            const Totals& totals = items[order[i]];
            if (i < limit)
                report.Bytes3(prefix + name(order[i]), totals.committed, totals.resident, totals.residentPrivate);
            else
                rest.Add(totals);
        }
        if (order.size() > limit)
            report.Bytes3(prefix + QStringLiteral("(%1 more)").arg(order.size() - limit), rest.committed, rest.resident, rest.residentPrivate);
    }

    void WriteProcess(Report& report, const AddressSpace& space, const HeapReport& heaps, qint64 jsHeap)
    {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));

        qint64 heapCommitted = 0;
        qint64 heapLive = 0;
        qint64 heapFree = 0;
        qint64 heapOverhead = 0;
        qint64 outsideRegions = 0;
        for (int i = 0; i < heaps.count; ++i)
        {
            heapCommitted += heaps.heaps[i].committed;
            heapLive += heaps.heaps[i].live;
            heapFree += heaps.heaps[i].freeBytes;
            heapOverhead += heaps.heaps[i].overhead;
            outsideRegions += heaps.heaps[i].outsideRegions;
        }

        const Totals total = [&] {
            Totals all;
            all.Add(space.image);
            all.Add(space.mapped);
            all.Add(space.privateMemory);
            return all;
        }();

        report.Title(QStringLiteral("Process"));
        report.Bytes(QStringLiteral("Private in RAM (Task Manager \"Memory\")"), total.residentPrivate);
        report.Bytes(QStringLiteral("Working set (with shared pages)"), static_cast<qint64>(counters.WorkingSetSize));
        report.Bytes(QStringLiteral("Peak working set"), static_cast<qint64>(counters.PeakWorkingSetSize));
        report.Bytes(QStringLiteral("Private committed (Commit size)"), static_cast<qint64>(counters.PrivateUsage));

        DWORD handles = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles);
        report.Count(QStringLiteral("Handles"), handles);
        report.Count(QStringLiteral("GDI objects"), GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
        report.Count(QStringLiteral("USER objects"), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS));

        report.Title(QStringLiteral("Address space, MB"));
        report.Header3(QString(), QStringLiteral("committed"), QStringLiteral("in RAM"), QStringLiteral("private"));
        report.Bytes3(QStringLiteral("Private memory"), space.privateMemory.committed, space.privateMemory.resident, space.privateMemory.residentPrivate);
        report.Bytes3(QStringLiteral("Modules (exe, DLL)"), space.image.committed, space.image.resident, space.image.residentPrivate);
        report.Bytes3(QStringLiteral("Mapped files"), space.mapped.committed, space.mapped.resident, space.mapped.residentPrivate);
        report.Bytes(QStringLiteral("Reserved, not committed (uses no RAM)"), space.reserved);

        report.Title(QStringLiteral("Who owns the private memory"));
        report.Bytes(QStringLiteral("Windows heaps (malloc, new), committed"), heapCommitted + outsideRegions);
        report.Bytes(QStringLiteral("  live blocks"), heapLive);
        report.Bytes(QStringLiteral("  free, kept by the heaps"), heapFree);
        report.Bytes(QStringLiteral("  block headers"), heapOverhead);
        report.Bytes(QStringLiteral("QML JS heap"), jsHeap);
        report.Bytes(QStringLiteral("Everything else"), qMax<qint64>(0, space.privateMemory.committed - heapCommitted - outsideRegions - jsHeap));
        report.Line(QStringLiteral("Live blocks growing = something holds data. Free growing while live does not = the heap keeps"));
        report.Line(QStringLiteral("what was freed, which is reused, not a leak. Everything else = thread stacks, graphics driver"));
        report.Line(QStringLiteral("buffers and textures, compiled QML, other VirtualAlloc."));
    }

    void WriteHeaps(Report& report, const HeapReport& heaps)
    {
        report.Title(QStringLiteral("Live heap blocks by size"));
        for (size_t i = 0; i < SizeClasses.size(); ++i)
        {
            const QString name = QString::fromLatin1(SizeClasses[i].name);
            report.Bytes(name, heaps.classBytes[i]);
            report.Count(name + QStringLiteral(", blocks"), heaps.classBlocks[i]);
        }

        report.Title(QStringLiteral("Heaps over 1 MB, MB"));
        report.Line(QStringLiteral("%1 %2 %3 %4 %5").arg(QStringLiteral("handle"), -20).arg(QStringLiteral("committed"), 11).arg(QStringLiteral("live"), 11).arg(QStringLiteral("free"), 11).arg(QStringLiteral("largest free"), 13));
        for (int i = 0; i < heaps.count; ++i)
        {
            const HeapTotals& heap = heaps.heaps[i];
            if (!heap.bWalked)
            {
                report.Line(QStringLiteral("0x%1 could not be walked").arg(heap.handle, 0, 16));
                continue;
            }
            if (heap.committed + heap.outsideRegions < 1024 * 1024)
                continue;
            const auto cell = [](qint64 value) { return QString::number(static_cast<double>(value) / (1024.0 * 1024.0), 'f', 1); };
            report.Line(QStringLiteral("%1 %2 %3 %4 %5")
                            .arg(QStringLiteral("0x%1").arg(heap.handle, 0, 16), -20)
                            .arg(cell(heap.committed + heap.outsideRegions), 11)
                            .arg(cell(heap.live), 11)
                            .arg(cell(heap.freeBytes), 11)
                            .arg(cell(heap.largestFree), 13));
        }
    }

    qint64 StringBytes(const QString& text)
    {
        return text.capacity() ? StringHeader + text.capacity() * 2 : 0;
    }

    void WriteAppData(Report& report)
    {
        report.Title(QStringLiteral("App data"));

        const LibrarySubsystem& library = LibrarySubsystem::Get();
        const QList<Track>& tracks = library.GetTracks();
        qint64 trackBytes = tracks.capacity() * static_cast<qint64>(sizeof(Track));
        int online = 0;
        int transient = 0;
        int missing = 0;
        for (const Track& track : tracks)
        {
            online += track.IsOnline() ? 1 : 0;
            transient += track.IsTransient() ? 1 : 0;
            missing += track.file.bMissing ? 1 : 0;
            trackBytes += StringBytes(track.file.path) + StringBytes(track.tags.title) + StringBytes(track.tags.artist)
                + StringBytes(track.tags.album) + StringBytes(track.tags.genre) + StringBytes(track.stream.pageUrl)
                + StringBytes(track.stream.thumbnailUrl) + StringBytes(track.stream.channelUrl);
            for (const QString& label : track.user.labels)
                trackBytes += StringBytes(label);
        }
        report.Count(QStringLiteral("Tracks"), tracks.size());
        report.Count(QStringLiteral("  online"), online);
        report.Count(QStringLiteral("  online, transient (search results etc.)"), transient);
        report.Count(QStringLiteral("  file missing"), missing);
        report.Bytes(QStringLiteral("  registry, about (without indexes)"), trackBytes);
        report.Count(QStringLiteral("Files waiting for tags"), library.GetPendingMetadataCount());

        const PlayerSubsystem& player = PlayerSubsystem::Get();
        report.Count(QStringLiteral("Player queue"), player.GetQueue().size());

        const QList<Playlist>& playlists = PlaylistSubsystem::Get().GetPlaylists();
        report.Count(QStringLiteral("Playlists"), playlists.size());
        report.Count(QStringLiteral("Download jobs"), DownloadSubsystem::Get().GetJobs().size());

        const auto rows = [&report](const char* name, QAbstractListModel* model) {
            report.Count(QStringLiteral("Rows: ") + QString::fromLatin1(name), model->rowCount());
        };
        LibraryViewModel& libraryView = LibraryViewModel::Get();
        rows("library", libraryView.GetTracks());
        rows("favorites", libraryView.GetFavorites());
        rows("recent", libraryView.GetRecent());
        rows("saved online", libraryView.GetOnline());
        rows("search results", SearchViewModel::Get().GetResults());
        rows("online page", OnlinePageViewModel::Get().GetTracks());
    }

    void CollectObjects(QObject* object, QSet<QObject*>& seen)
    {
        if (!object || seen.contains(object))
            return;
        seen.insert(object);

        for (QObject* child : object->children())
            CollectObjects(child, seen);

        if (const auto* item = qobject_cast<QQuickItem*>(object))
        {
            for (QQuickItem* child : item->childItems())
                CollectObjects(child, seen);
        }
        if (const auto* window = qobject_cast<QQuickWindow*>(object))
            CollectObjects(window->contentItem(), seen);
    }

    QString TypeName(const QObject* object)
    {
        QString name = QString::fromLatin1(object->metaObject()->className());
        for (const QLatin1StringView mark : {QLatin1StringView("_QMLTYPE_"), QLatin1StringView("_QML_")})
        {
            const qsizetype at = name.indexOf(mark);
            if (at > 0)
                name.truncate(at);
        }
        return name;
    }

    QString GraphicsApiName(QSGRendererInterface::GraphicsApi api)
    {
        switch (api)
        {
        case QSGRendererInterface::Direct3D11:
            return QStringLiteral("Direct3D 11");
        case QSGRendererInterface::Direct3D12:
            return QStringLiteral("Direct3D 12");
        case QSGRendererInterface::OpenGL:
            return QStringLiteral("OpenGL");
        case QSGRendererInterface::Vulkan:
            return QStringLiteral("Vulkan");
        case QSGRendererInterface::Software:
            return QStringLiteral("software");
        default:
            return QString::number(static_cast<int>(api));
        }
    }

    void WriteQml(Report& report, QQmlApplicationEngine* engine)
    {
        report.Title(QStringLiteral("QML"));
        if (!engine)
        {
            report.Line(QStringLiteral("No QML engine"));
            return;
        }

        if (const QV4::ExecutionEngine* v4 = engine->handle())
        {
            report.Bytes(QStringLiteral("JS heap, allocated blocks"), static_cast<qint64>(v4->memoryManager->getAllocatedMem()));
            report.Bytes(QStringLiteral("JS heap, used (with unswept garbage)"), static_cast<qint64>(v4->memoryManager->getUsedMem()));
            report.Bytes(QStringLiteral("JS heap, large items"), static_cast<qint64>(v4->memoryManager->getLargeItemsMem()));
        }

        if (QQuickWindow* window = AppInstance::getInstance() ? AppInstance::getInstance()->getMainWindow() : nullptr)
        {
            report.Line(QStringLiteral("Window: %1, %2x%3, graphics API %4")
                            .arg(window->isVisible() ? QStringLiteral("visible") : QStringLiteral("hidden"))
                            .arg(window->width())
                            .arg(window->height())
                            .arg(GraphicsApiName(window->rendererInterface()->graphicsApi())));
        }

        QSet<QObject*> objects;
        for (QObject* root : engine->rootObjects())
            CollectObjects(root, objects);

        QHash<QString, int> byType;
        QHash<QString, QSize> images;
        int imageItems = 0;
        for (QObject* object : objects)
        {
            ++byType[TypeName(object)];
            if (!object->inherits("QQuickImage"))
                continue;
            const QString source = object->property("source").toUrl().toString();
            if (source.isEmpty())
                continue;
            ++imageItems;
            images.insert(source, object->property("sourceSize").toSize());
        }

        report.Count(QStringLiteral("Objects of the window"), objects.size());

        qint64 imageBytes = 0;
        QList<QString> sources = images.keys();
        const auto decoded = [&images](const QString& source) {
            const QSize size = images.value(source);
            return static_cast<qint64>(size.width()) * size.height() * 4;
        };
        for (const QString& source : sources)
            imageBytes += decoded(source);
        std::sort(sources.begin(), sources.end(), [&](const QString& a, const QString& b) { return decoded(a) > decoded(b); });

        report.Count(QStringLiteral("Image items with a source"), imageItems);
        report.Count(QStringLiteral("  different images"), images.size());
        report.Bytes(QStringLiteral("  decoded size, about"), imageBytes);
        for (int i = 0; i < qMin(TopImages, static_cast<int>(sources.size())); ++i)
        {
            const QSize size = images.value(sources[i]);
            report.Line(QStringLiteral("    %1x%2  %3").arg(size.width()).arg(size.height()).arg(sources[i].left(100)));
        }
        report.Line(QStringLiteral("Images no item shows any more stay in Qt's cache for a while and are not counted here."));

        report.Title(QStringLiteral("QML objects by type"));
        QList<QString> types = byType.keys();
        std::sort(types.begin(), types.end(), [&](const QString& a, const QString& b) { return byType[a] > byType[b]; });
        for (int i = 0; i < qMin(TopQmlTypes, static_cast<int>(types.size())); ++i)
            report.Count(types[i], byType[types[i]]);
    }

    void WriteProcesses(Report& report)
    {
        report.Title(QStringLiteral("Threads and child processes"));

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            report.Line(QStringLiteral("No process list"));
            return;
        }

        QList<PROCESSENTRY32W> all;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry))
            all.append(entry);
        CloseHandle(snapshot);

        const DWORD self = GetCurrentProcessId();
        QSet<DWORD> family{self};
        for (bool bGrew = true; bGrew;)
        {
            bGrew = false;
            for (const PROCESSENTRY32W& process : all)
            {
                if (family.contains(process.th32ParentProcessID) && !family.contains(process.th32ProcessID))
                {
                    family.insert(process.th32ProcessID);
                    bGrew = true;
                }
            }
        }

        QList<ChildProcess> children;
        for (const PROCESSENTRY32W& process : all)
        {
            if (process.th32ProcessID == self)
                report.Count(QStringLiteral("Threads"), process.cntThreads);
            if (process.th32ProcessID == self || !family.contains(process.th32ProcessID))
                continue;

            ChildProcess child;
            child.name = QString::fromWCharArray(process.szExeFile);
            child.pid = process.th32ProcessID;
            if (const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.th32ProcessID))
            {
                PROCESS_MEMORY_COUNTERS_EX counters{};
                counters.cb = sizeof(counters);
                if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
                {
                    child.workingSet = static_cast<qint64>(counters.WorkingSetSize);
                    child.privateBytes = static_cast<qint64>(counters.PrivateUsage);
                }
                CloseHandle(handle);
            }
            children.append(child);
        }

        report.Count(QStringLiteral("Child processes"), children.size());
        const auto mb = [](qint64 value) { return QString::number(static_cast<double>(value) / (1024.0 * 1024.0), 'f', 1); };
        for (const ChildProcess& child : children)
            report.Line(QStringLiteral("  %1 (pid %2): working set %3 MB, private %4 MB").arg(child.name).arg(child.pid).arg(mb(child.workingSet), mb(child.privateBytes)));
    }

    QString Uptime()
    {
        FILETIME created;
        FILETIME unused;
        if (!GetProcessTimes(GetCurrentProcess(), &created, &unused, &unused, &unused))
            return {};
        const qint64 ticks = (static_cast<qint64>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
        const qint64 startMs = ticks / 10000 - 11644473600000LL;
        const qint64 seconds = (QDateTime::currentMSecsSinceEpoch() - startMs) / 1000;
        return QStringLiteral("%1 h %2 min %3 s").arg(seconds / 3600).arg(seconds / 60 % 60).arg(seconds % 60);
    }

    void RemoveOldReports(QDir& directory)
    {
        const QStringList reports = directory.entryList({QStringLiteral("memory-*.txt")}, QDir::Files, QDir::Name | QDir::Reversed);
        for (qsizetype i = MaxReports; i < reports.size(); ++i)
            directory.remove(reports[i]);
    }
}

QString MemoryDump::GetDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/memory-dumps");
}

QString MemoryDump::Write(const QString& reason)
{
    QElapsedTimer timer;
    timer.start();

    HeapReport heaps;
    ReadHeaps(heaps);
    const qint64 heapMs = timer.elapsed();

    const AddressSpace space = ReadAddressSpace();

    AppInstance* app = AppInstance::getInstance();
    QQmlApplicationEngine* engine = app ? app->getEngine() : nullptr;

    qint64 jsHeap = 0;
    if (engine && engine->handle())
    {
        const QV4::MemoryManager* memory = engine->handle()->memoryManager;
        jsHeap = static_cast<qint64>(memory->getAllocatedMem() + memory->getLargeItemsMem());
    }

    const QDateTime now = QDateTime::currentDateTime();
    const PlayerSubsystem& player = PlayerSubsystem::Get();

    Report report;
    report.Line(QStringLiteral("SoundLink %1 memory dump, %2").arg(QCoreApplication::applicationVersion(), now.toString(Qt::ISODate)));
#ifdef QT_DEBUG
    report.Line(QStringLiteral("Debug build"));
#else
    report.Line(QStringLiteral("Release build"));
#endif
    report.Line(QStringLiteral("Running for %1").arg(Uptime()));
    if (!reason.isEmpty())
        report.Line(QStringLiteral("Reason: %1").arg(reason));
    report.Line(QStringLiteral("Player: %1").arg(player.IsPlaying() ? QStringLiteral("playing") : QStringLiteral("not playing")));
    report.Line(QStringLiteral("Numbers in brackets: change since the previous dump of this run."));

    WriteProcess(report, space, heaps, jsHeap);
    WriteHeaps(report, heaps);
    WriteAppData(report);
    WriteQml(report, engine);
    WriteProcesses(report);

    report.Title(QStringLiteral("Modules by RAM, MB"));
    WriteTop(report, space.images, TopModules, QString(), ModuleName);

    report.Title(QStringLiteral("Mapped files by RAM, MB"));
    WriteTop(report, space.mappedFiles, TopMappedFiles, QStringLiteral("file "), MappedName);

    report.Line();
    report.Line(QStringLiteral("Heap walk %1 ms, whole dump %2 ms").arg(heapMs).arg(timer.elapsed()));
    report.Finish();

    QDir directory(GetDirectory());
    if (!directory.mkpath(QStringLiteral(".")))
        return {};

    const QString path = directory.filePath(QStringLiteral("memory-%1.txt").arg(now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    file.write(report.text.toUtf8());
    file.close();

    RemoveOldReports(directory);
    return path;
}
