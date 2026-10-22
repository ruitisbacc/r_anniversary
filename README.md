# r_anniversary

A C99 ASCII animation built with raylib. A calendar fades in on **2026**, turns its pages through the years, and stops on **2085**. It dissolves into a quiet room with **Ema, aged 80**, visible from her hair to her slippers, sitting in a wooden armchair with an open photo album on her lap. She remembers the fourth anniversary in 2026. The camera slowly approaches her eye, joining the copper iris and stars as we enter that memory. Colorful fireworks form a couple sharing a bouquet. They approach, kiss, embrace, and turn together as a ring of flowers blooms around them. The animation ends with **4 years together**.

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

The app opens in a 1280 × 720 window by default. The full animation loop is 74.5 seconds. To configure a fresh, explicitly x64 build, use a separate directory: `cmake -S . -B build-vs-x64 -G "Visual Studio 17 2022" -A x64`, then use `build-vs-x64` in the build, test, and run commands above.

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
| 0–2 s | Calendar fades in from black over 1.35 s, holding on `2026` |
| 2–6.8 s | 59 pages turn around their top binding, accelerating and slowing toward `2085` |
| 6.8–7.7 s | Calendar holds on `2085` |
| 7.7–9.3 s | Calendar dissolves into Ema's room; the same calendar appears on the wall |
| 9.3–14.5 s | Full seated figure, age caption, photo album and `NASE 4. VYROCI. / 2026`; subtle breathing and hand movement |
| 14.5–24 s | Camera approaches her face and eye, with a matched iris transition |
| 24–35.5 s | Eye and transition into stars |
| 35.5–43.5 s | Colorful fireworks |
| 44–49 s | Larger rocket rises and forms the couple |
| 49.5–53 s | Boy offers the bouquet; girl receives it |
| 53–55.5 s | Both step closer and lean in |
| 55.5–57 s | Kiss with closed eyes and floating ASCII hearts |
| 57–58.5 s | Arms move into an embrace; girl keeps the bouquet |
| 59–66 s | Couple turns together; 34 flowers sprout and open around them |
| 66–68.4 s | Couple and garden dissolve into colored particles |
| 68–73 s | `4 years together` |
| 73–74.5 s | Fade to black and repeat |

The 24-second introduction in `intro.c` is sampled onto the character grid in continuous world coordinates. The calendar has a binding, a month grid and pages that rotate, foreshorten, reveal their unprinted reverse and expose the next year. Ema has shaded cheeks and nose, lower eyelids, crow's feet, cheek and mouth folds, skin texture, ash/gold/silver hair strands and an irregular fringe. Her body adds a knitted cardigan with buttons, bent sleeves, age-marked hands, a ring, a pleated skirt, stockings and separate slippers. Her open album contains a miniature couple and the year 2026. The room includes a dusk window, a floor lamp, a rug and the complete wooden chair.

An eased camera moves toward the eye on the viewer's left, keeping it inside the frame throughout the approach. Both portrait eyes share the subsequent scene's iris renderer. A final blend matches the surrounding skin without closing the eye or fading to black. The introduction is separate from the celebration's local timeline; changing its duration shifts the celebration as a whole.

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
.\build\Release\r_anniversary.exe --time 12
```

Use `--time 1.6` for the calendar in 2026, `--time 3.8` during page turning, `--time 7` for 2085, `--time 12` for the full seated Ema, or `--time 18.5` during the close-up. Press Space to continue playback. Use `--time 54` for the couple's approach, `--time 56` for the kiss, or `--time 63` for the blooming garden. R restarts at the calendar's fade in.

Render a 4K frame without showing a window (the output directory must already exist):

```powershell
.\build\Release\r_anniversary.exe --hidden --time 40.4 --resolution 3840x2160 --capture build/fireworks-4k.png
```

Export key frames including all stages of the new scene:

```powershell
New-Item -ItemType Directory -Force build/seated-intro-captures
.\build\Release\r_anniversary.exe --hidden --export-frames build/seated-intro-captures --resolution 1920x1080
```

The app also supports selecting a font and limiting playback by frame count.

For a playback check, run `.\build\Release\r_anniversary.exe --hidden --frames 1500`. Animation advances using elapsed time, so this frame limit does not specify an exact duration. The final Windows check rendered 1500 frames at 1733 × 975 in 38.347 seconds (about 39 FPS overall), covering the entire introduction and its transition into the anniversary sequence. A hidden-window playback check exercises rendering; visual review uses exported frames.

## Verification and visual limits

The intro was configured and built with MSVC in Release mode without compiler warnings. All five CTest tests passed. Checks cover the full loop, repeatable frames, the initial fade, the actual printed digits `2026` and `2085`, the final calendar hold, the visible head, sleeves, album, both slippers and chair, portrait enlargement, the continuous splice into the eye, bouquet transfer, approach, embracing arms, changing screen order during the dance, progressive garden growth, and the final title. Key frames were rendered at 1920 × 1080 with raylib/OpenGL in a hidden window and visually inspected.

The calendar, room, woman, captions, couple and garden are original extensions beyond the reference video. Ema is a stylized frontal ASCII illustration, not a photographic likeness; fine wrinkles, fingers and album contents are limited by the 256 × 82 grid and become clearer during the zoom. Page turns use a projected paper surface. Captions use unaccented letters to preserve ASCII. The final eye blend changes the surrounding skin to the original copper palette. Rotation of the couple uses projected character silhouettes rather than full 3D figures; at a side view the two silhouettes overlap. The original phone recording's perspective and surrounding UI are excluded. For the final local viewing check, play from the beginning at fullscreen and watch the page-turn pacing, the full seated pose and the continuous move into her eye.
