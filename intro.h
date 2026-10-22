#ifndef EMA_INTRO_H
#define EMA_INTRO_H
#include "scene.h"

typedef AsciiCell (*IntroEyeSampler)(float x, float y);
#define INTRO_EYE_BLEND_START (INTRO_DURATION - 1.6f)
/* Calendar, seated Ema and camera move, joining the already rendered eye. */
void intro_render(float time, AsciiCell cells[SCENE_ROWS][SCENE_COLS], IntroEyeSampler eye);
#endif
