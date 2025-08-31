# Qt Music Player

**Qt Music Player** is a cross-platform open-source music player.  
It allows you to **listen, sync, download music, and modularly customize widgets and functionality** on any platform: Windows, Linux, Android.  
The project is built with **Qt + CMake** and can be compiled in just a few steps.

---

## Environment Setup and Dependencies

### Download the project and dependencies

1. Clone the repository:
```bash
git clone https://github.com/Stalker72742/MusicPlayer.git
cd MusicPlayer
```

2. Install dependencies

- Install **Qt 6.8.3** and **CMake (3.16+)**.
- Make sure all Git submodules are initialized and updated.

3. For Android projects

- If you are building for Android, install all dependencies required by Qt Creator in  
  **Edit → Preferences → SDKs → Android**.
- Also install **NDK 29.0.13599879** and platform dependencies for **Android 34**,  
  or specify the path to your NDK and Android platform version in the CMake settings.

After that, run:
```bash
adb devices
```

If `adb` is not added to your system `PATH`, open a terminal in the folder containing `adb.exe`.

Then specify your device in the CMake settings so that the APK is installed directly on it.

4. Generate CMake files and build:
```bash
cmake -B build -S .
```
