/* draw.h - poses for every skater state, scene assembly, HUD */
#ifndef DRAW_H
#define DRAW_H
#include "game.h"

static V3 loc(float yaw, V3 root, float x, float y, float z) {
    return v3(root.x + cosf(yaw) * x + sinf(yaw) * z, root.y + y, root.z - sinf(yaw) * x + cosf(yaw) * z);
}
static V3 dloc(float yaw, float x, float y, float z) {
    return v3(cosf(yaw) * x + sinf(yaw) * z, y, -sinf(yaw) * x + cosf(yaw) * z);
}

static void draw_held_board(V3 hand, float yaw) {
    V3 up = v3(0, 1, 0), bf = dloc(yaw, 0, 0, 1), br = dloc(yaw, 1, 0, 0);
    Xf bx; bx.o = vadd(hand, vadd(vmul(up, -0.10f), vmul(br, 0.06f)));
    bx.r = bf; bx.u = br; bx.f = up;
    draw_board(&bx, COL_DECK);
}

static void player_draw(void) {
    Player *p = &P;
    Pose ps; memset(&ps, 0, sizeof ps);
    int side = side_sign();
    float hs = hspeed();
    int heldBoard = 0; V3 heldHand = v3(0, 0, 0); float heldYaw = 0;
    int ridingBoard = 0; Xf bx;

    if (p->state == ST_FOOT || p->state == ST_CARRY) {
        float yaw = p->bodyYaw;
        float run = clampf((hs - 2.0f) / 3.5f, 0, 1), spd = clampf(hs / 5.8f, 0, 1);
        float stride = 0.08f + 0.30f * spd, lift = 0.03f + 0.17f * spd;
        float ph = p->walkPh;
        V3 root = p->pos;
        float bob = 0.025f * spd * fabsf(sinf(ph));
        float airTuck = (!p->grounded && p->state == ST_CARRY) ? 1.0f : 0.0f;
        ps.yaw = yaw; ps.pelvis = v3(root.x, root.y + 0.95f - bob - 0.10f * airTuck, root.z);
        float cl = cosf(ph), cr = cosf(ph + PI_F);
        ps.footL = loc(yaw, root, -0.11f, 0.09f + lift * fmaxf(0, cl) + 0.28f * airTuck, stride * sinf(ph) + 0.15f * airTuck);
        ps.footR = loc(yaw, root,  0.11f, 0.09f + lift * fmaxf(0, cr) + 0.20f * airTuck, stride * sinf(ph + PI_F) - 0.05f * airTuck);
        ps.footYawL = ps.footYawR = yaw;
        float arm = 0.06f + 0.30f * spd;
        ps.handL = loc(yaw, root, -0.27f, 0.98f + 0.12f * run, -arm * sinf(ph));
        ps.handR = loc(yaw, root,  0.27f, 0.98f + 0.12f * run, -arm * sinf(ph + PI_F));
        ps.lean = 0.04f + 0.26f * run;
        ps.kneePole = dloc(yaw, 0, 0.1f, 1);
        ps.elbowPoleL = dloc(yaw, -0.3f, -0.3f, -1); ps.elbowPoleR = dloc(yaw, 0.3f, -0.3f, -1);
        if (p->state == ST_CARRY || (p->throwT > 0 && p->throwT < 0.12f)) {
            ps.handR = loc(yaw, root, 0.30f, 1.00f, 0.12f);
            heldBoard = 1;
            if (p->state == ST_CARRY && !p->grounded) {              /* jump: raise the board overhead */
                ps.handR = loc(yaw, root, 0.20f, 1.75f, 0.25f);
                ps.handL = loc(yaw, root, -0.20f, 1.75f, 0.25f);
            }
        }
        if (p->throwT > 0) {
            float u = p->throwT / 0.34f;
            ps.handR = loc(yaw, root, 0.28f, lerpf(1.0f, 1.7f, smooth01(u * 1.5f)), lerpf(0.12f, 0.75f, smooth01(u)));
            ps.lean += 0.35f * u;
        }
        if (p->state == ST_CARRY && p->jumpKind == 2) {              /* square jump: full front flip about the waist */
            ps.pitch = p->flipPitch;
            ps.pelvis.y += 0.35f;
        }
        heldHand = ps.handR; heldYaw = yaw;
    } else if (p->state == ST_RIDE || p->state == ST_GRIND) {
        V3 bo0 = v3(p->pos.x, p->pos.y + 0.08f, p->pos.z);
        float liftG = 0;
        if (p->state == ST_RIDE && !p->grounded && p->grab >= 0) liftG = 0.45f * smooth01(p->grabT / 0.22f);
        V3 bo = v3(bo0.x, bo0.y + liftG + (p->trickActive ? 0.04f * sinf(clampf(p->trickT / p->trickDur, 0, 1) * PI_F) : 0), bo0.z);
        bx = xf_euler(bo, p->yaw + p->boardShove, p->boardPitch, p->boardRoll);
        Xf bf = xf_euler(bo, p->yaw, p->boardPitch * 0.4f, 0);
        V3 front = xf_pt(&bf, v3(0, 0.105f, 0.21f)), back = xf_pt(&bf, v3(0, 0.105f, -0.20f));
        if (p->pushing && p->state == ST_RIDE && p->grounded) {
            float s = sinf(p->pushPh);
            if (s > 0) {
                V3 fw = dloc(p->yaw, 0, 0, 1);
                V3 gp = v3(p->pos.x - fw.x * (0.3f + 0.45f * s), p->pos.y + 0.09f + 0.10f * (1 - s), p->pos.z - fw.z * (0.3f + 0.45f * s));
                back = vlerp(back, gp, smooth01(s * 1.6f));
            }
        }
        float by = p->yaw + side * 1.15f;
        ps.yaw = by;
        float hipH = 0.82f - 0.26f * p->crouch;
        if (p->grab >= 0 && p->state == ST_RIDE && !p->grounded) hipH = 0.60f;
        ps.pelvis = v3(bo0.x, bo0.y + 0.105f + hipH + 0.02f, bo0.z);
        if (side > 0) { ps.footL = front; ps.footR = back; } else { ps.footL = back; ps.footR = front; }
        ps.footYawL = ps.footYawR = p->yaw + side * 1.3f;
        ps.lean = 0.12f + 0.32f * p->crouch + ((p->grab >= 0 && !p->grounded) ? 0.55f : 0);
        if (p->manual) ps.pitch = -0.18f;
        ps.kneePole = dloc(by, 0, 0.2f, 1);
        float arm = 0.35f - 0.15f * p->crouch;
        float bal = p->state == ST_GRIND ? clampf(p->bal, -1, 1) * 0.25f : 0;
        ps.handL = vadd(ps.pelvis, dloc(by, -0.55f, arm + bal, 0.12f));
        ps.handR = vadd(ps.pelvis, dloc(by,  0.55f, arm - bal, 0.12f));
        ps.elbowPoleL = dloc(by, -0.4f, -0.5f, -0.5f); ps.elbowPoleR = dloc(by, 0.4f, -0.5f, -0.5f);
        if (p->state == ST_RIDE && !p->grounded && p->grab >= 0) {
            float sd = (float)side;
            V3 pts[6] = { v3(sd * 0.105f, 0, 0.05f), v3(0, 0.01f, 0.40f), v3(0, 0.01f, -0.40f),
                          v3(-sd * 0.105f, 0, 0), v3(sd * 0.10f, 0, 0.20f), v3(-sd * 0.10f, 0, -0.20f) };
            V3 tgt = xf_pt(&bx, pts[p->grab]);
            if (p->grab < 3) ps.handL = tgt; else ps.handR = tgt;
        }
        ridingBoard = 1;
    } else { /* ST_BAIL */
        float t = p->bailT;
        float k = smooth01(t / 0.35f) * (1.0f - smooth01((t - 1.5f) / 0.5f));
        float yaw = p->yaw; V3 root = p->pos;
        ps.yaw = yaw;
        ps.pitch = p->bailDir * 1.45f * k;
        ps.pelvis = v3(root.x, root.y + lerpf(0.95f, 0.30f, k), root.z);
        V3 s0 = loc(yaw, root, -0.11f, 0.09f, 0), s1 = loc(yaw, root, 0.11f, 0.09f, 0);
        V3 f0 = loc(yaw, root, -0.30f, 0.12f, -0.55f * p->bailDir * k), f1 = loc(yaw, root, 0.28f, 0.10f, -0.40f * p->bailDir * k);
        ps.footL = vlerp(s0, f0, k); ps.footR = vlerp(s1, f1, k);
        ps.footYawL = ps.footYawR = yaw;
        V3 h0 = loc(yaw, root, -0.27f, 0.98f, 0), h1 = loc(yaw, root, 0.27f, 0.98f, 0);
        V3 g0 = loc(yaw, root, -0.75f, 0.30f, 0.60f * p->bailDir), g1 = loc(yaw, root, 0.70f, 0.40f, 0.50f * p->bailDir);
        ps.handL = vlerp(h0, g0, k); ps.handR = vlerp(h1, g1, k);
        ps.kneePole = dloc(yaw, 0, 0.5f, 1);
        ps.elbowPoleL = dloc(yaw, -0.3f, 0, -1); ps.elbowPoleR = dloc(yaw, 0.3f, 0, -1);
    }

    /* blob shadow (pushed slightly back in the sort order) */
    {
        int n0 = g_nfg; g_unlit = 1;
        float gh = ground_h(p->pos.x, p->pos.z);
        if (p->state == ST_GRIND || p->state == ST_RIDE) gh = ground_h(p->pos.x, p->pos.z);
        float r = 0.55f;
        V3 c = v3(p->pos.x, gh + 0.015f, p->pos.z);
        for (int i = 0; i < 8; i++) {
            float a0 = TAU_F * i / 8, a1 = TAU_F * (i + 1) / 8;
            emit_tri(c, v3(c.x + cosf(a0) * r, c.y, c.z + sinf(a0) * r), v3(c.x + cosf(a1) * r, c.y, c.z + sinf(a1) * r), 0x2a2d31, or_dir(v3(0, 1, 0)));
        }
        for (int i = n0; i < g_nfg; i++) g_fg[i].key += 1.2f;
        g_unlit = 0;
    }

    draw_human(&ps);

    if (ridingBoard) draw_board(&bx, COL_DECK);
    else if (heldBoard) draw_held_board(heldHand, heldYaw);
    else if (p->state == ST_FOOT && !p->tbOn && p->throwT == 0) {         /* stowed on the back */
        Xf b; b.o = xf_pt(&ps.torso, v3(0, 0.28f, -0.16f));
        b.r = ps.torso.r; b.u = vmul(ps.torso.f, -1.0f); b.f = ps.torso.u;
        draw_board(&b, COL_DECK);
    }
    if (p->tbOn) {
        Xf tb = xf_euler(p->tbPos, p->tbYaw, p->tbPitch, p->tbRoll);
        draw_board(&tb, COL_DECK);
    }
}

static void scene_draw(void) {
    gfx_begin_scene();
    level_draw();
    player_draw();
    gfx_end_scene();
}

/* ------------------------------------------------------------------ HUD */
static void hud_bar(float x, float y, float w, float h, float v, u32 col) {
    gfx_rect(x - 2, y - 2, x + w + 2, y + h + 2, 0x000000);
    gfx_rect(x, y, x + w, y + h, 0x30343a);
    gfx_rect(x, y, x + w * clampf(v, 0, 1), y + h, col);
}

static void hud_draw(void) {
    Player *p = &P;
    char buf[96];
    if (!g_opt.showHud) return;
    snprintf(buf, sizeof buf, "SCORE %ld", p->score);
    gfx_text(24, 20, 2.5f, 0xffffff, buf);
    /* combo list */
    if (p->nNames > 0) {
        float y = 54;
        for (int i = 0; i < p->nNames; i++) {
            snprintf(buf, sizeof buf, "%s  +%d", p->names[i], p->namePts[i]);
            gfx_text(24, y, 2.0f, 0xffd23f, buf); y += 20;
        }
        snprintf(buf, sizeof buf, "COMBO %d  X%d", p->comboPts, p->nNames);
        gfx_text(24, y + 4, 2.2f, 0xffffff, buf);
    }
    if (p->msgT > 0) {
        gfx_text_c(g_sw * 0.5f, g_sh - 170, 3.0f, p->msgGood ? 0x6cf08c : 0xff5a4a, p->msg);
    }
    if (p->state == ST_GRIND) {
        gfx_text_c(g_sw * 0.5f, g_sh - 120, 2.4f, 0xffffff, p->grindName);
        gfx_text_c(g_sw * 0.5f, g_sh - 96, 1.6f, 0xcccccc, "KEEP THE BAR IN THE MIDDLE WITH LEFT STICK");
        hud_bar(g_sw * 0.5f - 120, g_sh - 76, 240, 12, 0.5f + p->bal * 0.5f, fabsf(p->bal) > 0.7f ? 0xff5a4a : 0x6cf08c);
    }
    snprintf(buf, sizeof buf, "%d KM/H", (int)(hspeed() * 3.6f));
    gfx_text(g_sw - 150, 20, 2.0f, 0xffffff, buf);
    const char *hint = "";
    switch (p->state) {
        case ST_FOOT:  hint = "R1 HOLD BOARD   TRIANGLE RIDE"; break;
        case ST_CARRY: hint = "R1 THROW   TRIANGLE RIDE   X JUMP   SQUARE FLIP JUMP"; break;
        case ST_RIDE:  hint = p->grounded ? "RIGHT STICK FLICK = TRICKS   X PUSH   L1 HOLD BOARD" : "L2/R2 GRAB   LEFT STICK SPIN"; break;
        case ST_GRIND: hint = "FLICK RIGHT STICK DOWN-UP TO OLLIE OUT"; break;
        case ST_BAIL:  hint = ""; break;
    }
    gfx_text_c(g_sw * 0.5f, g_sh - 30, 1.5f, 0xd0d0d0, hint);
    gfx_text(g_sw - 150, 44, 1.5f, 0xa0a0a0, "START MENU");
}
#endif
