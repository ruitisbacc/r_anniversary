#include "ascii_draw.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* One RGBA texel holds one genuine ASCII cell (glyph, red, green, blue).
   The GPU only draws those characters; all scene geometry stays in C99. */
static const char *fragment_body=
    "#ifdef GL_ES\nprecision highp float;\n#endif\n"
    "#if __VERSION__ >= 130\n"
    "in vec2 fragTexCoord; out vec4 finalColor;\n"
    "#define SAMPLE texture\n#define OUTPUT finalColor\n"
    "#else\nvarying vec2 fragTexCoord;\n"
    "#define SAMPLE texture2D\n#define OUTPUT gl_FragColor\n#endif\n"
    "uniform sampler2D texture0; uniform sampler2D glyphAtlas;\n"
    "uniform vec2 gridSize;\n"
    "void main() {\n"
    "  vec2 cellPosition = fragTexCoord*gridSize;\n"
    "  vec4 cell = SAMPLE(texture0,(floor(cellPosition)+0.5)/gridSize);\n"
    "  float glyph = clamp(floor(cell.r*255.0+0.5)-32.0,0.0,94.0);\n"
    "  vec2 tile = vec2(mod(glyph,16.0),floor(glyph/16.0));\n"
    "  float ink = SAMPLE(glyphAtlas,(tile+fract(cellPosition))/vec2(16.0,6.0)).a;\n"
    "  ink = smoothstep(0.08,0.70,ink);\n"
    "  OUTPUT = vec4(pow(cell.gba,vec3(0.85))*ink,1.0);\n"
    "}\n";

static Font load_font(const char *requested,int size) {
    const char *paths[]={
        "C:/Windows/Fonts/consolab.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Bold.ttf",
        "/System/Library/Fonts/Menlo.ttc"
    };
    const char *path=requested,*windows_dir=getenv("WINDIR");
    char windows_font[512];
    if (!path && windows_dir) {
        snprintf(windows_font,sizeof(windows_font),"%s/Fonts/consolab.ttf",windows_dir);
        if (FileExists(windows_font)) path=windows_font;
    }
    for (int i=0;!path && i<(int)(sizeof(paths)/sizeof(paths[0]));i++)
        if (FileExists(paths[i])) path=paths[i];
    if (path) {
        int file_size=0,codepoints[95];
        for (int i=0;i<95;i++) codepoints[i]=32+i;
        unsigned char *data=LoadFileData(path,&file_size);
        GlyphInfo *glyphs=data?LoadFontData(data,file_size,size,codepoints,95,FONT_DEFAULT):NULL;
        if (data) UnloadFileData(data);
        /* Keep the native glyph bitmaps. LoadFontEx's automatic atlas packer
           can drop the last characters at very small font sizes. */
        if (glyphs) return (Font){size,95,0,{0},NULL,glyphs};
    }
    return GetFontDefault();
}

static void unload_font(Font font) {
    if (!font.texture.id) UnloadFontData(font.glyphs,font.glyphCount);
}

bool ascii_renderer_init(AsciiRenderer *renderer,const char *font_path) {
    const char *version="#version 330\n";
    if (rlGetVersion()==RL_OPENGL_ES_20) version="#version 100\n";
    else if (rlGetVersion()==RL_OPENGL_ES_30) version="#version 300 es\n";
    else if (rlGetVersion()==RL_OPENGL_21) version="#version 120\n";
    char fragment[2048];
    snprintf(fragment,sizeof(fragment),"%s%s",version,fragment_body);
    renderer->shader=LoadShaderFromMemory(NULL,fragment);
    if (!renderer->shader.id || renderer->shader.id==rlGetShaderIdDefault()) return false;
    renderer->atlas_location=GetShaderLocation(renderer->shader,"glyphAtlas");
    int grid_location=GetShaderLocation(renderer->shader,"gridSize");
    if (renderer->atlas_location<0 || grid_location<0) return false;
    const float grid_size[2]={(float)SCENE_COLS,(float)SCENE_ROWS};
    SetShaderValue(renderer->shader,grid_location,grid_size,SHADER_UNIFORM_VEC2);
    Image blank=GenImageColor(SCENE_COLS,SCENE_ROWS,BLANK);
    renderer->cells=LoadTextureFromImage(blank);
    UnloadImage(blank);
    SetTextureFilter(renderer->cells,TEXTURE_FILTER_POINT);
    renderer->font_path=font_path;
    return renderer->cells.id!=0;
}

bool ascii_renderer_resize(AsciiRenderer *renderer,int width,int height) {
    float scale=fminf((float)width/SCENE_WIDTH,(float)height/SCENE_HEIGHT);
    int tile_width=(int)ceilf(CELL_WIDTH*scale),tile_height=(int)ceilf(CELL_HEIGHT*scale);
    if (tile_width<4) tile_width=4;
    if (tile_height<7) tile_height=7;
    if (tile_width==renderer->tile_width && tile_height==renderer->tile_height) return true;
    /* Rasterize at the actual on-screen character size, without shrinking a
       64-pixel font into a few pixels. Integer atlas tiles prevent bleeding. */
    Font font=load_font(renderer->font_path,tile_height);
    Image atlas=GenImageColor(16*tile_width,6*tile_height,BLANK);
    if (!atlas.data) {
        unload_font(font);
        return false;
    }
    for (int code=33;code<=126;code++) {
        int index=GetGlyphIndex(font,code),slot=code-32;
        GlyphInfo glyph=font.glyphs[index];
        if (!glyph.image.data) continue;
        Color *pixels=LoadImageColors(glyph.image);
        if (!pixels) continue;
        /* LoadFontData returns a grayscale coverage mask, not opaque gray
           paint. Preserve its coverage as alpha when packing the atlas. */
        for (int p=0;p<glyph.image.width*glyph.image.height;p++) {
            unsigned char coverage=glyph.image.format==PIXELFORMAT_UNCOMPRESSED_GRAYSCALE?pixels[p].r:pixels[p].a;
            pixels[p]=(Color){255,255,255,coverage};
        }
        Image ink={pixels,glyph.image.width,glyph.image.height,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Image tile=GenImageColor(tile_width,tile_height,BLANK);
        float font_scale=(float)tile_height/(float)font.baseSize;
        int advance=glyph.advanceX?glyph.advanceX:glyph.image.width;
        Rectangle source={0,0,(float)glyph.image.width,(float)glyph.image.height};
        Rectangle destination={
            roundf(((float)tile_width-(float)advance*font_scale)*0.5f+(float)glyph.offsetX*font_scale),
            roundf((float)glyph.offsetY*font_scale),
            roundf(source.width*font_scale),roundf(source.height*font_scale)};
        ImageDraw(&tile,ink,source,destination,WHITE);
        UnloadImageColors(pixels);
        ImageDraw(&atlas,tile,(Rectangle){0,0,(float)tile_width,(float)tile_height},
                  (Rectangle){(float)((slot%16)*tile_width),(float)((slot/16)*tile_height),
                              (float)tile_width,(float)tile_height},WHITE);
        UnloadImage(tile);
    }
    Texture2D next=LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    unload_font(font);
    if (!next.id) return false;
    if (renderer->atlas.id) UnloadTexture(renderer->atlas);
    renderer->atlas=next;
    renderer->tile_width=tile_width; renderer->tile_height=tile_height;
    SetTextureFilter(renderer->atlas,TEXTURE_FILTER_POINT);
    TraceLog(LOG_INFO,"ASCII: Sharp glyph atlas %d x %d pixels per character",tile_width,tile_height);
    return true;
}

void ascii_renderer_draw(AsciiRenderer *renderer,const AsciiCell *cells,int width,int height) {
    float scale=fminf((float)width/SCENE_WIDTH,(float)height/SCENE_HEIGHT);
    float draw_width=SCENE_WIDTH*scale,draw_height=SCENE_HEIGHT*scale;
    UpdateTexture(renderer->cells,cells);
    BeginShaderMode(renderer->shader);
    SetShaderValueTexture(renderer->shader,renderer->atlas_location,renderer->atlas);
    DrawTexturePro(renderer->cells,(Rectangle){0,0,(float)SCENE_COLS,(float)SCENE_ROWS},
                   (Rectangle){floorf(((float)width-draw_width)*0.5f),floorf(((float)height-draw_height)*0.5f),
                               draw_width,draw_height},(Vector2){0,0},0,WHITE);
    EndShaderMode();
}

void ascii_renderer_unload(AsciiRenderer *renderer) {
    if (renderer->atlas.id) UnloadTexture(renderer->atlas);
    if (renderer->cells.id) UnloadTexture(renderer->cells);
    if (renderer->shader.id && renderer->shader.id!=rlGetShaderIdDefault()) UnloadShader(renderer->shader);
    *renderer=(AsciiRenderer){0};
}
