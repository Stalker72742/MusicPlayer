#include "SettingsModel.h"

#include <QDebug>
#include <QJSEngine>

#include <QTomlUtils/QTomlUtils.h>

#include "AppConfigs.h"
#include "Catalog/ArtworkSubsystem.h"
#include "Discord/DiscordSubsystem.h"
#include "Library/LibrarySubsystem.h"
#include "Player/AudioDevices.h"
#include "Player/PlayerSubsystem.h"
#include "SettingsCategories.h"
#include "Sync/SyncSubsystem.h"

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

    auto toggle = [&bind](QString key, QString section, QString title, QString description) {
        Setting setting{key, section, title, description, Toggle, false, false};
        bind(setting);
        return setting;
    };

    auto configChoice = [this](QString key, QString section, QString title, QString description,
                            QList<std::pair<QString, QString>> choices) {
        QStringList titles;
        for (const auto& [id, choiceTitle] : choices)
            titles.append(choiceTitle);

        Setting setting{key, section, title, description, Choice, titles.first(), titles.first()};
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
    Setting downloadFolder{AppConfigs::SettingsKeys::DownloadFolder, QStringLiteral("library.downloads"),
        QStringLiteral("Download folder"),
        QStringLiteral("Where downloaded tracks go. Inside a music folder they also show up in other players."),
        Folder, QString(), QString()};
    bind(downloadFolder);

    Setting artworkFolder{AppConfigs::SettingsKeys::ArtworkFolder, QStringLiteral("library.artwork"),
        QStringLiteral("Artwork folder"),
        QStringLiteral("Pictures of artists, albums and genres, in Artists, Albums and Genres subfolders. "
                       "Put your own there named like the artist, \"Artist - Album\" or the genre, or replace any file."),
        Folder, QString(), QString()};
    bind(artworkFolder);
    const auto writeArtworkFolder = artworkFolder.write;
    artworkFolder.write = [writeArtworkFolder](const QVariant& value) {
        writeArtworkFolder(value);
        ArtworkSubsystem::Get().ApplyFolderSetting();
    };
    const auto resetArtworkFolder = artworkFolder.reset;
    artworkFolder.reset = [resetArtworkFolder] {
        resetArtworkFolder();
        ArtworkSubsystem::Get().ApplyFolderSetting();
    };

    QList<std::pair<QString, QString>> outputs{{QString(), QStringLiteral("System default")}};
    for (const AudioDevices::Device& device : AudioDevices::List())
        outputs.append({QString::fromUtf8(device.id), device.name});
    Setting output = configChoice(AppConfigs::SettingsKeys::OutputDevice, QStringLiteral("general.playback"),
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

    auto statusRow = [](QString key, QString section, QString title, QString description, Type type) {
        return Setting{key, section, title, description, type, QVariant(), QVariant()};
    };

    Setting discord = toggle(AppConfigs::SettingsKeys::DiscordPresence, QStringLiteral("general.app"),
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

    auto syncToggle = [&toggle](QString key, QString section, QString title, QString description) {
        Setting setting = toggle(key, section, title, description);
        const auto write = setting.write;
        setting.write = [write, key](const QVariant& value) {
            write(value);
            SyncSubsystem::Get().ApplySetting(key);
        };
        const auto reset = setting.reset;
        setting.reset = [reset, key] {
            reset();
            SyncSubsystem::Get().ApplySetting(key);
        };
        return setting;
    };

    Setting folders{AppConfigs::SettingsKeys::MusicScanFolders, QStringLiteral("library.folders"), QStringLiteral("Music folders"),
        QStringLiteral("Folders scanned for music, including subfolders"), FolderList, QStringList(), QStringList()};
    folders.read = [] { return QVariant(LibrarySubsystem::Get().GetFolders()); };
    folders.write = [](const QVariant& value) { LibrarySubsystem::Get().SetFolders(value.toStringList()); };
    folders.isModified = [] { return LibrarySubsystem::Get().AreFoldersModified(); };
    folders.reset = [] { LibrarySubsystem::Get().ResetFolders(); };

    settings = {
        toggle(AppConfigs::SettingsKeys::CloseToTray, QStringLiteral("general.app"), QStringLiteral("Close to tray"),
            QStringLiteral("Keep playing in the tray when the window is closed; otherwise closing quits")),
        toggle(AppConfigs::SettingsKeys::StartMinimized, QStringLiteral("general.app"), QStringLiteral("Start minimized"),
            QStringLiteral("Start in the tray without showing the window")),
        discord,

        output,
        configChoice(AppConfigs::SettingsKeys::AudioQuality, QStringLiteral("general.playback"), QStringLiteral("Audio quality"),
            QStringLiteral("Of YouTube streams and downloads. High is AAC, which every player and tag editor handles"),
            {{AppConfigs::AudioQuality::Best, QStringLiteral("Best")}, {AppConfigs::AudioQuality::High, QStringLiteral("High")},
                {AppConfigs::AudioQuality::Medium, QStringLiteral("Medium")}}),

        folders,

        artworkFolder,
        toggle(AppConfigs::SettingsKeys::FetchArtwork, QStringLiteral("library.artwork"), QStringLiteral("Download missing pictures"),
            QStringLiteral("Look artists, albums and genres without a picture up on Deezer and save what is found into the artwork folder")),

        downloadFolder,
        configChoice(AppConfigs::SettingsKeys::DownloadFormat, QStringLiteral("library.downloads"), QStringLiteral("Download format"),
            QStringLiteral("Files stay as YouTube serves them, nothing is converted. If a track lacks the format, the best available one is taken"),
            {{AppConfigs::DownloadFormat::Auto, QStringLiteral("By quality")}, {AppConfigs::DownloadFormat::M4a, QStringLiteral("M4A")},
                {AppConfigs::DownloadFormat::Opus, QStringLiteral("Opus")}}),
        toggle(AppConfigs::SettingsKeys::SaveWhileListening, QStringLiteral("library.downloads"), QStringLiteral("Save tracks while listening"),
            QStringLiteral("A YouTube track played to its end stays as a file, with no second download")),

        statusRow(QStringLiteral("online.browserExtension"), QStringLiteral("online.extension"), QStringLiteral("Browser extension"),
            QStringLiteral("Plays YouTube with your browser's own session, so bot checks pass the way they do there. "
                           "Install it unpacked from the folder, then pair it with the code shown here."),
            BrowserBridge),
        configChoice(AppConfigs::SettingsKeys::OnlineBrowserCookies, QStringLiteral("online.extension"), QStringLiteral("Browser cookies"),
            QStringLiteral("Only for a browser logged in to YouTube or after a sign-in wall, and only if the extension "
                           "allows it too. Cookies go to a temporary file deleted right after use."),
            {{AppConfigs::BrowserCookies::WhenNeeded, QStringLiteral("Only when needed")},
                {AppConfigs::BrowserCookies::Never, QStringLiteral("Never")}}),

        statusRow(QStringLiteral("online.backends"), QStringLiteral("online.advanced"), QStringLiteral("Sources"),
            QStringLiteral("What can be used right now"), OnlineBackends),
        configChoice(AppConfigs::SettingsKeys::OnlineSearchSource, QStringLiteral("online.advanced"), QStringLiteral("Search with"),
            QStringLiteral("Tried first; the other one takes over if it fails"),
            {{QStringLiteral("innertube"), QStringLiteral("InnerTube")}, {QStringLiteral("ytdlp"), QStringLiteral("yt-dlp")}}),
        configChoice(AppConfigs::SettingsKeys::OnlineStreamSource, QStringLiteral("online.advanced"), QStringLiteral("Play streams with"),
            QStringLiteral("Tried first; yt-dlp on its own takes over if the extension is missing or fails"),
            {{QStringLiteral("browser"), QStringLiteral("Browser extension")}, {QStringLiteral("ytdlp"), QStringLiteral("yt-dlp")}}),
        toggle(AppConfigs::SettingsKeys::AutoUpdateYtDlp, QStringLiteral("online.advanced"), QStringLiteral("Keep yt-dlp updated"),
            QStringLiteral("Check for a new yt-dlp every hour and after failed downloads")),

        syncToggle(AppConfigs::SettingsKeys::SyncEnabled, QStringLiteral("sync.devices"), QStringLiteral("Sync between devices"),
            QStringLiteral("Keep music files, playlists, tags and saved YouTube tracks the same on your paired devices: "
                           "over the local network or the internet, whenever they can reach each other")),
        statusRow(QStringLiteral("sync.thisDevice"), QStringLiteral("sync.devices"), QStringLiteral("This device"),
            QStringLiteral("The name other devices show"), SyncThisDevice),
        statusRow(QStringLiteral("sync.files"), QStringLiteral("sync.devices"), QStringLiteral("Received files"),
            QStringLiteral("Files from other devices go to this folder, keeping their subfolders: the download folder "
                           "when it is inside a music folder. A file this PC already has in its music folders stays where it is"),
            SyncFiles),
        statusRow(QStringLiteral("sync.pairing"), QStringLiteral("sync.devices"), QStringLiteral("Pair a device"),
            QStringLiteral("Show a code here and type it on the other device, or type the code the other device shows"),
            SyncPairing),
        statusRow(QStringLiteral("sync.deviceList"), QStringLiteral("sync.devices"), QStringLiteral("Devices"),
            QStringLiteral("Paired devices, and devices with SoundLink heard on this network"), SyncDevices),
        syncToggle(AppConfigs::SettingsKeys::SyncConfirmRemote, QStringLiteral("sync.devices"),
            QStringLiteral("Ask before replacing or deleting files"),
            QStringLiteral("A file changed or deleted on another device waits for your choice in Downloads › Sync. "
                           "Otherwise the change is applied, and deleted files go to the Recycle Bin")),

        statusRow(QStringLiteral("sync.server"), QStringLiteral("sync.network"), QStringLiteral("Server role"),
            QStringLiteral("Every device is a server. Devices that can reach each other connect by themselves: "
                           "on the local network, or over the internet to a device with a public address or a forwarded port"),
            SyncServer),
        syncToggle(AppConfigs::SettingsKeys::SyncUpnp, QStringLiteral("sync.network"), QStringLiteral("Forward the port on the router"),
            QStringLiteral("Ask the router over UPnP to let devices on the internet connect to this PC")),
        statusRow(QStringLiteral("sync.addresses"), QStringLiteral("sync.network"), QStringLiteral("Addresses of devices"),
            QStringLiteral("Where to look for paired devices besides the local network, e.g. a public address or a domain "
                           "with a forwarded port: host or host:port"),
            SyncAddresses),
    };

    for (Setting& setting : settings)
    {
        const SettingsCategory* category = FindCategoryOfSection(setting.section);
        Q_ASSERT_X(category, "SettingsModel", "setting in an unknown section");
        setting.category = category ? category->id : QString();
    }

    collapsedSections = QTomlUtils::FindPropertyValue<QStringList>(AppConfigs::Settings,
        AppConfigs::SettingsKeys::CollapsedSettingsSections).value_or(QStringList{});

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
        case SectionRole:     return setting.section;
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
        {SectionRole, "section"},
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

QString SettingsModel::sectionTitle(const QString& id) const
{
    return GetSettingsSectionTitle(id);
}

QString SettingsModel::sectionOf(const QString& key) const
{
    const int row = FindRow(key);
    return row >= 0 ? settings[row].section : QString();
}

void SettingsModel::setSectionCollapsed(const QString& id, bool bCollapsed)
{
    if (collapsedSections.contains(id) == bCollapsed || !FindCategoryOfSection(id))
        return;

    if (bCollapsed)
        collapsedSections.append(id);
    else
        collapsedSections.removeAll(id);
    QTomlUtils::SetPropertyValue(AppConfigs::Settings, AppConfigs::SettingsKeys::CollapsedSettingsSections, collapsedSections);
    emit collapsedSectionsChanged();
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
