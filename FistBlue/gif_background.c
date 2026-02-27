/* gif_background.c - Animated GIF background rendering for sf2ww */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "gif_background.h"
#include "gifdec.h"
#include "stb_image.h"

#include "sf2types.h"
#include "structs.h"

extern Game g;

void gif_bg_update_title(void);

/* Character select background (static PNG) */
static struct {
    GLuint  texture;
    int     loaded;
} cs_bg;

/* VS screen background (static PNG) */
static struct {
    GLuint  texture;
    int     loaded;
} vs_bg;

#define GIF_MAX_STAGES    12
#define CPS_VIEWPORT_W   384

#define SCROLL2_MIN_X    0x0100
#define SCROLL2_MAX_X    0x0280

static const char *stage_gif_names[GIF_MAX_STAGES] = {
    "stage_00_ryu.gif",
    "stage_01_ehonda.gif",
    "stage_02_blanka.gif",
    "stage_03_guile.gif",
    "stage_04_ken.gif",
    "stage_05_chunli.gif",
    "stage_06_zangief.gif",
    "stage_07_dhalsim.gif",
    "stage_08_bison.gif",
    "stage_09_sagat.gif",
    "stage_10_balrog.gif",
    "stage_11_vega.gif",
};

static struct {
    int        active;
    gd_GIF    *gif;
    GLuint     texture;
    uint8_t   *rgba;
    int        gif_w;
    int        gif_h;
    int        frame_timer;
    int        frame_delay_ms;
    int        needs_upload;    /* deferred texture creation flag */
} gb;

/* Title screen background (animated GIF) */
static struct {
    gd_GIF    *gif;
    GLuint     texture;
    uint8_t   *rgba;
    int        gif_w, gif_h;
    int        frame_timer, frame_delay_ms;
    int        needs_upload;
    int        loaded;
} title_bg;

void gif_bg_init(void)
{
    memset(&gb, 0, sizeof(gb));
    memset(&title_bg, 0, sizeof(title_bg));
}

void gif_bg_load_stage(int stage_id)
{
    char path[256];

    /* Clean up previous */
    if (gb.gif) {
        gd_close_gif(gb.gif);
        gb.gif = NULL;
    }
    if (gb.rgba) {
        free(gb.rgba);
        gb.rgba = NULL;
    }
    gb.active = 0;
    gb.needs_upload = 0;
    /* Don't delete GL texture here - might not be in GL context */

    if (stage_id < 0 || stage_id >= GIF_MAX_STAGES)
        return;

    snprintf(path, sizeof(path), "./assets/backgrounds/%s", stage_gif_names[stage_id]);
    printf("gif_bg: trying %s\n", path);

    gb.gif = gd_open_gif(path);
    if (!gb.gif) {
        printf("gif_bg: no GIF for stage %d\n", stage_id);
        return;
    }

    gb.gif_w = gb.gif->width;
    gb.gif_h = gb.gif->height;
    printf("gif_bg: opened %dx%d\n", gb.gif_w, gb.gif_h);

    gb.rgba = calloc(4, (size_t)gb.gif_w * gb.gif_h);
    if (!gb.rgba) {
        gd_close_gif(gb.gif);
        gb.gif = NULL;
        return;
    }

    /* Decode first frame */
    int ret = gd_get_frame(gb.gif);
    printf("gif_bg: gd_get_frame returned %d\n", ret);
    if (ret <= 0) {
        printf("gif_bg: failed to decode first frame\n");
        free(gb.rgba);
        gb.rgba = NULL;
        gd_close_gif(gb.gif);
        gb.gif = NULL;
        return;
    }
    gd_render_frame(gb.gif, gb.rgba);

    gb.frame_delay_ms = gb.gif->gce.delay * 10;
    if (gb.frame_delay_ms < 20)
        gb.frame_delay_ms = 100;
    gb.frame_timer = gb.frame_delay_ms;

    /* Defer GL texture creation to draw call (must be in GL context) */
    gb.needs_upload = 1;
    gb.active = 1;
    printf("gif_bg: loaded stage %d (%dx%d) delay=%dms\n",
           stage_id, gb.gif_w, gb.gif_h, gb.frame_delay_ms);
}

static void ensure_texture(void)
{
    if (!gb.needs_upload)
        return;
    gb.needs_upload = 0;

    if (gb.texture) {
        glDeleteTextures(1, &gb.texture);
        gb.texture = 0;
    }

    glGenTextures(1, &gb.texture);
    glBindTexture(GL_TEXTURE_2D, gb.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gb.gif_w, gb.gif_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, gb.rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gif_bg_update(void)
{
    gif_bg_update_title();

    if (!gb.active || !gb.gif)
        return;

    gb.frame_timer -= 12;
    if (gb.frame_timer > 0)
        return;

    int ret = gd_get_frame(gb.gif);
    if (ret <= 0) {
        gd_rewind(gb.gif);
        ret = gd_get_frame(gb.gif);
        if (ret <= 0)
            return;
    }
    gd_render_frame(gb.gif, gb.rgba);

    gb.frame_delay_ms = gb.gif->gce.delay * 10;
    if (gb.frame_delay_ms < 20)
        gb.frame_delay_ms = 100;
    gb.frame_timer = gb.frame_delay_ms;

    /* Re-upload on next draw */
    if (gb.texture) {
        glBindTexture(GL_TEXTURE_2D, gb.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gb.gif_w, gb.gif_h,
                        GL_RGBA, GL_UNSIGNED_BYTE, gb.rgba);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void gif_bg_draw(void)
{
    if (!gb.active || !gb.rgba)
        return;

    ensure_texture();

    if (!gb.texture)
        return;

    float scroll_frac = 0.0f;
    int scroll_range = SCROLL2_MAX_X - SCROLL2_MIN_X;
    if (scroll_range > 0) {
        int sx = g.CPS.Scroll2X;
        if (sx < SCROLL2_MIN_X) sx = SCROLL2_MIN_X;
        if (sx > SCROLL2_MAX_X) sx = SCROLL2_MAX_X;
        scroll_frac = (float)(sx - SCROLL2_MIN_X) / (float)scroll_range;
    }

    float visible_frac_x = (float)CPS_VIEWPORT_W / (float)gb.gif_w;
    if (visible_frac_x > 1.0f) visible_frac_x = 1.0f;

    float u_left  = scroll_frac * (1.0f - visible_frac_x);
    float u_right = u_left + visible_frac_x;

    float left   = -6.5f;
    float right  =  6.5f;
    float bottom = -4.5f;
    float top    =  4.5f;

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gb.texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(u_left,  0.0f); glVertex3f(left,  bottom, 0.0f);
    glTexCoord2f(u_right, 0.0f); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(u_right, 1.0f); glVertex3f(right, top,    0.0f);
    glTexCoord2f(u_left,  1.0f); glVertex3f(left,  top,    0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

int gif_bg_is_active(void)
{
    return gb.active;
}

/* Character select background */
void gif_bg_load_charselect(void)
{
    int w, h, channels;
    unsigned char *data = stbi_load("./assets/backgrounds/charselect.png", &w, &h, &channels, 4);
    if (!data) {
        printf("charselect_bg: failed to load PNG\n");
        return;
    }

    if (cs_bg.texture)
        glDeleteTextures(1, &cs_bg.texture);

    glGenTextures(1, &cs_bg.texture);
    glBindTexture(GL_TEXTURE_2D, cs_bg.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    cs_bg.loaded = 1;
    printf("charselect_bg: loaded %dx%d\n", w, h);
}

void gif_bg_draw_charselect(void)
{
    if (!cs_bg.loaded || !cs_bg.texture)
        return;

    float left   = -6.5f;
    float right  =  6.5f;
    float bottom = -4.5f;
    float top    =  4.5f;

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cs_bg.texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(left,  bottom, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(right, top,    0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(left,  top,    0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

int gif_bg_charselect_active(void)
{
    return cs_bg.loaded && g.mode0 == 2 && g.mode1 == 0;
}

/* VS screen background */
void gif_bg_load_vs_screen(void)
{
    int w, h, channels;
    unsigned char *data = stbi_load("./assets/backgrounds/vs_screen.png", &w, &h, &channels, 4);
    if (!data) {
        printf("vs_screen_bg: failed to load PNG\n");
        return;
    }

    if (vs_bg.texture)
        glDeleteTextures(1, &vs_bg.texture);

    glGenTextures(1, &vs_bg.texture);
    glBindTexture(GL_TEXTURE_2D, vs_bg.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    vs_bg.loaded = 1;
    printf("vs_screen_bg: loaded %dx%d\n", w, h);
}

void gif_bg_draw_vs_screen(void)
{
    if (!vs_bg.loaded || !vs_bg.texture)
        return;

    float left   = -6.5f;
    float right  =  6.5f;
    float bottom = -4.5f;
    float top    =  4.5f;

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, vs_bg.texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(left,  bottom, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(right, top,    0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(left,  top,    0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

int gif_bg_vs_screen_active(void)
{
    return vs_bg.loaded && g.mode0 == 2 && g.mode1 == 4 && g.mode2 == 4;
}

/* Title screen background */
void gif_bg_load_title(void)
{
    title_bg.loaded = 0;

    title_bg.gif = gd_open_gif("./assets/backgrounds/title.gif");
    if (!title_bg.gif) {
        printf("title_bg: no title.gif found\n");
        return;
    }

    title_bg.gif_w = title_bg.gif->width;
    title_bg.gif_h = title_bg.gif->height;

    title_bg.rgba = calloc(4, (size_t)title_bg.gif_w * title_bg.gif_h);
    if (!title_bg.rgba) {
        gd_close_gif(title_bg.gif);
        title_bg.gif = NULL;
        return;
    }

    int ret = gd_get_frame(title_bg.gif);
    if (ret <= 0) {
        free(title_bg.rgba);
        title_bg.rgba = NULL;
        gd_close_gif(title_bg.gif);
        title_bg.gif = NULL;
        return;
    }
    gd_render_frame(title_bg.gif, title_bg.rgba);

    title_bg.frame_delay_ms = title_bg.gif->gce.delay * 10;
    if (title_bg.frame_delay_ms < 20)
        title_bg.frame_delay_ms = 100;
    title_bg.frame_timer = title_bg.frame_delay_ms;

    title_bg.needs_upload = 1;
    title_bg.loaded = 1;
    printf("title_bg: loaded (%dx%d) delay=%dms\n",
           title_bg.gif_w, title_bg.gif_h, title_bg.frame_delay_ms);
}

static void ensure_title_texture(void)
{
    if (!title_bg.needs_upload)
        return;
    title_bg.needs_upload = 0;

    if (title_bg.texture) {
        glDeleteTextures(1, &title_bg.texture);
        title_bg.texture = 0;
    }

    glGenTextures(1, &title_bg.texture);
    glBindTexture(GL_TEXTURE_2D, title_bg.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, title_bg.gif_w, title_bg.gif_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, title_bg.rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gif_bg_update_title(void)
{
    if (!title_bg.loaded || !title_bg.gif)
        return;

    title_bg.frame_timer -= 12;
    if (title_bg.frame_timer > 0)
        return;

    int ret = gd_get_frame(title_bg.gif);
    if (ret <= 0) {
        gd_rewind(title_bg.gif);
        ret = gd_get_frame(title_bg.gif);
        if (ret <= 0) return;
    }
    gd_render_frame(title_bg.gif, title_bg.rgba);

    title_bg.frame_delay_ms = title_bg.gif->gce.delay * 10;
    if (title_bg.frame_delay_ms < 20)
        title_bg.frame_delay_ms = 100;
    title_bg.frame_timer = title_bg.frame_delay_ms;

    if (title_bg.texture) {
        glBindTexture(GL_TEXTURE_2D, title_bg.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, title_bg.gif_w, title_bg.gif_h,
                        GL_RGBA, GL_UNSIGNED_BYTE, title_bg.rgba);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void gif_bg_draw_title(void)
{
    if (!title_bg.loaded || !title_bg.rgba)
        return;

    ensure_title_texture();

    if (!title_bg.texture)
        return;

    float left   = -6.5f;
    float right  =  6.5f;
    float bottom = -4.5f;
    float top    =  4.5f;

    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, title_bg.texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(left,  bottom, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(right, top,    0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(left,  top,    0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

int gif_bg_title_active(void)
{
    return title_bg.loaded && g.InDemo && g.mode0 != 0xa;
}
