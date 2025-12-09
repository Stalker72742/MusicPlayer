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
protected:
    explicit AppInstance();
    ~AppInstance() override;

protected:

    //virtual void Init();

public:

    static AppInstance* getInstance();

    QList<SubsystemBase*> getSubsystems() const { return subsystems; }

    template <typename T>
    T* createSubsystem() {
        subsystems.append(new T(this));
        return qobject_cast<T*>(subsystems.last());
    }

    template <typename T>
    T* getSubsystem(){
        for(SubsystemBase* subsystem : subsystems){
            if(subsystem){
                if(T* castedSubsys = qobject_cast<T*>(subsystem)){
                    return castedSubsys;
                }
            }
        }

        return nullptr;
    }

    void addSubsystem(SubsystemBase* subsystem);

    template <typename mw>
    void createApp() {

        mw *window = new mw();
        window->show();
    }

    AppInstance(const AppInstance&) = delete;
    AppInstance& operator=(const AppInstance&) = delete;

protected:

    QList<SubsystemBase*> subsystems;
    static AppInstance* instance;
};

#endif //APPINSTANCE_H
