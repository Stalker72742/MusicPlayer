#pragma once

#include "IModuleInterface.h"

class QWidget;

class NewWindowsModule : public IModuleInterface, public IPlugin
{
public:
  void StartupModule() override;
  void ShutdownModule() override;
  QString GetModuleName() const override { return QStringLiteral("NewWindows"); }
  void init() override;
  void shutdown() override;
  QString name() const override { return QStringLiteral("NewWindows"); }

private:
  QWidget* m_window {nullptr};
};

PLUGIN_API IPlugin* createPlugin() {
  return new NewWindowsModule();
}

PLUGIN_API void destroyPlugin(IPlugin* plugin) {
  delete plugin;
}