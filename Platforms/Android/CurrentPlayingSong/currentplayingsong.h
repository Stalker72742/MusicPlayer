#ifndef CURRENTPLAYINGSONG_H
#define CURRENTPLAYINGSONG_H

#include <QWidget>

class song;

namespace Ui {
class currentPlayingSong;
}

class currentPlayingSong : public QWidget
{
    Q_OBJECT

public:
    explicit currentPlayingSong(QWidget *parent = nullptr);
    ~currentPlayingSong();

    void setSong(song* newSong);

private:
    Ui::currentPlayingSong *ui;

    song* currentSong;

    bool paused = false;
};

#endif // CURRENTPLAYINGSONG_H
