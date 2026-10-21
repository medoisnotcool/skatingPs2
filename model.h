/* model.h - skateboard + the skater (built from boxes / prisms / ellipsoids, posed with 2-bone IK) */
#ifndef MODEL_H
#define MODEL_H
#include "gfx.h"

/* ---- colours taken from the reference renders ---- */
#define COL_SKIN    0xc58c6a
#define COL_HAIR    0x2a1b12
#define COL_SHIRT_T 0x7d2433      /* maroon top band    */
#define COL_SHIRT_M 0xb9c9e6      /* pale blue mid band */
#define COL_SHIRT_B 0x1f3566      /* navy bottom band   */
#define COL_JEANS   0x5f88b6
#define COL_SHOE    0x9a9ca1
#define COL_SOLE    0xe9e9e4
#define COL_DECK    0xd9482b

/* ------------------------------------------------------------ skateboard */
static void draw_board(const Xf *bx, u32 deck) {
    emit_box(bx, v3(0, 0, 0), v3(0.105f, 0.010f, 0.30f), deck, 9.0f, 0);
    emit_box(bx, v3(0, 0.0125f, 0), v3(0.100f, 0.002f, 0.30f), 0x141414, 9.0f, 1);      /* grip tape */
    for (int e = 0; e < 2; e++) {                                                       /* nose + tail kicks */
        float sgn = e ? 1.0f : -1.0f;
        Xf loc = xf_euler(v3(0, 0.030f, 0.340f * sgn), 0, sgn * -0.40f, 0);
        Xf kx = xf_mul(bx, &loc);
        emit_box(&kx, v3(0, 0, 0), v3(0.105f, 0.010f, 0.060f), deck, 9.0f, 0);
        emit_box(&kx, v3(0, 0.0125f, 0), v3(0.100f, 0.002f, 0.060f), 0x141414, 9.0f, 1);
    }
    for (int t = 0; t < 2; t++) {
        float z = t ? 0.20f : -0.20f;
        emit_box(bx, v3(0, -0.026f, z), v3(0.075f, 0.014f, 0.018f), 0x8d9299, 9.0f, 1);
        emit_box(bx, v3(-0.098f, -0.050f, z), v3(0.014f, 0.030f, 0.030f), 0xf2f2f2, 9.0f, 1);
        emit_box(bx, v3( 0.098f, -0.050f, z), v3(0.014f, 0.030f, 0.030f), 0xf2f2f2, 9.0f, 1);
    }
}

/* ----------------------------------------------------------------- IK */
/* two-bone IK: returns the middle joint (elbow/knee). end is clamped to reach. */
static V3 ik2(V3 root, V3 *end, float l1, float l2, V3 pole) {
    V3 d = vsub(*end, root);
    float dist = vlen(d);
    float maxr = (l1 + l2) * 0.998f;
    if (dist > maxr) { d = vmul(d, maxr / dist); dist = maxr; *end = vadd(root, d); }
    if (dist < 0.05f) { dist = 0.05f; }
    V3 dir = vmul(d, 1.0f / dist);
    float a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist);
    float h2 = l1 * l1 - a * a;
    float h = h2 > 0 ? sqrtf(h2) : 0;
    V3 pp = vsub(pole, vmul(dir, vdot(pole, dir)));
    pp = vnorm(pp);
    if (vlen(pp) < 0.1f) pp = v3(0, 0, 1);
    return vadd(vadd(root, vmul(dir, a)), vmul(pp, h));
}

/* ---------------------------------------------------------------- skater */
typedef struct {
    V3 pelvis;                 /* world position of the pelvis */
    float yaw, pitch, roll;    /* whole-body orientation about the pelvis */
    float lean;                /* extra torso lean (forward positive) */
    V3 handL, handR;           /* world targets (wrists) */
    V3 footL, footR;           /* world targets (ankles) */
    float footYawL, footYawR;
    V3 kneePole;               /* world direction the knees point to */
    V3 elbowPoleL, elbowPoleR;
    Xf torso;                  /* output: torso frame (for attaching the stowed board) */
} Pose;

static void draw_shoe(V3 ankle, float yaw) {
    Xf f = xf_euler(ankle, yaw, 0, 0);
    emit_box(&f, v3(0, -0.035f, 0.065f), v3(0.052f, 0.030f, 0.135f), COL_SHOE, 9.0f, 0);
    emit_box(&f, v3(0, -0.075f, 0.065f), v3(0.056f, 0.012f, 0.140f), COL_SOLE, 9.0f, 1);
    emit_box(&f, v3(0, 0.010f, -0.010f), v3(0.046f, 0.030f, 0.055f), COL_SHOE, 9.0f, 1);
}

static void draw_human(Pose *p) {
    Xf B = xf_euler(p->pelvis, p->yaw, p->pitch, p->roll);
    Xf lean = xf_euler(v3(0, 0.02f, 0), 0, p->lean, 0);
    Xf T = xf_mul(&B, &lean);
    p->torso = T;

    V3 hipL = xf_pt(&B, v3(-0.10f, -0.02f, 0)), hipR = xf_pt(&B, v3(0.10f, -0.02f, 0));
    V3 shL = xf_pt(&T, v3(-0.215f, 0.50f, 0)), shR = xf_pt(&T, v3(0.215f, 0.50f, 0));

    /* legs */
    V3 fL = p->footL, fR = p->footR;
    V3 kL = ik2(hipL, &fL, 0.44f, 0.43f, p->kneePole);
    V3 kR = ik2(hipR, &fR, 0.44f, 0.43f, p->kneePole);
    emit_prism(hipL, kL, 0.078f, 0.060f, 6, COL_JEANS);
    emit_prism(kL, fL, 0.060f, 0.048f, 6, COL_JEANS);
    emit_prism(hipR, kR, 0.078f, 0.060f, 6, COL_JEANS);
    emit_prism(kR, fR, 0.060f, 0.048f, 6, COL_JEANS);
    draw_shoe(p->footL, p->footYawL);
    draw_shoe(p->footR, p->footYawR);

    /* pelvis + three-band shirt */
    emit_box(&B, v3(0, 0.00f, 0), v3(0.175f, 0.075f, 0.098f), COL_JEANS, 9.0f, 0);
    emit_box(&T, v3(0, 0.105f, 0), v3(0.172f, 0.088f, 0.100f), COL_SHIRT_B, 9.0f, 0);
    emit_box(&T, v3(0, 0.285f, 0), v3(0.178f, 0.092f, 0.103f), COL_SHIRT_M, 9.0f, 0);
    emit_box(&T, v3(0, 0.460f, 0), v3(0.192f, 0.088f, 0.105f), COL_SHIRT_T, 9.0f, 0);

    /* neck, head, hair, face details */
    emit_prism(xf_pt(&T, v3(0, 0.54f, 0)), xf_pt(&T, v3(0, 0.62f, 0.005f)), 0.050f, 0.045f, 6, COL_SKIN);
    V3 hc = v3(0, 0.735f, 0.012f);
    emit_ell(&T, hc, v3(0.098f, 0.122f, 0.108f), COL_SKIN, 8, 6);
    emit_ell(&T, v3(0, 0.775f, -0.012f), v3(0.108f, 0.108f, 0.112f), COL_HAIR, 8, 5);
    emit_ell(&T, v3(0, 0.842f, 0.012f), v3(0.085f, 0.040f, 0.090f), COL_HAIR, 7, 3);    /* curly top volume */
    emit_box(&T, v3(0, 0.700f, 0.116f), v3(0.052f, 0.010f, 0.012f), COL_HAIR, 9.0f, 1); /* moustache */
    emit_box(&T, v3(0, 0.725f, 0.118f), v3(0.012f, 0.022f, 0.012f), 0xb67f5f, 9.0f, 1); /* nose */
    emit_box(&T, v3(-0.040f, 0.752f, 0.110f), v3(0.012f, 0.008f, 0.006f), 0x1b1410, 9.0f, 1);
    emit_box(&T, v3( 0.040f, 0.752f, 0.110f), v3(0.012f, 0.008f, 0.006f), 0x1b1410, 9.0f, 1);

    /* arms (maroon sleeve on the upper arm) */
    V3 hL = p->handL, hR = p->handR;
    V3 eL = ik2(shL, &hL, 0.30f, 0.27f, p->elbowPoleL);
    V3 eR = ik2(shR, &hR, 0.30f, 0.27f, p->elbowPoleR);
    V3 sl = vlerp(shL, eL, 0.45f), sr = vlerp(shR, eR, 0.45f);
    emit_prism(shL, sl, 0.060f, 0.058f, 6, COL_SHIRT_T);
    emit_prism(sl, eL, 0.046f, 0.040f, 6, COL_SKIN);
    emit_prism(eL, hL, 0.040f, 0.032f, 6, COL_SKIN);
    emit_prism(shR, sr, 0.060f, 0.058f, 6, COL_SHIRT_T);
    emit_prism(sr, eR, 0.046f, 0.040f, 6, COL_SKIN);
    emit_prism(eR, hR, 0.040f, 0.032f, 6, COL_SKIN);
    Xf ident = xf_euler(v3(0, 0, 0), 0, 0, 0);
    ident.o = hL; emit_box(&ident, v3(0, -0.04f, 0), v3(0.035f, 0.05f, 0.025f), COL_SKIN, 9.0f, 0);
    ident.o = hR; emit_box(&ident, v3(0, -0.04f, 0), v3(0.035f, 0.05f, 0.025f), COL_SKIN, 9.0f, 0);
}
#endif
