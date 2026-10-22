#include "intro.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct { float r, g, b; } Tone;
static const float eye_x=-0.205f, eye_y=-0.185f, eye_scale=0.082f;
/* World coordinates keep the head, body and destination eye in one space. */
static const float head_x=0.36f, head_y=-0.46f, head_scale=0.34f;
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

/* All lettering is sampled in world space too: calendar pages and the album
   stay made of glyphs during the camera move. */
static const unsigned char *letter(char c) {
    static const unsigned char bits[][7]={
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},
        {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
        {17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
        {0,0,0,0,0,4,8},{0,0,0,0,0,0,4},
        {0,0,0,0,0,0,0}};
    const char *chars="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ,.";
    if (c>='a' && c<='z') c=(char)(c-'a'+'A');
    const char *found=strchr(chars,c);
    return bits[found?(int)(found-chars):38];
}
static int text_mask(const char *text,float x,float y,float top,float px,float py) {
    int length=(int)strlen(text);
    float left=-(float)(length*6-1)*px*0.5f;
    int cx=(int)floorf((x-left)/px),cy=(int)floorf((y-top)/py);
    if (cx<0 || cx>=length*6-1 || cy<0 || cy>=7 || cx%6==5) return 0;
    return (letter(text[cx/6])[cy]&(1u<<(4-cx%6)))!=0;
}

typedef struct { Tone color; float density; } Surface;
static float ellipse(float x,float y,float cx,float cy,float rx,float ry,float aa) {
    return 1-ease(1-aa,1+aa,sq((x-cx)/rx)+sq((y-cy)/ry));
}
static float box(float x,float y,float left,float top,float right,float bottom,float aa) {
    return ease(left-aa,left+aa,x)*(1-ease(right-aa,right+aa,x))
          *ease(top-aa,top+aa,y)*(1-ease(bottom-aa,bottom+aa,y));
}
static float segment(float x,float y,float ax,float ay,float bx,float by,float radius,float aa) {
    float dx=bx-ax,dy=by-ay;
    float t=sat(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy));
    return 1-ease(radius-aa,radius+aa,sqrtf(sq(x-ax-t*dx)+sq(y-ay-t*dy)));
}
static void paint(Surface *s,Tone color,float density,float mask) {
    s->color=tint(s->color,color,sat(mask));
    s->density=lerp(s->density,density,sat(mask));
}

static Surface calendar_page(float x,float y,int year,float light) {
    Surface s={shade((Tone){0.21f,0.18f,0.14f},light),0.07f+0.025f*grain(x,y)};
    char number[5]={(char)('0'+year/1000),(char)('0'+year/100%10),
                    (char)('0'+year/10%10),(char)('0'+year%10),0};
    paint(&s,(Tone){1.0f,0.85f,0.59f},0.86f,
          (float)text_mask(number,x,y,-0.38f,0.066f,0.045f));
    /* Legible weekday headings and dates, not anonymous dots. January 1's
       weekday follows the Gregorian calendar for each displayed year. */
    if (y>0.015f && y<0.87f && fabsf(x)<0.91f) {
        static const char *weekdays[]={"PO","UT","ST","CT","PA","SO","NE"};
        int column=(int)floorf((x+0.91f)/0.26f);
        float local_x=x-(-0.78f+(float)column*0.26f);
        float ink=0;
        if (y<0.145f) ink=(float)text_mask(weekdays[column],local_x,y,0.025f,0.012f,0.0155f);
        else {
            int row=(int)floorf((y-0.18f)/0.115f);
            int previous=year-1;
            int weekday=(previous+previous/4-previous/100+previous/400)%7;
            int day=row*7+column-weekday+1;
            if (row>=0 && row<6 && day>=1 && day<=31) {
                char date[3]={(char)('0'+day/10),(char)('0'+day%10),0};
                ink=(float)text_mask(day<10?date+1:date,local_x,y,0.18f+(float)row*0.115f,0.012f,0.0155f);
            }
        }
        Tone date_color=column>=5?(Tone){0.98f,0.57f,0.40f}:(Tone){0.83f,0.79f,0.66f};
        paint(&s,date_color,0.82f,ink);
    }
    return s;
}
static AsciiCell calendar(float x,float y,float aa,float time,float opacity) {
    Surface s={{0,0,0},0};
    float paper=box(x,y,-1,-0.79f,1,0.94f,aa);
    paint(&s,(Tone){0.24f,0.19f,0.13f},0.48f,box(x-0.025f,y-0.035f,-1,-0.79f,1,0.94f,aa));
    float turning=59*ease(2.0f,6.8f,time);
    int page=(int)floorf(turning),year=2026+page;
    float phase=turning-(float)page;
    Surface base=calendar_page(x,y,year<2085?year+1:2085,0.83f);
    paint(&s,base.color,base.density,paper);
    if (year<2085) {
        float angle=phase*3.14159265f,projection=cosf(angle);
        /* A page rotates around the top binding. Its reverse has no ink. */
        if (fabsf(projection)>0.035f) {
            float py=(y+0.46f)/projection-0.46f;
            float px=x/(1-0.05f*sinf(angle));
            float leaf=box(px,py,-0.98f,-0.46f,0.98f,0.91f,aa/fabsf(projection));
            Surface front=calendar_page(px,py,year,0.96f-0.30f*sinf(angle));
            if (projection<0) front=(Surface){{0.33f,0.28f,0.21f},0.15f};
            paint(&s,front.color,front.density,leaf);
            paint(&s,(Tone){0.97f,0.89f,0.72f},0.65f,
                  stroke(py-0.91f,0.010f,aa/fabsf(projection))*leaf);
        }
    }
    paint(&s,(Tone){0.25f,0.075f,0.055f},0.26f,box(x,y,-1,-0.79f,1,-0.46f,aa));
    paint(&s,(Tone){1.0f,0.95f,0.80f},0.94f,
          (float)text_mask("KALENDAR",x,y,-0.70f,0.036f,0.028f)*paper);
    for (int k=-1;k<=1;k+=2) {
        float ring=segment(x,y,(float)k*0.67f,-0.88f,(float)k*0.67f,-0.74f,0.018f,aa);
        paint(&s,(Tone){0.73f,0.67f,0.53f},0.76f,ring);
    }
    return make_cell(s.color,s.density,opacity);
}

/* A warm, quiet room. Every surface is evaluated at the glyph's world position;
   no screenshots, sprite textures or frame-dependent random numbers are used. */
static Surface room(float x,float y,float aa) {
    float light=bell(x,y,-0.8f,-0.3f,1.5f,1.8f);
    float n=grain(x,y);
    Surface s={shade((Tone){0.11f,0.10f,0.11f},0.35f+0.65f*light),0.04f+0.045f*n};
    if (y>0.64f) {
        float seams=powf(0.5f+0.5f*cosf((x/(y+0.25f))*17),18);
        s.color=shade((Tone){0.19f,0.13f,0.09f},0.38f+0.22f*light-0.20f*seams);
        s.density=0.07f+0.07f*n;
    }
    paint(&s,(Tone){0.25f,0.20f,0.15f},0.33f,stroke(y-0.64f,0.008f,aa));
    /* Blue dusk through a tall window, with a warm wooden frame. */
    float window=box(x,y,-1.38f,-0.76f,-0.88f,0.21f,aa);
    paint(&s,(Tone){0.31f,0.26f,0.20f},0.56f,window);
    float glass=box(x,y,-1.35f,-0.73f,-0.91f,0.18f,aa);
    paint(&s,shade((Tone){0.17f,0.28f,0.39f},0.52f+0.33f*light),0.24f,glass);
    paint(&s,(Tone){0.69f,0.70f,0.59f},0.59f,ellipse(x,y,-1.20f,-0.56f,0.055f,0.055f,aa*12)*glass);
    float mullion=fmaxf(stroke(x+1.13f,0.010f,aa),stroke(y+0.27f,0.010f,aa));
    paint(&s,(Tone){0.27f,0.24f,0.20f},0.48f,mullion*window);
    /* A floor lamp beside the chair. */
    paint(&s,(Tone){0.42f,0.31f,0.19f},0.51f,segment(x,y,1.03f,-0.17f,1.03f,0.80f,0.012f,aa));
    paint(&s,(Tone){0.37f,0.27f,0.18f},0.55f,ellipse(x,y,1.03f,0.82f,0.17f,0.032f,aa*10));
    float shade_width=0.15f+0.16f*sat((y+0.58f)/0.37f);
    float lampshade=box(x,y,1.03f-shade_width,-0.58f,1.03f+shade_width,-0.21f,aa);
    paint(&s,shade((Tone){0.83f,0.62f,0.35f},0.65f+0.23f*cosf(x*55)),0.48f,lampshade);
    /* A small rug anchors the feet and all four chair legs. */
    float rug=ellipse(x,y,0.36f,0.84f,0.75f,0.115f,aa*9);
    float weave=0.5f+0.5f*sinf(x*180)*sinf(y*230);
    paint(&s,shade((Tone){0.36f,0.25f,0.22f},0.36f+0.24f*weave),0.28f+0.12f*weave,rug);
    return s;
}

static void chair(Surface *s,float x,float y,float aa) {
    Tone wood={0.43f,0.27f,0.15f};
    for (int side=-1;side<=1;side+=2) {
        float sx=0.36f+(float)side*0.29f;
        paint(s,shade(wood,0.7f),0.57f,segment(x,y,sx,0.33f,sx+(float)side*0.065f,0.83f,0.025f,aa));
    }
    float back=box(x,y,0.01f,-0.32f,0.71f,0.37f,aa);
    paint(s,wood,0.61f,back);
    float upholstery=box(x,y,0.055f,-0.28f,0.665f,0.32f,aa);
    paint(s,shade((Tone){0.32f,0.25f,0.23f},0.72f+0.20f*sinf(y*110)),0.42f,upholstery);
    for (int side=-1;side<=1;side+=2) {
        float sx=0.36f+(float)side*0.35f;
        paint(s,shade(wood,0.86f),0.62f,segment(x,y,sx,0.12f,sx,0.78f,0.024f,aa));
    }
    paint(s,(Tone){0.48f,0.33f,0.22f},0.62f,box(x,y,-0.01f,0.29f,0.73f,0.38f,aa));
}

static void seated_body(Surface *s,float x,float y,float aa,float time) {
    float breath=0.003f*sinf(time*1.35f);
    float knit=0.5f+0.5f*sinf(x*315+1.4f*sinf(y*170));
    Tone cardigan=shade((Tone){0.34f,0.49f,0.45f},0.66f+0.25f*knit-0.19f*(x-0.36f));
    Tone skin={0.72f,0.53f,0.40f};
    /* Stockings, separate shins, and slippers remain wholly inside the frame. */
    for (int side=-1;side<=1;side+=2) {
        float sx=0.36f+(float)side*0.115f;
        paint(s,(Tone){0.44f,0.35f,0.29f},0.44f,
              segment(x,y,sx,0.44f,sx+(float)side*0.022f,0.77f,0.060f,aa));
        paint(s,(Tone){0.35f,0.25f,0.23f},0.54f,
              ellipse(x,y,sx+(float)side*0.046f,0.795f,0.099f,0.046f,aa*10));
        paint(s,(Tone){0.58f,0.43f,0.35f},0.54f,
              stroke(y-0.815f,0.005f,aa)*ellipse(x,y,sx+(float)side*0.046f,0.795f,0.099f,0.046f,aa*10));
    }
    float skirt=ellipse(x,y,0.36f,0.32f,0.27f,0.24f,aa*5);
    float pleats=0.5f+0.5f*cosf((x-0.36f)*78+2*sinf(y*12));
    paint(s,shade((Tone){0.38f,0.29f,0.36f},0.62f+0.32f*pleats),0.43f+0.15f*pleats,skirt);
    float torso=ellipse(x,y,0.36f,-0.015f+breath,0.225f,0.30f,aa*6);
    paint(s,cardigan,0.42f+0.20f*knit,torso);
    /* V neckline, knitted borders and individual brass buttons. */
    float v=segment(x,y,0.255f,-0.25f,0.36f,-0.04f,0.012f,aa)
           +segment(x,y,0.465f,-0.25f,0.36f,-0.04f,0.012f,aa);
    paint(s,(Tone){0.53f,0.61f,0.52f},0.58f,v*torso);
    paint(s,(Tone){0.20f,0.31f,0.29f},0.40f,stroke(x-0.36f,0.006f,aa)*torso*ease(-0.04f,0,y));
    for (int k=0;k<3;k++)
        paint(s,(Tone){0.73f,0.60f,0.36f},0.78f,ellipse(x,y,0.375f,0.005f+(float)k*0.066f,0.009f,0.011f,aa*12));
    /* Bent sleeves make the seated pose readable even before the close-up. */
    for (int side=-1;side<=1;side+=2) {
        float shoulder=0.36f+(float)side*0.17f,elbow=0.36f+(float)side*0.25f;
        float sleeve=fmaxf(segment(x,y,shoulder,-0.19f+breath,elbow,0.15f,0.066f,aa),
                          segment(x,y,elbow,0.15f,0.36f+(float)side*0.13f,0.245f,0.055f,aa));
        paint(s,shade(cardigan,0.89f),0.46f+0.15f*knit,sleeve);
    }
    /* Open photo album, warm paper, a spine and a miniature couple on the left. */
    float album_y=y+0.09f*(x-0.36f);
    paint(s,(Tone){0.40f,0.21f,0.16f},0.62f,box(x,album_y,0.17f,0.19f,0.56f,0.33f,aa));
    paint(s,(Tone){0.77f,0.69f,0.52f},0.41f,box(x,album_y,0.184f,0.199f,0.548f,0.315f,aa));
    paint(s,(Tone){0.36f,0.24f,0.17f},0.49f,stroke(x-0.365f,0.005f,aa)*box(x,album_y,0.18f,0.20f,0.55f,0.32f,aa));
    paint(s,(Tone){0.21f,0.28f,0.29f},0.44f,box(x,album_y,0.204f,0.212f,0.343f,0.300f,aa));
    for (int k=0;k<2;k++) {
        float px=0.243f+(float)k*0.063f;
        paint(s,(Tone){0.88f,0.66f,0.46f},0.66f,ellipse(x,album_y,px,0.236f,0.015f,0.018f,aa*15));
        paint(s,k?(Tone){0.70f,0.33f,0.45f}:(Tone){0.33f,0.50f,0.68f},0.55f,
              box(x,album_y,px-0.018f,0.255f,px+0.019f,0.29f,aa));
    }
    paint(s,(Tone){0.45f,0.29f,0.19f},0.65f,
          (float)text_mask("2026",(x-0.46f),album_y,0.244f,0.0058f,0.006f));
    /* Age-marked hands rest on the pages; one thumb gently strokes the photo. */
    for (int side=-1;side<=1;side+=2) {
        float hx=0.36f+(float)side*0.153f;
        float hy=0.268f+(side<0?0.005f*sinf(time*1.0f):0);
        float hand=ellipse(x,y,hx,hy,0.054f,0.029f,aa*12);
        float tendons=0.5f+0.5f*sinf((x+0.4f*y)*370);
        paint(s,shade(skin,0.78f+0.16f*tendons),0.51f+0.09f*tendons,hand);
        for (int k=0;k<3;k++)
            paint(s,shade(skin,0.58f),0.38f,
                  stroke(y-hy+0.009f-(float)k*0.010f,0.0015f,aa)*hand*0.60f);
        if (side<0) paint(s,(Tone){0.91f,0.74f,0.39f},0.82f,
                          stroke(x-hx-0.010f,0.004f,aa)*hand);
    }
    /* The wooden arms are visible beside, rather than through, her sleeves. */
    for (int side=-1;side<=1;side+=2) {
        float sx=0.36f+(float)side*0.34f;
        paint(s,(Tone){0.51f,0.34f,0.21f},0.64f,
              segment(x,y,sx-0.025f,0.13f,sx+0.025f,0.13f,0.025f,aa));
    }
}

static AsciiCell seated_ema(float x,float y,float aa,float time,IntroEyeSampler eye) {
    /* Use the detailed existing face as a continuous surface, now on a body.
       The lower portrait shoulders are clipped at the cardigan neckline. */
    float hx=(x-head_x)/head_scale,hy=(y-head_y)/head_scale;
    hx+=(hy+0.12f)*0.10f*(1-ease(13.5f,17.5f,time));
    float head_mask=1-ease(0.48f,0.57f,hy);
    AsciiCell face={' ',0,0,0};
    int face_visible=0;
    if (fabsf(hx)<0.61f && hy>-0.82f && hy<0.57f) {
        face=portrait(hx,hy,aa/head_scale,time,eye);
        float width=0.414f*(1-0.19f*ease(0.12f,0.52f,hy));
        /* Black pupil cells are opaque too; the room must not show through. */
        face_visible=face.glyph!=' ' || sq(hx/width)+sq((hy+0.055f)/0.615f)<1;
        if (face_visible && head_mask>=1) return face;
    }
    Surface s=room(x,y,aa);
    if (x>-0.06f && x<0.79f && y>-0.35f && y<0.85f) {
        chair(&s,x,y,aa);
        seated_body(&s,x,y,aa,time);
    }
    AsciiCell out=make_cell(s.color,s.density,1);
    if (face_visible) out=crossfade(out,face,head_mask);
    /* A smaller version of the very same calendar lives on the wall. */
    if (fabsf(x+0.48f)<0.245f && y>-0.74f && y<-0.28f) {
        AsciiCell cal=calendar((x+0.48f)/0.23f,(y+0.51f)/0.23f,aa/0.23f,7,0.63f);
        if (cal.glyph!=' ') out=cal;
    }
    return out;
}

static void caption(AsciiCell cells[SCENE_ROWS][SCENE_COLS],const char *text,int x0,int y0,float opacity) {
    int length=(int)strlen(text);
    for (int y=0;y<7;y++) for (int x=0;x<length*6-1;x++)
        if (x%6<5 && (letter(text[x/6])[y]&(1u<<(4-x%6))))
            for (int dy=0;dy<SCENE_DETAIL;dy++) for (int dx=0;dx<SCENE_DETAIL;dx++)
                cells[(y0+y)*SCENE_DETAIL+dy][(x0+x)*SCENE_DETAIL+dx]=
                    make_cell((Tone){1.0f,0.88f,0.69f},0.91f,opacity);
}

void intro_render(float time,AsciiCell cells[SCENE_ROWS][SCENE_COLS],IntroEyeSampler eye) {
    if (time>=INTRO_DURATION) return;
    float fade=ease(0,1.35f,time);
    float room_reveal=ease(7.7f,9.3f,time);
    float progress=ease(14.5f,23.4f,time);
    float zoom=expf(logf(1/(head_scale*eye_scale))*progress);
    float pan=ease(14.5f,21.0f,time);
    float blend=ease(INTRO_EYE_BLEND_START,INTRO_DURATION,time);
    float target_x=head_x+head_scale*eye_x,target_y=head_y+head_scale*eye_y;
    /* Keep the destination eye on a smooth screen-space path as magnification
       changes. Independent linear world panning would push it off screen. */
    float camera=1-(1-pan)/zoom;
    int row;
#ifdef _OPENMP
#pragma omp parallel for schedule(static) num_threads(4)
#endif
    for (row=0;row<SCENE_ROWS;row++) for (int col=0;col<SCENE_COLS;col++) {
        float x=((float)col+0.5f-SCENE_COLS*0.5f)*CELL_WIDTH/(SCENE_HEIGHT*0.5f);
        float y=((float)row+0.5f-SCENE_ROWS*0.5f)*CELL_HEIGHT/(SCENE_HEIGHT*0.5f);
        AsciiCell p={' ',0,0,0};
        if (room_reveal<1) p=calendar(x/0.82f,y/0.82f,0.009f/(0.82f*SCENE_DETAIL),time,fade);
        if (room_reveal>0) {
            AsciiCell scene=seated_ema(x/zoom+target_x*camera,y/zoom+target_y*camera,0.009f/(zoom*SCENE_DETAIL),time,eye);
            p=crossfade(p,scene,room_reveal);
        }
        cells[row][col]=blend>0?crossfade(p,cells[row][col],blend):p;
    }
    float label=ease(9.3f,10.2f,time)*(1-ease(13.0f,14.2f,time));
    if (label>0) caption(cells,"EMA, 80 LET",95,2,label);
    float memory=ease(10.5f,11.5f,time)*(1-ease(14.0f,15.3f,time));
    if (memory>0) {
        caption(cells,"NASE 4. VYROCI.",20,65,memory);
        caption(cells,"2026",50,74,memory);
    }
}
