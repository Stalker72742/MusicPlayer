//
// Created by Stalker7274 on 07.11.2026.
//

#pragma once

#include <QStringList>

#include <memory>
#include <vector>

class AppInstance;

/// @brief An optional part of the app that a build option compiles in, e.g. developer tools.
///
/// The core never names a module. A module's sources are added to the target by its CMake option and register
/// the module with SOUNDLINK_REGISTER_MODULE; AppInstance creates every registered module at the end of init()
/// and gives it the app, from which the module takes what it plugs into (tray menu, main window...).
/// Without the option nothing of the module is built, so nothing of it is left in the app.
class AppModule
{
public:
    using Factory = std::unique_ptr<AppModule> (*)();

    virtual ~AppModule() = default;

    /// @brief Plugs the module into the app. Called once, when the main window and the tray icon exist.
    virtual void Start(AppInstance& app) = 0;

    /// @brief Adds a module to the build's list; used by SOUNDLINK_REGISTER_MODULE before main().
    static bool Register(const char* name, Factory factory);

    /// @brief Creates every registered module, in the order of the names.
    static std::vector<std::unique_ptr<AppModule>> CreateAll();

    /// @brief Names of the modules built into this app.
    static QStringList GetNames();
};

/// @brief Registers a module class (default constructible, derived from AppModule) in its .cpp.
#define SOUNDLINK_REGISTER_MODULE(Type)                                                                               \
    namespace                                                                                                         \
    {                                                                                                                 \
        const bool Type##Registered = AppModule::Register(                                                            \
            #Type, []() -> std::unique_ptr<AppModule> { return std::make_unique<Type>(); });                          \
    }
