//
// Created by Stalker7274 on 02.07.2025.
//

// You may need to build the project (run Qt uic code generator) to get "ui_medialibItemWidget.h" resolved

#include "medialibitemwidget.h"

#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "ui_medialibItemWidget.h"
#include <QFile>
#include <QMouseEvent>

medialibItemWidget::medialibItemWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::medialibItemWidget) {
    ui->setupUi(this);
}

medialibItemWidget::medialibItemWidget(const QString& pathToPlaylist, const QString &playlistDescript, QWidget *parent) : QWidget(parent)
    , ui(new Ui::medialibItemWidget)
{
    ui->setupUi(this);

    if (!QDir(pathToPlaylist).exists()) {

        QFile file(pathToPlaylist);

        QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();

        ui->playlistName->setText(pathToPlaylist);
        ui->playlistDescript->setText("All songs : " + QString::number(json.keys().size()) + " songs");
        bJsonPlaylist = true;
    }else {

        int songs = 0;
        QDirIterator it(pathToPlaylist, {"*.mp3"}, QDir::Files, QDirIterator::Subdirectories );
        qDebug() << "Trying to find music in " << pathToPlaylist;

        while (it.hasNext()) {
            songs++;
            qDebug() << "Founded " << it.next();
        }

        ui->playlistName->setText("Media lib");
        ui->playlistDescript->setText("All songs : " + QString::number(songs) + " songs");

        bJsonPlaylist = false;
    }

    playlistPath = pathToPlaylist;
}

medialibItemWidget::~medialibItemWidget() {
    delete ui;
}

void medialibItemWidget::mousePressEvent(QMouseEvent *event) {
    if (event->type() == QEvent::TouchBegin or event->type() == QEvent::MouseButtonPress) {
        emit clicked(this);
    }

    QWidget::mousePressEvent(event);
}
