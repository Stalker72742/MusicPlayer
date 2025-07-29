#ifndef PLAYLISTSONG_H
#define PLAYLISTSONG_H

#include <QWidget>

namespace Ui {
class playlistSong;
}

class playlistSong : public QWidget
{
    Q_OBJECT

public:
    explicit playlistSong(int index, QString songPath, QWidget *parent = nullptr);
    ~playlistSong();

private:
    Ui::playlistSong *ui;

    int songIndex;
    QString path;
};

#endif // PLAYLISTSONG_H
