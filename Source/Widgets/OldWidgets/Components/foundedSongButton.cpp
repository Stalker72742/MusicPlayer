//
// Created by Stalker7274 on 27.03.2025.
//

#include "foundedSongButton.h"

#include <QJsonObject>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

foundedSongButton::foundedSongButton(QWidget *parent) {

    setupButtonWidgets();
}

foundedSongButton::foundedSongButton(QWidget *parent, const QJsonObject Json) {

    info = Json;

    setupButtonWidgets();
}

foundedSongButton::~foundedSongButton() {

}

void foundedSongButton::setupButtonWidgets() {

    if (info.isEmpty()) {

        button = new QPushButton("Empty", this);

    }else {

        button = new QPushButton(info["title"].toString(), this);
    }

    connect(button, &QPushButton::clicked, this, &foundedSongButton::OnClicked);

    QVBoxLayout *layout = new QVBoxLayout;

    layout->addWidget(button);

    setMaximumHeight(100);

    setMinimumHeight(50);
}

void foundedSongButton::OnClicked() {
    emit clicked(info);
}
