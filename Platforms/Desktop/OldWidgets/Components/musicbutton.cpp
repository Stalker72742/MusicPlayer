//
// Created by Stalker7274 on 22.03.2025.
//

#include "musicbutton.h"

#include <QDir>
#include <QProcess>
#include <QVBoxLayout>


MusicButton::MusicButton(QWidget *parent) :
    QWidget(parent){

    button = new QPushButton(this);

    QVBoxLayout *layout = new QVBoxLayout;

    layout->addWidget(button);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    setMaximumHeight(100);

    setMinimumHeight(50);
}

MusicButton::MusicButton(QWidget *parent, QJsonObject video) {

    button = new QPushButton(video["title"].toString(),this);

    connect(button,SIGNAL(clicked()),this,SLOT(OnClicked()));

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    setMaximumHeight(100);

    setMinimumHeight(50);

    musicPath.Url = video["url"].toString();

    const QString url = video["url"].toString();

    QProcess *process = new QProcess(this);

    QStringList args;

    args << "--get-url"
     << "--no-playlist"
     << "--no-warnings"
     << "--ignore-no-formats"
     << "--no-warnings"
     << "-f"
     << "bestaudio"
     << "--user-agent"
     << "Mozilla/5.0"
     << url;

    process->setProcessChannelMode(QProcess::MergedChannels);

    process->start("yt-dlp", args);

    connect(process, &QProcess::finished, this, [this, process, video](int exitCode) {
        if (exitCode == 0) {
            video["url"] = QString(process->readAllStandardOutput()).trimmed();

            qDebug() << "Done";
        }

        bInited = true;

        process->deleteLater();
    });
}

MusicButton::MusicButton(QWidget *parent, QString musicPathLoc) {

    if (true) {

        bHasLocalFile = true;

        musicPath.LocalPath = musicPathLoc;

        button = new QPushButton(musicPath.LocalPath.split("/").last(),this);

        QVBoxLayout *layout = new QVBoxLayout;

        layout->addWidget(button);

        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        setMaximumHeight(100);

        setMinimumHeight(50);
    }
}

MusicButton::~MusicButton() {

    delete button->layout();

    delete button;
}

void MusicButton::OnClicked() {

    if (bInited) {

        SignalClicked(musicPath);
    }
}
