// All scene imagery is drawn as individual, colored ASCII glyphs.
#include "raylib.h"
#include "scene.h"
#include "ascii_draw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float time;
    int frames, width, height;
    bool paused, hidden, fullscreen;
    const char *capture, *export_dir, *font_path;
} Options;

static void usage(const char *exe) {
    printf("Usage: %s [--time SECONDS] [--frames N] [--hidden] [--fullscreen]\n"
           "  [--capture FILE.png] [--export-frames EXISTING_DIRECTORY] [--font FILE.ttf] [--resolution WIDTHxHEIGHT]\n"
           "Space pause | R restart | Up/Down speed | Left/Right step | H help | F11 fullscreen | F12 capture | Esc quit\n",exe);
}
static bool parse_options(int argc, char **argv, Options *o) {
    for (int i=1;i<argc;i++) {
        const char *arg = argv[i];
        if (strcmp(arg,"--hidden")==0) o->hidden = true;
        else if (strcmp(arg,"--fullscreen")==0) o->fullscreen = true;
        else if (i+1<argc) {
            const char *value = argv[++i];
            char *end;
            if (strcmp(arg,"--time")==0) {
                o->time = strtof(value,&end);
                if (*end || end==value || !isfinite(o->time)) return false;
                o->time = scene_wrap_time(o->time); o->paused = true;
            } else if (strcmp(arg,"--frames")==0) {
                long n = strtol(value,&end,10);
                if (*end || end==value || n<1 || n>1000000) return false;
                o->frames = (int)n;
            } else if (strcmp(arg,"--resolution")==0) {
                long width=strtol(value,&end,10);
                if (end==value || *end!='x' || width<320 || width>7680) return false;
                const char *height_start=end+1;
                long height=strtol(height_start,&end,10);
                if (end==height_start || *end || height<200 || height>4320) return false;
                o->width=(int)width; o->height=(int)height;
            } else if (strcmp(arg,"--capture")==0) o->capture = value;
            else if (strcmp(arg,"--export-frames")==0) o->export_dir = value;
            else if (strcmp(arg,"--font")==0) o->font_path = value;
            else return false;
        } else return false;
    }
    if (o->capture && o->export_dir) return false;
    if (o->width && !o->capture && !o->export_dir) return false;
    return true;
}

/* Allocate at framebuffer resolution, never upscale an old low-resolution frame. */
static bool resize_canvas(RenderTexture2D *canvas, int width, int height) {
    if (width<1 || height<1) return true;
    if (canvas->texture.width==width && canvas->texture.height==height) return true;
    RenderTexture2D next = LoadRenderTexture(width,height);
    if (!next.id) return false;
    if (canvas->id) UnloadRenderTexture(*canvas);
    *canvas=next;
    SetTextureFilter(canvas->texture,TEXTURE_FILTER_POINT);
    TraceLog(LOG_INFO,"ASCII: Native render target %d x %d, grid %d x %d",width,height,SCENE_COLS,SCENE_ROWS);
    return true;
}

static void toggle_fullscreen(void) {
    static int window_width=1280,window_height=720;
    static Vector2 window_position={100,100};
    static bool fullscreen=false;
    static bool was_maximized=false;
    if (fullscreen) {
        ClearWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(window_width,window_height);
        SetWindowPosition((int)window_position.x,(int)window_position.y);
        if (was_maximized) MaximizeWindow();
    } else {
        int monitor=GetCurrentMonitor();
        Vector2 monitor_position=GetMonitorPosition(monitor);
        was_maximized=IsWindowState(FLAG_WINDOW_MAXIMIZED);
        if (was_maximized) RestoreWindow();
        window_width=GetScreenWidth(); window_height=GetScreenHeight();
        window_position=GetWindowPosition();
        /* Borderless native fullscreen avoids a video-mode switch or DPI upscale. */
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowPosition((int)monitor_position.x,(int)monitor_position.y);
        SetWindowSize(GetMonitorWidth(monitor),GetMonitorHeight(monitor));
    }
    fullscreen=!fullscreen;
    TraceLog(LOG_INFO,"ASCII: Fullscreen %s",fullscreen?"on (native borderless)":"off");
}

static bool capture_frame(RenderTexture2D canvas, const char *path) {
    Image image = LoadImageFromTexture(canvas.texture);
    bool ok;
    if (!image.data) return false;
    ImageFlipVertical(&image);
    ok = ExportImage(image,path);
    UnloadImage(image);
    if (!ok) fprintf(stderr,"Cannot write capture: %s\n",path);
    return ok;
}

static void render_canvas(RenderTexture2D canvas, AsciiRenderer *renderer, float time) {
    /* Larger grids must not consume the default Windows thread stack. */
    static AsciiCell cells[SCENE_ROWS][SCENE_COLS];
    scene_render(time,cells);
    BeginTextureMode(canvas);
    ClearBackground(BLACK);
    ascii_renderer_draw(renderer,&cells[0][0],canvas.texture.width,canvas.texture.height);
    EndTextureMode();
}

int main(int argc, char **argv) {
    Options options = {0};
    AsciiRenderer renderer={0};
    RenderTexture2D canvas = {0};
    float time, speed = 1.0f;
    bool paused, show_help = false;
    int result = 0, frame_count = 0;
    double start;
    if (argc==2 && strcmp(argv[1],"--help")==0) { usage(argv[0]); return 0; }
    if (!parse_options(argc,argv,&options)) { usage(argv[0]); return 2; }
    if (options.export_dir && !DirectoryExists(options.export_dir)) {
        fprintf(stderr,"Export directory must exist: %s\n",options.export_dir); return 2;
    }
    if (options.font_path && !FileExists(options.font_path)) {
        fprintf(stderr,"Font not found: %s\n",options.font_path); return 2;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | (options.hidden ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(1280,720,"4 years together | ASCII | F11 fullscreen | H help");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(640,360);
    if (options.fullscreen) toggle_fullscreen();
    SetTargetFPS(60);
    if (!resize_canvas(&canvas,options.width?options.width:GetRenderWidth(),
                       options.height?options.height:GetRenderHeight()) ||
        !ascii_renderer_init(&renderer,options.font_path) ||
        !ascii_renderer_resize(&renderer,canvas.texture.width,canvas.texture.height)) {
        ascii_renderer_unload(&renderer);
        if (canvas.id) UnloadRenderTexture(canvas);
        CloseWindow(); return 1;
    }
    time = options.time;
    paused = options.paused;
    start = GetTime();
    if (options.export_dir) {
        static const float times[] = {0.6f,1.6f,2.2f,3.8f,5.8f,7.0f,8.4f,9.5f,12.0f,14.0f,
                                     16.5f,18.5f,20.5f,22.8f,23.9f,INTRO_DURATION,
                                     INTRO_DURATION+3.0f,INTRO_DURATION+9.0f,
                                     INTRO_DURATION+14.6f,INTRO_DURATION+18.2f,
                                     INTRO_DURATION+23.8f,INTRO_DURATION+27.1f,
                                     INTRO_DURATION+32.0f,INTRO_DURATION+34.5f,
                                     INTRO_DURATION+39.0f,INTRO_DURATION+46.0f};
        for (int i=0;i<(int)(sizeof(times)/sizeof(times[0]));i++) {
            char path[1024];
            int n = snprintf(path,sizeof(path),"%s/frame_%04d.png",options.export_dir,(int)(times[i]*100));
            if (n<0 || n>=(int)sizeof(path)) { result=1; break; }
            render_canvas(canvas,&renderer,times[i]);
            if (!capture_frame(canvas,path)) { result=1; break; }
        }
    } else {
        while (!WindowShouldClose()) {
            bool take_capture = IsKeyPressed(KEY_F12);
            if (IsKeyPressed(KEY_F11) || (IsKeyPressed(KEY_ENTER) &&
                (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))) toggle_fullscreen();
            if (!options.width && !resize_canvas(&canvas,GetRenderWidth(),GetRenderHeight())) {
                result=1; break;
            }
            if (!ascii_renderer_resize(&renderer,canvas.texture.width,canvas.texture.height)) {
                result=1; break;
            }
            if (IsKeyPressed(KEY_SPACE)) paused = !paused;
            if (IsKeyPressed(KEY_R)) { time=0; paused=false; }
            if (IsKeyPressed(KEY_H)) show_help = !show_help;
            if (IsKeyPressed(KEY_UP)) speed=fminf(3.0f,speed+0.25f);
            if (IsKeyPressed(KEY_DOWN)) speed=fmaxf(0.25f,speed-0.25f);
            if (IsKeyPressed(KEY_RIGHT)) { time=scene_wrap_time(time+1.0f/30.0f); paused=true; }
            if (IsKeyPressed(KEY_LEFT)) { time=scene_wrap_time(time-1.0f/30.0f); paused=true; }
            if (!paused && !options.capture) time=scene_wrap_time(time+GetFrameTime()*speed);
            render_canvas(canvas,&renderer,time);
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(canvas.texture,
                (Rectangle){0,0,(float)canvas.texture.width,-(float)canvas.texture.height},
                (Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
                (Vector2){0,0},0,WHITE);
            if (show_help) {
                DrawRectangle(8,8,680,58,(Color){0,0,0,225});
                DrawText("SPACE pause | R restart | UP/DOWN speed | F11 fullscreen | H help | ESC quit",16,16,14,LIGHTGRAY);
                DrawText(TextFormat("LEFT/RIGHT step | F12 screenshot    %.2f s   %.2fx   %s   %d FPS",time,speed,
                                   paused?"PAUSED":"PLAYING",GetFPS()),16,39,14,LIGHTGRAY);
            }
            EndDrawing();
            frame_count++;
            if (options.capture) {
                if (!capture_frame(canvas,options.capture)) result=1;
                break;
            }
            if (take_capture) {
                char name[128];
                snprintf(name,sizeof(name),"eye-space-%06d-%04d.png",frame_count,(int)(time*100));
                if (!capture_frame(canvas,name)) result=1;
            }
            if (options.frames && frame_count>=options.frames) break;
        }
    }
    if (frame_count>0) printf("Rendered %d frames in %.3f s\n",frame_count,GetTime()-start);
    UnloadRenderTexture(canvas);
    ascii_renderer_unload(&renderer);
    CloseWindow();
    return result;
}
