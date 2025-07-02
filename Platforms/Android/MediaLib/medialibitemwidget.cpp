//
// Created by Stalker7274 on 02.07.2025.
//

// You may need to build the project (run Qt uic code generator) to get "ui_medialibItemWidget.h" resolved

#include "medialibitemwidget.h"
#include "ui_medialibItemWidget.h"
#include "../../../../Qt/6.8.3/mingw_64/include/QtGui/QMouseEvent"


medialibItemWidget::medialibItemWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::medialibItemWidget) {
    ui->setupUi(this);
}

medialibItemWidget::medialibItemWidget(const QString &playlistName, const QString &playlistDescript, QWidget *parent) : QWidget(parent)
    , ui(new Ui::medialibItemWidget)
{
    ui->setupUi(this);

    ui->playlistName->setText(playlistName);
    ui->playlistDescript->setText(playlistDescript);
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
