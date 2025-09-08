//
// Created by Stalker7274 on 12.04.2025.
//

// You may need to build the project (run Qt uic code generator) to get "ui_TitleBar.h" resolved

#include "titlebar.h"

#include <QMouseEvent>

#include "AppInstance.h"
#include "SubsystemBase.h"
#include "ui_TitleBar.h"
#include "ytSearcher.h"

TitleBar::TitleBar(QWidget *parent) :
    QWidget(parent), ui(new Ui::TitleBar) {

    ui->setupUi(this);

    connect(ui->Close, &QPushButton::clicked, this, &TitleBar::exitButtonClicked);
    connect(ui->Minimize, &QPushButton::clicked, this, &TitleBar::minimizeButtonClicked);
    connect(ui->Maximize, &QPushButton::clicked, this, &TitleBar::maximizeButtonClicked);
}

TitleBar::TitleBar(QMainWindow *mainWindow) :
    QWidget(mainWindow), ui(new Ui::TitleBar){

    ui->setupUi(this);

    MainWindow = mainWindow;

    connect(ui->Close, &QPushButton::clicked, this, &TitleBar::exitButtonClicked);
    connect(ui->Minimize, &QPushButton::clicked, this, &TitleBar::minimizeButtonClicked);
    connect(ui->Maximize, &QPushButton::clicked, this, &TitleBar::maximizeButtonClicked);

    connect(ui->searchLineEdit, &QLineEdit::editingFinished, this, &TitleBar::textChanged);
}

TitleBar::~TitleBar() {
    delete ui;
}

void TitleBar::mousePressEvent(QMouseEvent *event) {

    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPosition().toPoint() - MainWindow->frameGeometry().topLeft();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {

        MainWindow->move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void TitleBar::exitButtonClicked() {
    QApplication::quit();
}

void TitleBar::minimizeButtonClicked() {
    if (QWidget *window = this->window()) {
        window->showMinimized();
    }
}

void TitleBar::maximizeButtonClicked() {
    if (QWidget *window = this->window()) {
        if (window->isMaximized()) {
            window->showNormal();
        } else {
            window->showMaximized();
        }
    }
}

void TitleBar::textChanged() {

    qDebug() << ui->searchLineEdit->text();

    //ytSearcherSub* x = SubsystemBase::GetSubsystem<ytSearcherSub>();

    //if (x) {

      //  x->search(ui->searchLineEdit->text());
    //}
}
