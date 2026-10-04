#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QtQml/qqmlregistration.h>

/// @brief One line of the search popup: what it shows and what activating it does.
struct Suggestion
{
    /// @brief What the line opens or does.
    enum Kind
    {
        Hint,             ///< Completes the query with a tag.
        Page,             ///< Opens a page.
        SettingsCategory, ///< Opens a settings category.
        Setting,          ///< Points the settings screen at a setting.
        Track,            ///< Plays a library track.
        YouTube,          ///< Searches YouTube for text.
        Playlist          ///< Opens a playlist.
    };

    Kind kind = Hint;
    QString title;
    QString subtitle;
    QString icon;

    /// Short tag form shown on the right, e.g. "s:audio;crossfade".
    QString tag;

    /// Query that Tab puts into the search field.
    QString completion;

    /// Payload, depending on kind.
    int page = 0;
    QString category;
    QString settingKey;
    int trackId = 0;
    QString text;
    QString playlistId;
};

/// @brief The search popup lines; filled by SearchViewModel.
class SuggestionListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by SearchViewModel")

    Q_PROPERTY(int count READ GetCount NOTIFY countChanged)

public:
    /// @brief Model roles.
    enum Roles
    {
        KindRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        IconRole,
        TagRole,
        CompletionRole
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Number of rows (the count property).
    int GetCount() const { return static_cast<int>(suggestions.size()); }
    /// @brief The suggestion at a row, or nullptr.
    const Suggestion* At(int row) const;

    /// @brief Replaces the rows.
    void SetSuggestions(const QList<Suggestion>& newSuggestions);

signals:
    void countChanged();

private:
    QList<Suggestion> suggestions;
};
