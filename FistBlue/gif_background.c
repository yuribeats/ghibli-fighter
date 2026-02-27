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

#include "sf2types.h"
#include "structs.h"

extern Game g;

#define GIF_MAX_STAGES    12
#define CPS_VIEWPORT_W   384
#define CPS_VIEWPORT_H   224

/* Scroll2 stage bounds from gstate.c GSInitDimensions */
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
    long       rewind_pos;
} gb;

void gif_bg_init(void)
{
    memset(&gb, 0, sizeof(gb));
    gb.texture = 0;
    gb.active = 0;
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
    if (gb.texture) {
        glDeleteTextures(1, &gb.texture);
        gb.texture = 0;
    }
    gb.active = 0;

    if (stage_id < 0 || stage_id >= GIF_MAX_STAGES)
        return;

    snprintf(path, sizeof(path), "./assets/backgrounds/%s", stage_gif_names[stage_id]);

    gb.gif = gd_open_gif(path);
    if (!gb.gif) {
        printf("gif_bg: no GIF for stage %d (%s)\n", stage_id, path);
        return;
    }

    gb.gif_w = gb.gif->width;
    gb.gif_h = gb.gif->height;
    gb.rgba = calloc(4, (size_t)gb.gif_w * gb.gif_h);
    if (!gb.rgba) {
        gd_close_gif(gb.gif);
        gb.gif = NULL;
        return;
    }

    /* Decode first frame */
    if (gd_get_frame(gb.gif) <= 0) {
        printf("gif_bg: failed to decode first frame\n");
        free(gb.rgba);
        gb.rgba = NULL;
        gd_close_gif(gb.gif);
        gb.gif = NULL;
        return;
    }
    gd_render_frame(gb.gif, gb.rgba);

    /* Frame timing: GIF delay is in 1/100s, convert to ms */
    gb.frame_delay_ms = gb.gif->gce.delay * 10;
    if (gb.frame_delay_ms < 20)
        gb.frame_delay_ms = 100;  /* default ~10fps for 0-delay frames */
    gb.frame_timer = gb.frame_delay_ms;

    /* Create GL texture */
    glGenTextures(1, &gb.texture);
    glBindTexture(GL_TEXTURE_2D, gb.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gb.gif_w, gb.gif_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, gb.rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    gb.active = 1;
    printf("gif_bg: loaded %s (%dx%d) delay=%dms\n",
           stage_gif_names[stage_id], gb.gif_w, gb.gif_h, gb.frame_delay_ms);
}

void gif_bg_update(void)
{
    if (!gb.active || !gb.gif)
        return;

    /* Timer runs at 12ms per tick (game frame rate) */
    gb.frame_timer -= 12;
    if (gb.frame_timer > 0)
        return;

    /* Advance to next GIF frame */
    int ret = gd_get_frame(gb.gif);
    if (ret <= 0) {
        /* End of animation or error: loop back */
        gd_rewind(gb.gif);
        ret = gd_get_frame(gb.gif);
        if (ret <= 0)
            return;  /* single-frame GIF or broken file */
    }
    gd_render_frame(gb.gif, gb.rgba);

    /* Update frame delay from this frame's GCE */
    gb.frame_delay_ms = gb.gif->gce.delay * 10;
    if (gb.frame_delay_ms < 20)
        gb.frame_delay_ms = 100;
    gb.frame_timer = gb.frame_delay_ms;

    /* Re-upload texture */
    glBindTexture(GL_TEXTURE_2D, gb.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gb.gif_w, gb.gif_h,
                    GL_RGBA, GL_UNSIGNED_BYTE, gb.rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gif_bg_draw(void)
{
    if (!gb.active)
        return;

    /*
     * The game coordinate system after glScalef(0.3, -0.3, 0.3) in drawgame:
     *   Scroll2 tiles are drawn at TILE_SIZE_SCR2=0.5 units per 16px tile.
     *   Screen is ~24 tiles wide = 12.0 units, centered around 0.
     *   X range: roughly -6 to +6 units = 384 pixels.
     *   Y range: roughly -4 to +4 units = 224 pixels (after the -0.3 Y flip).
     *
     * Scroll2X ranges from 0x0100 (256) to 0x0280 (640).
     * Midpoint = 0x01C0 (448). Total scroll range = 384 pixels.
     *
     * We map the GIF so that:
     *   - At Scroll2X = SCROLL2_MIN_X, we show the left edge of the GIF
     *   - At Scroll2X = SCROLL2_MAX_X, we show the right edge
     *     (meaning the right edge of the GIF aligns with the right edge of the viewport)
     *
     * UV horizontal:
     *   visible_fraction = CPS_VIEWPORT_W / gif_w
     *   u_left = (Scroll2X - SCROLL2_MIN_X) / (SCROLL2_MAX_X - SCROLL2_MIN_X) * (1.0 - visible_fraction)
     *   u_right = u_left + visible_fraction
     */

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

    /* Vertical: show full GIF height, mapped to screen height */
    float v_top = 0.0f;
    float v_bot = 1.0f;

    /* Draw a full-screen quad in the same coordinate space as the scroll layers.
     * The screen spans roughly x: -6 to +6, y: -4 to +4 in GL units.
     * But the scroll layers use offsets like x-12 at TILE_SIZE_SCR2 = 0.5.
     * draw_scroll2 draws from x=-6 to x=39 with offset x-12, at size 0.5.
     * So tile x=0 maps to GL x = (0-12)*0.5 = -6.0
     * tile x=24 maps to GL x = (24-12)*0.5 = +6.0
     * Similarly y: tile y=0 maps to (0-8)*0.5 = -4.0
     * tile y=16 maps to (16-8)*0.5 = +4.0
     *
     * So our quad should go from (-6, -4) to (+6, +4).
     * But we need to go slightly larger to cover everything since scroll2
     * uses glTranslate for sub-tile offset. Use a bit of margin.
     */

    float left   = -6.5f;
    float right  =  6.5f;
    float bottom = -4.5f;
    float top    =  4.5f;

    glPushMatrix();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gb.texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(u_left,  v_top); glVertex3f(left,  top,    0.0f);
    glTexCoord2f(u_right, v_top); glVertex3f(right, top,    0.0f);
    glTexCoord2f(u_right, v_bot); glVertex3f(right, bottom, 0.0f);
    glTexCoord2f(u_left,  v_bot); glVertex3f(left,  bottom, 0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

int gif_bg_is_active(void)
{
    return gb.active;
}
