/*
 * 
 *		glutBasics.c
 *		MustardTiger2 GLUT frontend
 *
 *
 */
 
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <GLUT/glut.h>
#include <OpenGL/glext.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "trackball.h"

#include "sf2const.h"
#include "sf2types.h"
#include "sf2macros.h"
#include "task.h"
#include "structs.h"
#include "gfx_glut.h"
#include "lib.h"
#include "game.h"
#include "sf2io.h"
#include "gemu.h"
#include "workarounds.h"
#include "redhammer.h"
#include "gif_background.h"
#include "char_overlay.h"
#include "music_player.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


extern struct game g;
extern struct inputs gInputs;
extern int gShowHelp;

extern CPSGFXEMU gemu;

int time_wait = 12;

typedef struct {
   GLdouble x,y,z;
} recVec;

int gMainWindow = 0;

static int gControlsActive = 1;
static int gControlsTimer = 0;
static float gControlsFade = 1.0f;
static int gControlsFading = 0;

void SetLighting(unsigned int mode) {
#ifndef __EMSCRIPTEN__
    GLfloat mat_specular[] = {1.0, 1.0, 1.0, 1.0};
    GLfloat mat_shininess[] = {90.0};

    GLfloat position[4] = {0.0, 0.0, 12.0, 0.0};

    GLfloat ambient[4]  = {0.5, 0.5, 0.5, 1.0};
    GLfloat diffuse[4]  = {1.0, 1.0, 1.0, 1.0};
    GLfloat specular[4] = {1.0, 1.0, 1.0, 1.0};

    glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);

    switch (mode) {
        case 0:
            break;
        case 1:
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
            glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
            break;
        case 2:
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
            glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_TRUE);
            break;
        case 3:
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
            glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
            break;
        case 4:
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
            glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_TRUE);
            break;
    }

    glLightfv(GL_LIGHT0,GL_POSITION,position);
    glLightfv(GL_LIGHT0,GL_AMBIENT,ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,specular);
    glEnable(GL_LIGHT0);
#endif
}

void init (void) {
    manual_init();
    
    glShadeModel(GL_SMOOTH);
    glFrontFace(GL_CCW);
    
    glColor3f(1.0, 1.0, 1.0);
    gCameraReset ();
    
    glPolygonOffset (1.0, 1.0);
    SetLighting(4);
    glEnable(GL_LIGHTING);
}

void reshape (int w, int h) {
    glViewport(0,0,(GLsizei)w,(GLsizei)h);
    gfx_glut_reshape(w, h);
    glutPostRedisplay();
}

static void ctrl_string(float x, float y, const char *s) {
#ifndef __EMSCRIPTEN__
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *s++);
#endif
}

static void ctrl_key(float cx, float cy, float w, float h, const char *label) {
    float x0 = cx - w * 0.5f, y0 = cy - h * 0.5f;
    float x1 = cx + w * 0.5f, y1 = cy + h * 0.5f;
    glColor3f(0.15f, 0.15f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
    float tw = strlen(label) * 9.0f;
    ctrl_string(cx - tw * 0.5f, cy + 5.0f, label);
}

static void render_controls_overlay(void) {
    GLint vp[4];
    GLint matrixMode;
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glGetIntegerv(GL_VIEWPORT, vp);
    glViewport(0, 0, w, h);
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.0f, 0.0f, 0.0f, gControlsFade);
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f(w, 0);
    glVertex2f(w, h); glVertex2f(0, h);
    glEnd();

    float kw = 60.0f, kh = 32.0f, gap = 8.0f;
    float midx = w * 0.5f;
    float ml = midx - 220.0f;
    float ar = midx + 120.0f;

    glColor3f(1.0f, 1.0f, 1.0f);
    ctrl_string(midx - 36.0f, 60.0f, "CONTROLS");

    glColor3f(0.6f, 0.6f, 0.6f);
    ctrl_string(ml - 18.0f, 120.0f, "MOVEMENT");
    ctrl_string(ar + 10.0f, 120.0f, "ATTACKS");

    float my = 170.0f;
    ctrl_key(ml, my, kw, kh, "UP");
    ctrl_key(ml - kw - gap, my + kh + gap, kw, kh, "LEFT");
    ctrl_key(ml, my + kh + gap, kw, kh, "DOWN");
    ctrl_key(ml + kw + gap, my + kh + gap, kw, kh, "RIGHT");

    float ay = 170.0f, aw = 44.0f;
    ctrl_key(ar, ay, aw, kh, "Q");
    ctrl_key(ar + aw + gap, ay, aw, kh, "W");
    ctrl_key(ar + 2 * (aw + gap), ay, aw, kh, "E");

    glColor3f(0.6f, 0.6f, 0.6f);
    ctrl_string(ar - 4.0f, ay + kh * 0.5f + 18.0f, "LP");
    ctrl_string(ar + aw + gap - 4.0f, ay + kh * 0.5f + 18.0f, "MP");
    ctrl_string(ar + 2 * (aw + gap) - 4.0f, ay + kh * 0.5f + 18.0f, "HP");

    float ky = ay + kh + gap + 36.0f;
    ctrl_key(ar, ky, aw, kh, "A");
    ctrl_key(ar + aw + gap, ky, aw, kh, "S");
    ctrl_key(ar + 2 * (aw + gap), ky, aw, kh, "D");

    glColor3f(0.6f, 0.6f, 0.6f);
    ctrl_string(ar - 4.0f, ky + kh * 0.5f + 18.0f, "LK");
    ctrl_string(ar + aw + gap - 4.0f, ky + kh * 0.5f + 18.0f, "MK");
    ctrl_string(ar + 2 * (aw + gap) - 4.0f, ky + kh * 0.5f + 18.0f, "HK");

    float sy = 380.0f;
    glColor3f(1.0f, 1.0f, 1.0f);
    ctrl_key(midx - 60.0f, sy, 40.0f, kh, "5");
    ctrl_string(midx - 30.0f, sy + 5.0f, "INSERT COIN");
    ctrl_key(midx - 60.0f, sy + kh + gap + 10.0f, 40.0f, kh, "1");
    ctrl_string(midx - 30.0f, sy + kh + gap + 10.0f + 5.0f, "START GAME");
    ctrl_key(midx - 60.0f, sy + 2 * (kh + gap + 10.0f), 40.0f, kh, "ESC");
    ctrl_string(midx - 30.0f, sy + 2 * (kh + gap + 10.0f) + 5.0f, "QUIT");

    if (!gControlsFading && (gControlsTimer / 25) % 2 == 0) {
        ctrl_string(midx - 108.0f, h - 40.0f, "PRESS ANY KEY TO CONTINUE");
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(matrixMode);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
}

void maindisplay(void) {
    gfx_glut_drawgame();
#ifndef __EMSCRIPTEN__
    if (gControlsActive) {
        render_controls_overlay();
    }
#endif
    glutSwapBuffers();
}

void mouse (int button, int state, int x, int y) {
    switch (button) {
        case GLUT_LEFT_BUTTON:
            switch (state) {
                case GLUT_DOWN:
                    gfx_glut_mousedown(x, y);
                    break;
                case GLUT_UP:
                    gfx_glut_mouseup(x, y);
                    break;
            }
            break;
        case GLUT_RIGHT_BUTTON:
            switch (state) {
                case GLUT_DOWN:
                    gfx_glut_rightmousedown(x, y);
                    break;
                case GLUT_UP:
                    gfx_glut_rightmouseup(x, y);
                    break;
            }
            break;
    }
}

void mouseMotion(int x, int y) {
    gfx_glut_mousedragged(x, y);
}

void special(int key, int px, int py) {
    if (gControlsActive) { gControlsFading = 1; }
    switch (key) {
        case GLUT_KEY_UP:		gInputs.p10 |= JOY_UP;    break;
        case GLUT_KEY_DOWN:		gInputs.p10 |= JOY_DOWN;  break;
        case GLUT_KEY_LEFT:		gInputs.p10 |= JOY_LEFT;  break;
        case GLUT_KEY_RIGHT:	gInputs.p10 |= JOY_RIGHT; break;
        default:                                          break;
    }
}
void specialup(int key, int px, int py) {
    switch (key) {
        case GLUT_KEY_UP:       gInputs.p10 &= ~JOY_UP;    break;
        case GLUT_KEY_DOWN:     gInputs.p10 &= ~JOY_DOWN;  break;
        case GLUT_KEY_LEFT:     gInputs.p10 &= ~JOY_LEFT;  break;
        case GLUT_KEY_RIGHT:    gInputs.p10 &= ~JOY_RIGHT; break;
        default:                                           break;
    }
}

void keyup(unsigned char inkey, int px, int py) {
    switch (inkey) {
        case 'q': case 'Q':		gInputs.p10 &= ~(BUTTON_A);	            break;
        case 'w': case 'W':		gInputs.p10 &= ~(BUTTON_B);	            break;
        case 'e': case 'E':		gInputs.p10 &= ~(BUTTON_C);	            break;
        case 'a': case 'A':		gInputs.p11 &= ~(BUTTON_D >> 8);        break;
        case 's': case 'S':		gInputs.p11 &= ~(BUTTON_E >> 8);        break;
        case 'd': case 'D':		gInputs.p11 &= ~(BUTTON_F >> 8);        break;
        case '1':       gInputs.in0 &= ~IPT_START1;	            break;
        case '2':       gInputs.in0 &= ~IPT_START2;	            break;
        case '5':       gInputs.in0 &= ~IPT_COIN1;	            break;
        case '6':       gInputs.in0 &= ~IPT_COIN2;	            break;
        default: break;
    }
}
    
void key(unsigned char inkey, int px, int py){
#ifdef __EMSCRIPTEN__
    if (inkey == 27) { return; }
    EM_ASM({ if (window._sf2audio) window._sf2audio.play().catch(function(){}); });
#else
    if (inkey == 27) { exit(0); }
#endif
    if (gControlsActive) { gControlsFading = 1; }
    switch (inkey) {
        case 'q': case 'Q':		gInputs.p10 |=  BUTTON_A;	   break;
        case 'w': case 'W':		gInputs.p10 |=  BUTTON_B;	   break;
        case 'e': case 'E':		gInputs.p10 |=  BUTTON_C;	   break;
        case 'a': case 'A':		gInputs.p11 |=  BUTTON_D >> 8; break;
        case 's': case 'S':		gInputs.p11 |=  BUTTON_E >> 8; break;
        case 'd': case 'D':		gInputs.p11 |=  BUTTON_F >> 8; break;
        case '1':       gInputs.in0 |= IPT_START1;	   break;
        case '2':       gInputs.in0 |= IPT_START2;	   break;
        case '5':       gInputs.in0 |= IPT_COIN1;	   break;
        case '6':       gInputs.in0 |= IPT_COIN2;	   break;
            
#ifdef REDHAMMER
        case 'k':
            g.Player2.Energy = -1;      break;
        case 'K':
            g.Player1.Energy = -1;      break;
        case '[':
            gShowHelp = !gShowHelp;     break;
#endif
    }
}

void timerFunc(int value) {
    task_timer();
    gif_bg_update();

    if (gControlsActive) {
        gControlsTimer++;
        if (gControlsTimer >= 833) {
            gControlsFading = 1;
        }
        if (gControlsFading) {
            gControlsFade -= 0.04f;
            if (gControlsFade <= 0.0f) {
                gControlsFade = 0.0f;
                gControlsActive = 0;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(time_wait, timerFunc, 0);
}

int main(int argc, const char * argv[])
{
#ifdef __APPLE__
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
        if (url) {
            char path[1024];
            if (CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)path, sizeof(path)))
                chdir(path);
            CFRelease(url);
        }
    }
#endif
    load_cps_roms();

    glutInit(&argc, (char **)argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowPosition (300, 50);
    glutInitWindowSize (900, 600);
    gMainWindow = glutCreateWindow("sf2GL");

    init();					// standard GL init
    gfx_glut_init();
    gif_bg_init();
    char_overlay_init();
    gif_bg_load_charselect();
    gif_bg_load_vs_screen();
    gif_bg_load_title();
    atexit(music_player_stop);
    music_player_play("./assets/music/redrumlake.25minutes.mp3");

#ifndef __EMSCRIPTEN__
    glutIgnoreKeyRepeat(TRUE);
#endif

    glutReshapeFunc(reshape);
    glutDisplayFunc(maindisplay);
    glutKeyboardFunc(key);
    glutKeyboardUpFunc(keyup);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialup);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(40, timerFunc, 0);
    glutMainLoop();
    return 0;
}
