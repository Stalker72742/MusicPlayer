#include <QApplication>

#include "Source/Core/Public/Framework/AppInstance.h"
#include "Source/Core/Public/Framework/IModuleInterface.h"
#include "Source/UI/NewWindows/NewWindowsModule.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    AppInstance* appInstance = AppInstance::getInstance();

    ModuleManager::Get().RegisterModule("NewWindows", []() -> IModuleInterface* {
        return new NewWindowsModule();
    });
    ModuleManager::Get().LoadModule("NewWindows");

    const int result = app.exec();


    ModuleManager::Get().ShutdownAllModules();
    return result;
}