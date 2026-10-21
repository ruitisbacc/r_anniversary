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
int main(void) {
    int white=0,orange=0,colors[4]={0};
    float previous=0;
    CHECK(fabsf(scene_wrap_time(-0.5f)-(SCENE_DURATION-0.5f))<0.0001f);
    CHECK(scene_wrap_time(SCENE_DURATION)==0);
    CHECK(scene_wrap_time(INFINITY)==0);
    CHECK(FIREWORKS_END-FIREWORKS_START>=7 && FIREWORKS_END-FIREWORKS_START<=8);
    CHECK(FINALE_LAUNCH>FIREWORKS_END && FINALE_BURST-FINALE_LAUNCH>2);
    for (int i=0;i<385;i++) {
        float t=(float)i*0.1f;
        SceneState s=scene_state(t);
        CHECK(s.pupil_radius>=previous);
        previous=s.pupil_radius;
        scene_render(t,a);
        for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++)
            CHECK(a[y][x].glyph>=32 && a[y][x].glyph<=126);
    }
    static const float repeat_times[]={3,13,21.5f,27.25f,31,34};
    for (int i=0;i<6;i++) {
        scene_render(repeat_times[i],a);
        scene_render(9,b);
        scene_render(repeat_times[i],b);
        CHECK(memcmp(a,b,sizeof(a))==0);
        scene_render(repeat_times[i]+SCENE_DURATION,b);
        CHECK(memcmp(a,b,sizeof(a))==0);
    }
    scene_render(0,a); CHECK(lit_count()==0);
    scene_render(SCENE_DURATION-0.005f,a); CHECK(lit_count()==0);
    scene_render(2,a); CHECK(lit_count()>SCENE_COLS*SCENE_ROWS/2);
    for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
        AsciiCell c=a[y][x];
        if (c.r>110 && c.g>110 && c.b>100 && abs((int)c.r-(int)c.g)<20) white++;
        if (c.r>90 && c.r>c.g*1.5f && c.g>c.b*1.3f) orange++;
    }
    CHECK(white>1500); CHECK(orange>800);
    CHECK(a[SCENE_ROWS/2][SCENE_COLS/2-2].glyph==' ');
    CHECK(scene_state(5.5f).stars>0.5f);
    CHECK(scene_state(7.5f).pupil_radius<1.1f);
    CHECK(scene_state(11.8f).pupil_radius>1.93f);
    /* Every requested firework color must actually appear in the rendered output. */
    for (int i=0;i<80;i++) {
        scene_render(FIREWORKS_START+(float)i*0.1f,a);
        for (int y=0;y<SCENE_ROWS;y++) for (int x=0;x<SCENE_COLS;x++) {
            AsciiCell c=a[y][x];
            if (c.g>90 && c.g>c.r*1.5f && c.g>c.b*1.3f) colors[0]++;
            if (c.b>90 && c.r>c.g*1.4f && c.b>c.r*1.1f) colors[1]++;
            if (c.r>90 && c.r>c.g*1.5f && c.b>c.g*1.3f && c.r>c.b*1.1f) colors[2]++;
            if (c.b>90 && c.b>c.g*1.2f && c.g>c.r*1.3f) colors[3]++;
        }
    }
    for (int i=0;i<4;i++) CHECK(colors[i]>100);
    scene_render(19.8f,a);
    int quiet=lit_count();
    scene_render(21.4f,a); CHECK(lit_count()>quiet+10);
    scene_render(25.2f,a);
    int figure_count=lit_count();
    float before=bouquet_x();
    CHECK(figure_count>900);
    scene_render(29.3f,a);
    float after=bouquet_x();
    CHECK(after-before>25); /* The bouquet really transfers from left to right. */
    scene_render(31.8f,a); CHECK(lit_count()<figure_count/2);
    scene_render(34,a);
    CHECK(lit_count()>600 && lit_count()<2500);
    for (int y=0;y<SCENE_ROWS;y++) if (y<SCENE_ROWS/2-8 || y>SCENE_ROWS/2+8)
        for (int x=0;x<SCENE_COLS;x++) CHECK(a[y][x].r+a[y][x].g+a[y][x].b==0);
    printf("Scene checks: white=%d orange=%d; fireworks=%d/%d/%d/%d; bouquet %.1f -> %.1f; failures=%d\n",
           white,orange,colors[0],colors[1],colors[2],colors[3],before,after,failed);
    return failed?1:0;
}
