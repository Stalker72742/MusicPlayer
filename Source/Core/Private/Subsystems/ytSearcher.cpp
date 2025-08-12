//
// Created by Stalker7274 on 27.03.2025.
//

#include "ytSearcher.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QUrl>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ytSearcherSub::ytSearcherSub(QObject *parent) {

    process = new QProcess(this);

}

ytSearcherSub::~ytSearcherSub() {
    delete process;
}

void ytSearcherSub::search(const QString &Text) {

    connect(process, &QProcess::finished, this, &ytSearcherSub::searchFinished);

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

void ytSearcherSub::download(QUrl url, QString out) {

    QString outputTemplate = out + "/%(title)s.%(ext)s";

    QString shell = "/system/bin/sh";
    QString program = "sh";
    QStringList arguments;
    arguments << "-c" << "/data/data/com.termux/files/usr/bin/yt-dlp --version";

    QProcess *process = new QProcess(this);

    process->setProgram("sh");
    process->setArguments(arguments);

    process->start();

    connect(process, &QProcess::readyReadStandardOutput, [=]() {
        QByteArray output = process->readAllStandardOutput();
        qDebug() << "yt-dlp:" << output;
    });

    connect(process, &QProcess::readyReadStandardError, [=]() {
        QByteArray error = process->readAllStandardError();
        qDebug() << "yt-dlp error:" << error;
    });
}

void ytSearcherSub::searchFinished(int exitCode, QProcess::ExitStatus exitStatus) {

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QByteArray output = process->readAllStandardOutput();
        parseSearchResults(output);
    } else {
        qCritical() << "yt-dlp failed with exit code:" << exitCode;
        qCritical() << "Error:" << process->readAllStandardError();
    }
}

void ytSearcherSub::parseSearchResults(const QByteArray &data) {

    QList<QByteArray> lines = data.split('\n');

    if (data.isEmpty()) {
        return;
    }

    emit searchResults(QJsonObject(), startParsing);

    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull() || !doc.isObject()) {
            qWarning() << "Invalid JSON:" << line;
            continue;
        }
        QJsonObject video = doc.object();

        const QString title = video["title"].toString();
        const QString url = video["webpage_url"].toString();
        const QString id = video["id"].toString();
        const int duration = video["duration"].toInt();

        qDebug() << "Title:" << title;
        qDebug() << "URL:" << url;
        qDebug() << "ID:" << id;
        qDebug() << "Duration:" << duration << "seconds";
        qDebug() << "------------------------";

        QJsonObject filtredVideo;
        filtredVideo["title"] = title;
        filtredVideo["url"] = url;
        filtredVideo["duration"] = duration;

        emit searchResults(filtredVideo, pending);
    }

    emit searchResults(QJsonObject(), finishParsing);
}
