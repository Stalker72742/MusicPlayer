# UI Configuration Format

## Описание

Файл `ui_config.json` определяет доступные UI модули и их настройки для системы плагинов.

## Расположение

- **Рабочий конфиг**: `ui_config.json` (в корне проекта)
- **Пример с документацией**: `ui_config.example.json`

## Формат JSON

```json
{
  "ui_modules": [
    {
      "name": "UIModuleName",
      "platform": "windows|android|linux|macos|all",
      "main_window_class": "ClassName",
      "priority": 100,
      "enabled": true,
      "settings": { }
    }
  ]
}
```

## Поля конфигурации UI модуля

### Обязательные поля

| Поле | Тип | Описание |
|------|-----|----------|
| `name` | `string` | Уникальный идентификатор UI модуля. **ВАЖНО**: Должен совпадать с именем, возвращаемым `IUIFactory::getName()` |
| `platform` | `string` | Целевая платформа: `"windows"`, `"android"`, `"linux"`, `"macos"`, или `"all"` |
| `priority` | `number` | Приоритет выбора (0-1000). Чем выше значение, тем приоритетнее UI |
| `enabled` | `boolean` | Доступен ли этот UI для выбора |

### Опциональные поля

| Поле | Тип | Описание |
|------|-----|----------|
| `main_window_class` | `string` | Имя класса главного окна (для документации) |
| `library_path` | `string` | Путь к динамической библиотеке (.dll/.so) для режима плагинов |
| `description` | `string` | Описание UI модуля |
| `settings` | `object` | Произвольный JSON объект с настройками для UI |

## Система приоритетов

Рекомендуемые диапазоны приоритетов:

| Диапазон | Назначение | Примеры |
|----------|------------|---------|
| **200+** | Тестовые/отладочные UI | `TestUI` (200) |
| **100-199** | Production UI | `NewWindows` (100), `Android` (100) |
| **50-99** | Legacy/резервные UI | `OldWindows` (50) |
| **0-49** | Экспериментальные/отключенные | - |

## Логика выбора UI

При вызове `AppInstance::createUIFromConfig()`:

1. **Фильтрация по платформе** - выбираются UI для текущей платформы
2. **Фильтрация по enabled** - только с `enabled: true`
3. **Сортировка по priority** - от большего к меньшему
4. **Выбор победителя** - UI с наивысшим приоритетом
5. **При равенстве** - первый зарегистрированный в системе

## Секция settings

Объект `settings` может содержать любые данные для конкретного UI:

```json
"settings": {
  "theme": "dark",
  "frameless": true,
  "default_size": {
    "width": 1200,
    "height": 800
  },
  "colors": {
    "primary": "#1E88E5",
    "background": "#1E1E1E"
  },
  "features": {
    "animations": true,
    "blur": true
  }
}
```

Эти настройки доступны через `UIConfig::settings` после загрузки конфигурации.

## Примеры использования

### Простой конфиг для разработки

```json
{
  "ui_modules": [
    {
      "name": "TestUI",
      "platform": "windows",
      "priority": 200,
      "enabled": true,
      "settings": {}
    }
  ]
}
```

### Production конфиг с несколькими UI

```json
{
  "ui_modules": [
    {
      "name": "NewWindows",
      "platform": "windows",
      "priority": 100,
      "enabled": true,
      "settings": {
        "theme": "dark",
        "frameless": true
      }
    },
    {
      "name": "OldWindows",
      "platform": "windows",
      "priority": 50,
      "enabled": true,
      "settings": {
        "theme": "default"
      }
    },
    {
      "name": "Android",
      "platform": "android",
      "priority": 100,
      "enabled": true,
      "settings": {
        "qml_path": "qrc:/MainWindow/androidMainWindow.qml"
      }
    }
  ]
}
```

### Отключение UI

Чтобы временно отключить UI без удаления конфигурации:

```json
{
  "name": "TestUI",
  "enabled": false,
  "priority": 200,
  ...
}
```

## Динамические плагины (.dll/.so)

Для загрузки UI из динамической библиотеки:

```json
{
  "name": "CustomUI",
  "platform": "windows",
  "library_path": "./plugins/CustomUI.dll",
  "priority": 150,
  "enabled": true,
  "settings": {}
}
```

**Требования к плагину:**
- Экспортировать функцию `UI_REGISTER_FACTORY(FactoryClass, "ModuleName")`
- Реализовать интерфейс `IUIFactory`

## API для работы с конфигом

```cpp
// Загрузка конфига из файла
uiLoader->loadConfigFromFile("ui_config.json");

// Автоматический выбор лучшего UI
UIConfig* config = uiLoader->selectBestUI("windows");

// Создание UI по конфигу
QWidget* window = uiLoader->createDefaultUI();

// Доступ к настройкам
QJsonObject settings = config->settings;
QString theme = settings["theme"].toString();
```

## Связь с Factory System

Конфигурация работает вместе с системой фабрик:

1. **Статическая регистрация** (через `UI_AUTO_REGISTER`):
   ```cpp
   UI_AUTO_REGISTER(TestUIFactory, "TestUI")
   ```
   - Фабрика регистрируется автоматически при старте
   - `name` в JSON должен совпадать с `"TestUI"`

2. **Динамическая загрузка** (через `library_path`):
   - UIPluginLoader загружает .dll/.so
   - Ищет экспортированную фабрику с именем из `name`

## Проверка конфигурации

Текущий конфиг можно проверить через лог:

```
[INFO] UIPluginLoader: Loaded config for 'TestUI' (priority: 200, enabled: true)
[INFO] UIPluginLoader: Loaded config for 'NewWindows' (priority: 100, enabled: true)
[INFO] UIPluginLoader: Selected UI: 'TestUI' for platform 'windows'
```

## Дополнительные возможности

### Global settings (расширение)

Можно добавить глобальные настройки:

```json
{
  "ui_modules": [...],
  "global_settings": {
    "auto_select_ui": true,
    "fallback_ui": "OldWindows",
    "log_level": "info"
  }
}
```

### Platform mapping (расширение)

Определение доступных UI для платформ:

```json
{
  "ui_modules": [...],
  "platform_mapping": {
    "windows": ["TestUI", "NewWindows", "OldWindows"],
    "android": ["Android"]
  }
}
```

---

**Документация обновлена**: 2026-02-12
**Версия формата**: 1.0
