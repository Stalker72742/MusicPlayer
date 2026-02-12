//
// TestUI - Simple Hello World window
//

#pragma once

#ifndef TESTUI_MAINWINDOW_H
#define TESTUI_MAINWINDOW_H

#include <QMainWindow>

class QLabel;

class TestMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit TestMainWindow(QWidget* parent = nullptr);
    ~TestMainWindow() override = default;

private:
    QLabel* helloLabel;
};

#endif // TESTUI_MAINWINDOW_H
