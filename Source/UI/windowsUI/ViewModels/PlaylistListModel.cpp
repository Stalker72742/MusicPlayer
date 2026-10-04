#include "PlaylistListModel.h"

int PlaylistListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : GetCount();
}

QVariant PlaylistListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const PlaylistData& playlist = playlists.at(index.row());
    switch (role)
    {
        case IdRole:         return playlist.id;
        case NameRole:       return playlist.name;
        case SmartRole:      return playlist.bSmart;
        case TrackCountRole: return playlist.trackCount;
        case ArtTintRole:    return playlist.artTint;
        case ArtUrlRole:     return playlist.artUrl;
        default:             return {};
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    return {
        {IdRole, "playlistId"},
        {NameRole, "name"},
        {SmartRole, "smart"},
        {TrackCountRole, "trackCount"},
        {ArtTintRole, "artTint"},
        {ArtUrlRole, "artUrl"},
    };
}

void PlaylistListModel::SetPlaylists(const QList<PlaylistData>& newPlaylists)
{
    bool bSameRows = newPlaylists.size() == playlists.size();
    for (qsizetype row = 0; bSameRows && row < playlists.size(); ++row)
        bSameRows = playlists[row].id == newPlaylists[row].id;

    if (bSameRows)
    {
        playlists = newPlaylists;
        if (!playlists.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(playlists.size()) - 1));
        return;
    }

    const bool bCountChanged = newPlaylists.size() != playlists.size();

    beginResetModel();
    playlists = newPlaylists;
    endResetModel();

    if (bCountChanged)
        emit countChanged();
}
