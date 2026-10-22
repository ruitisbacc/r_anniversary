#ifndef ASCII_DRAW_H
#define ASCII_DRAW_H
#include "raylib.h"
#include "scene.h"

typedef struct {
    Shader shader;
    Texture2D cells, atlas;
    int atlas_location, tile_width, tile_height;
    const char *font_path;
} AsciiRenderer;

bool ascii_renderer_init(AsciiRenderer *renderer,const char *font_path);
bool ascii_renderer_resize(AsciiRenderer *renderer,int width,int height);
void ascii_renderer_draw(AsciiRenderer *renderer,const AsciiCell *cells,int width,int height);
void ascii_renderer_unload(AsciiRenderer *renderer);
#endif
