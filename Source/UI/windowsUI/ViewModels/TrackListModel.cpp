#include "TrackListModel.h"

#include <QSet>

int TrackListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : GetCount();
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const TrackData& track = tracks.at(index.row());
    switch (role)
    {
        case TrackIdRole:     return track.id;
        case NumberRole:      return index.row() + 1;
        case TitleRole:       return track.title;
        case ArtistRole:      return track.artist;
        case AlbumRole:       return track.album;
        case DateAddedRole:   return track.dateAdded.toString(QStringLiteral("MMM d, yyyy"));
        case DurationRole:    return FormatDuration(track.durationSec);
        case DurationSecRole: return track.durationSec;
        case LikedRole:       return track.bLiked;
        case ArtTintRole:     return track.artTint;
        case ArtUrlRole:      return track.artUrl;
        case OnlineRole:      return track.bOnline;
        case InLibraryRole:   return track.bInLibrary;
        case DownloadedRole:  return track.bDownloaded;
        default:              return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {TrackIdRole, "trackId"},
        {NumberRole, "number"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {DateAddedRole, "dateAdded"},
        {DurationRole, "duration"},
        {DurationSecRole, "durationSec"},
        {LikedRole, "liked"},
        {ArtTintRole, "artTint"},
        {ArtUrlRole, "artUrl"},
        {OnlineRole, "online"},
        {InLibraryRole, "inLibrary"},
        {DownloadedRole, "downloaded"},
    };
}

void TrackListModel::SetTracks(const QList<TrackData>& newTracks)
{
    bool bSameRows = newTracks.size() == tracks.size();
    for (qsizetype row = 0; bSameRows && row < tracks.size(); ++row)
        bSameRows = tracks[row].id == newTracks[row].id;

    if (bSameRows)
    {
        tracks = newTracks;
        if (!tracks.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(tracks.size()) - 1));
        return;
    }

    const bool bCountChanged = newTracks.size() != tracks.size();

    if (ApplyInPlace(newTracks))
    {
        if (!tracks.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(tracks.size()) - 1));
    }
    else
    {
        beginResetModel();
        tracks = newTracks;
        endResetModel();
    }

    if (bCountChanged)
        emit countChanged();
}

bool TrackListModel::ApplyInPlace(const QList<TrackData>& newTracks)
{
    QSet<int> newIds;
    newIds.reserve(newTracks.size());
    for (const TrackData& track : newTracks)
        newIds.insert(track.id);

    QSet<int> oldIds;
    oldIds.reserve(tracks.size());
    for (const TrackData& track : tracks)
        oldIds.insert(track.id);

    qsizetype oldRow = 0;
    for (const TrackData& track : newTracks)
    {
        if (!oldIds.contains(track.id))
            continue;

        while (oldRow < tracks.size() && !newIds.contains(tracks[oldRow].id))
            ++oldRow;
        if (oldRow >= tracks.size() || tracks[oldRow].id != track.id)
            return false;
        ++oldRow;
    }

    for (qsizetype last = tracks.size() - 1; last >= 0;)
    {
        if (newIds.contains(tracks[last].id))
        {
            --last;
            continue;
        }

        qsizetype first = last;
        while (first > 0 && !newIds.contains(tracks[first - 1].id))
            --first;

        beginRemoveRows({}, static_cast<int>(first), static_cast<int>(last));
        tracks.remove(first, last - first + 1);
        endRemoveRows();
        last = first - 1;
    }

    for (qsizetype row = 0; row < newTracks.size();)
    {
        if (oldIds.contains(newTracks[row].id))
        {
            ++row;
            continue;
        }

        qsizetype end = row;
        while (end < newTracks.size() && !oldIds.contains(newTracks[end].id))
            ++end;

        beginInsertRows({}, static_cast<int>(row), static_cast<int>(end) - 1);
        tracks.insert(row, end - row, TrackData{});
        for (qsizetype i = row; i < end; ++i)
            tracks[i] = newTracks[i];
        endInsertRows();
        row = end;
    }

    tracks = newTracks;
    return true;
}

int TrackListModel::RowOf(int trackId) const
{
    for (qsizetype row = 0; row < tracks.size(); ++row)
    {
        if (tracks[row].id == trackId)
            return static_cast<int>(row);
    }
    return -1;
}

QString TrackListModel::FormatDuration(int seconds)
{
    if (seconds >= 3600)
    {
        return QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600)
            .arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
