#pragma once

#include "IModuleInterface.h"

class QWidget;

class NewWindowsModule : public IModuleInterface
{
public:
  void StartupModule() override;
  void ShutdownModule() override;
  QString GetModuleName() const override { return QStringLiteral("NewWindows"); }

private:
  QWidget* m_window = nullptr;
};