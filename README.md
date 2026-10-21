# r_anniversary

A C99 ASCII animation built with raylib. A copper eye opens, then gives way to a colorful fireworks display and a couple sharing a flower. The animation ends with **4 years together**.

Every frame is drawn from colored ASCII characters. The application does not play back video or load scene images.

## Build and run

Requirements: CMake 3.20+, a C compiler, and raylib 5.0 or newer. From the project directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\r_anniversary.exe --fullscreen
```

If raylib is not installed, CMake fetches and builds version 5.5. This requires Git and internet access. To use a local raylib installation, pass its directory with `-DRAYLIB_ROOT="C:/path/to/raylib"` when configuring.

The app opens in a 1280 × 720 window by default. The full animation loop is 38.5 seconds.

## Controls

| Key | Action |
| --- | --- |
| Space | Pause or resume |
| R | Restart |
| Up / Down | Change playback speed |
| Left / Right | Step by 1/30 second and pause |
| H | Show or hide the on-screen help |
| F11 / Alt+Enter | Toggle fullscreen |
| F12 | Save a PNG capture |
| Esc | Quit |

## Command-line options

Run `r_anniversary.exe --help` to see all options. For example, start paused at a specific time:

```powershell
.\build\Release\r_anniversary.exe --time 27.1
```

Render a 4K frame without showing a window (the output directory must already exist):

```powershell
.\build\Release\r_anniversary.exe --hidden --time 16.4 --resolution 3840x2160 --capture build/fireworks-4k.png
```

The app also supports exporting a set of frames, selecting a font, and limiting playback by frame count.
