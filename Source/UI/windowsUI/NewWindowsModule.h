#pragma once

#include "IModuleInterface.h"

class QQmlApplicationEngine;

class NewWindowsModule : public IPlugin
{
protected:

  QQmlApplicationEngine *m_engine {nullptr};

public:
    void init() override;
    void shutdown() override;
    QString name() const override { return QStringLiteral("WindowsUI"); }

};

PLUGIN_API IPlugin* createPlugin() {
  return new NewWindowsModule();
}

PLUGIN_API void destroyPlugin(IPlugin* plugin) {
  delete plugin;
}