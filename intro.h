#ifndef EMA_INTRO_H
#define EMA_INTRO_H
#include "scene.h"

typedef AsciiCell (*IntroEyeSampler)(float x, float y);
/* Overlays the title/portrait, then joins the already rendered eye. */
void intro_render(float time, AsciiCell cells[SCENE_ROWS][SCENE_COLS], IntroEyeSampler eye);
#endif
