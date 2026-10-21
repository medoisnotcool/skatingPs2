/* ui.h - start menu, pause menu, options, key rebinding, help */
#ifndef UI_H
#define UI_H
#include "draw.h"

enum { MS_PLAY, MS_MAIN, MS_PAUSE, MS_OPT, MS_CTRL, MS_HELP };
static int g_ms = MS_MAIN, g_sel, g_optFrom = MS_MAIN, g_waitBind = -1, g_quit, g_wantRespawn;

static float g_prevLx, g_prevLy;
typedef struct { int up, down, left, right, ok, back; } Nav;

static Nav nav_read(void) {
    Nav n;
    n.up    = (g_in.pressed & PAD_UP)    || (g_in.ly > 0.6f && g_prevLy <= 0.6f);
    n.down  = (g_in.pressed & PAD_DOWN)  || (g_in.ly < -0.6f && g_prevLy >= -0.6f);
    n.left  = (g_in.pressed & PAD_LEFT)  || (g_in.lx < -0.6f && g_prevLx >= -0.6f);
    n.right = (g_in.pressed & PAD_RIGHT) || (g_in.lx > 0.6f && g_prevLx <= 0.6f);
    n.ok    = (g_in.pressed & PAD_CROSS) != 0;
    n.back  = (g_in.pressed & PAD_CIRCLE) != 0;
    g_prevLx = g_in.lx; g_prevLy = g_in.ly;
    return n;
}

#define OPT_ROWS 9
static const char *opt_label(int i, char *buf, int n) {
    static const char *dist[3] = { "NEAR", "MEDIUM", "FAR" }, *sens[3] = { "LOW", "MEDIUM", "HIGH" };
    switch (i) {
        case 0: snprintf(buf, n, "AUTO RUN: %s", g_opt.autorun ? "ON" : "OFF"); break;
        case 1: snprintf(buf, n, "STANCE: %s", g_opt.goofy ? "GOOFY" : "REGULAR"); break;
        case 2: snprintf(buf, n, "CAMERA DISTANCE: %s", dist[g_opt.camDist]); break;
        case 3: snprintf(buf, n, "CAMERA FOLLOW: %s", g_opt.camFollow ? "ON" : "OFF"); break;
        case 4: snprintf(buf, n, "FLICK SENSITIVITY: %s", sens[g_opt.sens]); break;
        case 5: snprintf(buf, n, "INVERT RIGHT STICK: %s", g_opt.invertRY ? "ON" : "OFF"); break;
        case 6: snprintf(buf, n, "SHOW HUD: %s", g_opt.showHud ? "ON" : "OFF"); break;
        case 7: snprintf(buf, n, "CONTROLS..."); break;
        default: snprintf(buf, n, "BACK"); break;
    }
    return buf;
}
static void opt_change(int i, int d) {
    switch (i) {
        case 0: g_opt.autorun ^= 1; break;
        case 1: g_opt.goofy ^= 1; break;
        case 2: g_opt.camDist = (g_opt.camDist + d + 3) % 3; break;
        case 3: g_opt.camFollow ^= 1; break;
        case 4: g_opt.sens = (g_opt.sens + d + 3) % 3; break;
        case 5: g_opt.invertRY ^= 1; break;
        case 6: g_opt.showHud ^= 1; break;
    }
}

static void menu_update(void) {
    Nav n = nav_read();
    int rows = 1;
    if (g_waitBind >= 0) {                                /* waiting for a button press to bind */
        if (g_in.pressed & PAD_SELECT) { g_waitBind = -1; return; }
        for (int i = 0; i < 10; i++)
            if (g_in.pressed & BTN_LIST[i]) { g_bind[g_waitBind] = BTN_LIST[i]; g_waitBind = -1; return; }
        return;
    }
    switch (g_ms) {
        case MS_MAIN: case MS_PAUSE: rows = 4; break;
        case MS_OPT: rows = OPT_ROWS; break;
        case MS_CTRL: rows = A_COUNT + 2; break;
        case MS_HELP: rows = 1; break;
    }
    if (n.up)   g_sel = (g_sel + rows - 1) % rows;
    if (n.down) g_sel = (g_sel + 1) % rows;

    if (g_ms == MS_MAIN) {
        if (n.ok) {
            if (g_sel == 0) { g_ms = MS_PLAY; }
            else if (g_sel == 1) { g_optFrom = MS_MAIN; g_ms = MS_OPT; g_sel = 0; }
            else if (g_sel == 2) { g_ms = MS_HELP; g_sel = 0; }
            else g_quit = 1;
        }
    } else if (g_ms == MS_PAUSE) {
        if ((g_in.pressed & PAD_START) || n.back) { g_ms = MS_PLAY; return; }
        if (n.ok) {
            if (g_sel == 0) g_ms = MS_PLAY;
            else if (g_sel == 1) { g_optFrom = MS_PAUSE; g_ms = MS_OPT; g_sel = 0; }
            else if (g_sel == 2) { g_wantRespawn = 1; g_ms = MS_PLAY; }
            else { g_ms = MS_MAIN; g_sel = 0; }
        }
    } else if (g_ms == MS_OPT) {
        if (n.back) { g_ms = g_optFrom; g_sel = 0; return; }
        if (g_sel < 7) {
            if (n.left) opt_change(g_sel, -1);
            if (n.right || n.ok) opt_change(g_sel, 1);
        } else if (n.ok) {
            if (g_sel == 7) { g_ms = MS_CTRL; g_sel = 0; }
            else { g_ms = g_optFrom; g_sel = 0; }
        }
    } else if (g_ms == MS_CTRL) {
        if (n.back) { g_ms = MS_OPT; g_sel = 7; return; }
        if (n.ok) {
            if (g_sel < A_COUNT) g_waitBind = g_sel;
            else if (g_sel == A_COUNT) { for (int i = 0; i < A_COUNT; i++) g_bind[i] = BIND_DEFAULT[i]; }
            else { g_ms = MS_OPT; g_sel = 7; }
        }
    } else if (g_ms == MS_HELP) {
        if (n.back || n.ok) { g_ms = MS_MAIN; g_sel = 2; }
    }
}

static void panel(float x0, float y0, float x1, float y1) {
    gfx_rect(x0 - 3, y0 - 3, x1 + 3, y1 + 3, 0xe9b52c);
    gfx_rect(x0, y0, x1, y1, 0x14171c);
}

static void menu_list(const char *title, const char **items, int n, float top) {
    float cx = g_sw * 0.5f;
    gfx_text_c(cx, top - 54, 3.2f, 0xe9b52c, title);
    for (int i = 0; i < n; i++) {
        u32 c = i == g_sel ? 0xffffff : 0x9aa0a8;
        if (i == g_sel) gfx_text(cx - 150, top + i * 34, 2.6f, 0xe9b52c, ">");
        gfx_text(cx - 120, top + i * 34, 2.6f, c, items[i]);
    }
}

static void menu_draw(void) {
    char buf[64];
    float cx = g_sw * 0.5f;
    if (g_ms == MS_MAIN) {
        panel(cx - 220, 110, cx + 220, 390);
        gfx_text_c(cx, 56, 5.0f, 0xffffff, "SKATE PARK");
        gfx_text_c(cx, 92, 1.6f, 0xe9b52c, "A FAN-MADE SKATE 3 STYLE SESSION");
        const char *it[4] = { "PLAY", "OPTIONS", "HOW TO PLAY", "QUIT" };
        menu_list("", it, 4, 160);
        gfx_text_c(cx, 360, 1.4f, 0x9aa0a8, "D-PAD / LEFT STICK + X");
    } else if (g_ms == MS_PAUSE) {
        panel(cx - 200, 120, cx + 200, 340);
        const char *it[4] = { "RESUME", "OPTIONS", "RESPAWN", "MAIN MENU" };
        menu_list("PAUSED", it, 4, 170);
    } else if (g_ms == MS_OPT) {
        panel(cx - 280, 60, cx + 280, 420);
        gfx_text_c(cx, 72, 3.2f, 0xe9b52c, "OPTIONS");
        for (int i = 0; i < OPT_ROWS; i++) {
            u32 c = i == g_sel ? 0xffffff : 0x9aa0a8;
            if (i == g_sel) gfx_text(cx - 260, 112 + i * 32, 2.4f, 0xe9b52c, ">");
            gfx_text(cx - 235, 112 + i * 32, 2.4f, c, opt_label(i, buf, sizeof buf));
        }
        gfx_text_c(cx, 400, 1.3f, 0x9aa0a8, "LEFT/RIGHT CHANGE   CIRCLE BACK");
    } else if (g_ms == MS_CTRL) {
        panel(cx - 290, 20, cx + 290, 430);
        gfx_text_c(cx, 30, 2.8f, 0xe9b52c, "CONTROLS");
        for (int i = 0; i < A_COUNT + 2; i++) {
            u32 c = i == g_sel ? 0xffffff : 0x9aa0a8;
            float y = 70 + i * 26;
            if (i == g_sel) gfx_text(cx - 275, y, 2.0f, 0xe9b52c, ">");
            if (i < A_COUNT) {
                gfx_text(cx - 255, y, 2.0f, c, ACT_NAME[i]);
                const char *bn = (g_waitBind == i) ? "PRESS A BUTTON" : btn_name(g_bind[i]);
                gfx_text(cx + 280 - gfx_text_w(bn, 2.0f) - 10, y, 2.0f, g_waitBind == i ? 0xffd23f : c, bn);
            } else gfx_text(cx - 255, y, 2.0f, c, i == A_COUNT ? "RESET TO DEFAULTS" : "BACK");
        }
        gfx_text_c(cx, 410, 1.2f, 0x9aa0a8, g_waitBind >= 0 ? "SELECT TO CANCEL" : "X = REBIND   RIGHT STICK FLICKS ARE FIXED");
    } else if (g_ms == MS_HELP) {
        panel(cx - 300, 24, cx + 300, 424);
        gfx_text_c(cx, 34, 2.8f, 0xe9b52c, "HOW TO PLAY");
        const char *L[] = {
            "LEFT STICK: MOVE / STEER     D-PAD L/R: CAMERA",
            "ON FOOT: HOLD RUN BUTTON TO RUN",
            "HOLD/THROW BOARD: PICK UP THE BOARD, AGAIN = THROW",
            "EQUIP: PUT THE BOARD DOWN AND RIDE (AGAIN = STEP OFF)",
            "RIDING: PUSH TO SPEED UP, BRAKE TO SLOW",
            "RIGHT STICK FLICKS (PULL BACK THEN FLICK):",
            " DOWN>UP OLLIE   DOWN>UP-LEFT KICKFLIP",
            " DOWN>UP-RIGHT HEELFLIP   DOWN>RIGHT/LEFT SHOVE-IT",
            " UP>DOWN NOLLIE   HALF CIRCLE = 360 / LASER FLIP",
            "IN AIR: LEFT STICK SPINS, GRAB BUTTONS + STICK GRAB",
            "LAND ON A RAIL TO GRIND, HOLD RIGHT STICK TO CHANGE",
            "CARRY BOARD (HOLD) WHILE RIDING: LET GO TO RIDE AGAIN",
            " CARRY + JUMP = JUMP, CARRY + TRICK JUMP = FLIP JUMP",
            "MANUAL: HOLD THE MANUAL BUTTON WHILE ROLLING"
        };
        for (int i = 0; i < 14; i++) gfx_text(cx - 285, 76 + i * 24, 1.5f, 0xd8dce2, L[i]);
        gfx_text_c(cx, 410, 1.3f, 0x9aa0a8, "X OR CIRCLE TO GO BACK");
    }
}
#endif
