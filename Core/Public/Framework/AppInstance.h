//
// Created by Stalker7274 on 17.04.2025.
//

#pragma once

#ifndef APPINSTANCE_H
#define APPINSTANCE_H

#include "PlayerSubsystem.h"

class AppInstance : public QObject {
Q_OBJECT
public:

    static AppInstance* getInstance();

    template <typename mw>
    void createApp() {

        mw *window = new mw();
        window->show();
    };

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

    AppInstance(const AppInstance&) = delete;
    AppInstance& operator=(const AppInstance&) = delete;

private:
    explicit AppInstance(QObject* parent = nullptr);
    ~AppInstance() override;

    QList<QObject*> subsystems;
    static AppInstance* instance;
};

#endif //APPINSTANCE_H
