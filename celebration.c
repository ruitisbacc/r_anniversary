#include "celebration.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct { float x,y; } Point;
typedef struct { float r,g,b; } Tint;
typedef struct { float launch,flight,x,apex,drift; int color,style; } Shell;
static const Tint palette[] = {
    {0.25f,1.0f,0.48f}, {0.68f,0.36f,1.0f},
    {1.0f,0.28f,0.65f}, {0.25f,0.59f,1.0f}
};
static const Tint gold={1.0f,0.72f,0.32f}, pearl={1.0f,0.93f,0.82f};
static float sat(float x) { return fmaxf(0,fminf(1,x)); }
static float lerp(float a,float b,float t) { return a+(b-a)*t; }
static float ease(float a,float b,float x) { float t=sat((x-a)/(b-a)); return t*t*(3-2*t); }
static float random01(int n,int salt) {
    uint32_t h=(uint32_t)n*0x9e3779b9u^(uint32_t)salt*0x85ebca6bu;
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    return (float)(h&0xffffffu)/16777215.0f;
}
static Tint tint_mix(Tint a,Tint b,float u) {
    return (Tint){lerp(a.r,b.r,u),lerp(a.g,b.g,u),lerp(a.b,b.b,u)};
}
static Point point_mix(Point a,Point b,float u) {
    return (Point){lerp(a.x,b.x,u),lerp(a.y,b.y,u)};
}
static float grid_x(float x) { return SCENE_COLS*0.5f+x*(SCENE_HEIGHT*0.5f)/CELL_WIDTH-0.5f; }
static float grid_y(float y) { return (y+1)*SCENE_ROWS*0.5f-0.5f; }
static float world_x(int x) { return ((float)x+0.5f-SCENE_COLS*0.5f)*CELL_WIDTH/(SCENE_HEIGHT*0.5f); }
static float world_y(int y) { return ((float)y+0.5f)*2/SCENE_ROWS-1; }

static void light_cell(AsciiCell grid[SCENE_ROWS][SCENE_COLS],int x,int y,
                       unsigned char ch,Tint color,float brightness) {
    if (x>=0 && x<SCENE_COLS && y>=0 && y<SCENE_ROWS && brightness>0.004f) {
        AsciiCell *c=&grid[y][x];
        float r=255*sat(color.r*brightness), g=255*sat(color.g*brightness), b=255*sat(color.b*brightness);
        if (r+g+b>(float)c->r+c->g+c->b) c->glyph=ch;
        c->r=(unsigned char)fminf(255,(float)c->r+r);
        c->g=(unsigned char)fminf(255,(float)c->g+g);
        c->b=(unsigned char)fminf(255,(float)c->b+b);
    }
}
static void spark(AsciiCell grid[SCENE_ROWS][SCENE_COLS],Point p,unsigned char ch,Tint color,float intensity) {
    float x=grid_x(p.x),y=grid_y(p.y);
    int ix=(int)floorf(x),iy=(int)floorf(y);
    float fx=x-(float)ix,fy=y-(float)iy;
    for (int dy=0;dy<2;dy++) for (int dx=0;dx<2;dx++)
        light_cell(grid,ix+dx,iy+dy,ch,color,intensity*(dx?fx:1-fx)*(dy?fy:1-fy));
}

static Point rocket_position(const Shell *s,float u) {
    /* Ballistic rise: quick at the ground, almost stationary at the fuse. */
    float rise=(1.0f-(1.0f-u)*(1.0f-u));
    return (Point){s->x+s->drift*u+0.012f*sinf(u*9),lerp(0.96f,s->apex,rise)};
}
static void rocket(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Shell *s,float t,int heavy) {
    float age=t-s->launch;
    if (age<0 || age>s->flight) return;
    for (int k=26;k>=0;k--) {
        float past=age-(float)k*0.018f;
        if (past<0) continue;
        Point p=rocket_position(s,past/s->flight);
        float tail=1-(float)k/27;
        Tint color=tint_mix((Tint){0.72f,0.22f,0.10f},gold,tail);
        spark(grid,p,k==0?'@':'|',color,tail*tail*(heavy?1.8f:1.2f));
        if (k%3==0) {
            p.x+=(random01(k,(int)(s->launch*100))-0.5f)*0.035f;
            p.y+=0.014f*(float)k;
            spark(grid,p,'.',gold,tail*0.45f);
        }
    }
    Point head=rocket_position(s,age/s->flight);
    spark(grid,head,heavy?'@':'*',pearl,heavy?3.8f:2.6f);
    if (heavy) {
        spark(grid,(Point){head.x-0.013f,head.y},'+',gold,0.8f);
        spark(grid,(Point){head.x+0.013f,head.y},'+',gold,0.8f);
    }
}
static Point ember_position(Point origin,float vx,float vy,float age,int style) {
    float drag=style==2?0.8f:1.45f;
    float travel=(1-expf(-drag*age))/drag;
    return (Point){origin.x+vx*travel,origin.y+vy*travel+0.18f*age*age};
}
static void burst(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Shell *s,float t,int index) {
    float age=t-s->launch-s->flight;
    float life=s->style==2?2.15f:1.75f;
    if (age<0 || age>life) return;
    Point origin=rocket_position(s,1);
    Tint color=palette[s->color];
    float fade=(1-ease(life*0.48f,life,age))*(1-ease(FIREWORKS_END-0.5f,FIREWORKS_END,t));
    for (int i=0;i<180;i++) {
        float angle=6.2831853f*((float)i/180+0.014f*random01(i,index+21));
        float speed=0.72f+0.50f*random01(i,index+31);
        float depth=0.18f+0.82f*sqrtf(random01(i,index+91));
        if (s->style==1) speed=i%3==0?0.55f:1.18f;
        float vx=cosf(angle)*speed*depth,vy=sinf(angle)*speed*depth;
        float twinkle=0.75f+0.25f*sinf(age*13+random01(i,5)*6.28f);
        Tint ember=tint_mix(color,gold,s->style==2?ease(0.3f,1.8f,age)*0.65f:0.08f);
        for (int tail=13;tail>=0;tail--) {
            float past=age-(float)tail*0.038f;
            if (past<0) continue;
            Point p=ember_position(origin,vx,vy,past,s->style);
            float trail=1-(float)tail/14;
            unsigned char ch=tail==0?'*':(fabsf(vx)>fabsf(vy)?'-':(vx*vy>0?'\\':'/'));
            if (age>1.15f || tail>9) ch='.';
            spark(grid,p,ch,ember,fade*twinkle*trail*trail*(tail==0?2.8f:1.0f));
        }
    }
    if (age<0.16f) {
        float flash=1-age/0.16f;
        spark(grid,origin,'@',pearl,5*flash);
        for (int k=-2;k<=2;k++) {
            spark(grid,(Point){origin.x+(float)k*0.025f,origin.y},'-',color,flash);
            spark(grid,(Point){origin.x,origin.y+(float)k*0.025f},'|',color,flash);
        }
    }
}
static void fireworks(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    static const Shell shells[]={
        {11.5f,1.05f,-0.94f,-0.48f, 0.12f,0,0},
        {12.1f,1.18f, 0.78f,-0.58f,-0.08f,1,1},
        {12.8f,1.00f,-0.22f,-0.30f,-0.04f,2,0},
        {13.5f,1.15f, 1.18f,-0.38f,-0.15f,3,2},
        {14.1f,1.05f,-1.20f,-0.46f, 0.13f,1,1},
        {14.8f,1.12f, 0.42f,-0.60f,-0.06f,0,0},
        {15.4f,1.00f,-0.62f,-0.28f, 0.10f,3,2},
        {16.0f,1.06f, 0.94f,-0.48f,-0.10f,2,0},
        {16.7f,1.05f,-0.12f,-0.60f, 0.04f,1,1}
    };
    for (int i=0;i<(int)(sizeof(shells)/sizeof(shells[0]));i++) {
        rocket(grid,&shells[i],t,0);
        burst(grid,&shells[i],t,i);
    }
}

/* Filled silhouettes and their contours remain ASCII cells, not raster art. */
static void paint(AsciiCell grid[SCENE_ROWS][SCENE_COLS],int x,int y,Tint color,float shade,unsigned char ch) {
    if (x<0 || x>=SCENE_COLS || y<0 || y>=SCENE_ROWS) return;
    grid[y][x]=(AsciiCell){ch,(unsigned char)(255*sat(color.r*shade)),
        (unsigned char)(255*sat(color.g*shade)),(unsigned char)(255*sat(color.b*shade))};
}
static void ellipse(AsciiCell grid[SCENE_ROWS][SCENE_COLS],Point center,float rx,float ry,Tint color,unsigned char ch) {
    int x0=(int)floorf(grid_x(center.x-rx)),x1=(int)ceilf(grid_x(center.x+rx));
    int y0=(int)floorf(grid_y(center.y-ry)),y1=(int)ceilf(grid_y(center.y+ry));
    for (int y=y0;y<=y1;y++) for (int x=x0;x<=x1;x++) {
        float nx=(world_x(x)-center.x)/rx,ny=(world_y(y)-center.y)/ry;
        float r2=nx*nx+ny*ny;
        if (r2<=1) paint(grid,x,y,color,0.65f+0.35f*sqrtf(1-r2),r2>0.78f?':':ch);
    }
}
static void limb(AsciiCell grid[SCENE_ROWS][SCENE_COLS],Point a,Point b,float radius,Tint color) {
    float distance=hypotf(b.x-a.x,b.y-a.y);
    int steps=(int)(distance/0.012f)+1;
    for (int k=0;k<=steps;k++) ellipse(grid,point_mix(a,b,(float)k/(float)steps),radius,radius,color,'o');
}
static void dress(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float sway) {
    for (int row=0;row<SCENE_ROWS;row++) {
        float y=world_y(row),u=(y-0.10f)/0.46f;
        if (u<0 || u>1) continue;
        float half=lerp(0.085f,0.215f,u),center=0.43f+sway*u;
        for (int col=0;col<SCENE_COLS;col++) {
            float x=world_x(col)-center;
            if (fabsf(x)<half) {
                float pleat=0.72f+0.28f*cosf(x*80-u*3);
                paint(grid,col,row,(Tint){0.83f,0.40f,0.72f},pleat,fabsf(x)>half-0.02f?'/':'v');
            }
        }
    }
}
static void bouquet(AsciiCell grid[SCENE_ROWS][SCENE_COLS],Point hand) {
    for (int k=0;k<7;k++) {
        float angle=6.2831853f*(float)k/7;
        Point flower={hand.x+0.062f*cosf(angle),hand.y-0.19f+0.045f*sinf(angle)};
        limb(grid,hand,flower,0.009f,(Tint){0.31f,0.80f,0.39f});
    }
    ellipse(grid,(Point){hand.x-0.033f,hand.y-0.08f},0.041f,0.018f,(Tint){0.33f,0.92f,0.44f},'/');
    ellipse(grid,(Point){hand.x+0.033f,hand.y-0.11f},0.041f,0.018f,(Tint){0.33f,0.92f,0.44f},'\\');
    for (int k=0;k<7;k++) {
        float angle=6.2831853f*(float)k/7;
        Point flower={hand.x+0.062f*cosf(angle),hand.y-0.19f+0.045f*sinf(angle)};
        Tint color=k%2?(Tint){1,0.36f,0.62f}:(Tint){0.85f,0.53f,1};
        ellipse(grid,flower,0.032f,0.028f,color,'@');
        paint(grid,(int)roundf(grid_x(flower.x)),(int)roundf(grid_y(flower.y)),pearl,1,'*');
    }
    ellipse(grid,hand,0.025f,0.018f,gold,'X');
}
static void people(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    const Tint skin={1.0f,0.76f,0.56f}, blue={0.32f,0.62f,0.89f}, pants={0.24f,0.36f,0.57f};
    const Tint hair={0.45f,0.25f,0.18f}, pink={0.86f,0.46f,0.73f};
    float offer=ease(GIFT_START,27.4f,t),receive=ease(27.4f,GIFT_END,t);
    float nod=0.025f*sinf(receive*3.14159265f);
    Point boy_hand=point_mix((Point){-0.28f,0.19f},(Point){0.045f,0.04f},offer);
    boy_hand=point_mix(boy_hand,(Point){-0.28f,0.28f},ease(28.7f,29.8f,t));
    Point girl_hand=point_mix((Point){0.28f,0.29f},(Point){0.075f,0.055f},offer);
    girl_hand=point_mix(girl_hand,(Point){0.27f,0.08f},receive);
    Point flowers=point_mix((Point){-0.28f,0.19f},(Point){0.055f,0.045f},offer);
    flowers=point_mix(flowers,(Point){0.27f,0.08f},receive);
    /* Shoes and legs. The boy wears trousers, the girl a flared dress. */
    limb(grid,(Point){-0.50f,0.22f},(Point){-0.53f,0.67f},0.049f,pants);
    limb(grid,(Point){-0.36f,0.22f},(Point){-0.31f,0.67f},0.049f,pants);
    ellipse(grid,(Point){-0.50f,0.69f},0.084f,0.035f,(Tint){0.42f,0.51f,0.61f},'=');
    ellipse(grid,(Point){-0.28f,0.69f},0.084f,0.035f,(Tint){0.42f,0.51f,0.61f},'=');
    limb(grid,(Point){0.36f,0.45f},(Point){0.33f,0.68f},0.030f,skin);
    limb(grid,(Point){0.49f,0.45f},(Point){0.53f,0.68f},0.030f,skin);
    ellipse(grid,(Point){0.30f,0.70f},0.067f,0.026f,pink,'=');
    ellipse(grid,(Point){0.51f,0.70f},0.067f,0.026f,pink,'=');
    /* Long hair sits behind the face and dress. */
    ellipse(grid,(Point){0.47f,-0.27f+nod},0.13f,0.29f,hair,'~');
    ellipse(grid,(Point){-0.43f,-0.025f},0.14f,0.29f,blue,'#');
    ellipse(grid,(Point){0.43f,-0.035f},0.115f,0.24f,pink,'x');
    dress(grid,0.009f*sinf(t*1.1f));
    limb(grid,(Point){-0.55f,-0.20f},(Point){-0.63f,0.07f},0.043f,blue);
    limb(grid,(Point){-0.63f,0.07f},(Point){-0.54f,0.22f},0.032f,skin);
    /* Boy's offering arm and girl's receiving hands interpolate independently. */
    Point elbow=point_mix((Point){-0.25f,0.07f},(Point){-0.18f,0.11f},offer);
    limb(grid,(Point){-0.33f,-0.19f},elbow,0.045f,blue);
    limb(grid,elbow,boy_hand,0.028f,skin);
    limb(grid,(Point){0.33f,-0.18f},(Point){0.23f,0.08f},0.035f,skin);
    limb(grid,(Point){0.23f,0.08f},girl_hand,0.026f,skin);
    Point other=point_mix((Point){0.56f,0.24f},(Point){0.28f,0.12f},receive);
    limb(grid,(Point){0.53f,-0.17f},(Point){0.60f,0.08f},0.032f,skin);
    limb(grid,(Point){0.60f,0.08f},other,0.026f,skin);
    limb(grid,(Point){-0.43f,-0.38f},(Point){-0.43f,-0.25f},0.043f,skin);
    limb(grid,(Point){0.43f,-0.35f+nod},(Point){0.43f,-0.24f},0.038f,skin);
    ellipse(grid,(Point){-0.43f,-0.45f},0.105f,0.142f,skin,'o');
    ellipse(grid,(Point){0.42f,-0.43f+nod},0.10f,0.136f,skin,'o');
    ellipse(grid,(Point){-0.46f,-0.55f},0.115f,0.073f,hair,'#');
    ellipse(grid,(Point){-0.52f,-0.48f},0.033f,0.095f,hair,'#');
    ellipse(grid,(Point){0.45f,-0.54f+nod},0.103f,0.064f,hair,'~');
    ellipse(grid,(Point){0.51f,-0.42f+nod},0.035f,0.135f,hair,'~');
    /* Profile noses, eyes and a small smile face inward. */
    ellipse(grid,(Point){-0.323f,-0.43f},0.028f,0.025f,skin,'>');
    ellipse(grid,(Point){0.317f,-0.41f+nod},0.025f,0.022f,skin,'<');
    paint(grid,(int)roundf(grid_x(-0.355f)),(int)roundf(grid_y(-0.485f)),(Tint){0.12f,0.14f,0.19f},1,'*');
    paint(grid,(int)roundf(grid_x(0.35f)),(int)roundf(grid_y(-0.46f+nod)),(Tint){0.12f,0.14f,0.19f},1,'*');
    paint(grid,(int)roundf(grid_x(-0.35f)),(int)roundf(grid_y(-0.365f)),(Tint){0.70f,0.29f,0.25f},1,'/');
    paint(grid,(int)roundf(grid_x(0.35f)),(int)roundf(grid_y(-0.35f+nod)),(Tint){0.75f,0.27f,0.39f},1,'\\');
    for (int k=0;k<4;k++) paint(grid,(int)roundf(grid_x(-0.41f)),(int)roundf(grid_y(-0.15f+(float)k*0.10f)),pearl,0.8f,':');
    bouquet(grid,flowers);
    ellipse(grid,boy_hand,0.030f,0.024f,skin,'o');
    ellipse(grid,girl_hand,0.028f,0.023f,skin,'o');
}
static void couple(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    AsciiCell figure[SCENE_ROWS][SCENE_COLS]={0};
    if (t<FINALE_BURST || t>TITLE_START+0.4f) return;
    people(figure,t);
    float form=ease(FINALE_BURST,COUPLE_READY,t);
    float dissolve=ease(DISSOLVE_START,TITLE_START+0.4f,t);
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell c=figure[y][x];
        if (!c.glyph) continue;
        int seed=y*SCENE_COLS+x;
        Point target={world_x(x),world_y(y)};
        Tint color={(float)c.r/255,(float)c.g/255,(float)c.b/255};
        if (form>=1 && dissolve<=0) { grid[y][x]=c; continue; }
        Point p=point_mix((Point){0,-0.48f},target,form);
        float arc=sinf(form*3.14159265f);
        p.x+=arc*(random01(seed,11)-0.5f)*0.5f;
        p.y-=arc*random01(seed,12)*0.28f;
        p.x+=dissolve*(random01(seed,13)-0.5f)*1.2f;
        p.y-=dissolve*(0.18f+0.55f*random01(seed,14));
        float alpha=ease(FINALE_BURST,FINALE_BURST+0.3f,t)*(1-dissolve);
        alpha*=1-ease(0.30f+0.55f*random01(seed,15),1,dissolve);
        color=tint_mix(gold,color,form);
        spark(grid,p,form<0.6f || dissolve>0.15f?'.':c.glyph,color,alpha);
    }
}

/* Compact 5x7 lowercase lettering, expanded into colored ASCII characters. */
static const unsigned char *letter(char c) {
    static const unsigned char blank[7]={0};
    static const unsigned char four[7]={2,6,10,18,31,2,2};
    static const unsigned char y[7]={0,0,17,17,15,1,14};
    static const unsigned char e[7]={0,0,14,17,31,16,14};
    static const unsigned char a[7]={0,0,14,1,15,17,15};
    static const unsigned char r[7]={0,0,22,25,16,16,16};
    static const unsigned char s[7]={0,0,15,16,14,1,30};
    static const unsigned char t[7]={8,8,30,8,8,9,6};
    static const unsigned char o[7]={0,0,14,17,17,17,14};
    static const unsigned char g[7]={0,0,15,17,15,1,14};
    static const unsigned char h[7]={16,16,22,25,17,17,17};
    switch(c) {
        case '4':return four; case 'y':return y; case 'e':return e;
        case 'a':return a; case 'r':return r; case 's':return s;
        case 't':return t; case 'o':return o; case 'g':return g; case 'h':return h;
        default:return blank;
    }
}
static void title(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    const char *message="4 years together";
    int length=(int)strlen(message),x0=(SCENE_COLS-(length*6-1)*2)/2;
    int y0=SCENE_ROWS/2-7;
    float visible=ease(TITLE_START,TITLE_START+1.3f,t);
    for (int k=0;k<length;k++) {
        const unsigned char *bits=letter(message[k]);
        for (int y=0;y<7;y++) for (int x=0;x<5;x++) if (bits[y]&(1u<<(4-x))) {
            Tint color=tint_mix((Tint){1,0.80f,0.88f},(Tint){0.76f,0.51f,1},(float)y/7);
            for (int dy=0;dy<2;dy++) for (int dx=0;dx<2;dx++)
                light_cell(grid,x0+k*12+x*2+dx,y0+y*2+dy,'#',color,visible);
        }
    }
}

void celebration_render(float time,AsciiCell cells[SCENE_ROWS][SCENE_COLS]) {
    if (time<FIREWORKS_START) return;
    if (time<FIREWORKS_END) fireworks(cells,time);
    const Shell finale={FINALE_LAUNCH,FINALE_BURST-FINALE_LAUNCH,-0.10f,-0.48f,0.10f,0,0};
    rocket(cells,&finale,time,1);
    couple(cells,time);
    if (time>=TITLE_START) title(cells,time);
    float fade=1-ease(37.0f,SCENE_DURATION,time);
    if (fade<1) for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell *c=&cells[y][x];
        c->r=(unsigned char)((float)c->r*fade);
        c->g=(unsigned char)((float)c->g*fade);
        c->b=(unsigned char)((float)c->b*fade);
        if (c->r+c->g+c->b<3) c->glyph=' ';
    }
}
