//
// Created by Stalker7274 on 01.07.2026.
//

#include "EventDispatcher.h"

void eventDisp::sendMessage(const QString &message)
{
    emit messageReceived(message);
}
