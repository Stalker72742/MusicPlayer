#include "IModuleInterface.h"
#include "AppInstance.h"

#ifdef Q_OS_WIN
    #include <QApplication>
    #include "NewWindowsModule.h"
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    QApplication app(argc, argv);

    // Инициализируем субсистемы ядра
    AppInstance* appInstance = AppInstance::getInstance();
    // appInstance->addSubsystem(new PlayerSubsystem(...));

    // Регистрируем и загружаем модуль — он сам создаст UI внутри StartupModule
    ModuleManager::Get().RegisterModule("NewWindows", []() -> IModuleInterface* {
        return new NewWindowsModule();
    });
    ModuleManager::Get().LoadModule("NewWindows");

    const int result = app.exec();

    ModuleManager::Get().ShutdownAllModules();
    return result;
#endif
}