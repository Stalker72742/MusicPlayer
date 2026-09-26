//
// Created by Stalker7274 on 12.07.2026.
//

#include "MedialibModel.h"

MedialibModel::MedialibModel(QObject* parent)
{
	
}

MedialibModel::trackInfo::trackInfo() {}

MedialibModel::trackInfo::trackInfo(uint32_t inIndex, const QString& inTrackName, const QDateTime& inDateAdded,
	uint32_t inDurationSec, const QString& inArtist)
{
	trackName = inTrackName;
}

int MedialibModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid() || tracks.isEmpty()) return 0;
	return static_cast<int>(tracks.size());
}

QVariant MedialibModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() >= tracks.size())
	{
		qDebug() << "Index not valid or index to high";
		return {};
	}

	const trackInfo &t = tracks.at(index.row());
	switch (role) {
		case IdxRole:        return static_cast<int>(t.idx);
		case NameRole:      return t.trackName;
		case ArtistRole:     return t.artist;
		case DateAddedRole:  return t.dateAdded.toString("MMM d, yyyy");
		case DurationRole:   return t.durationSec;
		default:

			qDebug() << "Unknown role, return empty data";
			return {};
	}
}

bool MedialibModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid() || index.row() >= tracks.size())
		return false;

	emit dataChanged(index, index, {role});
	return true;
}

QHash<int, QByteArray> MedialibModel::roleNames() const
{
	return {
	        { IdxRole,       "idx" },
			{ NameRole,     "title" },
			{ ArtistRole,    "artist" },
			{ DateAddedRole, "dateAdded" },
			{ DurationRole,  "duration" }
		};
}

Qt::ItemFlags MedialibModel::flags(const QModelIndex& index) const
{
	if (!index.isValid()) return Qt::NoItemFlags;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}