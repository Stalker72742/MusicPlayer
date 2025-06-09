//
// Created by Stalker7274 on 03.06.2025.
//

#include "medialibitem.h"
#include "ui_mediaLibItem.h"

#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

#include "AppInstance.h"
#include "AppInstanceLibs.h"

mediaLibItem::mediaLibItem(QWidget *parent, QString playlist) :
    QWidget(parent), ui(new Ui::mediaLibItem) {
    ui->setupUi(this);

    player = AppInstanceLibs::getAppInstance(this)->getSubsystem<PlayerSubsystem>();

    if (!playlist.isEmpty()) {

        playlistLabel = new QLabel();

        playlistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        playlistLabel->setStyleSheet("background-color: rgba(255, 255, 255, 0); color: white");

        playlistLabel->setText(playlist.split('/').last().split('.').first());

        playlistPath = playlist;

        ui->widgetBackground->layout()->addWidget(playlistLabel);

        playlistMenu = new QPushButton("000");

        playlistMenu->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        playlistMenu->setFixedSize(35, 35);

        playlistMenu->setStyleSheet("background-color: rgba(255, 255, 255, 0); color: white");

        ui->widgetBackground->layout()->addWidget(playlistMenu) ;

        QMenu *menu = new QMenu();

        menu->setStyleSheet("color:white; background-color: rgb(32, 32, 32);");

        QAction *remove = menu->addAction("Удалить");

        connect(playlistMenu, &QPushButton::clicked, [=]() {
            menu->exec(playlistMenu->mapToGlobal(QPoint(0, playlistMenu->height())));
        });

        ui->playlistName->deleteLater();

        connect(remove, &QAction::triggered, [=]() {

            this->deleteLater();
        });

        return;
    }

    qApp->installEventFilter(this);

    connect(ui->playlistName, &QLineEdit::editingFinished, [=]() {

        if (ui->playlistName->text().isEmpty()) {

            ui->playlistName->grabKeyboard();
            return;
        }

        qApp->removeEventFilter(this);

        playlistLabel = new QLabel();

        playlistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        playlistLabel->setStyleSheet("background-color: rgba(255, 255, 255, 0); color: white");

        playlistLabel->setText(ui->playlistName->text());

        playlistPath = player->createPlaylist(ui->playlistName->text());

        ui->widgetBackground->layout()->addWidget(playlistLabel);

        playlistMenu = new QPushButton("000");

        playlistMenu->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        playlistMenu->setFixedSize(35, 35);

        playlistMenu->setStyleSheet("background-color: rgba(255, 255, 255, 0); color: white");

        ui->widgetBackground->layout()->addWidget(playlistMenu) ;

        QMenu *menu = new QMenu();

        menu->setStyleSheet("color:white; background-color: rgb(32, 32, 32);");

        QAction *remove = menu->addAction("Удалить");

        connect(playlistMenu, &QPushButton::clicked, [=]() {
            menu->exec(playlistMenu->mapToGlobal(QPoint(0, playlistMenu->height())));
        });

        ui->playlistName->deleteLater();

        connect(remove, &QAction::triggered, [=]() {

            this->deleteLater();
        });

    });
}

mediaLibItem::~mediaLibItem() {
    delete ui;
}

void mediaLibItem::setFocus() {

    ui->playlistName->grabKeyboard();
}

bool mediaLibItem::eventFilter(QObject *watched, QEvent *event) {

    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* w = QApplication::widgetAt(QCursor::pos());
        if (w && w != ui->playlistName && !ui->playlistName->isAncestorOf(w)) {
            qDebug() << "Clicked outside QLineEdit!";

            ui->playlistName->clearFocus();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void mediaLibItem::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);

    if (event->button() == Qt::LeftButton and !playlistPath.isEmpty()) {

        player->setCurrentPlaylist(playlistPath);

    }
}
