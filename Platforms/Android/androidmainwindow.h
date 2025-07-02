#ifndef ANDROIDMAINWINDOW_H
#define ANDROIDMAINWINDOW_H

#include <QtWidgets/QMainWindow>

class QVBoxLayout;

namespace Ui {
class androidMainWindow;
}

class androidMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit androidMainWindow(QWidget *parent = nullptr);
    ~androidMainWindow() override;

public slots:

    void searchButtonClicked(bool checked);
    void mediaButtonClicked(bool checked);

private:
    Ui::androidMainWindow *ui;

    QVBoxLayout* mediaLibScrollLayout;
};

#endif // ANDROIDMAINWINDOW_H
