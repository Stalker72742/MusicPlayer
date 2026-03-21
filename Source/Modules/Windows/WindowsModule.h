#pragma once

#include "IModuleInterface.h"

// Windows platform module — точка входа для Windows сборки
// StartupModule: регистрирует субсистемы + инициализирует UI Plugin System
// ShutdownModule: чистит за собой
//
// UI создаётся либо автоматически через AppInstance::createUIFromConfig()
// либо снаружи руками — на твой выбор
class WindowsModule : public IModuleInterface
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
    QString GetModuleName() const override { return QStringLiteral("Windows"); }
};
