#include "scene.h"
#include "celebration.h"
#include "intro.h"
#include <math.h>
#include <stdint.h>

typedef struct { float r, g, b; } Light;

static float clamp(float x) { return fminf(1.0f, fmaxf(0.0f, x)); }
static float mix(float a, float b, float t) { return a + (b - a)*t; }
static float smooth(float a, float b, float x) {
    float t = clamp((x - a)/(b - a));
    return t*t*(3.0f - 2.0f*t);
}
static float wave(float x) { return 0.5f + 0.5f*sinf(x); }
static Light scale(Light c, float v) { return (Light){c.r*v, c.g*v, c.b*v}; }
static Light blend(Light a, Light b, float t) {
    return (Light){mix(a.r,b.r,t), mix(a.g,b.g,t), mix(a.b,b.b,t)};
}
static uint32_t hash(uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du;
    n ^= n >> 15; n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
static float noise(int x, int y, uint32_t seed) {
    return (float)(hash((uint32_t)x*73856093u ^ (uint32_t)y*19349663u ^ seed) & 0xffffffu)/16777215.0f;
}
/* Interpolated spatial noise moves with the eye, instead of boiling each frame. */
static float field(float x, float y, uint32_t seed) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = smooth(0,1,x-(float)ix), fy = smooth(0,1,y-(float)iy);
    return mix(mix(noise(ix,iy,seed), noise(ix+1,iy,seed),fx),
               mix(noise(ix,iy+1,seed), noise(ix+1,iy+1,seed),fx),fy);
}
static unsigned char glyph(float density, float grain) {
    static const char ramp[] = " .,:;!|/()frtjnuvczXYUJCLQ0OZqpdbkhao*#MW&8%B@$";
    int i = (int)(clamp(density + (grain-0.5f)*0.075f)*(float)(sizeof(ramp)-2));
    return (unsigned char)ramp[i];
}
static AsciiCell cell(unsigned char ch, Light color, float opacity) {
    AsciiCell c = {ch, (unsigned char)(255*clamp(color.r*opacity)),
                      (unsigned char)(255*clamp(color.g*opacity)),
                      (unsigned char)(255*clamp(color.b*opacity))};
    if (c.r < 3 && c.g < 3 && c.b < 3) c.glyph = ' ';
    return c;
}

float scene_wrap_time(float seconds) {
    float t;
    if (!isfinite(seconds)) return 0;
    t = fmodf(seconds, SCENE_DURATION);
    return t < 0 ? t + SCENE_DURATION : t;
}

SceneState scene_state(float seconds) {
    SceneState s;
    float approach;
    s.time = fmaxf(0,scene_wrap_time(seconds)-INTRO_DURATION);
    approach = smooth(4.0f, 13.0f, s.time);
    s.zoom = 1.0f + 0.075f*smooth(1.0f,4.0f,s.time) + 7.8f*approach*approach;
    s.pupil_radius = (0.145f + 0.12f*smooth(4.0f,9.0f,s.time))*s.zoom;
    s.stars = smooth(3.8f, 6.5f, s.time);
    s.exposure = 1.0f-smooth(FADE_START,STORY_DURATION,s.time);
    s.stars *= 1.0f-0.86f*smooth(11.0f,13.0f,s.time);
    s.stars *= 1.0f-smooth(29.0f,32.0f,s.time);
    return s;
}

static Light eye_surface(float x, float y, const SceneState *s, float grain, float *density) {
    float r = sqrtf(x*x+y*y), angle = atan2f(y,x);
    float opening = 1.0f; /* Already open in the preceding portrait. */
    float lid = 0.57f*powf(fmaxf(0,1.0f-powf(fabsf(x)/1.92f,1.7f)),0.72f)*opening;
    float lid_y = y + 0.025f + 0.025f*x;
    float inside = 1.0f-smooth(lid-0.018f,lid+0.018f,fabsf(lid_y));
    float folds = wave(37.0f*(fabsf(y)+0.10f*x*x) + 1.5f*field(x*3,y*4,12));
    float skin_grain = field(x*14,y*24,71);
    float skin = (0.19f+0.28f*folds+0.15f*skin_grain)*(0.65f+0.35f*smooth(-0.6f,0.6f,y));
    float crease = 1.0f - 0.75f*expf(-90.0f*fabsf(fabsf(lid_y)-lid-0.045f));
    Light color = scale((Light){0.60f,0.38f,0.25f},skin*crease);
    *density = 0.24f+0.32f*folds+0.10f*skin_grain;

    if (inside > 0) {
        float stripe = wave(y*67.0f + 0.45f*sinf(x*3.0f) + 0.25f*sinf(s->time*0.7f));
        float body = 0.64f+0.27f*stripe;
        float edge = 0.55f+0.45f*smooth(0.0f,0.16f,lid-fabsf(lid_y));
        float veins = 1.0f-0.17f*powf(wave(25*x + 4*sinf(y*18)),12);
        Light sclera = scale((Light){0.87f,0.89f,0.82f},body*edge*veins);
        float d = 0.57f+0.26f*stripe+0.07f*grain;
        float iris = 1.0f-smooth(0.565f,0.592f,r);
        if (iris > 0) {
            float bend = angle + 0.035f*sinf(r*32.0f + angle*7.0f);
            float fibers = 0.45f*wave(bend*53.0f + r*17.0f)
                         + 0.35f*wave(bend*97.0f - r*33.0f)
                         + 0.20f*wave(bend*31.0f + r*56.0f);
            float radial = field(cosf(angle)*58.0f,sinf(angle)*58.0f,22);
            float ring = wave(r*88 + 2*sinf(angle*13));
            float inner_r = s->pupil_radius/s->zoom;
            float collarette = expf(-650*(r-inner_r-0.075f)*(r-inner_r-0.075f));
            float rim = 1.0f-0.72f*smooth(0.49f,0.585f,r);
            float inner = mix(0.44f,1.0f,smooth(inner_r,inner_r+0.065f,r));
            float iris_light = (0.36f+0.43f*fibers+0.15f*radial+0.08f*ring)*rim*inner;
            Light copper = blend((Light){0.78f,0.34f,0.13f},(Light){1.0f,0.67f,0.31f},fibers);
            copper = scale(copper,iris_light*(0.90f+0.10f*stripe)+0.10f*collarette);
            sclera = blend(sclera,copper,iris);
            d = mix(d,0.29f+0.40f*fibers+0.13f*ring+0.07f*collarette,iris);
        }
        color = blend(color,sclera,inside);
        *density = mix(*density,d,inside);
    }
    return color;
}

/* Splat moving stars onto neighboring cells: their light crosses cell boundaries
   continuously. Their seeded world positions never change or respawn. */
static void star_glyph(AsciiCell stars[SCENE_ROWS][SCENE_COLS], float x, float y,
                       unsigned char ch, float brightness) {
    int ix=(int)floorf(x), iy=(int)floorf(y);
    for (int dy=1-SCENE_DETAIL;dy<=SCENE_DETAIL;dy++) for (int dx=1-SCENE_DETAIL;dx<=SCENE_DETAIL;dx++) {
        int col=ix+dx, row=iy+dy;
        if (col>=0 && col<SCENE_COLS && row>=0 && row<SCENE_ROWS) {
            float weight=fmaxf(0,1-fabsf(x-(float)col)/SCENE_DETAIL)
                        *fmaxf(0,1-fabsf(y-(float)row)/SCENE_DETAIL);
            AsciiCell c=cell(ch,(Light){0.79f,0.85f,0.87f},brightness*weight);
            if (c.r>stars[row][col].r) stars[row][col]=c;
        }
    }
}

static void star_field(const SceneState *s, AsciiCell stars[SCENE_ROWS][SCENE_COLS]) {
    float expansion=0.28f+1.12f*smooth(4.0f,13.5f,s->time);
    for (int row=0;row<SCENE_ROWS;row++) for (int col=0;col<SCENE_COLS;col++)
        stars[row][col]=(AsciiCell){' ',0,0,0};
    if (s->stars<=0) return;
    for (int k=0;k<380;k++) {
        float x=(noise(k,1,3791)-0.5f)*SCENE_COLS*expansion+SCENE_COLS*0.5f;
        float y=(noise(k,2,3791)-0.5f)*SCENE_ROWS*expansion+SCENE_ROWS*0.5f;
        float twinkle=0.72f+0.28f*wave(s->time*1.4f+noise(k,3,69)*6.28318f);
        float brightness=(0.35f+0.65f*noise(k,4,14))*twinkle*s->stars;
        unsigned char ch=k%9==0?'*':(k%4==0?'+':'.');
        star_glyph(stars,x,y,ch,brightness);
    }
    for (int k=0;k<16;k++) {
        float x=(noise(k,1,23)-0.5f)*(SCENE_COLS-8)*expansion+SCENE_COLS*0.5f;
        float y=(noise(k,2,23)-0.5f)*(SCENE_ROWS-6)*expansion+SCENE_ROWS*0.5f;
        float brightness=(0.85f+0.15f*wave(s->time*1.7f+(float)k))*s->stars;
        star_glyph(stars,x,y,'*',brightness);
        for (int d=-3;d<=3;d++) if (d) {
            float distance=(float)(d<0?-d:d);
            star_glyph(stars,x+(float)(d*SCENE_DETAIL),y,'-',brightness*(0.65f-0.14f*distance));
            if (d>=-2 && d<=2) star_glyph(stars,x,y+(float)(d*SCENE_DETAIL),'|',brightness*(0.68f-0.20f*distance));
        }
    }
}

/* Identical pupil, iris and reflection for the portrait and its destination. */
static AsciiCell portrait_eye(float x,float y) {
    SceneState s=scene_state(INTRO_DURATION);
    float d,grain=noise((int)floorf(x*72),(int)floorf(y*41),11);
    float r=sqrtf(x*x+y*y);
    float portal=1-smooth(s.pupil_radius-0.015f,s.pupil_radius+0.015f,r);
    Light color=eye_surface(x,y,&s,grain,&d);
    AsciiCell out=cell(glyph(d,grain),color,1-portal);
    float gx=(x-0.20f)/0.095f,gy=(y+0.075f)/0.22f;
    float glint=(1-smooth(0.40f,1,gx*gx+gy*gy))*(0.45f+0.55f*wave(y*166));
    if (glint>0.04f) {
        out.r=(unsigned char)mix(out.r,223,glint);
        out.g=(unsigned char)mix(out.g,231,glint);
        out.b=(unsigned char)mix(out.b,216,glint);
        if (glint>0.20f) out.glyph=glyph(0.61f+0.29f*glint,grain);
    }
    return out;
}

void scene_render(float seconds, AsciiCell cells[SCENE_ROWS][SCENE_COLS]) {
    float time=scene_wrap_time(seconds);
    /* The room is opaque until the final eye splice. Avoid calculating an
       entire hidden iris/celebration frame underneath every intro frame. */
    if (time<INTRO_EYE_BLEND_START) {
        intro_render(time,cells,portrait_eye);
        return;
    }
    SceneState s = scene_state(seconds);
    AsciiCell stars[SCENE_ROWS][SCENE_COLS];
    star_field(&s,stars);
    int row;
#ifdef _OPENMP
#pragma omp parallel for schedule(static) num_threads(4)
#endif
    for (row=0;row<SCENE_ROWS;row++) {
        for (int col=0;col<SCENE_COLS;col++) {
            float x = ((float)col+0.5f-SCENE_COLS*0.5f)*CELL_WIDTH/(SCENE_HEIGHT*0.5f);
            float y = ((float)row+0.5f-SCENE_ROWS*0.5f)*CELL_HEIGHT/(SCENE_HEIGHT*0.5f);
            float r = sqrtf(x*x+y*y), grain = noise(col,row,11);
            float portal = 1.0f-smooth(s.pupil_radius-0.015f,s.pupil_radius+0.015f,r);
            AsciiCell out;
            if (portal < 1.0f) {
                float d;
                Light eye = eye_surface(x/s.zoom,y/s.zoom,&s,grain,&d);
                out = cell(glyph(d,grain),eye,(1.0f-portal)*s.exposure);
            } else out = (AsciiCell){' ',0,0,0};
            if (portal > 0 && s.stars > 0) {
                AsciiCell space = stars[row][col];
                if (space.glyph != ' ') {
                    float opacity = portal*s.exposure;
                    out.r = (unsigned char)fminf(255,(float)out.r+(float)space.r*opacity);
                    out.g = (unsigned char)fminf(255,(float)out.g+(float)space.g*opacity);
                    out.b = (unsigned char)fminf(255,(float)out.b+(float)space.b*opacity);
                    if (portal>0.5f) out.glyph = space.glyph;
                }
            }
            /* The tall broken reflection straddles the pupil's right edge. */
            {
                float ex = x/s.zoom, ey = y/s.zoom;
                float gx = (ex-0.20f)/0.095f, gy = (ey+0.075f)/0.22f;
                float glint = (1.0f-smooth(0.40f,1.0f,gx*gx+gy*gy))
                            *(0.45f+0.55f*wave(ey*166))*(1.0f-smooth(6.2f,9.7f,s.time))*s.exposure;
                if (glint>0.04f) {
                    out.r = (unsigned char)mix(out.r,223,glint);
                    out.g = (unsigned char)mix(out.g,231,glint);
                    out.b = (unsigned char)mix(out.b,216,glint);
                    if (glint>0.20f) out.glyph = glyph(0.61f+0.29f*glint,grain);
                }
            }
            cells[row][col] = out;
        }
    }
    celebration_render(s.time,cells);
    intro_render(time,cells,portrait_eye);
}
