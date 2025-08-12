#ifndef ANDROIDMAINWINDOW_H
#define ANDROIDMAINWINDOW_H

#include <QtWidgets/QMainWindow>

class QVBoxLayout;
class currentPlayingSong;
class song;

namespace Ui {
class androidMainWindow;
}

class androidMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit androidMainWindow(QWidget *parent = nullptr);
    ~androidMainWindow() override;

    void playingNewSong(song* song);

public slots:

    void searchButtonClicked(bool checked);
    void mediaButtonClicked(bool checked);

    void playlistSelected();
    void songClicked();

    void searchLineEditFinished();

private:
    Ui::androidMainWindow *ui;
    QVBoxLayout* mediaLibScrollLayout;
    currentPlayingSong* currentSongWidget = nullptr;

    QString currentPlaylistPath;
};

#endif // ANDROIDMAINWINDOW_H
