#pragma once

#include "IModuleInterface.h"

class QQmlApplicationEngine;

class NewWindowsModule : public IPlugin
{
protected:

  QQmlApplicationEngine *m_engine {nullptr};

protected:

    void messageReceived(const QString& message);

public:
    void init() override;
    void shutdown() override;
    QString name() const override { return QStringLiteral("WindowsUI"); }
};