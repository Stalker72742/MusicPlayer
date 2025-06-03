//
// Created by Stalker7274 on 03.06.2025.
//
#pragma once

#include <QApplication>
#include <qcoreevent.h>
#include <QLineEdit>
#include <QObject>

#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H


class MyEventFilter : public QObject {
    QLineEdit* target;

public:
    MyEventFilter(QLineEdit* edit) : target(edit) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            QWidget* w = QApplication::widgetAt(QCursor::pos());
            if (w && w != target && !target->isAncestorOf(w)) {
                qDebug() << "Clicked outside QLineEdit!";

                target->clearFocus();
            }
        }
        return QObject::eventFilter(obj, event);
    }
};



#endif //EVENTHANDLER_H
