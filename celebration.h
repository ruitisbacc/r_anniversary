#ifndef CELEBRATION_H
#define CELEBRATION_H
#include "scene.h"

/* Add the fireworks, animated gift and title to a character grid. */
void celebration_render(float time, AsciiCell cells[SCENE_ROWS][SCENE_COLS]);
#endif
