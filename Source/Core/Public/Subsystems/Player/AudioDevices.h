//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class AudioDeviceNotifier;

/// @brief The audio outputs of the system, and word when they change.
class AudioDevices : public QObject
{
    Q_OBJECT

public:
    /// @brief An audio output.
    struct Device
    {
        QByteArray id; ///< Endpoint id, as kept in AppConfigs::SettingsKeys::OutputDevice.
        QString name;  ///< Name for the UI.
    };

    /// @brief Every active output.
    static QList<Device> List();
    /// @brief The endpoint a saved id means now: that device if present, else the system default.
    /// @param wanted Endpoint id; empty for the system default.
    /// @return Endpoint id; empty if there is no output at all.
    static QByteArray Resolve(const QByteArray& wanted);

    explicit AudioDevices(QObject* parent = nullptr);
    ~AudioDevices() override;

signals:
    /// @brief A device came or went, or the system default changed.
    void changed();

private:
    AudioDeviceNotifier* notifier = nullptr;
};
