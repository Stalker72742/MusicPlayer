//
// Created by Stalker7274 on 21.11.2025.
//

#include "SettingsSubsystem.h"
#include <QDir>

SettingsSubsystem::SettingsSubsystem(QObject* parent)
{
    SettingsDataPath = QDir::currentPath() + "/Saved/SettingsData"; // TODO move to static data and make function ::GetSavedDataFolder

    SettingsSubsystem::LoadSettings();
}

void SettingsSubsystem::LoadSettings()
{
    QFile file(SettingsDataPath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "SettingsSubsystem::LoadSettings: Error opening file:" << file.errorString();
        return;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    qint32 count;
    stream >> count;

    Settings.clear();

    for (qint32 i = 0; i < count; ++i)
    {
        QString key;
        SettingData value;

        stream >> key;
        stream >> value;

        Settings.insert(key, value);
    }

    file.close();
}

SettingsSubsystem::~SettingsSubsystem()
{
    SettingsSubsystem::SaveSettings();
}

void SettingsSubsystem::SaveSettings()
{
    QFile file(SettingsDataPath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "SettingsSubsystem::SaveSettings: Error opening file";
        return;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<qint32>(Settings.size());

    for (const auto& [key, value] : Settings.asKeyValueRange())
    {
        stream << key;
        stream << value;
    }

    file.close();
}

SettingData SettingsSubsystem::FindSetting(const QString& InSettingTag)
{
    if (Settings.contains(InSettingTag))
    {
        return Settings[InSettingTag];
    }

    return {};
}
