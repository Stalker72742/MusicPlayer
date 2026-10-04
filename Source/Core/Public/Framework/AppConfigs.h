//
// Created by Stalker7274 on 24.09.2026.
//

#pragma once

#include <QString>

/// @brief Config ids and setting keys of the application, for use with QTomlUtils.
namespace AppConfigs
{
    /// @brief Id of the Settings config.
    ///
    /// User values live in `Saved/Settings.toml`; defaults come from `Config/DefaultSettings.toml`
    /// and RegisterAppConfigs().
    inline const QString Settings = QStringLiteral("Settings");

    /// @brief Keys of every application setting in the Settings config.
    ///
    /// Only values that differ from the defaults are written to `Saved/Settings.toml`.
    namespace SettingsKeys
    {
        /// @brief bool: closing the window hides it to the tray (otherwise quits).
        inline const QString CloseToTray = QStringLiteral("general.closeToTray");
        /// @brief bool: start hidden in the tray.
        inline const QString StartMinimized = QStringLiteral("general.startMinimized");
        /// @brief bool: show the playing track in Discord (DiscordSubsystem).
        inline const QString DiscordPresence = QStringLiteral("general.discordPresence");
        /// @brief QString: `"en"`, `"ru"`. Saved only: there are no translations yet.
        inline const QString Language = QStringLiteral("general.language");

        /// @brief QStringList: folders scanned by LibrarySubsystem.
        inline const QString MusicScanFolders = QStringLiteral("library.musicScanFolders");
        /// @brief bool: rescan the music folders on startup.
        inline const QString ScanOnStartup = QStringLiteral("library.scanOnStartup");

        /// @brief bool: volume normalization. Saved only: not implemented in the player yet.
        inline const QString NormalizeVolume = QStringLiteral("audio.normalize");
        /// @brief double, seconds of crossfade. Saved only: not implemented in the player yet.
        inline const QString Crossfade = QStringLiteral("audio.crossfade");
        /// @brief QString: QAudioDevice id; empty for the system default (followed when it changes).
        inline const QString OutputDevice = QStringLiteral("audio.outputDevice");

        /// @brief double 0..1: player volume, restored on start.
        inline const QString PlayerVolume = QStringLiteral("player.volume");
        /// @brief bool: shuffle, restored on start.
        inline const QString PlayerShuffle = QStringLiteral("player.shuffle");
        /// @brief QString `"off"` / `"all"` / `"one"`: repeat mode, restored on start.
        inline const QString PlayerRepeat = QStringLiteral("player.repeat");

        /// @brief QString: id of the search backend tried first (`"innertube"`, `"ytdlp"`).
        inline const QString OnlineSearchSource = QStringLiteral("online.searchSource");
        /// @brief QString: id of the stream backend tried first (`"browser"`, `"ytdlp"`).
        inline const QString OnlineStreamSource = QStringLiteral("online.streamSource");

        /// @brief QString, one of AppConfigs::BrowserCookies.
        inline const QString OnlineBrowserCookies = QStringLiteral("online.browserCookies");

        /// @brief QString: where downloaded online tracks go.
        ///
        /// Inside a music folder by default, so a rescan finds them as the same tracks.
        inline const QString DownloadFolder = QStringLiteral("downloads.folder");
        /// @brief bool: hourly and failure-triggered yt-dlp updates (a missing yt-dlp is installed anyway).
        inline const QString AutoUpdateYtDlp = QStringLiteral("downloads.autoUpdateYtDlp");
        /// @brief QString, one of AppConfigs::AudioQuality: used for streams and downloads.
        inline const QString AudioQuality = QStringLiteral("downloads.quality");
    }

    /// @brief Values of SettingsKeys::AudioQuality.
    namespace AudioQuality
    {
        inline const QString Best = QStringLiteral("best");     ///< The best stream, usually Opus ~160 kbps.
        inline const QString High = QStringLiteral("high");     ///< AAC ~128 kbps (m4a): plays and tags everywhere.
        inline const QString Medium = QStringLiteral("medium"); ///< ~70 kbps, less traffic.
    }

    /// @brief Values of SettingsKeys::OnlineBrowserCookies: whether the browser extension may hand
    /// YouTube cookies to yt-dlp.
    namespace BrowserCookies
    {
        /// @brief Only for a logged-in browser's token or after a sign-in wall.
        inline const QString WhenNeeded = QStringLiteral("whenNeeded");
        /// @brief Cookies never leave the browser.
        inline const QString Never = QStringLiteral("never");
    }

    /// @brief Directory of the executable.
    QString GetAppRoot();

    /// @brief Reads a bool from the Settings config.
    /// @param key A key from SettingsKeys.
    /// @param fallback Returned when the key is missing or holds another type.
    bool GetBool(const QString& key, bool fallback = false);
    /// @brief Reads a number from the Settings config.
    /// @param key A key from SettingsKeys.
    /// @param fallback Returned when the key is missing or holds another type.
    double GetDouble(const QString& key, double fallback = 0);
    /// @brief Reads a string from the Settings config.
    /// @param key A key from SettingsKeys.
    /// @param fallback Returned when the key is missing or holds another type.
    QString GetString(const QString& key, const QString& fallback = {});

    /// @brief Registers the application configs and the code defaults of every setting.
    ///
    /// Called once from AppInstance::init(), before any subsystem reads its settings.
    void RegisterAppConfigs();
}
