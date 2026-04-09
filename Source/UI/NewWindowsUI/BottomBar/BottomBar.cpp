//
// Created by Stalker7274 on 02.12.2025.
//

#include "BottomBar.h"
#include "PlayerSubsystem.h"
#include "SubsystemBase.h"
#include "Player/SubObjects/song.h"

#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

BottomBar::BottomBar(QWidget *parent) : QWidget(parent)
{
    // Получаем PlayerSubsystem
    playerSubsystem = SubsystemBase::GetSubsystem<PlayerSubsystem>();

    auto mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(13, 0, 13, 0);

    // === ЛЕВАЯ ЧАСТЬ (инфо о треке) ===
    auto trackInfoWidget = new QWidget(this);
    trackInfoWidget->setFixedWidth(200);
    auto trackInfoLayout = new QHBoxLayout(trackInfoWidget);
    trackInfoLayout->setContentsMargins(0, 0, 0, 0);

    auto albumCover = new QWidget(this);
    albumCover->setStyleSheet("background-color: #757575; border-radius: 8px");
    albumCover->setFixedSize(47, 47);

    auto trackTextLayout = new QVBoxLayout();
    trackTextLayout->setSpacing(0);
    trackTextLayout->setContentsMargins(8, 0, 0, 0);

    trackTitle = new QLabel("Song name");
    trackArtist = new QLabel("Song author");
    trackTitle->setStyleSheet("color: #fff; font-size: 15px;");
    trackArtist->setStyleSheet("color: #AAAAAA; font-size: 13px;");

    trackTextLayout->addWidget(trackTitle);
    trackTextLayout->addWidget(trackArtist);

    trackInfoLayout->addWidget(albumCover);
    trackInfoLayout->addLayout(trackTextLayout);
    trackInfoLayout->addStretch();

    // === ЦЕНТР (контролы плеера) ===
    auto playerControlsLayout = new QVBoxLayout();
    playerControlsLayout->setAlignment(Qt::AlignCenter);

    auto transportLayout = new QHBoxLayout();
    transportLayout->setAlignment(Qt::AlignCenter);

    shuffleBtn = new QPushButton("Mix");
    prevBtn = new QPushButton("Prev");
    playBtn = new QPushButton("Play");
    nextBtn = new QPushButton("Next");
    repeatBtn = new QPushButton("Loop");

    QString btnStyle = "background-color: #fff; color: #000;";
    shuffleBtn->setStyleSheet(btnStyle);
    prevBtn->setStyleSheet(btnStyle);
    playBtn->setStyleSheet(btnStyle);
    nextBtn->setStyleSheet(btnStyle);
    repeatBtn->setStyleSheet(btnStyle);

    transportLayout->addWidget(shuffleBtn);
    transportLayout->addWidget(prevBtn);
    transportLayout->addWidget(playBtn);
    transportLayout->addWidget(nextBtn);
    transportLayout->addWidget(repeatBtn);

    auto seekLayout = new QHBoxLayout();
    currentTime = new QLabel("0:00");
    totalTime = new QLabel("0:00");
    seekSlider = new QSlider(Qt::Horizontal);
    seekSlider->setFixedWidth(400);

    QString sliderStyle = R"(
        QSlider::groove:horizontal {
            height: 4px;
            background: #505050;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -6px 0;
            background: white;
            border: 1px solid #cccccc;
            border-radius: 8px;
        }
        QSlider::sub-page:horizontal {
            background: #FFFFFF;
            border-radius: 2px;
        }
    )";

    seekSlider->setStyleSheet(sliderStyle);
    currentTime->setStyleSheet("color: #fff;");
    totalTime->setStyleSheet("color: #fff;");

    seekLayout->addWidget(currentTime);
    seekLayout->addWidget(seekSlider);
    seekLayout->addWidget(totalTime);

    playerControlsLayout->addLayout(transportLayout);
    playerControlsLayout->addLayout(seekLayout);

    // === ПРАВАЯ ЧАСТЬ (громкость и прочее) ===
    auto extrasWidget = new QWidget(this);
    extrasWidget->setFixedWidth(200);
    auto extrasLayout = new QHBoxLayout(extrasWidget);
    extrasLayout->setContentsMargins(0, 0, 0, 0);

    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setFixedWidth(100);
    volumeSlider->setStyleSheet(sliderStyle);

    extrasLayout->addStretch();
    extrasLayout->addWidget(volumeSlider);

    // === СБОРКА ===
    mainLayout->addWidget(trackInfoWidget);
    mainLayout->addStretch();
    mainLayout->addLayout(playerControlsLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(extrasWidget);

    // === ПОДКЛЮЧЕНИЕ К PLAYERSUBSYSTEM ===
    if (playerSubsystem) {
        // Подключаем сигналы от PlayerSubsystem
        connect(playerSubsystem, &PlayerSubsystem::updateMusicDuration, this, &BottomBar::updateTimeSlider);
        connect(playerSubsystem, &PlayerSubsystem::OnSongChanged, this, &BottomBar::onSongChanged);

        // Подключаем кнопки управления
        connect(playBtn, &QPushButton::clicked, this, &BottomBar::playPause);
        connect(nextBtn, &QPushButton::clicked, this, &BottomBar::playNext);
        connect(prevBtn, &QPushButton::clicked, this, &BottomBar::playPrevious);

        // Подключаем слайдер громкости
        connect(volumeSlider, &QSlider::valueChanged, playerSubsystem, &PlayerSubsystem::SetVolume);
        volumeSlider->setValue(playerSubsystem->getVolume());
    }
}

BottomBar::~BottomBar()
{
}

void BottomBar::playPause()
{
    if (!playerSubsystem) return;

    if (bPaused) {
        playerSubsystem->Resume();
        bPaused = false;
        playBtn->setText("Pause");
    } else {
        playerSubsystem->Pause();
        bPaused = true;
        playBtn->setText("Play");
    }
}

void BottomBar::playNext()
{
    if (!playerSubsystem) return;
    playerSubsystem->NextSong();
}

void BottomBar::playPrevious()
{
    if (!playerSubsystem) return;
    playerSubsystem->PreviousSong();
}

void BottomBar::updateTimeSlider(int time)
{
    if (!playerSubsystem) return;

    const qint64 maxDuration = playerSubsystem->getMaxDuration() / 1000;

    seekSlider->setValue(static_cast<int>(static_cast<float>(time) / static_cast<float>(maxDuration) * 100.0));

    const qint64 minutes = time / 60;
    const qint64 seconds = time % 60;

    const QString str = QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));

    currentTime->setText(str);

    // Обновляем общее время
    const qint64 totalMinutes = maxDuration / 60;
    const qint64 totalSeconds = maxDuration % 60;
    const QString totalStr = QString("%1:%2")
        .arg(totalMinutes, 2, 10, QChar('0'))
        .arg(totalSeconds, 2, 10, QChar('0'));

    totalTime->setText(totalStr);
}

void BottomBar::onSongChanged(song* Song)
{
    if (!Song) return;

    trackTitle->setText(Song->getName());
    trackArtist->setText("Unknown Artist");
}
