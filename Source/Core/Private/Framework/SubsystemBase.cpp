//
// Created by Stalker7274 on 08.09.2025.
//

#include "SubsystemBase.h"

#include <QCoreApplication>

SubsystemBase::SubsystemBase()
{
    QCoreApplication* app = QCoreApplication::instance();
    Q_ASSERT_X(app, "SubsystemBase", "subsystems can only be used after QApplication is created");

    if (app)
        connect(app, &QCoreApplication::aboutToQuit, this, &SubsystemBase::Deinitialize);
}
