#include "DownloadsViewModel.h"

#include <QDesktopServices>
#include <QDir>
#include <QJSEngine>
#include <QLocale>
#include <QUrl>

#include "Downloads/DownloadSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "LibraryViewModel.h"
#include "Playlists/PlaylistSubsystem.h"

namespace
{
    qreal Progress(const DownloadSubsystem::Job& job)
    {
        if (job.state == DownloadSubsystem::Job::State::Done)
            return 1;
        return job.total > 0 ? qBound<qreal>(0, static_cast<qreal>(job.received) / job.total, 1) : -1;
    }
}

int DownloadListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : GetCount();
}

QVariant DownloadListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Row& row = rows.at(index.row());
    switch (role)
    {
        case TrackIdRole:  return row.trackId;
        case TitleRole:    return row.title;
        case ArtistRole:   return row.artist;
        case ArtUrlRole:   return row.artUrl;
        case ArtTintRole:  return row.artTint;
        case StateRole:    return row.state;
        case ProgressRole: return row.progress;
        case SizeTextRole: return row.sizeText;
        case ErrorRole:    return row.error;
        default:           return {};
    }
}

QHash<int, QByteArray> DownloadListModel::roleNames() const
{
    return {
        {TrackIdRole, "trackId"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {ArtUrlRole, "artUrl"},
        {ArtTintRole, "artTint"},
        {StateRole, "jobState"},
        {ProgressRole, "progress"},
        {SizeTextRole, "sizeText"},
        {ErrorRole, "error"},
    };
}

void DownloadListModel::SetRows(const QList<Row>& newRows)
{
    bool bSameRows = newRows.size() == rows.size();
    for (qsizetype i = 0; bSameRows && i < rows.size(); ++i)
        bSameRows = rows[i].trackId == newRows[i].trackId;

    if (bSameRows)
    {
        rows = newRows;
        if (!rows.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(rows.size()) - 1));
        return;
    }

    const bool bCountChanged = newRows.size() != rows.size();
    beginResetModel();
    rows = newRows;
    endResetModel();
    if (bCountChanged)
        emit countChanged();
}

DownloadsViewModel::DownloadsViewModel()
{
    DownloadSubsystem& downloads = DownloadSubsystem::Get();
    connect(&downloads, &DownloadSubsystem::jobsChanged, this, &DownloadsViewModel::Rebuild);
    connect(&downloads, &DownloadSubsystem::progressChanged, this, &DownloadsViewModel::Rebuild);
    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::tracksChanged, this, &DownloadsViewModel::Rebuild);

    Rebuild();
}

DownloadsViewModel& DownloadsViewModel::Get()
{
    static DownloadsViewModel instance;
    return instance;
}

DownloadsViewModel* DownloadsViewModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

void DownloadsViewModel::Rebuild()
{
    const QLocale locale;

    QList<DownloadListModel::Row> rows;
    for (const DownloadSubsystem::Job& job : DownloadSubsystem::Get().GetJobs())
    {
        DownloadListModel::Row row;
        row.trackId = static_cast<int>(job.track.value);
        row.state = static_cast<DownloadListModel::State>(job.state);
        row.progress = Progress(job);
        row.error = job.error;

        if (job.total > 0 && job.state == DownloadSubsystem::Job::State::Downloading)
            row.sizeText = locale.formattedDataSize(job.received) + " / " + locale.formattedDataSize(job.total);
        else if (job.total > 0)
            row.sizeText = locale.formattedDataSize(job.total);

        if (const Track* track = LibrarySubsystem::Get().FindTrack(job.track))
        {
            const TrackData data = LibraryViewModel::ToTrackData(*track);
            row.title = data.title;
            row.artist = data.artist;
            row.artUrl = data.artUrl;
            row.artTint = data.artTint;
        }
        rows.append(row);
    }

    jobsModel.SetRows(rows);
    emit changed();
}

int DownloadsViewModel::GetActiveCount() const
{
    return DownloadSubsystem::Get().GetActiveCount();
}

int DownloadsViewModel::GetFinishedCount() const
{
    return static_cast<int>(DownloadSubsystem::Get().GetJobs().size()) - GetActiveCount();
}

QString DownloadsViewModel::GetFolder() const
{
    return QDir::toNativeSeparators(DownloadSubsystem::Get().GetFolder());
}

void DownloadsViewModel::download(int trackId)
{
    DownloadSubsystem::Get().Enqueue({TrackId{static_cast<quint32>(trackId)}});
}

int DownloadsViewModel::downloadPlaylist(const QString& playlistId)
{
    return DownloadSubsystem::Get().Enqueue(PlaylistSubsystem::Get().Resolve(playlistId));
}

void DownloadsViewModel::cancel(int trackId)
{
    DownloadSubsystem::Get().Cancel(TrackId{static_cast<quint32>(trackId)});
}

void DownloadsViewModel::retry(int trackId)
{
    DownloadSubsystem::Get().Retry(TrackId{static_cast<quint32>(trackId)});
}

void DownloadsViewModel::clearFinished()
{
    DownloadSubsystem::Get().ClearFinished();
}

void DownloadsViewModel::removeDownload(int trackId)
{
    DownloadSubsystem::Get().RemoveDownload(TrackId{static_cast<quint32>(trackId)});
}

void DownloadsViewModel::openFolder()
{
    const QString folder = DownloadSubsystem::Get().GetFolder();
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

int DownloadsViewModel::stateOf(int trackId) const
{
    const DownloadSubsystem::Job* job = DownloadSubsystem::Get().FindJob(TrackId{static_cast<quint32>(trackId)});
    return job ? static_cast<int>(job->state) : -1;
}

qreal DownloadsViewModel::progressOf(int trackId) const
{
    const DownloadSubsystem::Job* job = DownloadSubsystem::Get().FindJob(TrackId{static_cast<quint32>(trackId)});
    return job ? Progress(*job) : -1;
}
