//
// Created by Stalker7274 on 24.09.2026.
//

#include "AppConfigs.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QStringList>

#include <QTomlUtils/QTomlUtils.h>

QString AppConfigs::GetAppRoot()
{
    return QCoreApplication::applicationDirPath();
}

void AppConfigs::RegisterAppConfigs()
{
    const QString appRoot = GetAppRoot();

    if (!QTomlUtils::RegisterConfig(Settings, appRoot + "/Saved/Settings.toml", appRoot + "/Config/DefaultSettings.toml"))
    {
        qCritical() << "[AppConfigs] Failed to register config" << Settings;
        return;
    }

    QStringList musicFolders;

    const QString systemMusic = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (!systemMusic.isEmpty())
        musicFolders.append(systemMusic);

    musicFolders.append(appRoot + "/Music");

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::MusicScanFolders, musicFolders);
}
