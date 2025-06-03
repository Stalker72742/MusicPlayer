//
// Created by Stalker7274 on 03.06.2025.
//

// You may need to build the project (run Qt uic code generator) to get "ui_mediaLibItem.h" resolved

#include "medialibitem.h"
#include "ui_mediaLibItem.h"

#include <QMenu>
#include <QAction>

mediaLibItem::mediaLibItem(QString playlistPath, QWidget *parent) :
    QWidget(parent), ui(new Ui::mediaLibItem) {
    ui->setupUi(this);

    QMenu *menu = new QMenu();

    menu->setStyleSheet("color:white; background-color: rgb(32, 32, 32);");

    QAction *remove = menu->addAction("Удалить");

    connect(ui->playlistMenuButton, &QPushButton::clicked, [=]() {
        menu->exec(ui->playlistMenuButton->mapToGlobal(QPoint(0, ui->playlistMenuButton->height())));
    });

    connect(remove, &QAction::triggered, [=]() {

        this->deleteLater();
    });

}

mediaLibItem::~mediaLibItem() {
    delete ui;
}
