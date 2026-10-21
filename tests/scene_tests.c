#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AsciiCell a[SCENE_ROWS][SCENE_COLS],b[SCENE_ROWS][SCENE_COLS];
static int failed;
static void check(int success,int line,const char *expression) {
    if (!success) { fprintf(stderr,"FAIL line %d: %s\n",line,expression); failed++; }
}
#define CHECK(expr) check((expr),__LINE__,#expr)
static void story_render(float time,AsciiCell cells[SCENE_ROWS][SCENE_COLS]) {
    scene_render(INTRO_DURATION+time,cells);
}
static SceneState story_state(float time) { return scene_state(INTRO_DURATION+time); }
static int lit_count(void) {
    int count=0;
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++)
        if (a[y][x].glyph!=' ' && a[y][x].r+a[y][x].g+a[y][x].b>30) count++;
    return count;
}
static float bouquet_x(void) {
    float sum=0; int count=0;
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell c=a[y][x];
        if (c.g>65 && c.g>c.r*1.5f && c.g>c.b*1.4f) { sum+=(float)x; count++; }
    }
    CHECK(count>5);
    return count?sum/(float)count:0;
}
static float shirt_distance(void) {
    float blue_sum=0,pink_sum=0;
    int blue_count=0,pink_count=0;
    for (int y=33;y<46;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell c=a[y][x];
        if (c.b>70 && c.b>c.r*1.5f && c.g>c.r*1.3f) { blue_sum+=(float)x; blue_count++; }
        if (c.r>90 && c.b>70 && c.r>c.g*1.4f && c.b>c.g*1.2f) { pink_sum+=(float)x; pink_count++; }
    }
    CHECK(blue_count>5 && pink_count>5);
    return blue_count && pink_count?pink_sum/(float)pink_count-blue_sum/(float)blue_count:0;
}
static int garden_cells(void) {
    int count=0;
    /* Outside the couple and bouquet: these cells can only be garden flowers. */
    for (int y=50;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++)
        if ((x<SCENE_COLS/2-32 || x>SCENE_COLS/2+32) &&
            a[y][x].r+a[y][x].g+a[y][x].b>100) count++;
    return count;
}
static int embracing_arm_cells(void) {
    int count=0;
    /* The girl's forearm crosses the boy's blue shirt only in the embrace. */
    for (int y=35;y<44;y++) for (int x=SCENE_COLS/2-19;x<SCENE_COLS/2-7;x++) {
        AsciiCell c=a[y][x];
        if (c.r>120 && c.g>c.r*0.6f && c.g<c.r*0.85f && c.b<c.g*0.9f) count++;
    }
    return count;
}
int main(void) {
    int white=0,orange=0,colors[4]={0};
    float previous=0;
    CHECK(fabsf(scene_wrap_time(-0.5f)-(SCENE_DURATION-0.5f))<0.0001f);
    CHECK(scene_wrap_time(SCENE_DURATION)==0);
    CHECK(scene_wrap_time(INFINITY)==0);
    CHECK(FIREWORKS_END-FIREWORKS_START>=7 && FIREWORKS_END-FIREWORKS_START<=8);
    CHECK(FINALE_LAUNCH>FIREWORKS_END && FINALE_BURST-FINALE_LAUNCH>2);
    for (int i=0;(float)i*0.1f<SCENE_DURATION;i++) {
        float t=(float)i*0.1f;
        SceneState s=scene_state(t);
        CHECK(s.pupil_radius>=previous);
        previous=s.pupil_radius;
        scene_render(t,a);
        for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++)
            CHECK(a[y][x].glyph>=32 && a[y][x].glyph<=126);
    }
    static const float repeat_times[]={2,5.75f,7,9,11,14.5f,15.75f,16,19,29,37.5f,43.25f,47,48,50.5f,53,55,58,59,62};
    for (int i=0;i<(int)(sizeof(repeat_times)/sizeof(repeat_times[0]));i++) {
        scene_render(repeat_times[i],a);
        scene_render(9,b);
        scene_render(repeat_times[i],b);
        CHECK(memcmp(a,b,sizeof(a))==0);
        scene_render(repeat_times[i]+SCENE_DURATION,b);
        CHECK(memcmp(a,b,sizeof(a))==0);
    }
    scene_render(0,a); CHECK(lit_count()==0);
    scene_render(SCENE_DURATION-0.005f,a); CHECK(lit_count()==0);
    scene_render(2,a);
    CHECK(lit_count()>800 && lit_count()<2500);
    /* The year is above the name, with a clear gap between the two lines. */
    for (int y=44;y<51;y++) for (int x=0;x<SCENE_COLS;x++) CHECK(a[y][x].glyph==' ');
    scene_render(4.1f,a); CHECK(lit_count()==0);
    scene_render(6,a);
    int portrait_count=lit_count();
    CHECK(portrait_count>3000 && portrait_count<10000);
    CHECK(a[SCENE_ROWS/2][0].glyph==' ');
    scene_render(11,a); CHECK(lit_count()>portrait_count*2);
    CHECK(scene_state(INTRO_DURATION-0.1f).stars==0);
    /* Crossing the splice must not flash black or reopen the eyelid. */
    scene_render(INTRO_DURATION-0.01f,a);
    scene_render(INTRO_DURATION,b);
    long splice_difference=0;
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        splice_difference+=abs((int)a[y][x].r-(int)b[y][x].r)
                          +abs((int)a[y][x].g-(int)b[y][x].g)
                          +abs((int)a[y][x].b-(int)b[y][x].b);
    }
    CHECK(splice_difference<SCENE_COLS*SCENE_ROWS*3);
    story_render(2,a); CHECK(lit_count()>SCENE_COLS*SCENE_ROWS/2);
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell c=a[y][x];
        if (c.r>110 && c.g>110 && c.b>100 && abs((int)c.r-(int)c.g)<20) white++;
        if (c.r>90 && c.r>c.g*1.5f && c.g>c.b*1.3f) orange++;
    }
    CHECK(white>1500); CHECK(orange>800);
    CHECK(a[SCENE_ROWS/2][SCENE_COLS/2-2].glyph==' ');
    CHECK(story_state(5.5f).stars>0.5f);
    CHECK(story_state(7.5f).pupil_radius<1.1f);
    CHECK(story_state(11.8f).pupil_radius>1.93f);
    /* Every requested firework color must actually appear in the rendered output. */
    for (int i=0;i<80;i++) {
        story_render(FIREWORKS_START+(float)i*0.1f,a);
        for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
            AsciiCell c=a[y][x];
            if (c.g>90 && c.g>c.r*1.5f && c.g>c.b*1.3f) colors[0]++;
            if (c.b>90 && c.r>c.g*1.4f && c.b>c.r*1.1f) colors[1]++;
            if (c.r>90 && c.r>c.g*1.5f && c.b>c.g*1.3f && c.r>c.b*1.1f) colors[2]++;
            if (c.b>90 && c.b>c.g*1.2f && c.g>c.r*1.3f) colors[3]++;
        }
    }
    for (int i=0;i<4;i++) CHECK(colors[i]>100);
    story_render(19.8f,a);
    int quiet=lit_count();
    story_render(21.4f,a); CHECK(lit_count()>quiet+10);
    story_render(25.2f,a);
    int figure_count=lit_count();
    float before=bouquet_x();
    CHECK(figure_count>900);
    story_render(GIFT_END,a);
    float after=bouquet_x();
    CHECK(after-before>25); /* The bouquet really transfers from left to right. */
    float apart=shirt_distance();
    story_render(APPROACH_END,a);
    float together=shirt_distance();
    int before_hug=embracing_arm_cells();
    CHECK(together>0 && together<apart*0.65f);
    CHECK(lit_count()>figure_count/2); /* They remain visible through the kiss. */
    story_render(HUG_READY,a);
    CHECK(shirt_distance()<apart*0.65f);
    CHECK(embracing_arm_cells()>before_hug+5);
    CHECK(garden_cells()==0);
    story_render(36.5f,a);
    int sprouts=garden_cells();
    story_render(39,a);
    int blooms=garden_cells();
    CHECK(sprouts>20 && blooms>sprouts+100);
    story_render(38,a);
    CHECK(shirt_distance()<0); /* A half turn swaps the two people's screen order. */
    story_render(40,a);
    CHECK(shirt_distance()>0); /* They continue turning while the garden stays. */
    story_render(TITLE_START-0.2f,a); CHECK(lit_count()<figure_count/2);
    story_render(TITLE_START+2,a);
    CHECK(lit_count()>600 && lit_count()<2500);
    for (int y=0;y<SCENE_ROWS;y++) if (y<SCENE_ROWS/2-8 || y>SCENE_ROWS/2+8)
        for (int x=0;x<SCENE_COLS;x++) CHECK(a[y][x].r+a[y][x].g+a[y][x].b==0);
    printf("Scene checks: white=%d orange=%d; fireworks=%d/%d/%d/%d; bouquet %.1f -> %.1f; failures=%d\n",
           white,orange,colors[0],colors[1],colors[2],colors[3],before,after,failed);
    return failed?1:0;
}
