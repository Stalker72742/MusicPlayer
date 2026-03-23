#include <QApplication>

#include "Source/Core/Public/Framework/AppInstance.h"
#include "Source/Core/Public/Framework/IModuleInterface.h"
#include "Source/UI/NewWindows/NewWindowsModule.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Инициализируем субсистемы ядра
    AppInstance* appInstance = AppInstance::getInstance();
    //appInstance->addSubsystem(new PlayerSubsystem(...));

    ModuleManager::Get().RegisterModule("NewWindows", []() -> IModuleInterface* {
        return new NewWindowsModule();
    });
    ModuleManager::Get().LoadModule("NewWindows");

    const int result = app.exec();

    ModuleManager::Get().ShutdownAllModules();
    return result;
}