#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVariant>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QQmlEngine;

/// @brief All settings shown in the UI. QML singleton.
///
/// Stored in the Settings config (Saved/Settings.toml) under their key
/// (AppConfigs::SettingsKeys). A setting is either bound to the config directly or to an owner that keeps it there
/// through read/write/isModified/reset (e.g. music folders to LibrarySubsystem). Status rows hold no value.
class SettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    /// @brief [{ id, title, icon }] in display order.
    Q_PROPERTY(QVariantList categories READ GetCategories CONSTANT)

public:
    /// @brief How a setting is edited and drawn.
    enum Type
    {
        Toggle,
        Slider,
        Choice,
        FolderList,
        Folder,

        /// Status rows drawn from OnlineViewModel rather than a value.
        BrowserBridge,
        OnlineBackends
    };
    Q_ENUM(Type)

    /// @brief Model roles.
    enum Roles
    {
        KeyRole = Qt::UserRole + 1,
        CategoryRole,
        TitleRole,
        DescriptionRole,
        TypeRole,
        ValueRole,
        ModifiedRole,
        MinimumRole,
        MaximumRole,
        StepRole,
        UnitRole,
        OptionsRole
    };

    /// @brief The instance shared by C++ and QML.
    static SettingsModel& Get();
    /// @brief QML singleton factory; returns Get().
    static SettingsModel* create(QQmlEngine*, QJSEngine*);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief The categories property.
    QVariantList GetCategories() const;

    /// @brief What search needs to find a setting.
    struct SettingInfo
    {
        QString key;
        QString category;
        QString title;
        QString description;
    };
    /// @brief Every setting, for search.
    QList<SettingInfo> GetSettingInfos() const;

    /// @brief Title of a category by id.
    Q_INVOKABLE QString categoryTitle(const QString& id) const;

    /// @brief The current value, e.g. for QML that acts on a setting once (closing the window).
    Q_INVOKABLE QVariant valueOf(const QString& key) const;
    /// @brief Stores a new value; the type follows the setting's default.
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value);
    /// @brief Restores the default value.
    Q_INVOKABLE void resetToDefault(const QString& key);

    /// @brief Adds a folder to a FolderList setting.
    Q_INVOKABLE void addFolder(const QString& key, const QUrl& folder);
    /// @brief Removes a folder from a FolderList setting.
    Q_INVOKABLE void removeFolder(const QString& key, const QString& folder);

    /// @brief Sets the folder of a Folder setting.
    Q_INVOKABLE void setFolder(const QString& key, const QUrl& folder);

private:
    SettingsModel();

    struct Setting
    {
        QString key;
        QString category;
        QString title;
        QString description;
        Type type = Toggle;
        QVariant value;
        QVariant defaultValue;
        double minimum = 0;
        double maximum = 1;
        double step = 1;
        QString unit;
        QStringList options;

        std::function<QVariant()> read;
        std::function<void(const QVariant&)> write;
        std::function<bool()> isModified;
        std::function<void()> reset;

        QVariant GetValue() const { return read ? read() : value; }
    };

    int FindRow(const QString& key) const;
    void NotifyValueChanged(int row);

    QList<Setting> settings;
};

/// @brief Settings of one category, for a list on the settings screen.
class SettingsCategoryFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString category READ GetCategory WRITE SetCategory NOTIFY categoryChanged)

public:
    explicit SettingsCategoryFilter(QObject* parent = nullptr);

    QString GetCategory() const { return category; }
    void SetCategory(const QString& value);

    /// @brief Row of the setting in this filtered list, -1 if it is not here.
    Q_INVOKABLE int rowOf(const QString& key) const;

signals:
    void categoryChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString category;
};
