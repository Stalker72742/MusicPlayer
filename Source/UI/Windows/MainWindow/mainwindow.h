//
// Created by Stalker7274 on 12.04.2025.
//

#pragma once

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include "ytSearcherSub.h"

class PlayerSubsystem;
class AppInstance;

QT_BEGIN_NAMESPACE
namespace Ui { class mainWindow; }
QT_END_NAMESPACE

class mainWindow : public QMainWindow {
Q_OBJECT

public:
    explicit mainWindow(QObject *Parent = nullptr);
    ~mainWindow() override;

    AppInstance *getAppInstance() const;

    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

public slots:

    void addItemToPlaylist(QJsonObject Json, ESearchResultsState searchResultsState);

    void playPause();

    void playNext() const;

    void playPrevious() const;

    void createPlaylist();

    void updatePlaylistItems();

    void updateTimeSlider(int time) const;

    void drawMediaLib(QList<QString> songs);

private:
    Ui::mainWindow *ui;

    QObject *parent;

    PlayerSubsystem *playerSubsystem;

    bool bPaused = true;
};


#endif //MAINWINDOW_H
