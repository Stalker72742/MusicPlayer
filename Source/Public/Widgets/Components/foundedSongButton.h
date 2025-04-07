//
// Created by Stalker7274 on 27.03.2025.
//

#ifndef FOUNDEDSONGBUTTON_H
#define FOUNDEDSONGBUTTON_H
#include <QJsonObject>
#include <QPushButton>
#include <QWidget>

class foundedSongButton : public QWidget {
Q_OBJECT

public:
    explicit foundedSongButton(QWidget *parent = nullptr);

    explicit foundedSongButton(QWidget *parent = nullptr, const QJsonObject Json = QJsonObject());

    ~foundedSongButton() override;

    void setupButtonWidgets();

private:

    void OnClicked();

    QPushButton *button;

    QJsonObject info;

signals:

    void clicked(QJsonObject json);
};



#endif //FOUNDEDSONGBUTTON_H
