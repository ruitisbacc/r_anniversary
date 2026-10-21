#include "intro.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct { float r, g, b; } Tone;
static const float eye_x=-0.205f, eye_y=-0.185f, eye_scale=0.082f;
static float sat(float v) { return fminf(1,fmaxf(0,v)); }
static float lerp(float a,float b,float t) { return a+(b-a)*t; }
static float ease(float a,float b,float v) { float t=sat((v-a)/(b-a)); return t*t*(3-2*t); }
static float sq(float v) { return v*v; }
static float bell(float x,float y,float cx,float cy,float rx,float ry) {
    return expf(-2*(sq((x-cx)/rx)+sq((y-cy)/ry)));
}
static Tone tint(Tone a,Tone b,float t) {
    return (Tone){lerp(a.r,b.r,t),lerp(a.g,b.g,t),lerp(a.b,b.b,t)};
}
static Tone shade(Tone c,float v) { return (Tone){c.r*v,c.g*v,c.b*v}; }
static float grain(float x,float y) {
    uint32_t h=(uint32_t)(int)floorf(x*530)*73856093u ^ (uint32_t)(int)floorf(y*530)*19349663u;
    h^=h>>16; h*=0x7feb352du; h^=h>>15;
    return (float)(h&65535u)/65535;
}
static float stroke(float distance,float width,float aa) {
    return 1-ease(width,width+aa,fabsf(distance));
}
static AsciiCell make_cell(Tone c,float density,float opacity) {
    static const char ramp[]=" .,:;irsxouvczXYUJCLQ0OZqpdbkhao*#MW&8%B@$";
    AsciiCell out={(unsigned char)ramp[(int)(sat(density)*(sizeof(ramp)-2))],
        (unsigned char)(255*sat(c.r*opacity)),(unsigned char)(255*sat(c.g*opacity)),
        (unsigned char)(255*sat(c.b*opacity))};
    if (out.r+out.g+out.b<8) out.glyph=' ';
    return out;
}
static AsciiCell crossfade(AsciiCell a,AsciiCell b,float t) {
    float av=(a.r+a.g+a.b)*(1-t),bv=(b.r+b.g+b.b)*t;
    return (AsciiCell){bv>av?b.glyph:a.glyph,
        (unsigned char)lerp(a.r,b.r,t),(unsigned char)lerp(a.g,b.g,t),(unsigned char)lerp(a.b,b.b,t)};
}

/* A continuous portrait surface: illumination, folds and hair live in face
   coordinates, so the camera reveals detail instead of enlarging a bitmap. */
static AsciiCell portrait(float x,float y,float aa,float time,IntroEyeSampler eye) {
    float ax=fabsf(x), n=grain(x,y), density=0, coverage=0;
    Tone c={0,0,0};
    float shoulder=sq(x/0.86f)+sq((y-1.09f)/0.47f);
    float neck=sq(x/0.195f)+sq((y-0.52f)/0.34f);
    float hair_shape=powf(fabsf((x+0.014f)/0.565f),2.65f)+sq((y+0.095f)/0.70f);
    if (shoulder<1.02f) {
        float knit=0.5f+0.5f*sinf(x*240+2*sinf(y*110));
        coverage=1-ease(0.97f,1.02f,shoulder);
        c=shade((Tone){0.27f,0.40f,0.39f},0.44f+0.32f*knit+0.14f*cosf(x*11));
        density=0.35f+0.22f*knit;
    }
    if (neck<1 && y>0.26f) {
        float folds=0.5f+0.5f*sinf((y+0.3f*x*x)*126);
        c=shade((Tone){0.70f,0.48f,0.35f},0.54f+0.17f*folds-0.2f*x);
        density=0.46f+0.11f*folds; coverage=1;
    }
    /* Back of the bob, with gently curling ends. */
    if (hair_shape<1 && (y<0.46f || ax>0.29f)) {
        float strand=0.5f+0.5f*sinf(x*230+7*sinf(y*9)+3*sinf(x*23));
        c=shade((Tone){0.73f,0.66f,0.50f},0.40f+0.48f*strand);
        density=0.30f+0.28f*strand; coverage=1-ease(0.96f,1,hair_shape);
    }
    float face_width=0.414f*(1-0.19f*ease(0.12f,0.52f,y));
    float face=sq(x/face_width)+sq((y+0.055f)/0.615f);
    if (face<1) {
        float z=sqrtf(fmaxf(0,1-face));
        float light=0.31f+0.66f*z-0.31f*x;
        float cheek=bell(x,y,-0.25f,0.055f,0.16f,0.17f)+bell(x,y,0.25f,0.055f,0.16f,0.17f);
        float socket=bell(ax,y,0.205f,-0.19f,0.185f,0.12f);
        float hollow=bell(ax,y,0.28f,0.22f,0.105f,0.19f);
        light+=0.19f*cheek-0.25f*socket-0.25f*hollow;
        light+=0.21f*bell(x,y,-0.025f,0.005f,0.058f,0.25f);
        light-=0.34f*bell(x,y,0.065f,0.035f,0.04f,0.19f);
        c=tint((Tone){0.84f,0.66f,0.51f},(Tone){0.85f,0.52f,0.43f},0.23f*cheek);
        /* Fine age spots are anchored to skin, never reseeded per frame. */
        float spots=0.10f*powf(0.5f+0.5f*sinf(x*189+sinf(y*134))*sinf(y*213),9);
        light-=spots; light+=(n-0.5f)*0.045f;
        float wrinkles=0;
        for (int k=0;k<4;k++) {
            float line=-0.37f-(float)k*0.036f+0.15f*x*x+0.003f*sinf(x*32+k);
            wrinkles+=0.14f*stroke(y-line,0.0012f,aa)*(1-ease(0.22f,0.36f,ax));
        }
        /* Lower lids, tear troughs and outward crow's feet. */
        float ex=ax-0.205f;
        for (int k=0;k<3;k++) {
            float under=-0.123f+0.022f*(float)k-1.7f*ex*ex;
            wrinkles+=0.28f*stroke(y-under,0.0025f,aa)*(1-ease(0.09f,0.16f,fabsf(ex)));
            float crow=-0.18f+(ax-0.32f)*((float)k-0.6f)*0.7f+0.012f*(float)k;
            wrinkles+=0.17f*stroke(y-crow,0.0012f,aa)*ease(0.30f,0.33f,ax)*(1-ease(0.37f,0.41f,ax));
        }
        /* Nasolabial folds, cheek creases, chin and vertical lip lines. */
        float fold=0.103f+0.29f*(y-0.07f)+0.025f*sinf((y-0.07f)*13);
        wrinkles+=0.34f*stroke(ax-fold,0.003f,aa)*ease(0.06f,0.11f,y)*(1-ease(0.29f,0.37f,y));
        for (int k=0;k<4;k++) {
            float crease=0.28f+0.023f*(float)k-0.22f*y+0.018f*sinf(y*17+(float)k);
            wrinkles+=0.15f*stroke(ax-crease,0.0015f,aa)*ease(-0.03f,0.04f,y)*(1-ease(0.28f,0.42f,y));
        }
        wrinkles+=0.12f*stroke(y-(0.41f-0.8f*x*x),0.002f,aa)*(1-ease(0.09f,0.19f,ax));
        wrinkles+=0.10f*powf(0.5f+0.5f*cosf(x*240),12)*bell(x,y,0,0.244f,0.13f,0.07f);
        light-=wrinkles;
        c=shade(c,light);
        density=0.37f+0.36f*light+0.06f*n;
        coverage=1;
        /* Nose tip, nostrils, philtrum and a quiet closed-lip smile. */
        float nose=bell(x,y,-0.015f,0.098f,0.072f,0.044f);
        c=tint(c,(Tone){0.82f,0.63f,0.48f},nose*0.6f);
        float nostril=bell(ax,y,0.057f,0.127f,0.024f,0.014f);
        c=shade(c,1-0.85f*nostril);
        c=shade(c,1-0.16f*bell(x,y,0,0.19f,0.022f,0.058f));
        float smile=0.292f-1.5f*x*x;
        float lip=(1-ease(0.10f,0.151f,ax))*stroke(y-smile,0.013f,aa);
        c=tint(c,(Tone){0.48f,0.27f,0.245f},lip*0.85f);
        float seam=stroke(y-smile+0.004f,0.0018f,aa)*(1-ease(0.12f,0.155f,ax));
        c=shade(c,1-0.85f*seam);
        c=tint(c,(Tone){0.81f,0.59f,0.48f},0.30f*bell(x,y,0,0.316f,0.09f,0.018f));
        /* Brows: sparse ash-blond hairs following the orbital ridge. */
        float brow=-0.291f+1.0f*ex*ex+0.04f*ex;
        float brow_mask=stroke(y-brow,0.009f,aa)*(1-ease(0.10f,0.145f,fabsf(ex)));
        c=tint(c,(Tone){0.29f,0.25f,0.19f},brow_mask*(0.52f+0.25f*sinf(x*370+y*130)));
        /* Both eyes use the very same iris/lid renderer as the following scene. */
        float eye_local_x=(x-(x<0?eye_x:-eye_x))/eye_scale;
        float eye_local_y=(y-eye_y)/eye_scale;
        float patch=(1-ease(1.80f,2.00f,fabsf(eye_local_x)))
                   *(1-ease(0.59f,0.85f,fabsf(eye_local_y)));
        if (patch>0) {
            AsciiCell detail=eye(eye_local_x,eye_local_y);
            AsciiCell skin=make_cell(c,density,1);
            AsciiCell out=crossfade(skin,detail,patch);
            c=(Tone){out.r/255.0f,out.g/255.0f,out.b/255.0f};
            if (patch>0.5f) return out;
        }
    }
    /* Front fringe: an irregular curved edge and separated fine locks. */
    float fringe=-0.365f+0.055f*cosf(x*8)+0.014f*sinf(x*61)+0.009f*sinf(x*113);
    float side=0.366f+0.03f*sinf(y*10);
    float hair_front=(1-ease(fringe-aa,fringe+aa,y));
    hair_front=fmaxf(hair_front,ease(side-0.018f,side+0.018f,ax)*(1-ease(0.36f,0.53f,y)));
    if (hair_shape<1 && hair_front>0) {
        float bend=x+0.055f*sinf(y*5)+0.045f*sinf(8*x)*(y+0.65f);
        float strands=0.5f+0.5f*sinf(bend*305+2*sinf(bend*73));
        float locks=0.5f+0.5f*sinf(bend*63+2*y);
        float silver=powf(0.5f+0.5f*sinf(bend*471),9);
        float sheen=0.62f+0.38f*bell(x,y,-0.22f,-0.54f,0.4f,0.36f);
        Tone hair=tint((Tone){0.40f,0.35f,0.25f},(Tone){0.79f,0.74f,0.60f},0.25f+0.47f*strands+0.15f*locks);
        hair=tint(hair,(Tone){0.87f,0.85f,0.76f},silver*0.30f);
        c=tint(c,shade(hair,sheen+0.23f*strands),hair_front);
        density=lerp(density,0.32f+0.29f*strands+0.10f*silver,hair_front);
        coverage=fmaxf(coverage,1-ease(0.97f,1,hair_shape));
    }
    /* Tiny breath in the light, with the geometry held still for the camera. */
    return make_cell(c,density,coverage*(0.985f+0.015f*sinf(time*1.2f)));
}

static const unsigned char *letter(char c) {
    static const unsigned char bits[][7]={
        {14,17,19,21,25,17,14}, {14,17,1,2,4,8,31},
        {31,16,16,30,1,1,30}, {14,17,17,14,17,17,14},
        {31,16,16,30,16,16,31}, {17,27,21,21,17,17,17},
        {14,17,17,31,17,17,17}, {0,0,0,0,0,4,8},
        {12,4,4,4,4,4,14}, {0,0,14,17,31,16,14},
        {8,8,30,8,8,9,6}, {0,0,0,0,0,0,0}};
    const char *chars="0258EMA,let";
    const char *found=strchr(chars,c);
    return bits[found?(int)(found-chars):11];
}
static void text_line(AsciiCell cells[SCENE_ROWS][SCENE_COLS],const char *text,int y0,int sx,int sy,float opacity) {
    int length=(int)strlen(text),x0=(SCENE_COLS-(length*6-1)*sx)/2;
    for (int k=0;k<length;k++) for (int y=0;y<7;y++) for (int x=0;x<5;x++)
        if (letter(text[k])[y]&(1u<<(4-x)))
            for (int dy=0;dy<sy;dy++) for (int dx=0;dx<sx;dx++) {
                Tone color=tint((Tone){1,0.88f,0.67f},(Tone){0.71f,0.54f,0.37f},(float)y/7);
                cells[y0+y*sy+dy][x0+k*6*sx+x*sx+dx]=make_cell(color,0.72f,opacity);
            }
}

void intro_render(float time,AsciiCell cells[SCENE_ROWS][SCENE_COLS],IntroEyeSampler eye) {
    if (time>=INTRO_DURATION) return;
    float reveal=ease(4.1f,5.6f,time);
    float progress=ease(7.0f,15.4f,time);
    float zoom=expf(logf(1/eye_scale)*progress);
    float pan=ease(7.0f,12.8f,time);
    float blend=ease(14.1f,INTRO_DURATION,time);
    for (int row=0;row<SCENE_ROWS;row++) for (int col=0;col<SCENE_COLS;col++) {
        AsciiCell p={' ',0,0,0};
        if (reveal>0) {
            float x=((float)col+0.5f-SCENE_COLS*0.5f)*CELL_WIDTH/(SCENE_HEIGHT*0.5f);
            float y=((float)row+0.5f-SCENE_ROWS*0.5f)*CELL_HEIGHT/(SCENE_HEIGHT*0.5f);
            p=portrait(x/zoom+eye_x*pan,y/zoom+eye_y*pan,0.009f/zoom,time,eye);
            p.r=(unsigned char)(p.r*reveal); p.g=(unsigned char)(p.g*reveal); p.b=(unsigned char)(p.b*reveal);
        }
        cells[row][col]=crossfade(p,cells[row][col],blend);
    }
    if (time<4.1f) {
        float opacity=ease(0,0.9f,time)*(1-ease(3.1f,4.1f,time));
        text_line(cells,"2085",23,4,3,opacity);
        text_line(cells,"EMA, 80 let",51,2,2,opacity);
    }
}
