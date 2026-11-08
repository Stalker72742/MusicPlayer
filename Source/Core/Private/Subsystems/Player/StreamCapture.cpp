//
// Created by Stalker7274 on 05.11.2026.
//

#include "StreamCapture.h"

#include <QDir>
#include <QFileInfo>

bool StreamCapture::Start(const QString& path)
{
    Abort();

    QDir().mkpath(QFileInfo(path).absolutePath());
    file.setFileName(path);
    return file.open(QIODevice::ReadWrite | QIODevice::Truncate);
}

void StreamCapture::Abort()
{
    if (file.isOpen())
        file.remove();
    total = 0;
    ranges.clear();
}

void StreamCapture::Add(qint64 offset, const QByteArray& bytes)
{
    if (!file.isOpen() || bytes.isEmpty() || !file.seek(offset) || file.write(bytes) != bytes.size())
        return;

    qint64 start = offset;
    qint64 end = offset + bytes.size();

    auto it = ranges.upperBound(start);
    if (it != ranges.begin())
    {
        auto previous = std::prev(it);
        if (previous.value() >= start)
        {
            start = previous.key();
            end = std::max(end, previous.value());
            it = ranges.erase(previous);
        }
    }
    while (it != ranges.end() && it.key() <= end)
    {
        end = std::max(end, it.value());
        it = ranges.erase(it);
    }
    ranges.insert(start, end);
}

bool StreamCapture::IsComplete() const
{
    return total > 0 && ranges.size() == 1 && ranges.firstKey() == 0 && ranges.first() >= total;
}

QString StreamCapture::Take()
{
    const QString path = file.fileName();
    file.close();
    total = 0;
    ranges.clear();
    return path;
}
