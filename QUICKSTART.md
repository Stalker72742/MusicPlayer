# MusicPlayer - Quick Start Guide

## 🚀 Быстрый запуск (Windows)

### Шаг 1: Скачать проект
```bash
git clone https://github.com/Stalker72742/MusicPlayer.git
cd MusicPlayer
git checkout claude/explore-capabilities-011CUtTTLbNXbpUwdrZsWDkj
```

### Шаг 2: Собрать и запустить
```bash
# Просто запустить скрипт:
build.bat

# Или полная пересборка:
rebuild.bat
```

### Шаг 3: Готово!
Приложение запустится автоматически и покажет **Hello World** окно!

> **TestUI** будет выбран автоматически (приоритет 200) для демонстрации работы UI Plugin System!

---

## ⚙️ Требования

**Минимальные:**
- Windows 10/11
- Qt 6.8.3 (установить в `D:/Qt/6.8.3/mingw_64`)
- MinGW 13.1.0
- CMake 3.16+

**Или:**
- Qt Creator (всё включено)

---

## 🎨 Выбор UI

Доступно **3 UI модуля** (автоматически регистрируются):

| UI | Приоритет | Описание |
|----|-----------|----------|
| **TestUI** | 200 | Простое Hello World окно (для тестирования) |
| **NewWindows** | 100 | Современный UI с frameless окном |
| **OldWindows** | 50 | Классический UI |

По умолчанию выбирается **TestUI** (самый высокий приоритет).

### Сменить UI через приоритет:

Отредактируй приоритет в Factory файле:

**TestUI** - `Source/UI/TestUI/TestUIFactory.cpp`:
```cpp
int getPriority() const override { return 200; }  // Самый высокий
```

**NewWindows** - `Source/UI/NewWindows/NewWindowsFactory.cpp`:
```cpp
int getPriority() const override { return 100; }
```

**OldWindows** - `Source/UI/Windows/OldWindowsFactory.cpp`:
```cpp
int getPriority() const override { return 50; }
```

Измени приоритет на нужный (больше = выше) и пересобери!

### Через конфиг (ui_config.json):
```json
{
  "ui_modules": [
    {
      "name": "TestUI",
      "priority": 200,
      "enabled": true
    },
    {
      "name": "NewWindows",
      "priority": 100,
      "enabled": true
    }
  ]
}
```

---

## 🔧 Ручная сборка

```bash
# 1. Создать build папку
mkdir build && cd build

# 2. Сконфигурировать
cmake .. -G "MinGW Makefiles" -DUSE_NEW_UI=ON -DCMAKE_BUILD_TYPE=Release

# 3. Собрать (используя все ядра)
cmake --build . --config Release -j8

# 4. Запустить
.\Release\SoundLink.exe
```

---

## 📱 Android сборка

```bash
# Используй Qt Creator с Android Kit
# Или:
cmake .. -DANDROID=ON -DANDROID_ABI=arm64-v8a
cmake --build .
```

---

## 🐛 Проблемы?

### Qt не найден?
Измени путь в `CMakeLists.txt`:
```cmake
set(CMAKE_PREFIX_PATH "D:/Qt/6.8.3/mingw_64")
```

### MinGW не найден?
```bash
# Добавь в PATH:
set PATH=D:\Qt\Tools\mingw1310_64\bin;%PATH%
```

### Ошибки линковки?
```bash
# Пересобери:
rebuild.bat
```

---

## ✨ Что нового в этой версии?

- ✅ **Zero-dependency UI architecture** - main.cpp не зависит от UI
- ✅ **Auto-registration system** - UI модули регистрируются автоматически
- ✅ **Factory pattern** - IUIFactory интерфейс
- ✅ **Plugin support** - готово для динамических плагинов
- ✅ **Config-based UI** - выбор UI через JSON

---

## 📚 Документация

- Архитектура: см. `Source/Core/Public/Framework/UIFactory.h`
- Примеры: см. `main_with_plugin_system_example.cpp`
- Конфиг: см. `ui_config.json`

---

**Создано с ❤️ Claude Code**
