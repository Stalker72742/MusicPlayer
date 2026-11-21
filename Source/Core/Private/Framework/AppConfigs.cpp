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

bool AppConfigs::GetBool(const QString& key, bool fallback)
{
    return QTomlUtils::FindPropertyValue<bool>(Settings, key).value_or(fallback);
}

double AppConfigs::GetDouble(const QString& key, double fallback)
{
    return QTomlUtils::FindPropertyValue<double>(Settings, key).value_or(fallback);
}

QString AppConfigs::GetString(const QString& key, const QString& fallback)
{
    return QTomlUtils::FindPropertyValue<QString>(Settings, key).value_or(fallback);
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

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::CloseToTray, true);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::StartMinimized, false);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::DiscordPresence, true);

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::MusicScanFolders, musicFolders);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::ArtworkFolder,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/artwork");
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::FetchArtwork, true);

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::OutputDevice, QString());

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::PlayerVolume, 0.8);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::PlayerShuffle, false);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::PlayerRepeat, QStringLiteral("off"));

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::AutoUpdateYtDlp, true);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::AudioQuality, AudioQuality::High);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::DownloadFormat, DownloadFormat::Auto);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SaveWhileListening, false);

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::OnlineSearchSource, QStringLiteral("innertube"));
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::OnlineStreamSource, QStringLiteral("browser"));
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::OnlineBrowserCookies, BrowserCookies::WhenNeeded);

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::DownloadFolder,
        (systemMusic.isEmpty() ? appRoot + "/Music" : systemMusic) + "/SoundLink");

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::CollapsedSettingsSections, QStringList{QStringLiteral("online.advanced")});

    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncEnabled, false);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncDeviceName, QString());
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncPort, 47777);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncUpnp, true);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncConfirmRemote, false);
    QTomlUtils::SetDefaultValue(Settings, SettingsKeys::SyncManualAddresses, QStringList());
}
