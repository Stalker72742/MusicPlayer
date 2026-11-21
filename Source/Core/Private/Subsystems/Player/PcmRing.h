//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

#include <algorithm>
#include <atomic>
#include <cstring>

/// @brief Decoded samples on their way from the decoder thread to the output thread.
class PcmRing
{
public:
    explicit PcmRing(qsizetype capacity)
        : bytes(capacity, Qt::Uninitialized)
    {
    }

    /// @brief Stores samples, waiting while full.
    /// @param bInterrupted Checked while waiting; set it and call Wake() to get out.
    /// @return false if interrupted.
    bool Write(const char* data, qsizetype size, const std::atomic<bool>& bInterrupted)
    {
        QMutexLocker lock(&mutex);
        while (size > 0)
        {
            while (used == bytes.size())
            {
                if (bInterrupted)
                    return false;
                space.wait(&mutex);
            }
            if (bInterrupted)
                return false;

            const qsizetype end = (start + used) % bytes.size();
            const qsizetype chunk = std::min({size, bytes.size() - used, bytes.size() - end});
            std::memcpy(bytes.data() + end, data, static_cast<size_t>(chunk));
            used += chunk;
            data += chunk;
            size -= chunk;
        }
        return true;
    }

    /// @brief Takes what is there, up to size bytes, without waiting.
    /// @return Bytes taken.
    qsizetype Read(char* data, qsizetype size)
    {
        QMutexLocker lock(&mutex);
        qsizetype done = 0;
        while (done < size && used > 0)
        {
            const qsizetype chunk = std::min({size - done, used, bytes.size() - start});
            std::memcpy(data + done, bytes.constData() + start, static_cast<size_t>(chunk));
            start = (start + chunk) % bytes.size();
            used -= chunk;
            done += chunk;
        }
        totalRead += done;
        if (done > 0)
            space.wakeAll();
        return done;
    }

    /// @brief Drops the samples and restarts the count of TotalRead().
    void Clear()
    {
        QMutexLocker lock(&mutex);
        start = 0;
        used = 0;
        totalRead = 0;
        space.wakeAll();
    }

    /// @brief Wakes a waiting Write().
    void Wake()
    {
        QMutexLocker lock(&mutex);
        space.wakeAll();
    }

    /// @brief Bytes read since the last Clear().
    qint64 TotalRead() const
    {
        QMutexLocker lock(&mutex);
        return totalRead;
    }

    bool IsEmpty() const
    {
        QMutexLocker lock(&mutex);
        return used == 0;
    }

private:
    mutable QMutex mutex;
    QWaitCondition space;
    QByteArray bytes;
    qsizetype start = 0;
    qsizetype used = 0;
    qint64 totalRead = 0;
};
