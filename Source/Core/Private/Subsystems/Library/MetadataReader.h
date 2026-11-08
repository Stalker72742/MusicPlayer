//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QThreadPool>

#include "Library/Track.h"

/// @brief Reads tags and duration of queued files through libavformat, a few at a time on background threads.
///
/// A file stays open only while it is being read.
class MetadataReader : public QObject
{
    Q_OBJECT

public:
    explicit MetadataReader(QObject* parent = nullptr);
    ~MetadataReader() override;

    /// @brief Queues a file unless it is already waiting; tagsRead() reports the result.
    void Enqueue(TrackId id, const QString& path);
    /// @brief Drops all queued files; those being read still report.
    void Clear();

    /// @brief Files queued or being read.
    int GetPendingCount() const { return static_cast<int>(queue.size()) + running; }

signals:
    /// @brief A file has been read.
    /// @param bSuccess false for unreadable files; tags are then empty.
    void tagsRead(TrackId id, const TagsComponent& tags, bool bSuccess);
    /// @brief The queue has run empty.
    void idle();

private:
    struct Request
    {
        TrackId id;
        QString path;
    };

    void StartNext();
    void OnRead(TrackId id, const TagsComponent& tags, bool bSuccess);

    QThreadPool pool;
    QList<Request> queue;
    QSet<TrackId> queued;
    int running = 0;
};
