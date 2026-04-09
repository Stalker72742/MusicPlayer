//
// Created by Stalker7274 on 02.12.2025.
//

#ifndef SOUNDLINK_BOTTOMBAR_H
#define SOUNDLINK_BOTTOMBAR_H

#include <QWidget>

class PlayerSubsystem;
class QLabel;
class QPushButton;
class QSlider;
class song;

class BottomBar : public QWidget
{
    Q_OBJECT

public:
    explicit BottomBar(QWidget *parent = nullptr);
    ~BottomBar() override;

public slots:
    void playPause();
    void playNext();
    void playPrevious();
    void updateTimeSlider(int time);
    void onSongChanged(song* Song);

private:
    PlayerSubsystem* playerSubsystem;

    // UI элементы
    QLabel* trackTitle;
    QLabel* trackArtist;
    QLabel* currentTime;
    QLabel* totalTime;

    QPushButton* shuffleBtn;
    QPushButton* prevBtn;
    QPushButton* playBtn;
    QPushButton* nextBtn;
    QPushButton* repeatBtn;

    QSlider* seekSlider;
    QSlider* volumeSlider;

    bool bPaused = true;
};

#endif // SOUNDLINK_BOTTOMBAR_H
