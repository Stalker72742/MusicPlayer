//
// Created by Stalker7274 on 05.11.2025.
//

#ifndef SOUNDLINK_STATICDATA_H
#define SOUNDLINK_STATICDATA_H
#include <QtWidgets/QWidget>

#include "SubsystemBase.h"


class staticData : public SubsystemBase
{
    Q_OBJECT
public:

    staticData(QObject* Parent);

    static QString mediaLibFolder;
    static QString allMediaConfigName;
};


#endif // SOUNDLINK_STATICDATA_H
