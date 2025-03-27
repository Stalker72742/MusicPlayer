//
// Created by Stalker7274 on 27.03.2025.
//

#include "ytSearcherSub.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

ytSearcherSub::ytSearcherSub(QObject *parent) {
    process = new QProcess();


}

ytSearcherSub::~ytSearcherSub() {
    delete process;
}

void ytSearcherSub::search(const QString &Text) {

    qDebug() << Text;

    connect(process, &QProcess::finished, this, &ytSearcherSub::processFinished);

    QStringList arguments;

    if (Text.isEmpty()) {
        qDebug() << "Empty query";
        return;
    }

    arguments << "ytsearch10:" + Text
              << "--dump-json" << "--no-playlist" << "--quiet" << "--ignore-errors" << "--flat-playlist";

    process->setProcessChannelMode(QProcess::MergedChannels);
    process->start("yt-dlp", arguments);
}

void ytSearcherSub::download(QUrl url) {

}

void ytSearcherSub::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QByteArray output = process->readAllStandardOutput();
        parseResults(output);
    } else {
        qCritical() << "yt-dlp failed with exit code:" << exitCode;
        qCritical() << "Error:" << process->readAllStandardError();
    }
}

void ytSearcherSub::parseResults(const QByteArray &data) {

    QList<QByteArray> lines = data.split('\n');

    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull() || !doc.isObject()) {
            qWarning() << "Invalid JSON:" << line;
            continue;
        }

        QJsonObject video = doc.object();

        QString title = video["title"].toString();
        QString url = video["webpage_url"].toString();
        QString id = video["id"].toString();
        const int duration = video["duration"].toInt();

        qDebug() << "Title:" << title;
        qDebug() << "URL:" << url;
        qDebug() << "ID:" << id;
        qDebug() << "Duration:" << duration << "seconds";
        qDebug() << "------------------------";

        QJsonObject filtredVideo;
        filtredVideo["title"] = title;
        filtredVideo["url"] = url;

    }
}
