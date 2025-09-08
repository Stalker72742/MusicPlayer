//
// Created by Stalker7274 on 08.09.2025.
//

#ifndef MUSICPLAYER_SUBSYSTEMBASE_H
#define MUSICPLAYER_SUBSYSTEMBASE_H

#include "AppInstance.h"

class PlayerSubsystem;

class SubsystemBase : public QObject
{
    Q_OBJECT
public:

    explicit SubsystemBase(QObject* parent = nullptr);

    template <typename T>
    static T* GetSubsystem() {
        for (auto subsystem : AppInstance::getInstance()->getSubsystems()) {
            if (T* var = qobject_cast<T*>(subsystem)) {
                return var;
            }
        }

        return AppInstance::getInstance()->createSubsystem<T>();
    }
};

#endif // MUSICPLAYER_SUBSYSTEMBASE_H
