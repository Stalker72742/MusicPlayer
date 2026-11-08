#pragma once

#include <QList>
#include <QString>

/// @brief A titled group of settings inside a category; the settings screen can collapse it.
struct SettingsSection
{
    QString id; ///< Unique across categories: `<category>.<name>`.
    QString title;
};

/// @brief A settings category.
///
/// GetSettingsCategories() is the single list of settings categories: the sidebar, the settings screen tabs,
/// the settings model and navigation all read it, so they cannot drift apart.
struct SettingsCategory
{
    QString id;
    QString title;
    QString icon;
    QList<SettingsSection> sections; ///< In display order.
};

/// @brief Every settings category in display order. A new category or section is added only here.
inline const QList<SettingsCategory>& GetSettingsCategories()
{
    static const QList<SettingsCategory> categories{
        {QStringLiteral("general"), QStringLiteral("General"), QStringLiteral("settings"),
            {
                {QStringLiteral("general.app"), QStringLiteral("Application")},
                {QStringLiteral("general.playback"), QStringLiteral("Playback")},
            }},
        {QStringLiteral("library"), QStringLiteral("Library"), QStringLiteral("library"),
            {
                {QStringLiteral("library.folders"), QStringLiteral("Music folders")},
                {QStringLiteral("library.artwork"), QStringLiteral("Artwork")},
                {QStringLiteral("library.downloads"), QStringLiteral("Downloads")},
            }},
        {QStringLiteral("online"), QStringLiteral("Online"), QStringLiteral("globe"),
            {
                {QStringLiteral("online.extension"), QStringLiteral("Browser extension")},
                {QStringLiteral("online.advanced"), QStringLiteral("Advanced")},
            }},
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

/// @brief The category holding a section, nullptr if there is no such section.
inline const SettingsCategory* FindCategoryOfSection(const QString& sectionId)
{
    for (const SettingsCategory& category : GetSettingsCategories())
    {
        for (const SettingsSection& section : category.sections)
        {
            if (section.id == sectionId)
                return &category;
        }
    }
    return nullptr;
}

/// @brief Title of a section by id, empty if there is no such section.
inline QString GetSettingsSectionTitle(const QString& sectionId)
{
    if (const SettingsCategory* category = FindCategoryOfSection(sectionId))
    {
        for (const SettingsSection& section : category->sections)
        {
            if (section.id == sectionId)
                return section.title;
        }
    }
    return {};
}
