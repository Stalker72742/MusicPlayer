//
// Created by Stalker7274 on 22.03.2025.
//

#include "musicbutton.h"

#include <QDir>
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
}

MusicButton::MusicButton(QWidget *parent, QString musicPathLoc) {

    if (true) {

        hasLocalFile = true;
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

}

void MusicButton::OnClicked() {

    SignalClicked(musicPath);
}
