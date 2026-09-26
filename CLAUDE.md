# SoundLink (MusicPlayer) — Qt/C++ проект

Десктопный музыкальный плеер **SoundLink** на Qt 6.8.3 / C++23 / CMake. Git-репозиторий (github.com/Stalker72742/MusicPlayer).

Код пишет один автор; комментарии и логи частично на русском. Даты в шапках файлов (`Created by Stalker7274 on ...`) — ориентир по возрасту кода.

## Сборка (Windows, MinGW)

CMake-корень — `source/` (не корень репозитория). Пресет `QtMinGW` в `source/CMakeUserPresets.json`:
Qt `D:/Qt/6.8.3/mingw_64`, компиляторы `d:/Qt/Tools/mingw1310_64`, Ninja `d:/Qt/Tools/Ninja`. Рабочая сборка лежит в `build/QtMinGW/` (там `SoundLink.exe`).

```bash
cmake --build "D:/CPP Projects/SoundLink/MusicPlayer/build/QtMinGW"
```

Post-build шаги корневого `source/CMakeLists.txt`:
1. `windeployqt` рядом с exe (и отдельно для каждого плагина).
2. Копирование `core/libMusicPlayerCore.dll` в корень билд-папки.
3. `python scripts/GeneratePluginsFiles.py` — находит все `project(...)` в `CMakeLists.txt` под `source/` и копирует их DLL в `<build>/Plugins/<Name>/Binaries/`. Нужен `python` в PATH.

Тестов нет. `.clang-format` в корне репозитория (LLVM-база, Allman-скобки, отступ 4). Пути в CMake местами в другом регистре, чем на диске (`Public/Framework` vs `public/framework`) — собирается только на регистронезависимой ФС (Windows).

## Архитектура

Приложение = маленькое ядро + плагины (DLL), загружаемые в рантайме.

### Точка входа — `source/main.cpp`
`QApplication` → `AppInstance::getInstance()` → регистрирует `eventDisp` → `PluginLoader::FindAndLoadPlugins()` → `exec()`. Больше ни одна подсистема сейчас не создаётся.

### Ядро — `source/core/` → `MusicPlayerCore.dll` (SHARED)
- `public/framework/AppInstance.h` — синглтон, хранит `QList<SubsystemBase*>`; `addSubsystem`, `createSubsystem<T>`, `getSubsystem<T>` (через `qobject_cast`).
- `public/framework/SubsystemBase.h` — базовый `QObject` подсистем; статический `SubsystemBase::GetSubsystem<T>()` — основной способ достать подсистему из плагина.
- `public/framework/IModuleInterface.h` — интерфейс `IPlugin { init(); shutdown(); name(); }`, типы `CreatePluginFn`/`DestroyPluginFn`.
- `public/framework/PluginLoader.{h,cpp}` — рекурсивно ищет `*.dll` в `<cwd>/Plugins`, пропускает имена на `qt*`/`qml*`, грузит через `LoadLibraryA`/`dlopen`, берёт `createPlugin`/`destroyPlugin`, после загрузки всех вызывает `init()`. Путь относительный к **текущей рабочей директории** — exe нужно запускать из билд-папки. Прочие DLL, скопированные windeployqt (libgcc, D3Dcompiler и т.п.), дают в логе «Missing createPlugin» — это ожидаемо.
- `public/subsystems/modSubsystem/PluginFactory.h` — макрос `PLUGIN_EXPORT(ClassName)`, генерирует `extern "C"` фабрики. Каждый плагин вызывает его в своём .cpp.
- `public/subsystems/eventDispatcher/EventDispatcher.h` — `eventDisp`: шина строковых сообщений (`sendMessage` → сигнал `messageReceived`). Единственный канал связи между плагинами. Известные сообщения: `ShowWindow`, `HideWindow`, `InvertVisibility`.
- `public/subsystems/Player/` — `PlayerSubsystem` (плейлисты, очередь, громкость, биндинг `QSlider`), абстрактный `playerBackend` (`play()` чисто виртуальный, остальное — пустые заглушки), `playlist` (список `song*` + индекс, `constructDir` собирает из папки `*.mp3`), `song` (локальный файл или URL; стрим получает через внешний `yt-dlp`). **Конкретной реализации `playerBackend` нет, `PlayerSubsystem` нигде не создаётся** — воспроизведение пока не подключено. Остатки Android/Java-бэкенда закомментированы.
- `public/subsystems/FileManager/` — статические хелперы чтения плейлистов: JSON-файлы вида `{ "Название": "путь/к/файлу" }` в `AppDataLocation`. Не зарегистрирован в `AppInstance`.
- `public/subsystems/Settings/` — `SettingsSubsystem`: `QMap<QString, SettingData>` в бинарном `QDataStream` (`<cwd>/Saved/SettingsData`); `SettingData` хранит значение как `QByteArray` + `ESettingType`. Не зарегистрирован.
- Конфиги — внешняя либа **QTomlUtils** (git submodule `third_party/QTomlUtils`, https://github.com/Stalker72742/QTomlUtils, API в её `README.md`; после клона нужен `git submodule update --init`). Подключается в `core/CMakeLists.txt` через `add_subdirectory(${QTOMLUTILS_DIR})`, include — `<QTomlUtils/QTomlUtils.h>`. Кратко: `RegisterConfig(id, path, defaultsPath)`, `FindPropertyValue<T>(id, "a.b", default)`, `SetPropertyValue`, `SetDefaultValue`, `ResetToDefault`; пользовательский файл хранит только отличия от дефолтов, как в UE. Типы — только из списка в хедере (скаляры + `QList` от них), без `QVariant`. Собирается SHARED — чтобы реестр был один на ядро и все плагины; `MusicPlayerCore` линкует её PUBLIC, post-build копирует DLL к exe.
- `public/framework/AppConfigs.h` — id конфигов приложения и константы ключей (`AppConfigs::Settings`, `AppConfigs::SettingsKeys::MusicScanFolders`), `GetAppRoot()` (= `applicationDirPath`), `RegisterAppConfigs()` — вызывается в `main.cpp` до загрузки плагинов. Конфиг `Settings`: `<appRoot>/Saved/Settings.toml`, дефолты `<appRoot>/Config/DefaultSettings.toml` + код. Новые настройки: ключ-константа в `SettingsKeys` + `SetDefaultValue` в `RegisterAppConfigs()`.
- `public/data/trackInfo.h`, `Player/Data/songPath.h` — пустой / устаревший.

Замечание по раскладке: `.cpp` лежат и в `private/`, и в `public/` (исторически), экспорт через `MYLIB_EXPORT` (+ `WINDOWS_EXPORT_ALL_SYMBOLS`).

### Плагины — `source/ui/`
Каждый — отдельная SHARED-библиотека без префикса `lib`, линкуется с `MusicPlayerCore`, добавлена в `add_dependencies` и в `PLUGINS` корневого CMake.

- `ui/tray/` → `Tray.dll` — `TrayModule : QObject, IPlugin`. Иконка в трее (`:/icons/ApplicationIcon/icon_512.png` из ресурсов windowsUI), меню Show/Hide/Quit; клики шлёт через `eventDisp`.
- `ui/windowsUI/` → `windowsUI.dll` — `NewWindowsModule : IPlugin` (name `"WindowsUI"`). Создаёт `QQmlApplicationEngine`, регистрирует в модуле `SoundLink 1.0` синглтон `Theme` и тип `MedialibModel`, грузит `qrc:/Widgets/WindowsMainWindow.qml`, слушает `eventDisp` для показа/скрытия окна.
  - `Models/MedialibModel` — `QAbstractListModel` (роли `idx`, `title`, `artist`, `dateAdded`, `duration`). Пока **пустой**, ни к чему из ядра не подключён.
  - QML (`Widgets/`): `WindowsMainWindow.qml` — безрамочное окно (своя титулка, ресайз-ручка, закрытие = hide в трей). `Theme.qml` — палитра/метрики (тёмная тема). `Sections/` — `AppTitleBar`, `SideBar`, `LibraryView`, `PlayerBar`, `ResizeHandle`. `Screens/Medialib.qml`. `Components/` — мелкие контролы. Данные в `PlayerBar` захардкожены (моки), сигналы в C++ не проброшены.
  - Все QML/иконки должны быть перечислены в `qml.qrc`, иначе не попадут в сборку. `Resources/Resources.qrc` пустой и не подключён; `Resources/app.rc` — иконка exe.

### Добавление нового плагина
1. Папка в `source/ui/<name>/` (или где угодно под `source/`) с `CMakeLists.txt`, `project(<Name>)`, `add_library(<Name> SHARED ...)`, `PREFIX ""`, линк с `MusicPlayerCore`.
2. Класс, наследующий `IPlugin`, + `PLUGIN_EXPORT(Class);` в .cpp.
3. В `source/CMakeLists.txt`: `add_subdirectory`, `add_dependencies`, добавить имя в `PLUGINS`.
4. Общение с другими модулями — через `eventDisp`, доступ к подсистемам — `SubsystemBase::GetSubsystem<T>()`.

## Текущее состояние
Работает: запуск, загрузка плагинов, трей, QML-окно с макетом медиатеки. Не сделано: реальный аудио-бэкенд, регистрация `PlayerSubsystem`/`FileManager`/`Settings`, связка QML ↔ ядро, наполнение `MedialibModel`. Android-ресурсы удалены из рабочего дерева (незакоммичено).
