//
// Created by Stalker7274 on 19.04.2025.
//

#pragma once

#ifndef APPINSTANCELIBS_H
#define APPINSTANCELIBS_H
#include <QObject>


class AppInstance;

class AppInstanceLibs {

public:

    static AppInstance* getAppInstance(QObject *Object);
};


#endif //APPINSTANCELIBS_H
