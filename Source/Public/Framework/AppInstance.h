//
// Created by Stalker7274 on 17.04.2025.
//

#pragma once

#ifndef APPINSTANCE_H
#define APPINSTANCE_H

#include "mainwindow.h"
#include "PlayerSubsystem.h"

class AppInstance : public QObject {
Q_OBJECT
public:
    explicit AppInstance(QObject *parent = nullptr);

    ~AppInstance() override;

    void createApp();

    template <typename T>
    T* getSubsystem() {

        foreach(QObject *subsystem, subsystems) {

            if (T* result = qobject_cast<T*>(subsystem)) {

                return result;
            }
        }

        subsystems.append(new T());

        return qobject_cast<T*>(subsystems.last());
    };

    QMainWindow *MainWindow;

private:

    QList<QObject*> subsystems;
};

#endif //APPINSTANCE_H
