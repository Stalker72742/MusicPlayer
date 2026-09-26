//
// Created by Stalker7274 on 12.07.2026.
//

#pragma once

#include <QAbstractListModel>
#include <qdatetime.h>
#include <qqmlintegration.h>

class MedialibModel : public QAbstractListModel
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(int count READ count NOTIFY countChanged)

public:

	explicit MedialibModel(QObject *parent = nullptr);

	enum Roles {
		IdxRole = Qt::UserRole + 1,
		NameRole,
		ArtistRole,
		DateAddedRole,
		DurationRole,
	};
	Q_ENUM(Roles)

protected:

	struct trackInfo
	{
		uint32_t idx {1};
		QString trackName {""};
		QString artist {""};
		QDateTime dateAdded;
		uint32_t durationSec {0};

		explicit trackInfo();
		explicit trackInfo(uint32_t inIndex, const QString &inTrackName, const QDateTime& inDateAdded, uint32_t inDurationSec, const QString& inArtist = "");
	};

	QList<trackInfo> tracks;

signals:
	void countChanged();

public:

	int rowCount(const QModelIndex &parent) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	QHash<int, QByteArray> roleNames() const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

public:

	int count() const { return static_cast<int>(tracks.size()); }
};
