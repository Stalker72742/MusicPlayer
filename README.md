# SoundLink

**SoundLink** is a desktop music player for Windows built with Qt 6, C++23 and QML.
It plays your local music library and music from YouTube — streamed or downloaded — in one place.

Current version: **0.2**

## Features

- **Local library** — scans your music folders in the background, reads tags and duration, caches the result
  so the library is available immediately on startup.
- **YouTube search and streaming** — search from the title bar (InnerTube, with yt-dlp as a fallback),
  infinite scrolling of results, playback without downloading.
- **Downloads** — a download queue for online tracks; a downloaded file becomes the local copy of the same track,
  so likes, tags and playlists stay.
- **Playlists** — static playlists with drag-and-drop ordering and smart playlists built from rules
  (artist, album, genre, tag, year, duration, play count, liked, source, added / played within N days).
- **Search everywhere** — one search field for pages, settings, tracks, playlists and YouTube, with tags:
  `yt:`, `lib:`, `go:`, `pl:`, `s:<category>;<setting>`.
- **Player** — queue, shuffle, repeat (off / all / one), perceptual volume, output device selection.
- **System integration** — tray icon (closing the window hides it to the tray), media keys and the Windows media
  overlay (System Media Transport Controls), Discord Rich Presence.
- **Tools managed for you** — yt-dlp is installed and kept up to date automatically; ffmpeg and deno are
  installed on first start.
- **Browser extension (optional)** — the SoundLink Bridge extension (Chrome, Firefox 128+) lends the player
  a PO token from your browser's YouTube session for more reliable streaming. Paired once with a 6-digit code;
  cookies never leave the browser unless you allow it.
- **Built-in updates** — the app checks GitHub releases and installs updates through `Updater.exe`.

Not done yet: interface translations, volume normalization and crossfade (the settings exist but are not applied),
cover art for local files.

## Repository layout

| Path | Contents |
|---|---|
| `source/` | CMake root: `main.cpp`, `core/` (framework and subsystems), `ui/` (tray icon, QML module, view models) |
| `extension/` | The browser extension (MV3, one codebase for Chrome and Firefox) |
| `third_party/QTomlUtils` | Submodule: TOML configs with default layers ([QTomlUtils](https://github.com/Stalker72742/QTomlUtils)) |
| `third_party/QAppUpdater` | Submodule: updater, `bin/` layout and release packaging ([QAppUpdater](https://github.com/Stalker72742/QAppUpdater)) |
| `build-release.bat` | One-click Release build |
| `Doxyfile` | API documentation config |

## Building (Windows, MinGW)

### Requirements

- Qt **6.8.3** for MinGW 64-bit with the modules Multimedia and WebSockets
- MinGW 13.1 (`Qt/Tools/mingw1310_64`), CMake 3.22+ and Ninja — all available from the Qt installer

### Get the sources

```bash
git clone --recursive https://github.com/Stalker72742/MusicPlayer.git
```

In an existing clone, fetch the submodules with:

```bash
git submodule update --init
```

### Configure and build

The CMake root is `source/`, not the repository root:

```bash
cmake -S source -B source/build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/mingw_64 -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe
```

```bash
cmake --build source/build/Release
```

If Ninja is not in `PATH`, add `-DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe` to the configure command.

The build output looks like an installed program: `SoundLink.exe` and `Updater.exe` with a single `bin/` folder
holding the Qt, FFmpeg and MinGW runtime, so the exe runs straight from the build folder without Qt in `PATH`.

### Release builds with presets

`build-release.bat` builds a standalone copy into `dist/` (`build-release.bat package` also makes
`packages/SoundLink-<version>-win64.zip`). It uses the workflow presets `release-dist` / `release-package`
from `source/CMakeUserPresets.json`. That file is machine-specific and not committed — create it with your paths:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "QtMinGW",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "generator": "Ninja",
            "cacheVariables": {
                "CMAKE_C_COMPILER": "D:/Qt/Tools/mingw1310_64/bin/gcc.exe",
                "CMAKE_CXX_COMPILER": "D:/Qt/Tools/mingw1310_64/bin/g++.exe",
                "CMAKE_PREFIX_PATH": "D:/Qt/6.8.3/mingw_64",
                "CMAKE_MAKE_PROGRAM": "D:/Qt/Tools/Ninja/ninja.exe"
            }
        },
        {
            "name": "QtMinGW-Release",
            "inherits": "QtMinGW",
            "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
        }
    ],
    "buildPresets": [
        { "name": "release-dist", "configurePreset": "QtMinGW-Release", "targets": ["SoundLink_deploy"] },
        { "name": "release-package", "configurePreset": "QtMinGW-Release", "targets": ["SoundLink_package"] }
    ],
    "workflowPresets": [
        {
            "name": "release-dist",
            "steps": [
                { "type": "configure", "name": "QtMinGW-Release" },
                { "type": "build", "name": "release-dist" }
            ]
        },
        {
            "name": "release-package",
            "steps": [
                { "type": "configure", "name": "QtMinGW-Release" },
                { "type": "build", "name": "release-package" }
            ]
        }
    ]
}
```

The version is set in one place — `project(SoundLink VERSION ...)` in `source/CMakeLists.txt` — and goes into
the exe resources, the app's About screen and the release package name.

## Browser extension

The extension is optional: without it streaming goes through yt-dlp alone.

1. In SoundLink open **Settings › Online** and click **Open extension folder**.
2. Load that folder as an unpacked extension: `chrome://extensions` → Developer mode → **Load unpacked**
   (Chrome), or `about:debugging` → **Load Temporary Add-on** (Firefox).
3. Enter the 6-digit code SoundLink shows into the extension popup.

The extension only connects to the app on `127.0.0.1`. Its PO token appears after any video has been played
in the browser.

## Where data is stored

- Settings — `Saved/Settings.toml` next to the exe (only values that differ from the defaults).
- Library cache, playlists, download queue, paired browsers, yt-dlp / ffmpeg / deno —
  `%LOCALAPPDATA%/SoundLink`.

## API documentation

Headers are documented with Doxygen comments. To generate HTML documentation into `docs/`, run from the
repository root:

```bash
doxygen Doxyfile
```

## License

[MIT](LICENSE)
