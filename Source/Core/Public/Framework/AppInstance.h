//
// Created by Stalker7274 on 17.04.2025.
//
#pragma once

#ifndef APPINSTANCE_H
#define APPINSTANCE_H

#include <QObject>

class SubsystemBase;

class AppInstance : public QObject {
Q_OBJECT
public:

    static AppInstance* getInstance();

    [[nodiscard]] QList<SubsystemBase*> getSubsystems() const { return subsystems; }

    template <typename T>
    T* createSubsystem() {
        subsystems.append(new T(this));
        return qobject_cast<T*>(subsystems.last());
    }

    void addSubsystem(SubsystemBase* subsystem);

    template <typename mw>
    void createApp() {

        mw *window = new mw();
        window->show();
    }

    AppInstance(const AppInstance&) = delete;
    AppInstance& operator=(const AppInstance&) = delete;

private:
    explicit AppInstance(QObject* parent = nullptr);
    ~AppInstance() override;

    QList<SubsystemBase*> subsystems;
    static AppInstance* instance;
};

#endif //APPINSTANCE_H
