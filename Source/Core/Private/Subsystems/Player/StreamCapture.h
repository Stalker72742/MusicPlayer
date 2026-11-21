//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QByteArray>
#include <QFile>
#include <QMap>
#include <QString>

/// @brief Collects the bytes of a playing stream into a file until the whole source is there.
///
/// Bytes may come out of order and repeat: a seek skips ahead and a later read fills the gap.
class StreamCapture
{
public:
    /// @brief Starts a new temporary file, replacing the one being collected.
    /// @param path Where the bytes go.
    /// @return false if the file cannot be written.
    bool Start(const QString& path);
    /// @brief Forgets the bytes and deletes the file.
    void Abort();

    bool IsActive() const { return file.isOpen(); }

    void SetSize(qint64 size) { total = size; }
    /// @brief Stores bytes read from the source.
    void Add(qint64 offset, const QByteArray& bytes);
    /// @brief Whether the whole source is collected.
    bool IsComplete() const;

    /// @brief Closes a complete file and gives it up to the caller.
    /// @return Its path.
    QString Take();

private:
    QFile file;
    qint64 total = 0;
    /// Start of a collected range -> its end, with no ranges touching.
    QMap<qint64, qint64> ranges;
};
