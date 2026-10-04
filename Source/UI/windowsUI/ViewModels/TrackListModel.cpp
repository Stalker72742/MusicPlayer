#include "TrackListModel.h"

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

    bool bAppended = newTracks.size() > tracks.size();
    for (qsizetype row = 0; bAppended && row < tracks.size(); ++row)
        bAppended = tracks[row].id == newTracks[row].id;

    if (bAppended)
    {
        const int oldCount = static_cast<int>(tracks.size());
        if (oldCount > 0)
        {
            for (qsizetype row = 0; row < oldCount; ++row)
                tracks[row] = newTracks[row];
            emit dataChanged(index(0), index(oldCount - 1));
        }

        beginInsertRows({}, oldCount, static_cast<int>(newTracks.size()) - 1);
        for (qsizetype row = oldCount; row < newTracks.size(); ++row)
            tracks.append(newTracks[row]);
        endInsertRows();

        emit countChanged();
        return;
    }

    const bool bCountChanged = newTracks.size() != tracks.size();

    beginResetModel();
    tracks = newTracks;
    endResetModel();

    if (bCountChanged)
        emit countChanged();
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
