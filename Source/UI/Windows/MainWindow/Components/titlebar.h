//
// Created by Stalker7274 on 12.04.2025.
//

#pragma once

#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QMainWindow>
#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class TitleBar; }
QT_END_NAMESPACE

class TitleBar : public QWidget {
Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    explicit TitleBar(QMainWindow* mainWindow);
    ~TitleBar() override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

public slots:

    void exitButtonClicked();

    void minimizeButtonClicked();

    void maximizeButtonClicked();

    void textChanged();

private:
    Ui::TitleBar *ui;

    QPoint dragPosition;

    QWidget *MainWindow;
};


#endif //TITLEBAR_H
