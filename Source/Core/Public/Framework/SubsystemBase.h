//
// Created by Stalker7274 on 08.09.2025.
//

#ifndef MUSICPLAYER_SUBSYSTEMBASE_H
#define MUSICPLAYER_SUBSYSTEMBASE_H

#include <QDebug>
#include <QObject>

/// @brief Non-template base of every subsystem: hooks Deinitialize() to application shutdown.
class SubsystemBase : public QObject
{
    Q_OBJECT

protected:
    /// @brief Connects Deinitialize() to QCoreApplication::aboutToQuit. Requires a QCoreApplication.
    SubsystemBase();

    /// @brief Called on QCoreApplication::aboutToQuit.
    ///
    /// Release anything that must not outlive the application here (network, processes, pending saves):
    /// the subsystem itself is destroyed only at static destruction, after QApplication.
    virtual void Deinitialize() {}
};

/// @brief Singleton subsystem (CRTP, Meyers singleton), created on the first Get().
///
/// Usage:
/// @code
/// class MySubsystem : public Subsystem<MySubsystem>
/// {
///     Q_OBJECT
///     friend class Subsystem<MySubsystem>;
///     MySubsystem();
/// };
///
/// MySubsystem::Get().DoSomething();
/// @endcode
/// @tparam T The subsystem class itself.
template <typename T>
class Subsystem : public SubsystemBase
{
public:
    /// @brief The single instance, created on first use (only after QApplication).
    static T& Get()
    {
        static T instance;
        return instance;
    }

protected:
    Subsystem() = default;
};

#endif // MUSICPLAYER_SUBSYSTEMBASE_H
