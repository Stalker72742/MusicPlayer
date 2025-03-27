#include "mainwindow.h"
#include <QHBoxLayout>
#include "musicbutton.h"
#include "PlayerSubsystem.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setGeometry(0, 0, 800, 600);

    player = new PlayerSubsystem(this);

    centralWidget = new QWidget;

    setCentralWidget(centralWidget);

    centralLayout = new QVBoxLayout;

    centralLayout->setContentsMargins(0,0,0,0);

    centralLayout->setSpacing(0);

    centralWidget->setLayout(centralLayout);

    centralWidget->setContentsMargins(0,0,0,0);

    SetupLeftPanel();

    SetupMusicInfo();

    SetupBottomPanel();

    updatePlaylist();
}

void MainWindow::SetupBottomPanel() {

    QWidget* bottomPanel = new QWidget;

    QWidget* bottomButtons = new QWidget;

    bottomButtons->setStyleSheet("background-color: rgb(255, 255, 255);");

    bottomButtonsLayout = new QHBoxLayout();

    volumeSlider = new QSlider(Qt::Horizontal);

    bottomButtons->setLayout(bottomButtonsLayout);

    bottomPanel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

    bottomPanel->setMinimumHeight(75);

    bottomPanel->setContentsMargins(0,0,0,0);

    bottomPanel->setStyleSheet("background-color: black;");

    centralLayout->addWidget(bottomPanel);

    bottomPanelLayout = new QVBoxLayout();

    bottomPanelLayout->setAlignment(bottomButtons, Qt::AlignCenter);

    bottomPanel->setLayout(bottomPanelLayout);

    timeSlider = new QSlider(Qt::Horizontal);

    timeSlider->setMinimumHeight(15);

    bottomPanelLayout->addWidget(timeSlider);

    bottomPanelLayout->addWidget(bottomButtons);

    playButton = new QPushButton("Play");

    previousSongButton = new QPushButton("Previous");

    nextSongButton = new QPushButton("Next");

    playButton->setStyleSheet("background-color: white;");

    previousSongButton->setStyleSheet("background-color: white;");

    nextSongButton->setStyleSheet("background-color: white;");

    playButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    playButton->setMinimumSize(150, 50);

    playButton->setMaximumSize(150, 50);

    QWidget* test = new QWidget();

    test->setStyleSheet("background-color: black;");

    test->setFixedWidth(80);

    bottomButtonsLayout->addWidget(test);

    bottomButtonsLayout->addStretch();

    bottomButtonsLayout->addWidget(previousSongButton);

    bottomButtonsLayout->addWidget(playButton);

    bottomButtonsLayout->addWidget(nextSongButton);

    auto x = new QSpacerItem(15, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

    x->setAlignment(Qt::AlignRight);

    bottomButtonsLayout->addSpacerItem(x);

    bottomButtonsLayout->addWidget(volumeSlider);

    volumeSlider->setFixedWidth(80);

    volumeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    volumeSlider->setMinimum(0);

    volumeSlider->setMaximum(100);

    volumeSlider->setValue(static_cast<int>(player->getVolume() * 100));

    connect(playButton, &QPushButton::clicked, this, &MainWindow::pauseResume);

    connect(previousSongButton, &QPushButton::clicked, this, &MainWindow::previousSong);

    connect(nextSongButton, &QPushButton::clicked, this, &MainWindow::nextSong);

    connect(volumeSlider, &QSlider::valueChanged, this, &MainWindow::setVolume);
}

void MainWindow::SetupLeftPanel() {

    QWidget* leftPanel = new QWidget;

    //leftPanel->setStyleSheet("background-color: black;");

    centralLayout->addWidget(leftPanel);

    QVBoxLayout* leftPanelLayout = new QVBoxLayout;

    leftPanel->setLayout(leftPanelLayout);

    playListScrollArea = new QScrollArea;

    playListLayout = new QVBoxLayout();

    playListScrollArea->setLayout(playListLayout);

    playListScrollArea->setMinimumHeight(0);

    playListScrollArea->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    leftPanelLayout->addWidget(playListScrollArea);

    scrollArea = new QScrollArea;

    leftPanelLayout->addWidget(scrollArea);

    leftPanel->setContentsMargins(0,0,0,0);

    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

    scrollArea->setWidgetResizable(true);

    leftPanel->setMinimumWidth(75);

    leftPanel->setMaximumWidth(180);

    QWidget *window = new QWidget;

    scrollLayout = new QVBoxLayout(window);

    window->setLayout(scrollLayout);

    textEdit = new QTextEdit(this);

    QWidget *searchWidget = new QWidget;

    QHBoxLayout *searchLayout = new QHBoxLayout();

    searchWidget->setLayout(searchLayout);

    scrollLayout->addWidget(searchWidget);

    searchLayout->addWidget(textEdit);

    textEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    textEdit->setMinimumHeight(15);

    textEdit->setMaximumHeight(35);

    searchButton = new QPushButton("S");

    searchLayout->addWidget(searchButton);

    searchButton->setMaximumWidth(20);

    scrollLayout->addStretch();

    window->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    window->adjustSize();

    scrollArea->setWidget(window);
}

void MainWindow::updatePlaylist() {

    QStringList Songs = player->getSongNames();

    for (const QString songName : Songs) {

        playListLayout->addWidget(new MusicButton(playListScrollArea, songName));

    }
}

void MainWindow::SetupMusicInfo() {

}

MainWindow::~MainWindow()
{
    delete player;
}

void MainWindow::pauseResume() {

    bIsPaused = !bIsPaused;

    if (bIsPaused) {

        player->Resume();

    } else {

        player->Pause();

    }
}

void MainWindow::nextSong() const {

    player->NextSong();

}

void MainWindow::previousSong() const {

    player->PreviousSong();

}

void MainWindow::setVolume() const {

    player->SetVolume(static_cast<float>(volumeSlider->value()) / 100);
}
