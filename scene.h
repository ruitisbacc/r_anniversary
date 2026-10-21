#ifndef EYE_SPACE_SCENE_H
#define EYE_SPACE_SCENE_H

#define SCENE_COLS 256
#define SCENE_ROWS 82
#define CELL_WIDTH 8
#define CELL_HEIGHT 14
#define SCENE_WIDTH (SCENE_COLS * CELL_WIDTH)
#define SCENE_HEIGHT (SCENE_ROWS * CELL_HEIGHT)
#define INTRO_DURATION 16.0f
#define STORY_DURATION 50.5f
#define SCENE_DURATION (INTRO_DURATION + STORY_DURATION)

#define FIREWORKS_START 11.5f
#define FIREWORKS_END 19.5f
#define FINALE_LAUNCH 20.0f
#define FINALE_BURST 22.8f
#define COUPLE_READY 25.0f
#define GIFT_START 25.5f
#define GIFT_END 29.0f
#define APPROACH_END 31.5f
#define KISS_END 33.0f
#define HUG_READY 34.5f
#define SPIN_START 35.0f
#define SPIN_END 42.0f
#define DISSOLVE_START 42.0f
#define TITLE_START 44.0f
#define FADE_START 49.0f

typedef struct {
    unsigned char glyph;
    unsigned char r, g, b;
} AsciiCell;
typedef struct {
    /* Time within the eye/celebration sequence (zero throughout the intro). */
    float time, zoom, pupil_radius, stars, exposure;
} SceneState;

/* Pure functions: no window, resources, random state, or frame-rate dependency. */
float scene_wrap_time(float seconds);
SceneState scene_state(float seconds);
void scene_render(float seconds, AsciiCell cells[SCENE_ROWS][SCENE_COLS]);

#endif
