#ifndef ANDROIDMAINWINDOW_H
#define ANDROIDMAINWINDOW_H

#include <QtWidgets/QMainWindow>

namespace Ui {
class androidMainWindow;
}

class androidMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit androidMainWindow(QWidget *parent = nullptr);
    ~androidMainWindow();

private:
    Ui::androidMainWindow *ui;
};

#endif // ANDROIDMAINWINDOW_H
