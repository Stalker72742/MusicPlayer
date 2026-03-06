# SoundLink - Project Guide for Claude

## Project Overview

**SoundLink** - Qt6 music player with a modular UI plugin system.
Built with C++23 + Qt6 (MinGW on Windows).

Main executable: `SoundLink.exe`
Core library: `MusicPlayerCore.dll` (SHARED)

---

## Architecture: Mod System

The project uses a **dynamic plugin system** where UI modules are separate .dll files
loaded at runtime. The core application has **zero direct dependencies** on UI code.

### Key Principle: Zero-Dependency Design
- `main.cpp` includes ONLY `UIPluginSystem.h` and core subsystems
- NO UI headers in main.cpp or AppInstance
- UI modules are loaded dynamically via `QLibrary`
- Communication through `IUIFactory` pure interface

---

## Directory Structure

```
MusicPlayer/
├── main.cpp                         # Entry point, zero UI dependencies
├── ui_config.json                   # Active UI configuration
├── ui_config.example.json           # Example config with all options
├── CMakeLists.txt                   # Root build file
│
├── Source/
│   ├── Core/                        # MusicPlayerCore.dll
│   │   ├── CMakeLists.txt           # SHARED library, WINDOWS_EXPORT_ALL_SYMBOLS ON
│   │   ├── Public/
│   │   │   └── Framework/
│   │   │       ├── UIFactory.h      # IUIFactory interface + macros
│   │   │       ├── UIPluginSystem.h # UIPluginLoader class
│   │   │       └── AppInstance.h    # Singleton app instance
│   │   └── Private/
│   │       └── Framework/
│   │           ├── UIPluginSystem.cpp
│   │           └── AppInstance.cpp
│   │
│   └── UI/
│       ├── NewWindows/              # NewWindowsUI.dll - modern UI
│       │   ├── CMakeLists.txt
│       │   ├── NewWindowsFactory.h/cpp
│       │   ├── MainWindow/
│       │   ├── TitleBar/
│       │   └── BottomBar/
│       ├── Windows/                 # OldWindowsUI.dll - classic UI
│       │   ├── CMakeLists.txt
│       │   ├── OldWindowsFactory.h/cpp
│       │   ├── MainWindow/
│       │   ├── MediaLib/
│       │   ├── OldWidgets/
│       │   └── Subsystems/
│       ├── TestUI/                  # TestUI.dll - Hello World for testing
│       │   ├── CMakeLists.txt
│       │   ├── TestUIFactory.h/cpp
│       │   ├── TestMainWindow.h/cpp
│       └── Android/                 # Android QML UI (not a plugin)
│
└── docs/
    └── UI_CONFIG_FORMAT.md          # JSON config documentation
```

---

## Core Interfaces

### IUIFactory (`Source/Core/Public/Framework/UIFactory.h`)

Every UI module must implement this interface:

```cpp
class IUIFactory {
public:
    virtual QWidget* createMainWindow() = 0;
    virtual const char* getName() const = 0;     // "TestUI", "NewWindows"
    virtual const char* getVersion() const = 0;  // "1.0.0"
    virtual const char* getPlatform() const = 0; // "windows", "android", "all"
    virtual int getPriority() const = 0;         // 200=highest, 50=lowest
    virtual void initialize(AppInstance* app) = 0;
    virtual void shutdown() = 0;
};
```

### Priority System
| Priority | Type | Example |
|----------|------|---------|
| 200+ | Test/Dev UI | TestUI (200) |
| 100-199 | Production UI | NewWindows (100), OldWindows (50) |
| 50-99 | Legacy/Fallback | OldWindows (50) |

### Key Macros (`UIFactory.h`)

```cpp
// For dynamic loading (.dll) - exports C function createUIFactory()
UI_REGISTER_FACTORY(MyFactory, "MyModuleName")

// For static linking - auto-registers via global constructor (NOT USED NOW)
UI_AUTO_REGISTER(MyFactory, "MyModuleName")
```

**IMPORTANT**: Both macros are currently in factory .cpp files but modules
are loaded as SHARED libraries. `UI_REGISTER_FACTORY` is the one that matters
for dynamic loading - it exports `createUIFactory()` C function.

---

## Creating a New UI Module

### 1. Create folder: `Source/UI/MyUI/`

### 2. CMakeLists.txt (copy from TestUI pattern):
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyUI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(MyUI SHARED
    MyUIFactory.h
    MyUIFactory.cpp
    MyMainWindow.h
    MyMainWindow.cpp
)

target_link_libraries(MyUI PUBLIC Qt6::Core Qt6::Widgets)

# Link with Core only when building from root project
if(TARGET MusicPlayerCore)
    target_link_libraries(MyUI PRIVATE MusicPlayerCore)
endif()

target_include_directories(MyUI
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Core headers
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Subsystems
)

target_compile_definitions(MyUI PRIVATE UI_LIBRARY_EXPORT)

if(WIN32)
    set_target_properties(MyUI PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set_target_properties(MyUI PROPERTIES
    CXX_STANDARD 23
    PREFIX ""
    OUTPUT_NAME "MyUI"
)
```

### 3. Factory header (MyUIFactory.h):
```cpp
#pragma once
#include "UIFactory.h"

class MyUIFactory : public IUIFactory {
public:
    QWidget* createMainWindow() override;
    const char* getName() const override { return "MyUI"; }
    const char* getVersion() const override { return "1.0.0"; }
    const char* getPlatform() const override { return "windows"; }
    int getPriority() const override { return 150; }
    void initialize(AppInstance* app) override;
    void shutdown() override;
private:
    AppInstance* m_app = nullptr;
};
```

### 4. Factory implementation (MyUIFactory.cpp):
```cpp
#include "MyUIFactory.h"
#include "MyMainWindow.h"  // Include here, NOT in header!

QWidget* MyUIFactory::createMainWindow() {
    return new MyMainWindow();
}
void MyUIFactory::initialize(AppInstance* app) { m_app = app; }
void MyUIFactory::shutdown() { m_app = nullptr; }

// MUST have this - exports createUIFactory() for dynamic loading
UI_REGISTER_FACTORY(MyUIFactory, "MyUI")
```

### 5. Register in root CMakeLists.txt:
```cmake
add_subdirectory(Source/UI/MyUI)

# In WINDOWS_UI_LIBRARY list:
set(WINDOWS_UI_LIBRARY NewWindowsUI OldWindowsUI TestUI MyUI)

# Add POST_BUILD copy command:
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:MyUI>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying MyUI.dll to executable directory"
)
```

### 6. Add to ui_config.json:
```json
{
  "name": "MyUI",
  "platform": "windows",
  "library_path": "MyUI.dll",
  "priority": 150,
  "enabled": true,
  "settings": {}
}
```

---

## Build System

### Root CMakeLists.txt Key Sections

```cmake
# 1. Build all UI libraries
add_subdirectory(Source/UI/NewWindows)
add_subdirectory(Source/UI/Windows)
add_subdirectory(Source/UI/TestUI)

# 2. UI libraries listed for dependency tracking (auto-builds them)
set(WINDOWS_UI_LIBRARY NewWindowsUI OldWindowsUI TestUI)

# 3. Link them PRIVATELY to SoundLink (creates build dependency)
target_link_libraries(SoundLink PRIVATE MusicPlayerCore ${WINDOWS_UI_LIBRARY} ...)

# 4. POST_BUILD: auto-copy each .dll to exe folder
add_custom_command(TARGET SoundLink POST_BUILD
    COMMAND copy_if_different $<TARGET_FILE:TestUI> $<TARGET_FILE_DIR:SoundLink>
    ...
)
```

**Why link AND dynamic load?**
- Linking as PRIVATE forces CMake to build UI libs before SoundLink
- POST_BUILD copies .dll to exe directory
- At runtime, SoundLink does NOT use linked symbols - UIPluginLoader
  loads .dll fresh via QLibrary and calls `createUIFactory()`

### Build Commands
```bash
# Full rebuild
rebuild.bat

# Normal build
build.bat

# Pull latest
git pull origin claude/explore-capabilities-011CUtTTLbNXbpUwdrZsWDkj
```

---

## Runtime: How UI Loading Works

```
SoundLink.exe starts
    │
    ▼
AppInstance::initializeUISystem()
    │
    ├─► UIPluginLoader::scanForPlugins(".")
    │       Searches for: TestUI.dll, NewWindowsUI.dll, OldWindowsUI.dll
    │       Loads each via QLibrary
    │       Calls createUIFactory() → gets IUIFactory*
    │       Registers in m_configs map
    │
    ├─► loadConfigFromFile("ui_config.json")
    │       Updates priority/enabled from JSON
    │       Adds library_path if not already loaded
    │
    └─► AppInstance::createUIFromConfig()
            │
            ▼
        UIPluginLoader::selectBestUI("windows")
            Filters: platform == "windows" AND enabled == true
                     AND (factory != null OR libraryPath not empty)
            Sorts by priority (highest first)
            Returns best UIConfig*
            │
            ▼
        UIPluginLoader::createUI(name)
            factory->createMainWindow() → QWidget*
            window->show()
```

---

## ui_config.json Format

```json
{
  "ui_modules": [
    {
      "name": "TestUI",           // Must match IUIFactory::getName()
      "platform": "windows",      // "windows", "android", "all"
      "library_path": "TestUI.dll", // Relative to exe, used if not already loaded
      "priority": 200,            // Higher = preferred
      "enabled": true,
      "settings": {               // Custom settings, accessible via UIConfig::settings
        "theme": "light"
      }
    }
  ]
}
```

**Selection logic in UIPluginLoader::selectBestUI():**
1. Filter by platform
2. Filter `enabled == true`
3. Filter `factory != null || !libraryPath.isEmpty()`
4. Return highest priority

---

## Key Files to Know

| File | Purpose |
|------|---------|
| `Source/Core/Public/Framework/UIFactory.h` | IUIFactory interface, macros |
| `Source/Core/Public/Framework/UIPluginSystem.h/cpp` | Plugin loader, scanner |
| `Source/Core/Private/Framework/AppInstance.cpp` | UI system init, scanForPlugins call |
| `Source/UI/TestUI/TestUIFactory.cpp` | Reference implementation |
| `CMakeLists.txt` (root) | Build + auto-copy DLLs |
| `ui_config.json` | Active module configuration |
| `docs/UI_CONFIG_FORMAT.md` | Config documentation |

---

## Common Problems & Fixes

### "Found 0 registered UI factories"
- Static library initialization issue (old problem, now fixed with SHARED)
- All UI modules are now SHARED (.dll), not STATIC

### "Failed to resolve createUIFactory"
- UI_REGISTER_FACTORY macro not in the .cpp file
- Check that `UI_REGISTER_FACTORY(FactoryClass, "Name")` exists

### "Cannot find -lMusicPlayerCore" (standalone build)
- Building UI subproject standalone without building Core first
- Fixed with `if(TARGET MusicPlayerCore)` conditional linking
- Must still have MusicPlayerCore.dll in same folder for linking

### "No suitable UI found for platform: windows"
- ui_config.json loaded but no factories registered
- Check that .dll files exist in exe directory
- Check that `library_path` in config points to correct .dll

### DLLs not in exe folder
- POST_BUILD commands in root CMakeLists.txt auto-copy them
- Must build from ROOT project, not UI subproject

---

## Planned / TODO

Based on discussion, these features are planned but not yet implemented:

1. **Mods/ folder structure** - organize .dll into `Mods/ModuleName/` subfolders
2. **module.json per mod** - metadata, version, dependencies per module
3. **Dependency management** - `getRequiredDependencies()` in IUIFactory
4. **Semantic versioning** - `>=2.0.0` version checks between modules
5. **Auto-load order** - load Core before UI modules that depend on it
6. **Config generator** - CLI tool to scan Mods/ and generate ui_config.json
7. **Deploy script** - copy build to release folder automatically

---

## Git Branch

Active development branch:
```
claude/explore-capabilities-011CUtTTLbNXbpUwdrZsWDkj
```
