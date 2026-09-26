//
// Created by Stalker7274 on 24.09.2026.
//

#pragma once

#include <QString>

#ifdef MYLIB_BUILD
#  define MYLIB_EXPORT Q_DECL_EXPORT
#else
#  define MYLIB_EXPORT Q_DECL_IMPORT
#endif

// Config ids and keys for use with QTomlUtils.
namespace AppConfigs
{
    // Saved/Settings.toml; defaults from Config/DefaultSettings.toml and RegisterAppConfigs().
    inline const QString Settings = QStringLiteral("Settings");

    namespace SettingsKeys
    {
        // QStringList
        inline const QString MusicScanFolders = QStringLiteral("library.musicScanFolders");
    }

    // Directory of the executable.
    MYLIB_EXPORT QString GetAppRoot();

    // Must run before plugins are loaded.
    MYLIB_EXPORT void RegisterAppConfigs();
}
