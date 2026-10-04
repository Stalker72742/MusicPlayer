#include "SettingsModel.h"

#include <QAudioDevice>
#include <QDebug>
#include <QJSEngine>
#include <QMediaDevices>

#include <QTomlUtils/QTomlUtils.h>

#include "AppConfigs.h"
#include "Discord/DiscordSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Player/PlayerSubsystem.h"
#include "SettingsCategories.h"

SettingsModel::SettingsModel()
{
    auto bind = [this](Setting& setting) {
        const QString key = setting.key;
        const QMetaType type = setting.defaultValue.metaType();
        setting.read = [key, type] {
            if (type == QMetaType::fromType<bool>())
                return QVariant(AppConfigs::GetBool(key));
            if (type == QMetaType::fromType<double>())
                return QVariant(AppConfigs::GetDouble(key));
            return QVariant(AppConfigs::GetString(key));
        };
        setting.write = [this, key, type](const QVariant& value) {
            if (type == QMetaType::fromType<bool>())
                QTomlUtils::SetPropertyValue(AppConfigs::Settings, key, value.toBool());
            else if (type == QMetaType::fromType<double>())
                QTomlUtils::SetPropertyValue(AppConfigs::Settings, key, value.toDouble());
            else
                QTomlUtils::SetPropertyValue(AppConfigs::Settings, key, value.toString());
            NotifyValueChanged(FindRow(key));
        };
        setting.isModified = [key] { return QTomlUtils::IsOverridden(AppConfigs::Settings, key); };
        setting.reset = [this, key] {
            QTomlUtils::ResetToDefault(AppConfigs::Settings, key);
            NotifyValueChanged(FindRow(key));
        };
    };

    auto toggle = [&bind](QString key, QString category, QString title, QString description) {
        Setting setting{key, category, title, description, Toggle, false, false};
        bind(setting);
        return setting;
    };

    auto configChoice = [this](QString key, QString category, QString title, QString description,
                            QList<std::pair<QString, QString>> choices) {
        QStringList titles;
        for (const auto& [id, choiceTitle] : choices)
            titles.append(choiceTitle);

        Setting setting{key, category, title, description, Choice, titles.first(), titles.first()};
        setting.options = titles;
        setting.read = [key, choices] {
            const QString id = QTomlUtils::FindPropertyValue<QString>(AppConfigs::Settings, key).value_or(QString());
            for (const auto& [choiceId, choiceTitle] : choices)
            {
                if (choiceId == id)
                    return QVariant(choiceTitle);
            }
            return QVariant(choices.first().second);
        };
        setting.write = [this, key, choices](const QVariant& value) {
            for (const auto& [choiceId, choiceTitle] : choices)
            {
                if (choiceTitle == value.toString())
                    QTomlUtils::SetPropertyValue(AppConfigs::Settings, key, choiceId);
            }
            NotifyValueChanged(FindRow(key));
        };
        setting.isModified = [key] { return QTomlUtils::IsOverridden(AppConfigs::Settings, key); };
        setting.reset = [this, key] {
            QTomlUtils::ResetToDefault(AppConfigs::Settings, key);
            NotifyValueChanged(FindRow(key));
        };
        return setting;
    };
    Setting downloadFolder{AppConfigs::SettingsKeys::DownloadFolder, QStringLiteral("downloads"),
        QStringLiteral("Download folder"),
        QStringLiteral("Where downloaded tracks go. Inside a music folder they also show up in other players."),
        Folder, QString(), QString()};
    bind(downloadFolder);

    QList<std::pair<QString, QString>> outputs{{QString(), QStringLiteral("System default")}};
    for (const QAudioDevice& device : QMediaDevices::audioOutputs())
        outputs.append({QString::fromUtf8(device.id()), device.description()});
    Setting output = configChoice(AppConfigs::SettingsKeys::OutputDevice, QStringLiteral("audio"),
        QStringLiteral("Output device"), QStringLiteral("Where the sound goes"), outputs);
    const auto writeOutput = output.write;
    output.write = [writeOutput](const QVariant& value) {
        writeOutput(value);
        PlayerSubsystem::Get().ApplyOutputDevice();
    };
    const auto resetOutput = output.reset;
    output.reset = [resetOutput] {
        resetOutput();
        PlayerSubsystem::Get().ApplyOutputDevice();
    };

    auto statusRow = [](QString key, QString title, QString description, Type type) {
        return Setting{key, QStringLiteral("online"), title, description, type, QVariant(), QVariant()};
    };

    Setting discord = toggle(AppConfigs::SettingsKeys::DiscordPresence, QStringLiteral("general"),
        QStringLiteral("Discord status"), QStringLiteral("Show what you are listening to on your Discord profile"));
    const auto writeDiscord = discord.write;
    discord.write = [writeDiscord](const QVariant& value) {
        writeDiscord(value);
        DiscordSubsystem::Get().ApplySetting();
    };
    const auto resetDiscord = discord.reset;
    discord.reset = [resetDiscord] {
        resetDiscord();
        DiscordSubsystem::Get().ApplySetting();
    };

    Setting crossfade{AppConfigs::SettingsKeys::Crossfade, QStringLiteral("audio"), QStringLiteral("Crossfade"),
        QStringLiteral("Blend the end of a track into the next one (coming soon)"), Slider, 0.0, 0.0};
    crossfade.maximum = 12;
    crossfade.unit = QStringLiteral("s");
    bind(crossfade);

    Setting folders{AppConfigs::SettingsKeys::MusicScanFolders, QStringLiteral("library"), QStringLiteral("Music folders"),
        QStringLiteral("Folders scanned for music, including subfolders"), FolderList, QStringList(), QStringList()};
    folders.read = [] { return QVariant(LibrarySubsystem::Get().GetFolders()); };
    folders.write = [](const QVariant& value) { LibrarySubsystem::Get().SetFolders(value.toStringList()); };
    folders.isModified = [] { return LibrarySubsystem::Get().AreFoldersModified(); };
    folders.reset = [] { LibrarySubsystem::Get().ResetFolders(); };

    settings = {
        toggle(AppConfigs::SettingsKeys::CloseToTray, QStringLiteral("general"), QStringLiteral("Close to tray"),
            QStringLiteral("Keep playing in the tray when the window is closed; otherwise closing quits")),
        toggle(AppConfigs::SettingsKeys::StartMinimized, QStringLiteral("general"), QStringLiteral("Start minimized"),
            QStringLiteral("Start in the tray without showing the window")),
        discord,
        configChoice(AppConfigs::SettingsKeys::Language, QStringLiteral("general"), QStringLiteral("Language"),
            QStringLiteral("Interface language (only English for now)"),
            {{QStringLiteral("en"), QStringLiteral("English")}, {QStringLiteral("ru"), QStringLiteral("Русский")}}),

        folders,
        toggle(AppConfigs::SettingsKeys::ScanOnStartup, QStringLiteral("library"), QStringLiteral("Scan on startup"),
            QStringLiteral("Look for new files every time the app starts")),

        output,
        toggle(AppConfigs::SettingsKeys::NormalizeVolume, QStringLiteral("audio"), QStringLiteral("Normalize volume"),
            QStringLiteral("Play all tracks at a similar loudness (coming soon)")),
        crossfade,

        statusRow(QStringLiteral("online.browserExtension"), QStringLiteral("Browser extension"),
            QStringLiteral("Plays YouTube with your browser's own session, so bot checks pass the way they do there. "
                           "Install it unpacked from the folder, then pair it with the code shown here."),
            BrowserBridge),
        configChoice(AppConfigs::SettingsKeys::OnlineSearchSource, QStringLiteral("online"), QStringLiteral("Search with"),
            QStringLiteral("Tried first; the other one takes over if it fails"),
            {{QStringLiteral("innertube"), QStringLiteral("InnerTube")}, {QStringLiteral("ytdlp"), QStringLiteral("yt-dlp")}}),
        configChoice(AppConfigs::SettingsKeys::OnlineStreamSource, QStringLiteral("online"), QStringLiteral("Play streams with"),
            QStringLiteral("Tried first; yt-dlp on its own takes over if the extension is missing or fails"),
            {{QStringLiteral("browser"), QStringLiteral("Browser extension")}, {QStringLiteral("ytdlp"), QStringLiteral("yt-dlp")}}),
        configChoice(AppConfigs::SettingsKeys::OnlineBrowserCookies, QStringLiteral("online"), QStringLiteral("Browser cookies"),
            QStringLiteral("Only for a browser logged in to YouTube or after a sign-in wall, and only if the extension "
                           "allows it too. Cookies go to a temporary file deleted right after use."),
            {{AppConfigs::BrowserCookies::WhenNeeded, QStringLiteral("Only when needed")},
                {AppConfigs::BrowserCookies::Never, QStringLiteral("Never")}}),
        statusRow(QStringLiteral("online.backends"), QStringLiteral("Sources"),
            QStringLiteral("What can be used right now"), OnlineBackends),

        downloadFolder,
        toggle(AppConfigs::SettingsKeys::AutoUpdateYtDlp, QStringLiteral("downloads"), QStringLiteral("Keep yt-dlp updated"),
            QStringLiteral("Check for a new yt-dlp every hour and after failed downloads")),
        configChoice(AppConfigs::SettingsKeys::AudioQuality, QStringLiteral("downloads"), QStringLiteral("Audio quality"),
            QStringLiteral("Of streams and downloads. High is AAC, which every player and tag editor handles"),
            {{AppConfigs::AudioQuality::Best, QStringLiteral("Best")}, {AppConfigs::AudioQuality::High, QStringLiteral("High")},
                {AppConfigs::AudioQuality::Medium, QStringLiteral("Medium")}}),
    };

    for (const Setting& setting : settings)
        Q_ASSERT_X(IsSettingsCategory(setting.category), "SettingsModel", "setting in an unknown category");

    connect(&LibrarySubsystem::Get(), &LibrarySubsystem::foldersChanged, this,
        [this] { NotifyValueChanged(FindRow(AppConfigs::SettingsKeys::MusicScanFolders)); });
}

SettingsModel& SettingsModel::Get()
{
    static SettingsModel instance;
    return instance;
}

SettingsModel* SettingsModel::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(&Get(), QJSEngine::CppOwnership);
    return &Get();
}

int SettingsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(settings.size());
}

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Setting& setting = settings.at(index.row());
    switch (role)
    {
        case KeyRole:         return setting.key;
        case CategoryRole:    return setting.category;
        case TitleRole:       return setting.title;
        case DescriptionRole: return setting.description;
        case TypeRole:        return setting.type;
        case ValueRole:       return setting.GetValue();
        case ModifiedRole:    return setting.isModified ? setting.isModified() : setting.value != setting.defaultValue;
        case MinimumRole:     return setting.minimum;
        case MaximumRole:     return setting.maximum;
        case StepRole:        return setting.step;
        case UnitRole:        return setting.unit;
        case OptionsRole:     return setting.options;
        default:              return {};
    }
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {CategoryRole, "category"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {TypeRole, "type"},
        {ValueRole, "value"},
        {ModifiedRole, "modified"},
        {MinimumRole, "minimum"},
        {MaximumRole, "maximum"},
        {StepRole, "step"},
        {UnitRole, "unit"},
        {OptionsRole, "options"},
    };
}

QVariantList SettingsModel::GetCategories() const
{
    QVariantList result;
    for (const SettingsCategory& category : GetSettingsCategories())
    {
        result.append(QVariantMap{
            {QStringLiteral("id"), category.id},
            {QStringLiteral("title"), category.title},
            {QStringLiteral("icon"), category.icon},
        });
    }
    return result;
}

QList<SettingsModel::SettingInfo> SettingsModel::GetSettingInfos() const
{
    QList<SettingInfo> infos;
    for (const Setting& setting : settings)
        infos.append({setting.key, setting.category, setting.title, setting.description});
    return infos;
}

QVariant SettingsModel::valueOf(const QString& key) const
{
    const int row = FindRow(key);
    return row >= 0 ? settings[row].GetValue() : QVariant();
}

QString SettingsModel::categoryTitle(const QString& id) const
{
    for (const SettingsCategory& category : GetSettingsCategories())
    {
        if (category.id == id)
            return category.title;
    }
    return {};
}

int SettingsModel::FindRow(const QString& key) const
{
    for (qsizetype row = 0; row < settings.size(); ++row)
    {
        if (settings[row].key == key)
            return static_cast<int>(row);
    }

    qWarning() << "[SettingsModel] Unknown setting:" << key;
    return -1;
}

void SettingsModel::setValue(const QString& key, const QVariant& value)
{
    const int row = FindRow(key);
    if (row < 0)
        return;

    Setting& setting = settings[row];

    QVariant converted = value;
    if (!converted.convert(setting.defaultValue.metaType()) || converted == setting.GetValue())
        return;

    if (setting.write)
    {
        setting.write(converted);
        return;
    }

    setting.value = converted;
    NotifyValueChanged(row);
}

void SettingsModel::resetToDefault(const QString& key)
{
    const int row = FindRow(key);
    if (row < 0)
        return;

    if (settings[row].reset)
        settings[row].reset();
    else
        setValue(key, settings[row].defaultValue);
}

void SettingsModel::addFolder(const QString& key, const QUrl& folder)
{
    const int row = FindRow(key);
    if (row < 0 || !folder.isLocalFile())
        return;

    QStringList folders = settings[row].GetValue().toStringList();
    folders.append(folder.toLocalFile());
    setValue(key, folders);
}

void SettingsModel::removeFolder(const QString& key, const QString& folder)
{
    const int row = FindRow(key);
    if (row < 0)
        return;

    QStringList folders = settings[row].GetValue().toStringList();
    folders.removeAll(folder);
    setValue(key, folders);
}

void SettingsModel::setFolder(const QString& key, const QUrl& folder)
{
    if (folder.isLocalFile())
        setValue(key, folder.toLocalFile());
}

void SettingsModel::NotifyValueChanged(int row)
{
    if (row < 0)
        return;

    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {ValueRole, ModifiedRole});
}

SettingsCategoryFilter::SettingsCategoryFilter(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(&SettingsModel::Get());
}

void SettingsCategoryFilter::SetCategory(const QString& value)
{
    if (category == value)
        return;

    category = value;
    invalidateRowsFilter();
    emit categoryChanged();
}

int SettingsCategoryFilter::rowOf(const QString& key) const
{
    for (int row = 0; row < rowCount(); ++row)
    {
        if (index(row, 0).data(SettingsModel::KeyRole).toString() == key)
            return row;
    }
    return -1;
}

bool SettingsCategoryFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(SettingsModel::CategoryRole).toString() == category;
}
