#include "CatalogViewModel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QProcess>

#include "Catalog/ArtworkSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Navigation.h"
#include "Player/PlayerSubsystem.h"

namespace
{
    std::optional<CatalogKind> KindOf(const QString& id)
    {
        return ParseCatalogKind(id);
    }

    QString TracksText(qsizetype count)
    {
        return count == 1 ? QStringLiteral("1 track") : QStringLiteral("%1 tracks").arg(count);
    }

    void RevealInExplorer(const QString& path, bool bSelect)
    {
#ifdef Q_OS_WIN
        if (bSelect)
        {
            QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
            return;
        }
#endif
        QDesktopServices::openUrl(QUrl::fromLocalFile(bSelect ? QFileInfo(path).absolutePath() : path));
    }
}

CatalogListModel::CatalogListModel(CatalogKind kind, QObject* parent)
    : QAbstractListModel(parent)
    , kind(kind)
{
}

int CatalogListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries.size());
}

QVariant CatalogListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const CatalogEntry& entry = entries.at(index.row());
    switch (role)
    {
        case KeyRole:        return entry.key;
        case KindRole:       return CatalogKindId(entry.kind);
        case NameRole:       return entry.name;
        case SubtitleRole:   return CatalogViewModel::Subtitle(entry);
        case TrackCountRole: return static_cast<int>(entry.tracks.size());
        case InitialsRole:   return CatalogViewModel::Initials(entry.name);
        case TintRole:       return CatalogViewModel::Tint(entry.name);
        case ImageUrlRole:
        {
            const QUrl url = ArtworkSubsystem::Get().GetImageUrl(entry.kind, entry.imageName);
            if (url.isEmpty())
            {
                // Not from inside data(): a lookup can finish right away and change the model.
                QMetaObject::invokeMethod(&ArtworkSubsystem::Get(), [entry] { ArtworkSubsystem::Get().Request(entry); },
                    Qt::QueuedConnection);
            }
            return url;
        }
        default: return {};
    }
}

QHash<int, QByteArray> CatalogListModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {KindRole, "kind"},
        {NameRole, "name"},
        {SubtitleRole, "subtitle"},
        {TrackCountRole, "trackCount"},
        {ImageUrlRole, "imageUrl"},
        {InitialsRole, "initials"},
        {TintRole, "tint"},
    };
}

void CatalogListModel::Refresh()
{
    const QList<CatalogEntry>& next = CatalogSubsystem::Get().GetEntries(kind);

    bool bSameRows = next.size() == entries.size();
    for (qsizetype row = 0; bSameRows && row < next.size(); ++row)
        bSameRows = next[row].key == entries[row].key;

    if (bSameRows)
    {
        entries = next;
        if (!entries.isEmpty())
            emit dataChanged(index(0), index(rowCount() - 1));
        return;
    }

    const qsizetype oldCount = entries.size();
    beginResetModel();
    entries = next;
    endResetModel();
    if (oldCount != entries.size())
        emit countChanged();
}

void CatalogListModel::RefreshImages()
{
    if (!entries.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1), {ImageUrlRole});
}

CatalogViewModel::CatalogViewModel()
{
    CatalogSubsystem& catalog = CatalogSubsystem::Get();
    ArtworkSubsystem& artwork = ArtworkSubsystem::Get();

    connect(&catalog, &CatalogSubsystem::catalogChanged, this, [this] {
        artistsModel.Refresh();
        albumsModel.Refresh();
        genresModel.Refresh();
        RefreshCurrent();
    });
    connect(&artwork, &ArtworkSubsystem::imagesChanged, this, [this] {
        artistsModel.RefreshImages();
        albumsModel.RefreshImages();
        genresModel.RefreshImages();
        emit currentChanged();
    });
    connect(&artwork, &ArtworkSubsystem::lookupFinished, this, &CatalogViewModel::currentChanged);
    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &CatalogViewModel::RefreshCurrent);
    connect(&Navigation::Get(), &Navigation::collectionChanged, this, &CatalogViewModel::RefreshCurrent);

    artistsModel.Refresh();
    albumsModel.Refresh();
    genresModel.Refresh();
    RefreshCurrent();
}

CatalogViewModel& CatalogViewModel::Get()
{
    static CatalogViewModel instance;
    return instance;
}

CatalogViewModel* CatalogViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

const CatalogEntry* CatalogViewModel::FindCurrent() const
{
    const Navigation& navigation = Navigation::Get();
    const std::optional<CatalogKind> kind = KindOf(navigation.GetCollectionKind());
    return kind ? CatalogSubsystem::Get().Find(*kind, navigation.GetCollectionKey()) : nullptr;
}

void CatalogViewModel::RefreshCurrent()
{
    QList<TrackData> data;
    if (const CatalogEntry* entry = FindCurrent())
    {
        const LibrarySubsystem& library = LibrarySubsystem::Get();
        for (TrackId id : entry->tracks)
        {
            if (const Track* track = library.FindTrack(id))
                data.append(LibraryViewModel::ToTrackData(*track));
        }
    }
    currentTracksModel.SetTracks(data);
    emit currentChanged();
}

QString CatalogViewModel::GetCurrentKind() const
{
    const CatalogEntry* entry = FindCurrent();
    return entry ? CatalogKindId(entry->kind) : QString();
}

QString CatalogViewModel::GetCurrentName() const
{
    const CatalogEntry* entry = FindCurrent();
    return entry ? entry->name : QString();
}

QString CatalogViewModel::GetCurrentSubtitle() const
{
    const CatalogEntry* entry = FindCurrent();
    if (!entry)
        return {};

    switch (entry->kind)
    {
        case CatalogKind::Artist: return QStringLiteral("Artist · ") + TracksText(entry->tracks.size());
        case CatalogKind::Genre:  return QStringLiteral("Genre · ") + TracksText(entry->tracks.size());
        case CatalogKind::Album:
            return QStringLiteral("Album · ") + entry->artist + (entry->year > 0 ? QStringLiteral(" · %1").arg(entry->year) : QString())
                + QStringLiteral(" · ") + TracksText(entry->tracks.size());
    }
    return {};
}

QUrl CatalogViewModel::GetCurrentImage() const
{
    const CatalogEntry* entry = FindCurrent();
    if (!entry)
        return {};

    const QUrl url = ArtworkSubsystem::Get().GetImageUrl(entry->kind, entry->imageName);
    if (url.isEmpty())
    {
        QMetaObject::invokeMethod(&ArtworkSubsystem::Get(), [entry = *entry] { ArtworkSubsystem::Get().Request(entry); },
            Qt::QueuedConnection);
    }
    return url;
}

QString CatalogViewModel::GetCurrentInitials() const
{
    const CatalogEntry* entry = FindCurrent();
    return entry ? Initials(entry->name) : QString();
}

QColor CatalogViewModel::GetCurrentTint() const
{
    const CatalogEntry* entry = FindCurrent();
    return entry ? Tint(entry->name) : QColor();
}

bool CatalogViewModel::IsCurrentFetching() const
{
    const CatalogEntry* entry = FindCurrent();
    return entry && ArtworkSubsystem::Get().IsFetching(entry->kind, entry->imageName);
}

void CatalogViewModel::open(const QString& kind, const QString& key)
{
    Navigation::Get().openCollection(kind, key);
}

bool CatalogViewModel::hasOfTrack(const QString& kind, int trackId) const
{
    const std::optional<CatalogKind> parsed = KindOf(kind);
    return parsed && !CatalogSubsystem::Get().GetKeysOf(*parsed, TrackId{static_cast<quint32>(trackId)}).isEmpty();
}

bool CatalogViewModel::openOfTrack(const QString& kind, int trackId)
{
    const std::optional<CatalogKind> parsed = KindOf(kind);
    if (!parsed)
        return false;

    const QStringList keys = CatalogSubsystem::Get().GetKeysOf(*parsed, TrackId{static_cast<quint32>(trackId)});
    if (keys.isEmpty())
        return false;

    open(kind, keys.first());
    return true;
}

QVariantList CatalogViewModel::trackIdsOf(const QString& kind, const QString& key) const
{
    QVariantList result;
    if (const CatalogEntry* entry = FindEntry(kind, key))
    {
        for (TrackId id : entry->tracks)
            result.append(static_cast<int>(id.value));
    }
    return result;
}

void CatalogViewModel::playGroup(const QString& kind, const QString& key)
{
    const CatalogEntry* entry = FindEntry(kind, key);
    if (entry && !entry->tracks.isEmpty())
        PlayerSubsystem::Get().PlayQueue(QList<TrackId>(entry->tracks.cbegin(), entry->tracks.cend()), 0);
}

bool CatalogViewModel::hasImageOf(const QString& kind, const QString& key) const
{
    const CatalogEntry* entry = FindEntry(kind, key);
    return entry && !ArtworkSubsystem::Get().FindImage(entry->kind, entry->imageName).isEmpty();
}

QString CatalogViewModel::nameOf(const QString& kind, const QString& key) const
{
    const CatalogEntry* entry = FindEntry(kind, key);
    return entry ? entry->name : QString();
}

QString CatalogViewModel::setImageOf(const QString& kind, const QString& key, const QUrl& file)
{
    const CatalogEntry* entry = FindEntry(kind, key);
    if (!entry || !file.isLocalFile())
        return QStringLiteral("Pick an image file.");
    return ArtworkSubsystem::Get().SetImage(entry->kind, entry->imageName, file.toLocalFile());
}

void CatalogViewModel::removeImageOf(const QString& kind, const QString& key)
{
    if (const CatalogEntry* entry = FindEntry(kind, key))
        ArtworkSubsystem::Get().RemoveImage(entry->kind, entry->imageName);
}

void CatalogViewModel::findImageOnlineOf(const QString& kind, const QString& key)
{
    if (const CatalogEntry* entry = FindEntry(kind, key))
    {
        ArtworkSubsystem::Get().FindOnline(*entry);
        emit currentChanged();
    }
}

void CatalogViewModel::showImageInFolderOf(const QString& kind, const QString& key)
{
    const CatalogEntry* entry = FindEntry(kind, key);
    if (!entry)
        return;

    const QString path = ArtworkSubsystem::Get().FindImage(entry->kind, entry->imageName);
    if (path.isEmpty())
        RevealInExplorer(ArtworkSubsystem::Get().GetKindFolder(entry->kind), false);
    else
        RevealInExplorer(path, true);
}

const CatalogEntry* CatalogViewModel::FindEntry(const QString& kind, const QString& key) const
{
    const std::optional<CatalogKind> parsed = KindOf(kind);
    return parsed ? CatalogSubsystem::Get().Find(*parsed, key) : nullptr;
}

QStringList CatalogViewModel::names(const QString& kind) const
{
    const std::optional<CatalogKind> parsed = KindOf(kind);
    return parsed ? CatalogSubsystem::Get().GetNames(*parsed) : QStringList();
}

QString CatalogViewModel::setImage(const QUrl& file)
{
    const CatalogEntry* entry = FindCurrent();
    if (!entry || !file.isLocalFile())
        return QStringLiteral("Pick an image file.");
    return ArtworkSubsystem::Get().SetImage(entry->kind, entry->imageName, file.toLocalFile());
}

void CatalogViewModel::removeImage()
{
    if (const CatalogEntry* entry = FindCurrent())
        ArtworkSubsystem::Get().RemoveImage(entry->kind, entry->imageName);
}

void CatalogViewModel::findImageOnline()
{
    if (const CatalogEntry* entry = FindCurrent())
    {
        ArtworkSubsystem::Get().FindOnline(*entry);
        emit currentChanged();
    }
}

void CatalogViewModel::showImageInFolder()
{
    const CatalogEntry* entry = FindCurrent();
    if (!entry)
        return;

    const QString path = ArtworkSubsystem::Get().FindImage(entry->kind, entry->imageName);
    if (path.isEmpty())
        RevealInExplorer(ArtworkSubsystem::Get().GetKindFolder(entry->kind), false);
    else
        RevealInExplorer(path, true);
}

void CatalogViewModel::openArtworkFolder()
{
    const ArtworkSubsystem& artwork = ArtworkSubsystem::Get();
    artwork.GetKindFolder(CatalogKind::Artist);
    RevealInExplorer(artwork.GetFolder(), false);
}

QString CatalogViewModel::Initials(const QString& name)
{
    QString initials;
    for (const QString& word : name.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        const auto first = std::find_if(word.cbegin(), word.cend(), [](QChar c) { return c.isLetterOrNumber(); });
        if (first != word.cend())
            initials.append(first->toUpper());
        if (initials.size() == 2)
            break;
    }
    return initials.isEmpty() ? name.left(1).toUpper() : initials;
}

QColor CatalogViewModel::Tint(const QString& name)
{
    const int hue = static_cast<int>(qHash(name.toCaseFolded()) % 360);
    return QColor::fromHsl(hue, 70, 42);
}

QString CatalogViewModel::Subtitle(const CatalogEntry& entry)
{
    switch (entry.kind)
    {
        case CatalogKind::Album:
            return entry.artist + (entry.year > 0 ? QStringLiteral(" · %1").arg(entry.year) : QString());
        default:
            return TracksText(entry.tracks.size());
    }
}
