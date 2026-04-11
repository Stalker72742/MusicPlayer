#pragma once

#include "IModuleInterface.h"

class QWidget;

class TrayModule : public IModuleInterface, public IPlugin
{
public:
  void StartupModule() override;
  void ShutdownModule() override;
  QString GetModuleName() const override { return QStringLiteral("NewWindows"); }
  void init() override;
  void shutdown() override;
  QString name() const override { return QStringLiteral("NewWindows"); }

};

PLUGIN_API IPlugin* createPlugin() {
  return new TrayModule();
}

PLUGIN_API void destroyPlugin(IPlugin* plugin) {
  delete plugin;
}