//
// Created by Stalker7274 on 21.11.2025.
//

#ifndef SOUNDLINK_SETTINGSSUBSYSTEM_H
#define SOUNDLINK_SETTINGSSUBSYSTEM_H
#include "Data/SettingData.h"
#include "SubsystemBase.h"


class SettingsSubsystem : public SubsystemBase
{
    Q_OBJECT
public:
    explicit SettingsSubsystem(QObject* parent = nullptr);
    ~SettingsSubsystem() override;

protected:

    QString SettingsDataPath;

protected:

    QMap<QString, SettingData> Settings;

protected:

    virtual void SaveSettings();
    virtual void LoadSettings();

public:

    virtual SettingData FindSetting(const QString& InSettingTag);

};


#endif // SOUNDLINK_SETTINGSSUBSYSTEM_H
