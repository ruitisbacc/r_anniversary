# r_anniversary

A C99 ASCII animation built with raylib. **2085** appears above **EMA, 80 let**, fades away, and reveals an elderly woman with a dirty-blond bob and bangs. The camera slowly approaches her eye, joining the copper iris and stars. Colorful fireworks form a couple sharing a bouquet. They approach, kiss, embrace, and turn together as a ring of flowers blooms around them. The animation ends with **4 years together**.

Every frame is drawn from colored ASCII characters. The application does not play back video or load scene images.

## Build and run

Requirements: CMake 3.20+, a C compiler, and raylib 5.0 or newer. From the project directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\r_anniversary.exe --fullscreen
```

If raylib is not installed, CMake fetches and builds version 5.5. This requires Git and internet access. To use a local raylib installation, pass its directory with `-DRAYLIB_ROOT="C:/path/to/raylib"` when configuring.

The app opens in a 1280 × 720 window by default. The full animation loop is 66.5 seconds. To configure a fresh, explicitly x64 build, use a separate directory: `cmake -S . -B build-vs-x64 -G "Visual Studio 17 2022" -A x64`, then use `build-vs-x64` in the build, test, and run commands above.

MinGW alternative (with GCC and mingw32-make on PATH):

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
.\build-mingw\r_anniversary.exe --fullscreen
```

## Scene timing

| Time | Action |
| --- | --- |
| 0–4.1 s | `2085` above `EMA, 80 let`; fade in, hold, fade out |
| 4.1–7 s | Portrait fades in, then holds |
| 7–16 s | Slow camera move into her eye; matched iris transition |
| 16–27.5 s | Eye and transition into stars |
| 27.5–35.5 s | Colorful fireworks |
| 36–41 s | Larger rocket rises and forms the couple |
| 41.5–45 s | Boy offers the bouquet; girl receives it |
| 45–47.5 s | Both step closer and lean in |
| 47.5–49 s | Kiss with closed eyes and floating ASCII hearts |
| 49–50.5 s | Arms move into an embrace; girl keeps the bouquet |
| 51–58 s | Couple turns together; 34 flowers sprout and open around them |
| 58–60.4 s | Couple and garden dissolve into colored particles |
| 60–65 s | `4 years together` |
| 65–66.5 s | Fade to black and repeat |

The portrait in `intro.c` is a procedural surface sampled onto the character grid: shaded cheeks and nose, lower eyelids, crow's feet, cheek and mouth folds, skin texture, ash/gold/silver hair strands, an irregular fringe, and a knitted top. An eased camera moves toward the eye on the viewer's left. Both portrait eyes share the subsequent scene's iris renderer. A final blend matches the surrounding skin without closing the eye or fading to black. The 16-second introduction is separate from the existing celebration's local timeline.

The dance accelerates and slows smoothly. Depth controls which person appears in front, the skirt flares slightly, and stems sway gently. Flower positions and growth delays are deterministic; pausing, stepping, or seeking to the same time produces the same image.

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
.\build\Release\r_anniversary.exe --time 6
```

Use `--time 2` for the opening title, `--time 6` for the portrait, or `--time 11` midway through the zoom. Press Space to continue playback. Use `--time 45` for the couple's approach, `--time 48` for the kiss, or `--time 55` for the blooming garden. R restarts at the opening title.

Render a 4K frame without showing a window (the output directory must already exist):

```powershell
.\build\Release\r_anniversary.exe --hidden --time 32.4 --resolution 3840x2160 --capture build/fireworks-4k.png
```

Export key frames including all stages of the new scene:

```powershell
New-Item -ItemType Directory -Force build/intro-captures
.\build\Release\r_anniversary.exe --hidden --export-frames build/intro-captures --resolution 1920x1080
```

The app also supports selecting a font and limiting playback by frame count.

For a short playback check, run `.\build\Release\r_anniversary.exe --hidden --frames 1080`. The tested Windows machine rendered those frames in 19.508 seconds (about 55 FPS), including the title, portrait, zoom, and beginning of the eye sequence. This was a hidden-window playback check; the visual review used exported frames.

## Verification and visual limits

The intro was configured and built with MSVC in Release mode without compiler warnings. All five CTest tests passed. Checks cover the full loop, repeatable frames, separated title lines, portrait reveal and enlargement, the continuous splice into the eye, bouquet transfer, approach, embracing arms, changing screen order during the dance, progressive garden growth, and the final title. Key frames were rendered at 1920 × 1080 with raylib/OpenGL in a hidden window and visually inspected.

The woman, opening text, couple, and garden are original extensions beyond the reference video. The portrait is a stylized frontal ASCII illustration, not a photographic likeness; fine wrinkles and individual hairs are limited by the 256 × 82 grid. The final eye blend also changes the surrounding skin to the original copper palette. Rotation of the couple uses projected character silhouettes rather than full 3D figures; at a side view the two silhouettes overlap. The original phone recording's perspective and surrounding UI are excluded.
