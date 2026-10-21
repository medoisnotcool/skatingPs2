/* input.h - DualShock reading, key bindings, options */
#ifndef INPUT_H
#define INPUT_H
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include "math3d.h"

typedef struct {
    u16 down, pressed;
    float lx, ly, rx, ry;       /* -1..1, +y = stick pushed UP */
    int analog;                 /* 1 if a DualShock in analog mode is detected */
} Input;
static Input g_in;

/* actions that can be re-bound in the options menu */
enum { A_RUN, A_HOLD, A_EQUIP, A_CARRY, A_JUMP, A_TRICKJUMP, A_PUSH, A_BRAKE, A_MANUAL, A_GRABL, A_GRABR, A_COUNT };
static const char *ACT_NAME[A_COUNT] = {
    "RUN (HOLD)", "HOLD / THROW BOARD", "EQUIP / STEP OFF", "CARRY BOARD (HOLD)", "JUMP WITH BOARD",
    "TRICK JUMP WITH BOARD", "PUSH", "BRAKE", "MANUAL (HOLD)", "GRAB (LEFT HAND)", "GRAB (RIGHT HAND)"
};
static const u16 BIND_DEFAULT[A_COUNT] = {
    PAD_CIRCLE, PAD_R1, PAD_TRIANGLE, PAD_L1, PAD_CROSS, PAD_SQUARE, PAD_CROSS, PAD_CIRCLE, PAD_SQUARE, PAD_L2, PAD_R2
};
static u16 g_bind[A_COUNT];
#define ACT_DOWN(a)  (g_in.down & g_bind[a])
#define ACT_PRESS(a) (g_in.pressed & g_bind[a])

static const u16 BTN_LIST[10] = { PAD_CROSS, PAD_CIRCLE, PAD_TRIANGLE, PAD_SQUARE, PAD_L1, PAD_R1, PAD_L2, PAD_R2, PAD_L3, PAD_R3 };
static const char *btn_name(u16 m) {
    switch (m) {
        case PAD_CROSS: return "X";        case PAD_CIRCLE: return "CIRCLE";
        case PAD_TRIANGLE: return "TRIANGLE"; case PAD_SQUARE: return "SQUARE";
        case PAD_L1: return "L1";          case PAD_R1: return "R1";
        case PAD_L2: return "L2";          case PAD_R2: return "R2";
        case PAD_L3: return "L3";          case PAD_R3: return "R3";
        case PAD_SELECT: return "SELECT";
    }
    return "?";
}

typedef struct {
    int autorun;      /* 0/1 */
    int goofy;        /* 0 regular, 1 goofy */
    int camDist;      /* 0 near, 1 mid, 2 far */
    int camFollow;    /* 0/1 */
    int sens;         /* 0 low, 1 med, 2 high : right-stick flick sensitivity */
    int invertRY;     /* 0/1 */
    int showHud;      /* 0/1 */
} Options;
static Options g_opt;

static void options_defaults(void) {
    g_opt.autorun = 0; g_opt.goofy = 0; g_opt.camDist = 1; g_opt.camFollow = 1;
    g_opt.sens = 1; g_opt.invertRY = 0; g_opt.showHud = 1;
    for (int i = 0; i < A_COUNT; i++) g_bind[i] = BIND_DEFAULT[i];
}

/* ------------------------------------------------------------ pad driver */
static char g_padBuf[256] __attribute__((aligned(64)));
static int g_padOK;

static int pad_wait_ready(void) {
    for (int i = 0; i < 400000; i++) {
        int s = padGetState(0, 0);
        if (s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1) return 1;
    }
    return 0;
}

static void pad_init(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    g_padOK = padPortOpen(0, 0, g_padBuf);
    if (g_padOK) {
        pad_wait_ready();
        /* ask for DualShock (analog) mode and lock it */
        if (padInfoMode(0, 0, PAD_MODECURID, 0) != 0)
            padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
        pad_wait_ready();
    }
    memset(&g_in, 0, sizeof g_in);
}

static float stick_norm(int raw) {
    float v = ((float)raw - 128.0f) / 127.0f;
    return clampf(v, -1.0f, 1.0f);
}
static void stick_dead(float *x, float *y) {
    float m = sqrtf(*x * *x + *y * *y);
    const float dz = 0.22f;
    if (m < dz) { *x = 0; *y = 0; return; }
    float s = (m - dz) / (1.0f - dz);
    if (s > 1) s = 1;
    *x = *x / m * s; *y = *y / m * s;
}

static int g_padMode, g_padModeAge = 9999;

static void pad_poll(void) {
    struct padButtonStatus b;
    u16 prev = g_in.down;
    int st = padGetState(0, 0);
    g_in.lx = g_in.ly = g_in.rx = g_in.ry = 0;
    g_in.analog = 0;
    if (!g_padOK || (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1)) {
        g_in.down = 0; g_in.pressed = 0;
        return;
    }
    if (padRead(0, 0, &b) != 0) {
        g_in.down = (u16)(0xffff ^ b.btns);
        if (++g_padModeAge > 60) { g_padMode = padInfoMode(0, 0, PAD_MODECURID, 0); g_padModeAge = 0; }
        if (g_padMode == PAD_TYPE_DUALSHOCK) {
            g_in.analog = 1;
            g_in.lx = stick_norm(b.ljoy_h);  g_in.ly = -stick_norm(b.ljoy_v);
            g_in.rx = stick_norm(b.rjoy_h);  g_in.ry = -stick_norm(b.rjoy_v);
            stick_dead(&g_in.lx, &g_in.ly);
            stick_dead(&g_in.rx, &g_in.ry);
        }
        if (!g_in.analog) {                                 /* digital pad: D-pad doubles as the left stick */
            if (g_in.down & PAD_LEFT)  g_in.lx -= 1;
            if (g_in.down & PAD_RIGHT) g_in.lx += 1;
            if (g_in.down & PAD_UP)    g_in.ly += 1;
            if (g_in.down & PAD_DOWN)  g_in.ly -= 1;
        }
    }
    g_in.pressed = g_in.down & (u16)~prev;
}
#endif
