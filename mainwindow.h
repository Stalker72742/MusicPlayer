#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QSlider>
#include "PlayerSubsystem.h"
#include "ytSearcherSub.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

private:

    bool bIsPaused = false;

    PlayerSubsystem* player;

    ytSearcherSub* ytSearcher;

    QScrollArea *scrollArea;

    QScrollArea *playListScrollArea;

    QVBoxLayout *playListLayout;

    QVBoxLayout *scrollLayout;

    QTextEdit *textEdit;

    QPushButton *searchButton;

    QWidget *centralWidget;

    QVBoxLayout *centralLayout;

    QVBoxLayout *bottomPanelLayout;

    QHBoxLayout *bottomButtonsLayout;

    QPushButton *playButton;

    QPushButton *nextSongButton;

    QPushButton *previousSongButton;

    QSlider *volumeSlider;

    QSlider *timeSlider;


//Functions for creating widgets
    void SetupBottomPanel();

    void SetupMusicInfo();

    void SetupLeftPanel();

    void updatePlaylist();

private slots:

    void pauseResume();

    void nextSong() const;

    void previousSong() const;

    void setVolume() const;

    void search() const;

    void onSearchResults(QJsonObject json);

    void OnFoundedButtonClicked(QJsonObject json);

};
#endif // MAINWINDOW_H
