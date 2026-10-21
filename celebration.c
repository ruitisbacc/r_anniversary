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
/* Local body coordinates keep the gift, kiss and dance in the same continuous pose.
   Rotation projects a shared horizontal orbit, with the farther person drawn first. */
typedef struct {
    Point center;
    float turn, depth, lean, step, skirt, shade;
    int girl;
} Person;
static const Tint skin={1.0f,0.76f,0.61f}, blue={0.37f,0.68f,1.0f};
static const Tint hair={0.43f,0.24f,0.17f}, pink={0.98f,0.48f,0.77f};

static Point body_point(const Person *p,float x,float y) {
    float lean=p->lean*sat((0.26f-y)/0.69f);
    return (Point){p->center.x+(x+lean)*p->turn,p->center.y+y};
}
static Tint body_color(const Person *p,Tint color) {
    return (Tint){color.r*p->shade,color.g*p->shade,color.b*p->shade};
}
static void body_ellipse(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p,
                         float x,float y,float rx,float ry,Tint color,unsigned char ch) {
    ellipse(grid,body_point(p,x,y),rx*(0.70f+0.30f*fabsf(p->turn)),ry,body_color(p,color),ch);
}
static void body_limb(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p,
                      float ax,float ay,float bx,float by,float radius,Tint color) {
    limb(grid,body_point(p,ax,ay),body_point(p,bx,by),radius,body_color(p,color));
}
static void person_dress(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p) {
    for (int row=0;row<SCENE_ROWS;row++) {
        float u=(world_y(row)-p->center.y-0.10f)/0.46f;
        if (u<0 || u>1) continue;
        float half=lerp(0.085f,0.215f+p->skirt,u)*(0.78f+0.22f*fabsf(p->turn));
        float center=p->center.x+0.025f*p->skirt*sinf(u*4)*p->turn;
        for (int col=0;col<SCENE_COLS;col++) {
            float x=world_x(col)-center;
            float hem=0.012f*sinf(x*48+p->skirt*12)*u*u;
            if (fabsf(x)<half && u<1+hem) {
                float pleat=0.72f+0.28f*cosf(x*80-u*3+p->skirt*8);
                paint(grid,col,row,body_color(p,pink),pleat,fabsf(x)>half-0.02f?'/':'v');
            }
        }
    }
}
static void person_body(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p,float kiss) {
    float direction=p->girl?-1.0f:1.0f;
    Tint clothes=p->girl?pink:blue;
    Tint trousers={0.27f,0.39f,0.64f};
    for (int k=0;k<2;k++) {
        float side=k?1.0f:-1.0f;
        float hip=side*0.07f,foot=side*0.105f+p->step*side;
        body_limb(grid,p,hip,p->girl?0.45f:0.22f,foot,0.67f,
                  p->girl?0.030f:0.049f,p->girl?skin:trousers);
        body_ellipse(grid,p,foot+direction*0.025f,0.70f,p->girl?0.067f:0.081f,0.028f,
                     p->girl?pink:(Tint){0.48f,0.57f,0.70f},'=');
    }
    if (p->girl) body_ellipse(grid,p,0.04f,-0.27f,0.13f,0.29f,hair,'~');
    body_ellipse(grid,p,0,-0.025f,p->girl?0.115f:0.14f,p->girl?0.24f:0.29f,clothes,p->girl?'x':'#');
    if (p->girl) person_dress(grid,p);
    else for (int k=0;k<4;k++)
        body_ellipse(grid,p,0.02f,-0.15f+(float)k*0.10f,0.009f,0.012f,pearl,':');
    body_limb(grid,p,0,-0.38f,0,-0.25f,0.040f,skin);
    body_ellipse(grid,p,0,-0.445f,0.103f,0.139f,skin,'o');
    body_ellipse(grid,p,-direction*0.025f,-0.55f,0.110f,0.068f,hair,p->girl?'~':'#');
    body_ellipse(grid,p,-direction*0.085f,-0.46f,0.034f,p->girl?0.148f:0.085f,hair,p->girl?'~':'#');
    /* Back-of-head coverage and closed eyes make the turn and kiss readable. */
    float back=sat(p->depth*4);
    if (back>0.65f) body_ellipse(grid,p,0,-0.455f,0.101f,0.13f,hair,'~');
    else {
        body_ellipse(grid,p,direction*0.106f,-0.425f,0.024f,0.021f,skin,p->girl?'<':'>');
        Point eye=body_point(p,direction*0.062f,-0.478f);
        paint(grid,(int)roundf(grid_x(eye.x)),(int)roundf(grid_y(eye.y)),
              (Tint){0.16f,0.12f,0.17f},1,kiss>0.5f?'-':'*');
        body_ellipse(grid,p,direction*0.077f,-0.405f,0.022f,0.018f,
                     tint_mix(skin,(Tint){1,0.35f,0.48f},0.55f),':');
        body_ellipse(grid,p,direction*0.096f,-0.375f,0.017f,0.010f,
                     (Tint){0.95f,0.38f,0.49f},kiss>0.5f?'o':'_');
    }
}
static void person_arm(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p,
                       float side,Point elbow,Point hand) {
    Point shoulder=body_point(p,side*0.105f,-0.18f);
    limb(grid,shoulder,elbow,p->girl?0.032f:0.043f,body_color(p,p->girl?skin:blue));
    limb(grid,elbow,hand,0.027f,body_color(p,skin));
    ellipse(grid,hand,0.030f,0.024f,body_color(p,skin),'o');
}
static void person_arms(AsciiCell grid[SCENE_ROWS][SCENE_COLS],const Person *p,
                        const Person *partner,float offer,float receive,float hug,Point flowers) {
    if (!p->girl) {
        Point hand=point_mix(body_point(p,0.15f,0.19f),body_point(p,0.475f,0.04f),offer);
        hand=point_mix(hand,body_point(p,0.15f,0.28f),receive);
        hand=point_mix(hand,body_point(partner,0.075f,0.04f),hug);
        Point elbow=point_mix(body_point(p,0.23f,0.10f),body_point(p,0.24f,-0.035f),hug);
        person_arm(grid,p,1,elbow,hand);
        hand=point_mix(body_point(p,-0.11f,0.22f),body_point(partner,-0.035f,-0.13f),hug);
        elbow=point_mix(body_point(p,-0.20f,0.07f),body_point(p,-0.15f,0.04f),hug);
        person_arm(grid,p,-1,elbow,hand);
    } else {
        Point hand=point_mix(body_point(p,-0.15f,0.29f),body_point(p,-0.355f,0.055f),offer);
        hand=point_mix(hand,body_point(p,-0.16f,0.08f),receive);
        hand=point_mix(hand,body_point(partner,-0.06f,-0.10f),hug);
        Point elbow=point_mix(body_point(p,-0.20f,0.08f),body_point(p,-0.23f,-0.10f),hug);
        person_arm(grid,p,-1,elbow,hand);
        Point other=point_mix(body_point(p,0.13f,0.24f),body_point(p,-0.15f,0.12f),receive);
        other=point_mix(other,flowers,hug);
        person_arm(grid,p,1,body_point(p,0.17f,0.08f),other);
    }
}
static void kiss_hearts(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    static const char *shape[]={" ** ** ","*******"," ***** ","  ***  ","   *   "};
    for (int k=0;k<3;k++) {
        float age=t-APPROACH_END-0.3f-(float)k*0.6f;
        if (age<0 || age>3.0f) continue;
        float alpha=ease(0,0.4f,age)*(1-ease(1.8f,3,age));
        Point center={((float)k-1)*0.17f+0.025f*sinf(age*2),-0.66f-0.065f*age};
        for (int y=0;y<5;y++) for (int x=0;x<7;x++) if (shape[y][x]!=' ')
            spark(grid,(Point){center.x+((float)x-3)*0.014f,center.y+(float)y*0.018f},
                  '*',(Tint){1,0.38f,0.62f},alpha*1.5f);
    }
}
static void garden(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t,int foreground) {
    const Tint green={0.34f,0.86f,0.43f};
    static const Tint petals[]={{1,0.43f,0.68f},{0.78f,0.51f,1},{1,0.84f,0.42f},
                               {0.45f,0.73f,1},{1,0.66f,0.79f}};
    for (int k=0;k<34;k++) {
        float angle=6.2831853f*(float)k/34;
        if ((sinf(angle)>0)!=(foreground!=0)) continue;
        float start=SPIN_START+0.12f+(float)(k%9)*0.23f+random01(k,76)*0.7f;
        float grow=ease(start,start+1.9f,t),open=ease(start+0.8f,start+2.7f,t);
        if (grow<0.015f) continue;
        Point root={1.15f*cosf(angle),0.72f+0.20f*sinf(angle)};
        float height=0.15f+random01(k,71)*0.19f;
        float sway=0.018f*sinf(t*1.6f+angle*2)*grow;
        Point tip={root.x+sway,root.y-height*grow};
        for (int j=0;j<8;j++) {
            float u=(float)j/7;
            Point stem={lerp(root.x,tip.x,u)+0.015f*sinf(u*3.14159265f+angle)*grow,
                        lerp(root.y,tip.y,u)};
            spark(grid,stem,'|',green,1.5f);
        }
        for (int side=-1;side<=1;side+=2) {
            Point leaf=point_mix(root,tip,side<0?0.38f:0.62f);
            Point end={leaf.x+(float)side*0.050f*grow,leaf.y-0.032f*grow};
            limb(grid,leaf,end,0.009f,green);
            ellipse(grid,end,0.024f*grow,0.012f,green,side<0?'/':'\\');
        }
        Tint color=petals[k%5];
        if (open<0.1f) ellipse(grid,tip,0.016f,0.023f,color,'o');
        else {
            float size=(0.031f+random01(k,77)*0.012f)*open;
            for (int petal=0;petal<5;petal++) {
                float a=6.2831853f*(float)petal/5-1.5707963f;
                Point petal_center={tip.x+cosf(a)*size,tip.y+sinf(a)*size};
                ellipse(grid,petal_center,size*0.72f,size*0.60f,color,'@');
            }
            ellipse(grid,tip,0.016f,0.015f,gold,'*');
        }
    }
}
static void people(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    float offer=ease(GIFT_START,27.4f,t),receive=ease(27.4f,GIFT_END,t);
    float approach=ease(GIFT_END,APPROACH_END,t),hug=ease(KISS_END,HUG_READY,t);
    float kiss=ease(APPROACH_END-0.5f,APPROACH_END,t)*(1-ease(KISS_END,KISS_END+0.6f,t));
    float spin_time=fmaxf(0,fminf(t,SPIN_END)-SPIN_START);
    /* Integral of a smooth speed ramp; ease to rest before dissolving. */
    float ramp=ease(0,1.6f,spin_time);
    float u=sat(spin_time/1.6f),v=sat((spin_time-(SPIN_END-SPIN_START-1.2f))/1.2f);
    float angle=1.8f*((spin_time<1.6f?1.6f*(u*u*u-0.5f*u*u*u*u):spin_time-0.8f)
                      -1.2f*(v*v*v-0.5f*v*v*v*v));
    float turn=cosf(angle),depth=sinf(angle);
    float separation=lerp(0.43f,0.183f,approach);
    separation=lerp(separation,0.142f,hug);
    float walk=0.018f*sinf(approach*12.56637f)*sinf(approach*3.14159265f);
    float bob=0.009f*ramp*sinf(angle*2);
    Person boy={{-separation*turn,-0.040f*depth+bob},turn,depth,
                0.075f*approach*(1-hug)+0.010f*hug,walk,0,1-0.10f*sat(depth),0};
    Person girl={{separation*turn,0.040f*depth+bob},turn,-depth,
                 -0.075f*approach*(1-hug)-0.010f*hug,-walk,0.040f*ramp,1-0.10f*sat(-depth),1};
    Point flowers=point_mix(body_point(&boy,0.15f,0.19f),body_point(&boy,0.485f,0.045f),offer);
    flowers=point_mix(flowers,body_point(&girl,-0.16f,0.08f),receive);
    flowers=point_mix(flowers,body_point(&girl,0.19f,0.04f),hug);
    garden(grid,t,0);
    const Person *back=depth>=0?&boy:&girl,*front=depth>=0?&girl:&boy;
    person_body(grid,back,kiss);
    person_arms(grid,back,front,offer,receive,hug,flowers);
    if (back->girl) bouquet(grid,flowers);
    person_body(grid,front,kiss);
    person_arms(grid,front,back,offer,receive,hug,flowers);
    if (front->girl) bouquet(grid,flowers);
    /* Before transfer, the bouquet is held in the boy's offering hand. */
    if (receive<0.5f) bouquet(grid,flowers);
    kiss_hearts(grid,t);
    garden(grid,t,1);
}
static void couple(AsciiCell grid[SCENE_ROWS][SCENE_COLS],float t) {
    AsciiCell figure[SCENE_ROWS][SCENE_COLS]={0};
    if (t<FINALE_BURST || t>TITLE_START+0.4f) return;
    people(figure,fminf(t,DISSOLVE_START));
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
    float fade=1-ease(FADE_START,STORY_DURATION,time);
    if (fade<1) for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell *c=&cells[y][x];
        c->r=(unsigned char)((float)c->r*fade);
        c->g=(unsigned char)((float)c->g*fade);
        c->b=(unsigned char)((float)c->b*fade);
        if (c->r+c->g+c->b<3) c->glyph=' ';
    }
}
