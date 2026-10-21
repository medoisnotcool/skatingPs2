/* game.h - skater state machine, skate physics, trick recognition, scoring, camera */
#ifndef GAME_H
#define GAME_H
#include "gfx.h"
#include "level.h"
#include "model.h"
#include "input.h"

#define G_ACC 9.8f

enum { ST_FOOT, ST_CARRY, ST_RIDE, ST_GRIND, ST_BAIL };

/* ---------------------------------------------------------------- tricks */
typedef struct { const char *name; float flip, shove, wrap; int pts; } TrickDef;
enum { T_OLLIE, T_NOLLIE, T_KICKFLIP, T_HEELFLIP, T_BSSHOVE, T_FSSHOVE, T_VARKF, T_VARHF, T_HARDFLIP, T_INWARD,
       T_TREFLIP, T_LASER, T_NKF, T_NHF, T_NBSSHOVE, T_NFSSHOVE, T_IMPOSSIBLE, T_DBLKF, T_DBLHF, T_COUNT };
static const TrickDef TRICKS[T_COUNT] = {
    {"OLLIE", 0, 0, 0, 100},           {"NOLLIE", 0, 0, 0, 150},
    {"KICKFLIP", 1, 0, 0, 300},        {"HEELFLIP", -1, 0, 0, 300},
    {"BS POP SHOVE-IT", 0, -0.5f, 0, 250}, {"FS POP SHOVE-IT", 0, 0.5f, 0, 250},
    {"VARIAL KICKFLIP", 1, -0.5f, 0, 450}, {"VARIAL HEELFLIP", -1, 0.5f, 0, 450},
    {"HARDFLIP", 1, 0.5f, 0, 500},     {"INWARD HEELFLIP", -1, -0.5f, 0, 500},
    {"360 FLIP", 1, -1.0f, 0, 700},    {"LASER FLIP", -1, 1.0f, 0, 700},
    {"NOLLIE KICKFLIP", 1, 0, 0, 400}, {"NOLLIE HEELFLIP", -1, 0, 0, 400},
    {"NOLLIE BS SHOVE-IT", 0, -0.5f, 0, 350}, {"NOLLIE FS SHOVE-IT", 0, 0.5f, 0, 350},
    {"IMPOSSIBLE", 0, 0, 1, 600},      {"DOUBLE KICKFLIP", 2, 0, 0, 800}, {"DOUBLE HEELFLIP", -2, 0, 0, 800}
};
/* right-stick sectors, clockwise from up: 0 U, 1 UR, 2 R, 3 DR, 4 D, 5 DL, 6 L, 7 UL */
static signed char g_trickTab[8][8];
static void tricktab_init(void) {
    memset(g_trickTab, -1, sizeof g_trickTab);
    g_trickTab[4][0] = T_OLLIE;   g_trickTab[4][7] = T_KICKFLIP; g_trickTab[4][1] = T_HEELFLIP;
    g_trickTab[4][2] = T_BSSHOVE; g_trickTab[4][6] = T_FSSHOVE;
    g_trickTab[5][2] = T_VARKF;   g_trickTab[3][6] = T_VARHF;
    g_trickTab[5][0] = T_HARDFLIP; g_trickTab[3][0] = T_INWARD;
    g_trickTab[5][7] = T_DBLKF;   g_trickTab[3][1] = T_DBLHF;
    g_trickTab[6][2] = T_IMPOSSIBLE; g_trickTab[2][6] = T_IMPOSSIBLE;
    g_trickTab[0][4] = T_NOLLIE;  g_trickTab[0][5] = T_NKF;    g_trickTab[0][3] = T_NHF;
    g_trickTab[0][2] = T_NBSSHOVE; g_trickTab[0][6] = T_NFSSHOVE;
}

/* ---------------------------------------------------------------- player */
typedef struct {
    V3 pos, vel;                 /* pos = ground contact point under the skater */
    float yaw, bodyYaw;
    int state, grounded, fromL1;
    float walkPh, crouch, pushPh; int pushing;
    float throwT;                /* >0 while the throw animation runs */
    int jumpKind; float flipPitch;
    /* ride / air */
    float boardRoll, boardShove, boardPitch, trickT, trickDur, flipTarget, shoveTarget, wrapTarget;
    int trickActive; int popNollie;
    float spin, airT; int grab; float grabT;
    float manualT; int manual;
    /* combo */
    int comboPts; int nNames; char names[6][30]; int namePts[6];
    long score; float msgT; char msg[48]; int msgGood;
    /* grind */
    int rail; float railT; int railDir; float grindT; float bal; int slide; char grindName[24]; float noRail;
    /* bail */
    float bailT; float bailDir;
    /* thrown board */
    int tbOn; V3 tbPos, tbVel; float tbYaw, tbPitch, tbRoll, tbSpin; int tbRest;
} Player;
static Player P;
static float g_camYaw, g_clock;
static V3 g_camPos;
static float g_impact;
static int g_landedFlag;

static inline int side_sign(void) { return g_opt.goofy ? -1 : 1; }
static float hspeed(void) { return sqrtf(P.vel.x * P.vel.x + P.vel.z * P.vel.z); }

static void player_reset(void) {
    memset(&P, 0, sizeof P);
    P.pos = v3(0, 0, -16.0f);
    P.grounded = 1; P.state = ST_FOOT; P.rail = -1; P.grab = -1;
    g_camYaw = 0; g_camPos = v3(0, 3, -22);
}

/* ------------------------------------------------------------- combo/score */
static void combo_add(const char *name, int pts) {
    if (P.nNames >= 6) { P.comboPts += pts; return; }
    for (int i = 0; i < P.nNames; i++) if (strcmp(P.names[i], name) == 0) { P.comboPts += pts / 2; return; }
    strncpy(P.names[P.nNames], name, 29); P.names[P.nNames][29] = 0;
    P.namePts[P.nNames] = pts; P.nNames++; P.comboPts += pts;
}
static void combo_clear(void) { P.comboPts = 0; P.nNames = 0; }
static void combo_bank(void) {
    if (P.nNames == 0 && P.comboPts == 0) return;
    int mult = P.nNames > 0 ? P.nNames : 1;
    int total = P.comboPts * mult;
    P.score += total;
    snprintf(P.msg, sizeof P.msg, "LANDED  +%d  (X%d)", total, mult);
    P.msgGood = 1; P.msgT = 2.4f;
    combo_clear();
}
static void say(const char *s, int good) { strncpy(P.msg, s, 47); P.msg[47] = 0; P.msgGood = good; P.msgT = 2.0f; }

/* --------------------------------------------------- right stick tracking */
typedef struct { int sec; float t; } SEnt;
static SEnt g_hist[8]; static int g_nh, g_armed = 1;
static float g_sensPull[3] = { 0.66f, 0.56f, 0.44f }, g_sensFlick[3] = { 0.88f, 0.78f, 0.64f };

static int stick_sector(float x, float y) {
    float ang = atan2f(x, y);                       /* 0 = up, clockwise positive */
    int idx = (int)floorf((ang + PI_F / 8) / (PI_F / 4));
    idx %= 8; if (idx < 0) idx += 8;
    return idx;
}
static void hist_push(int sec) {
    if (g_nh > 0 && g_hist[g_nh - 1].sec == sec) { g_hist[g_nh - 1].t = g_clock; return; }
    if (g_nh == 8) { for (int i = 1; i < 8; i++) g_hist[i - 1] = g_hist[i]; g_nh = 7; }
    g_hist[g_nh].sec = sec; g_hist[g_nh].t = g_clock; g_nh++;
}
static void hist_clear(void) { g_nh = 0; g_armed = 1; }

/* returns trick id or -1; call once per frame */
static int stick_track(void) {
    float rx = g_in.rx, ry = g_opt.invertRY ? -g_in.ry : g_in.ry;
    if (g_opt.goofy) rx = -rx;
    float m = sqrtf(rx * rx + ry * ry);
    float pull = g_sensPull[g_opt.sens], flick = g_sensFlick[g_opt.sens];
    int sec = m > pull ? stick_sector(rx, ry) : -1;
    if (m < 0.35f) g_armed = 1;
    if (sec >= 0) hist_push(sec);
    while (g_nh > 0 && g_clock - g_hist[0].t > 0.7f) { for (int i = 1; i < g_nh; i++) g_hist[i - 1] = g_hist[i]; g_nh--; }
    if (!g_armed || sec < 0) return -1;
    /* circle gesture: >=4 consecutive neighbouring sectors the same way */
    if (g_nh >= 4) {
        int dir = 0, ok = 1;
        for (int i = g_nh - 3; i < g_nh; i++) {
            int d = (g_hist[i].sec - g_hist[i - 1].sec + 8) % 8;
            int step = d == 1 ? 1 : (d == 7 ? -1 : 0);
            if (step == 0 || (dir && step != dir)) { ok = 0; break; }
            dir = step;
        }
        if (ok && dir) { g_armed = 0; g_nh = 0; return dir > 0 ? T_LASER : T_TREFLIP; }
    }
    if (m >= flick) {
        for (int i = g_nh - 1; i >= 0; i--) {
            if (g_hist[i].sec == sec) continue;
            if (g_clock - g_hist[i].t > 0.5f) break;
            int t = g_trickTab[g_hist[i].sec][sec];
            if (t >= 0) { g_armed = 0; g_nh = 0; return t; }
            break;
        }
    }
    return -1;
}

/* ----------------------------------------------------- movement + collision */
static void move_collide(float dt, int rideMode) {
    Player *p = &P;
    g_impact = 0; g_landedFlag = 0;
    float nx = p->pos.x + p->vel.x * dt, nz = p->pos.z + p->vel.z * dt;
    const float mx = ARENA_X - 0.5f, mz = ARENA_Z - 0.5f;
    if (nx > mx) { nx = mx; if (p->vel.x > 0) { g_impact = fmaxf(g_impact, p->vel.x); p->vel.x *= -0.15f; } }
    if (nx < -mx) { nx = -mx; if (p->vel.x < 0) { g_impact = fmaxf(g_impact, -p->vel.x); p->vel.x *= -0.15f; } }
    if (nz > mz) { nz = mz; if (p->vel.z > 0) { g_impact = fmaxf(g_impact, p->vel.z); p->vel.z *= -0.15f; } }
    if (nz < -mz) { nz = -mz; if (p->vel.z < 0) { g_impact = fmaxf(g_impact, -p->vel.z); p->vel.z *= -0.15f; } }

    float ref = p->grounded ? ground_h(p->pos.x, p->pos.z) : p->pos.y;
    const float STEP = 0.5f;
    if (ground_h(nx, nz) > ref + STEP) {
        int bx = ground_h(nx, p->pos.z) > ref + STEP, bz = ground_h(p->pos.x, nz) > ref + STEP;
        if (bx) { g_impact = fmaxf(g_impact, absf_(p->vel.x)); nx = p->pos.x; p->vel.x *= -0.1f; }
        if (bz) { g_impact = fmaxf(g_impact, absf_(p->vel.z)); nz = p->pos.z; p->vel.z *= -0.1f; }
        if (!bx && !bz) { g_impact = fmaxf(g_impact, hspeed()); nx = p->pos.x; nz = p->pos.z; p->vel.x *= -0.1f; p->vel.z *= -0.1f; }
    }
    p->pos.x = nx; p->pos.z = nz;
    float gh = ground_h(nx, nz);
    if (p->grounded) {
        float yff = p->pos.y + p->vel.y * dt - 0.5f * G_ACC * dt * dt;
        int leave = rideMode ? (gh < yff - 0.03f) : (gh < p->pos.y - 0.25f);
        if (leave) {
            p->grounded = 0;
            if (rideMode) { p->pos.y = yff; p->vel.y -= G_ACC * dt; } else p->vel.y = 0;
        } else {
            float nvy = (gh - p->pos.y) / dt;
            p->pos.y = gh;
            p->vel.y = rideMode ? clampf(nvy, -9.0f, 8.0f) : 0;
        }
    } else {
        p->pos.y += p->vel.y * dt - 0.5f * G_ACC * dt * dt;
        p->vel.y -= G_ACC * dt;
        if (p->pos.y <= gh && p->vel.y <= 0) { p->pos.y = gh; g_landedFlag = 1; }
    }
}

/* ------------------------------------------------------------ thrown board */
static void tb_update(float dt) {
    if (!P.tbOn) return;
    if (!P.tbRest) {
        P.tbVel.y -= G_ACC * dt;
        P.tbPos = vadd(P.tbPos, vmul(P.tbVel, dt));
        P.tbRoll += P.tbSpin * dt; P.tbYaw += P.tbSpin * 0.3f * dt;
        float gh = ground_h(P.tbPos.x, P.tbPos.z);
        if (P.tbPos.x > ARENA_X - 0.3f || P.tbPos.x < -ARENA_X + 0.3f) { P.tbVel.x *= -0.4f; P.tbPos.x = clampf(P.tbPos.x, -ARENA_X + 0.3f, ARENA_X - 0.3f); }
        if (P.tbPos.z > ARENA_Z - 0.3f || P.tbPos.z < -ARENA_Z + 0.3f) { P.tbVel.z *= -0.4f; P.tbPos.z = clampf(P.tbPos.z, -ARENA_Z + 0.3f, ARENA_Z - 0.3f); }
        if (P.tbPos.y < gh + 0.05f) {
            P.tbPos.y = gh + 0.05f;
            if (P.tbVel.y < -1.5f) { P.tbVel.y *= -0.35f; P.tbVel.x *= 0.7f; P.tbVel.z *= 0.7f; P.tbSpin *= 0.5f; }
            else { P.tbVel.y = 0; P.tbVel.x *= 0.9f; P.tbVel.z *= 0.9f; P.tbSpin *= 0.8f;
                   if (fabsf(P.tbVel.x) + fabsf(P.tbVel.z) < 0.3f) P.tbRest = 1; }
        }
    } else {
        P.tbRoll *= 0.85f; if (fabsf(P.tbRoll) < 0.02f) P.tbRoll = 0;
        P.tbPos.y = ground_h(P.tbPos.x, P.tbPos.z) + 0.07f;
    }
}
static void tb_spawn(V3 pos, V3 vel, float yaw, float spin) {
    P.tbOn = 1; P.tbPos = pos; P.tbVel = vel; P.tbYaw = yaw; P.tbPitch = 0; P.tbRoll = 0; P.tbSpin = spin; P.tbRest = 0;
}

/* ------------------------------------------------------------------- bail */
static void bail_start(const char *why) {
    P.state = ST_BAIL; P.bailT = 0; P.bailDir = (P.vel.x * sinf(P.yaw) + P.vel.z * cosf(P.yaw)) >= 0 ? 1.0f : -1.0f;
    P.grounded = P.grounded; P.trickActive = 0; P.grab = -1; P.manual = 0;
    V3 bo = v3(P.pos.x, P.pos.y + 0.3f, P.pos.z);
    tb_spawn(bo, v3(P.vel.x * 0.7f + 1.0f, 3.0f, P.vel.z * 0.7f), P.yaw, 9.0f);
    P.bodyYaw = P.yaw;
    combo_clear();
    say(why, 0);
}

/* -------------------------------------------------------------------- pops */
static void do_pop(int id) {
    const TrickDef *t = &TRICKS[id];
    float hs = hspeed();
    P.vel.y = 4.7f + fminf(hs * 0.10f, 1.3f);
    P.grounded = 0; P.pos.y += 0.02f;
    P.trickActive = 1; P.trickT = 0;
    P.trickDur = 0.42f + 0.12f * (fabsf(t->flip) + fabsf(t->shove) * 1.2f + t->wrap);
    float mir = g_opt.goofy ? -1.0f : 1.0f;
    P.flipTarget = t->flip * TAU_F * mir; P.shoveTarget = t->shove * TAU_F * mir; P.wrapTarget = t->wrap * TAU_F;
    P.popNollie = (id == T_NOLLIE || id == T_NKF || id == T_NHF || id == T_NBSSHOVE || id == T_NFSSHOVE);
    P.boardRoll = P.boardShove = P.boardPitch = 0;
    P.spin = 0; P.airT = 0; P.grab = -1; P.grabT = 0; P.manual = 0; P.manualT = 0;
    combo_add(t->name, t->pts);
}

static void grind_start(int ri, float t) {
    const Rail *r = &g_rail[ri];
    P.state = ST_GRIND; P.rail = ri; P.railT = t; P.grindT = 0; P.bal = 0; P.grounded = 0;
    V3 ab = vsub(r->b, r->a);
    float sp = P.vel.x * ab.x + P.vel.z * ab.z;
    float hl = sqrtf(ab.x * ab.x + ab.z * ab.z);
    float vproj = sp / hl;
    P.railDir = vproj >= 0 ? 1 : -1;
    float speed = fmaxf(absf_(vproj), 3.5f);
    P.vel = v3(ab.x / hl * P.railDir * speed, 0, ab.z / hl * P.railDir * speed);
    P.manual = 0;
    float railYaw = atan2f(ab.x * P.railDir, ab.z * P.railDir);
    float d = wrapPi(P.yaw - railYaw);
    if (d > PI_F / 2) d -= PI_F; else if (d < -PI_F / 2) d += PI_F;
    P.slide = absf_(d) > 0.65f;
    P.trickActive = 0; P.boardRoll = P.boardShove = 0;
    strcpy(P.grindName, P.slide ? "BOARDSLIDE" : "50-50 GRIND");
    combo_add(P.grindName, P.slide ? 350 : 250);
}

/* landing for riding/air */
static void ride_land(void) {
    int ok = 1;
    if (P.trickActive) {
        float prog = P.trickT / P.trickDur;
        if (prog < 0.78f) ok = 0;
    }
    float sp = fmodf(absf_(P.spin), PI_F);
    float dist = fminf(sp, PI_F - sp);
    if (dist > 0.95f) ok = 0;
    P.grounded = 1;
    P.vel.y = 0;
    if (!ok) { bail_start(P.trickActive && P.trickT / P.trickDur < 0.78f ? "BAIL - SKETCHY LANDING" : "BAIL - BAD ANGLE"); return; }
    int n = (int)(absf_(P.spin) / PI_F + 0.5f);
    if (n >= 1) {
        char nm[30];
        snprintf(nm, sizeof nm, "%d SPIN", n * 180);
        combo_add(nm, 150 * n);
    }
    if (P.grab >= 0 && P.grabT > 0.3f) { /* grab was already added when held long enough */ }
    P.trickActive = 0; P.boardRoll = P.boardShove = P.boardPitch = 0; P.spin = 0; P.grab = -1; P.grabT = 0;
    P.crouch = 0.8f;
    combo_bank();
}

static const char *GRAB_NAME[6] = { "INDY GRAB", "NOSEGRAB", "TAILGRAB", "MELON GRAB", "MUTE GRAB", "STALEFISH" };

/* ------------------------------------------------------------ per-state updates */
static float cam_yaw_for_input(void) { return g_camYaw; }

static void update_foot(float dt) {
    Player *p = &P;
    float lx = g_in.lx, ly = g_in.ly;
    float m = fminf(1.0f, sqrtf(lx * lx + ly * ly));
    float cy = cam_yaw_for_input();
    V3 cf = v3(sinf(cy), 0, cosf(cy)), cr = v3(cosf(cy), 0, -sinf(cy));
    V3 wd = vadd(vmul(cr, lx), vmul(cf, ly));
    int run = g_opt.autorun ? !ACT_DOWN(A_RUN) : ACT_DOWN(A_RUN);
    float top = run ? 5.8f : 2.3f;
    V3 want = vmul(vnorm(wd), top * fminf(1.0f, m * 1.15f));
    if (m < 0.01f) want = v3(0, 0, 0);
    float acc = p->grounded ? (m > 0.01f ? 22.0f : 26.0f) : 4.0f;
    p->vel.x = approachf(p->vel.x, want.x, acc * dt);
    p->vel.z = approachf(p->vel.z, want.z, acc * dt);
    if (m > 0.05f && p->grounded) {
        float tgt = atan2f(wd.x, wd.z);
        p->bodyYaw += clampf(wrapPi(tgt - p->bodyYaw), -13.0f * dt, 13.0f * dt);
    }
    p->yaw = p->bodyYaw;
    float hs = hspeed();
    p->walkPh += hs * dt * (run ? 2.2f : 2.8f);
    p->crouch = approachf(p->crouch, 0, 4 * dt);

    /* carry-jumps */
    if (p->state == ST_CARRY && p->grounded) {
        if (ACT_PRESS(A_JUMP))       { p->vel.y = 5.6f; p->grounded = 0; p->jumpKind = 1; p->pos.y += 0.02f; }
        else if (ACT_PRESS(A_TRICKJUMP)) { p->vel.y = 6.8f; p->grounded = 0; p->jumpKind = 2; p->flipPitch = 0; p->pos.y += 0.02f; }
    }
    if (!p->grounded && p->jumpKind == 2) p->flipPitch += (TAU_F / 0.80f) * dt;

    move_collide(dt, 0);
    if (g_landedFlag) { p->grounded = 1; p->vel.y = 0; p->jumpKind = 0; p->flipPitch = 0; }

    /* throw animation */
    if (p->throwT > 0) {
        float before = p->throwT;
        p->throwT += dt;
        if (before < 0.12f && p->throwT >= 0.12f) {
            V3 f = v3(sinf(p->bodyYaw), 0, cosf(p->bodyYaw));
            V3 start = vadd(p->pos, vadd(vmul(f, 0.6f), v3(0, 1.45f, 0)));
            tb_spawn(start, vadd(vadd(vmul(f, 9.0f), v3(0, 3.6f, 0)), vmul(p->vel, 0.6f)), p->bodyYaw, 14.0f);
        }
        if (p->throwT > 0.34f) p->throwT = 0;
    }
}

static void begin_ride_from_foot(void) {
    P.state = ST_RIDE; P.fromL1 = 0; P.yaw = P.bodyYaw; P.grounded = 1; P.jumpKind = 0; P.flipPitch = 0;
    P.tbOn = 0; P.throwT = 0; P.vel.x *= 0.85f; P.vel.z *= 0.85f; P.vel.y = 0;
    P.boardRoll = P.boardShove = P.boardPitch = 0; P.spin = 0; P.trickActive = 0; P.crouch = 0.4f; hist_clear();
}

static void update_ride(float dt) {
    Player *p = &P;
    float hs = hspeed();
    V3 fwd = v3(sinf(p->yaw), 0, cosf(p->yaw)), rgt = v3(cosf(p->yaw), 0, -sinf(p->yaw));
    int pop = stick_track();
    p->pushing = 0;
    if (p->noRail > 0) p->noRail -= dt;

    if (p->grounded) {
        /* manual */
        p->manual = ACT_DOWN(A_MANUAL) && hs > 1.5f && p->manualT < 6.0f;
        if (p->manual) {
            float before = p->manualT; p->manualT += dt;
            if (before < 0.5f && p->manualT >= 0.5f) combo_add("MANUAL", 100);
            if (p->manualT > 0.5f) p->comboPts += 2;                     /* ~120 points per second */
        }
        else if (!ACT_DOWN(A_MANUAL)) { if (p->manualT > 0.8f) combo_bank(); p->manualT = 0; }

        float rate = lerpf(3.1f, 1.5f, clampf(hs / 12.0f, 0, 1));
        p->yaw += g_in.lx * rate * dt * (hs > 0.3f ? 1.0f : 0.7f);
        fwd = v3(sinf(p->yaw), 0, cosf(p->yaw)); rgt = v3(cosf(p->yaw), 0, -sinf(p->yaw));
        float vf = p->vel.x * fwd.x + p->vel.z * fwd.z;
        float vs = p->vel.x * rgt.x + p->vel.z * rgt.z;
        /* push */
        if (ACT_DOWN(A_PUSH) && hs < 9.5f && !p->manual) {
            float dir = vf >= -0.6f ? 1.0f : -1.0f;
            vf += dir * 5.2f * dt; p->pushing = 1; p->pushPh += 7.0f * dt;
        }
        if (ACT_DOWN(A_BRAKE) && !ACT_DOWN(A_PUSH)) vf = approachf(vf, 0, 8.0f * dt);
        vs *= expf(-11.0f * dt);                           /* wheels resist sideways motion */
        vf *= expf(-0.12f * dt);                           /* rolling drag */
        p->vel.x = fwd.x * vf + rgt.x * vs;
        p->vel.z = fwd.z * vf + rgt.z * vs;
        /* gravity along the slope */
        float gx, gz; ground_grad(p->pos.x, p->pos.z, &gx, &gz);
        float k = G_ACC / (1.0f + gx * gx + gz * gz);
        p->vel.x -= gx * k * dt; p->vel.z -= gz * k * dt;
        hs = hspeed();
        if (hs > 17.0f) { p->vel.x *= 17.0f / hs; p->vel.z *= 17.0f / hs; }
        /* crouch before a pop (stick pulled back) */
        float want = (g_in.ry < -0.45f) ? 0.75f : (p->manual ? 0.2f : 0.15f);
        p->crouch = approachf(p->crouch, want, 5 * dt);
        if (pop >= 0) do_pop(pop);
        /* hop / ollie also with a clean flick up while in manual is fine */
    } else {
        /* ---- airborne ---- */
        p->airT += dt;
        float spinRate = g_in.lx * 6.0f * dt;
        p->yaw += spinRate; p->spin += spinRate;
        p->crouch = approachf(p->crouch, 0.65f, 4 * dt);
        if (p->trickActive) p->trickT += dt;
        /* grabs */
        int g = -1;
        float ry = g_opt.invertRY ? -g_in.ry : g_in.ry;
        if (ACT_DOWN(A_GRABL)) g = ry > 0.5f ? 1 : (ry < -0.5f ? 2 : 0);
        else if (ACT_DOWN(A_GRABR)) g = ry > 0.5f ? 4 : (ry < -0.5f ? 5 : 3);
        if (g != p->grab) { p->grab = g; p->grabT = 0; }
        if (p->grab >= 0) { p->grabT += dt; if (p->grabT > 0.28f && p->grabT - dt <= 0.28f) combo_add(GRAB_NAME[p->grab], 180); }
        /* rail lock */
        if (p->noRail <= 0 && p->vel.y < 2.5f) {
            float t;
            int ri = rail_find(v3(p->pos.x, p->pos.y + 0.1f, p->pos.z), &t);
            if (ri >= 0) { grind_start(ri, t); return; }
        }
    }

    move_collide(dt, 1);
    if (g_impact > 8.0f && p->state == ST_RIDE) { bail_start("BAIL - SLAMMED INTO IT"); return; }
    if (g_impact > 3.0f) { p->vel.x *= 0.6f; p->vel.z *= 0.6f; }
    if (g_landedFlag) ride_land();
}

static void update_grind(float dt) {
    Player *p = &P;
    const Rail *r = &g_rail[p->rail];
    int pop = stick_track();
    p->grindT += dt;
    V3 ab = vsub(r->b, r->a);
    float len = vlen(ab);
    float speed = hspeed();
    speed = fmaxf(speed * expf(-0.25f * dt), 2.6f);
    p->railT += p->railDir * speed * dt / len * (sqrtf(ab.x * ab.x + ab.z * ab.z) / len + 0.0001f) * 1.0f;
    V3 q = rail_point(r, clampf(p->railT, 0, 1));
    p->pos = v3(q.x, q.y - 0.03f, q.z);
    float hl = sqrtf(ab.x * ab.x + ab.z * ab.z);
    p->vel = v3(ab.x / hl * p->railDir * speed, 0, ab.z / hl * p->railDir * speed);
    float railYaw = atan2f(ab.x * p->railDir, ab.z * p->railDir);
    /* variant from held right stick */
    float ry = g_opt.invertRY ? -g_in.ry : g_in.ry;
    int var = 0;
    if (sqrtf(g_in.rx * g_in.rx + ry * ry) > 0.55f) { int s = stick_sector(g_opt.goofy ? -g_in.rx : g_in.rx, ry); var = (s == 0 || s == 1 || s == 7) ? 1 : ((s == 4 || s == 3 || s == 5) ? 2 : (s == 6 ? 3 : 4)); }
    const char *nm;
    if (!p->slide) nm = var == 0 ? "50-50 GRIND" : var == 1 ? "NOSEGRIND" : var == 2 ? "5-0 GRIND" : var == 3 ? "CROOKED GRIND" : "SMITH GRIND";
    else           nm = var == 1 ? "NOSESLIDE" : var == 2 ? "TAILSLIDE" : (var == 3 || var == 0) ? "BOARDSLIDE" : "LIPSLIDE";
    if (strcmp(nm, p->grindName) != 0) { strcpy(p->grindName, nm); combo_add(nm, p->slide ? 350 : 250); }
    float tgtYaw = p->slide ? railYaw + PI_F * 0.5f * side_sign() : railYaw;
    p->yaw += clampf(wrapPi(tgtYaw - p->yaw), -9.0f * dt, 9.0f * dt);
    p->comboPts += 1;                                     /* ~60 points per second of grind */
    /* balance */
    p->bal += (0.30f * sinf(p->grindT * 2.3f + 1.0f) + 0.35f * p->bal) * dt - g_in.lx * 1.5f * dt;
    p->crouch = approachf(p->crouch, 0.55f, 5 * dt);
    if (pop >= 0) {                                       /* pop out of the grind */
        p->state = ST_RIDE; p->noRail = 0.35f;
        p->vel.y = 0; p->grounded = 0; p->rail = -1;
        do_pop(pop);
        return;
    }
    if (p->railT <= 0.0f || p->railT >= 1.0f || fabsf(p->bal) > 1.0f) {
        int slipped = fabsf(p->bal) > 1.0f;
        p->state = ST_RIDE; p->noRail = 0.35f; p->grounded = 0; p->rail = -1;
        p->vel.y = slipped ? 1.6f : (r->b.y - r->a.y) / len * p->railDir * speed * 0.5f + 0.4f;
        p->trickActive = 0; p->spin = 0; p->airT = 0;
        if (slipped) say("SLIPPED OFF THE RAIL", 0);
    }
}

static void update_bail(float dt) {
    Player *p = &P;
    p->bailT += dt;
    p->vel.x *= expf(-2.4f * dt); p->vel.z *= expf(-2.4f * dt);
    move_collide(dt, 0);
    if (g_landedFlag) { p->grounded = 1; p->vel.y = 0; }
    if (p->bailT > 2.0f) {
        p->state = ST_FOOT; p->vel = v3(0, 0, 0); p->grounded = 1; p->pos.y = ground_h(p->pos.x, p->pos.z);
        p->bodyYaw = p->yaw; p->throwT = 0; p->jumpKind = 0; p->flipPitch = 0;
    }
}

/* ------------------------------------------------------------------ master update */
static void player_update(float dt) {
    Player *p = &P;
    g_clock += dt;
    if (p->msgT > 0) p->msgT -= dt;
    tb_update(dt);

    /* ----- discrete actions ----- */
    if (p->state == ST_FOOT || p->state == ST_CARRY) {
        if (ACT_PRESS(A_HOLD) && p->grounded && p->throwT == 0) {
            if (p->state == ST_FOOT && !p->tbOn) { p->state = ST_CARRY; p->fromL1 = 0; }
            else if (p->state == ST_FOOT && p->tbOn) { /* pick the thrown board back up */ p->tbOn = 0; p->state = ST_CARRY; p->fromL1 = 0; }
            else if (p->state == ST_CARRY) { p->state = ST_FOOT; p->fromL1 = 0; p->throwT = 0.0001f; }
        }
        if (ACT_PRESS(A_EQUIP) && p->grounded && p->throwT == 0) begin_ride_from_foot();
        if (p->state == ST_CARRY && p->fromL1 && !ACT_DOWN(A_CARRY) && p->grounded) begin_ride_from_foot();
    } else if (p->state == ST_RIDE) {
        if (p->grounded && ACT_PRESS(A_EQUIP)) { p->state = ST_FOOT; p->bodyYaw = hspeed() > 1.0f ? atan2f(p->vel.x, p->vel.z) : p->yaw; p->vel.x *= 0.6f; p->vel.z *= 0.6f; p->manual = 0; combo_bank(); }
        else if (p->grounded && ACT_PRESS(A_CARRY)) { p->state = ST_CARRY; p->fromL1 = 1; p->bodyYaw = hspeed() > 1.0f ? atan2f(p->vel.x, p->vel.z) : p->yaw; p->manual = 0; combo_bank(); }
    }

    switch (p->state) {
        case ST_FOOT: case ST_CARRY: update_foot(dt); break;
        case ST_RIDE:  update_ride(dt); break;
        case ST_GRIND: update_grind(dt); break;
        case ST_BAIL:  update_bail(dt); break;
    }

    /* animated board angles during tricks */
    if (p->state == ST_RIDE) {
        if (p->trickActive) {
            float e = smooth01(p->trickT / p->trickDur);
            p->boardRoll = p->flipTarget * e;
            p->boardShove = p->shoveTarget * e;
            float pit = -0.38f * sinf(clampf(p->trickT / 0.5f, 0, 1) * PI_F);
            p->boardPitch = (p->popNollie ? -pit : pit) - p->wrapTarget * e;
        } else if (p->manual) {
            p->boardPitch = approachf(p->boardPitch, -0.32f, 3 * dt);
        } else {
            p->boardPitch = approachf(p->boardPitch, 0, 6 * dt);
            p->boardRoll = 0; p->boardShove = 0;
        }
    } else if (p->state == ST_GRIND) {
        float tgt = 0;
        if (!strcmp(p->grindName, "NOSEGRIND") || !strcmp(p->grindName, "NOSESLIDE")) tgt = 0.30f;
        if (!strcmp(p->grindName, "5-0 GRIND") || !strcmp(p->grindName, "TAILSLIDE")) tgt = -0.30f;
        p->boardPitch = approachf(p->boardPitch, tgt, 2.5f * dt);
        p->boardRoll = 0; p->boardShove = 0;
    }
}

/* ---------------------------------------------------------------- camera */
static void camera_update(float dt, int snap) {
    Player *p = &P;
    float hs = hspeed();
    if (g_opt.camFollow) {
        int moving = (p->state == ST_RIDE || p->state == ST_GRIND) ? hs > 2.0f : hs > 0.8f;
        if (moving) {
            float tgt = atan2f(p->vel.x, p->vel.z);
            float rate = (p->state == ST_RIDE || p->state == ST_GRIND) ? 2.4f : 1.5f;
            g_camYaw += wrapPi(tgt - g_camYaw) * fminf(1.0f, rate * dt);
        }
    }
    if (g_in.analog) {
        if (g_in.down & PAD_LEFT)  g_camYaw -= 1.9f * dt;
        if (g_in.down & PAD_RIGHT) g_camYaw += 1.9f * dt;
    }
    static const float DIST[3] = { 4.0f, 5.2f, 6.8f };
    float dist = DIST[g_opt.camDist];
    V3 target = v3(p->pos.x, p->pos.y + 1.15f, p->pos.z);
    V3 back = v3(-sinf(g_camYaw), 0, -cosf(g_camYaw));
    V3 want = vadd(target, vadd(vmul(back, dist), v3(0, 0.9f + dist * 0.18f, 0)));
    want.x = clampf(want.x, -ARENA_X + 0.8f, ARENA_X - 0.8f);
    want.z = clampf(want.z, -ARENA_Z + 0.8f, ARENA_Z - 0.8f);
    float gh = ground_h(want.x, want.z);
    if (want.y < gh + 0.6f) want.y = gh + 0.6f;
    want.y = clampf(want.y, 0.6f, ARENA_H - 1.0f);
    float k = snap ? 1.0f : 1.0f - expf(-9.0f * dt);
    g_camPos = vlerp(g_camPos, want, k);
    gfx_camera(g_camPos, target, 62.0f);
}
#endif
