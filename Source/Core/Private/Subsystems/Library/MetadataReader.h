//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include <QList>
#include <QObject>
#include <QTimer>

#include "Library/Track.h"

class QMediaPlayer;

/// @brief Reads tags and duration of queued files one at a time through QMediaPlayer (no extra dependencies).
class MetadataReader : public QObject
{
    Q_OBJECT

public:
    explicit MetadataReader(QObject* parent = nullptr);
    ~MetadataReader() override;

    /// @brief Queues a file; tagsRead() reports the result.
    void Enqueue(TrackId id, const QString& path);
    /// @brief Drops all queued files.
    void Clear();

    /// @brief Files queued or being read.
    int GetPendingCount() const { return static_cast<int>(queue.size()) + (current.IsValid() ? 1 : 0); }

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
    void Finish(bool bSuccess);

    QMediaPlayer* player = nullptr;
    QTimer timeout;
    QList<Request> queue;
    TrackId current;
};
