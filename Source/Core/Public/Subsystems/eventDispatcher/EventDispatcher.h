//
// Created by Stalker7274 on 01.07.2026.
//
#pragma once

#include "SubsystemBase.h"

#ifdef MYLIB_BUILD
#  define MYLIB_EXPORT Q_DECL_EXPORT
#else
#  define MYLIB_EXPORT Q_DECL_IMPORT
#endif

class MYLIB_EXPORT eventDisp : public SubsystemBase {
    Q_OBJECT
public:

    explicit eventDisp(QObject *parent = nullptr) : SubsystemBase(parent) {}

    virtual void sendMessage(const QString& message);

signals:

    void messageReceived(const QString& message);
};
