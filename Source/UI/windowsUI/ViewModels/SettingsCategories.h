#pragma once

#include <QList>
#include <QString>

/// @brief A settings category.
///
/// GetSettingsCategories() is the single list of settings categories: the sidebar, the settings screen tabs,
/// the settings model and navigation all read it, so they cannot drift apart.
struct SettingsCategory
{
    QString id;
    QString title;
    QString icon;
};

/// @brief Every settings category in display order. A new category is added only here.
inline const QList<SettingsCategory>& GetSettingsCategories()
{
    static const QList<SettingsCategory> categories{
        {QStringLiteral("general"), QStringLiteral("General"), QStringLiteral("settings")},
        {QStringLiteral("library"), QStringLiteral("Library"), QStringLiteral("library")},
        {QStringLiteral("audio"), QStringLiteral("Audio"), QStringLiteral("audio")},
        {QStringLiteral("online"), QStringLiteral("Online"), QStringLiteral("globe")},
        {QStringLiteral("downloads"), QStringLiteral("Downloads"), QStringLiteral("download")},
    };
    return categories;
}

/// @brief Whether id names a settings category.
inline bool IsSettingsCategory(const QString& id)
{
    for (const SettingsCategory& category : GetSettingsCategories())
    {
        if (category.id == id)
            return true;
    }
    return false;
}
