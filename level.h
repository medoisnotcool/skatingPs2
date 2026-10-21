/* level.h - the closed skatepark: boxes, ramps, quarter pipes, rails + height queries */
#ifndef LEVEL_H
#define LEVEL_H
#include "gfx.h"

#define ARENA_X 30.0f
#define ARENA_Z 22.0f
#define ARENA_H 12.0f

typedef struct { float x0, z0, x1, z1, h; u32 col; } Box;
/* axis 0: slopes along X, axis 1: slopes along Z. hLo = height at x0/z0, hHi = height at x1/z1 */
typedef struct { float x0, z0, x1, z1; int axis; float hLo, hHi; int curved; u32 col; } Ramp;
typedef struct { V3 a, b; int ledge; } Rail;

static Box  g_box[48];  static int g_nbox;
static Ramp g_ramp[24]; static int g_nramp;
static Rail g_rail[24]; static int g_nrail;

static void add_box(float x0, float z0, float x1, float z1, float h, u32 col) {
    Box *b = &g_box[g_nbox++]; b->x0 = x0; b->z0 = z0; b->x1 = x1; b->z1 = z1; b->h = h; b->col = col;
}
static void add_ramp(float x0, float z0, float x1, float z1, int axis, float hLo, float hHi, int curved, u32 col) {
    Ramp *r = &g_ramp[g_nramp++];
    r->x0 = x0; r->z0 = z0; r->x1 = x1; r->z1 = z1; r->axis = axis; r->hLo = hLo; r->hHi = hHi; r->curved = curved; r->col = col;
}
static void add_rail(float ax, float ay, float az, float bx, float by, float bz, int ledge) {
    Rail *r = &g_rail[g_nrail++]; r->a = v3(ax, ay, az); r->b = v3(bx, by, bz); r->ledge = ledge;
}

static float ramp_height(const Ramp *r, float x, float z) {
    if (x < r->x0 || x > r->x1 || z < r->z0 || z > r->z1) return 0.0f;
    float s = r->axis == 0 ? (x - r->x0) / (r->x1 - r->x0) : (z - r->z0) / (r->z1 - r->z0);
    if (r->curved) {
        float hmax = r->hLo > r->hHi ? r->hLo : r->hHi;
        float t = r->hHi > r->hLo ? s : 1.0f - s;
        return hmax * t * t * sqrtf(t);                 /* t^2.5 : tangent at the bottom, steep at the lip */
    }
    return lerpf(r->hLo, r->hHi, s);
}

static float ground_h(float x, float z) {
    float h = 0;
    for (int i = 0; i < g_nbox; i++) {
        const Box *b = &g_box[i];
        if (x >= b->x0 && x <= b->x1 && z >= b->z0 && z <= b->z1 && b->h > h) h = b->h;
    }
    for (int i = 0; i < g_nramp; i++) {
        float rh = ramp_height(&g_ramp[i], x, z);
        if (rh > h) h = rh;
    }
    return h;
}

/* slope of the ground (dh/dx, dh/dz); returns 0 across abrupt edges so box corners do not push the skater */
static void ground_grad(float x, float z, float *gx, float *gz) {
    const float e = 0.18f;
    float hx0 = ground_h(x - e, z), hx1 = ground_h(x + e, z);
    float hz0 = ground_h(x, z - e), hz1 = ground_h(x, z + e);
    *gx = (absf_(hx1 - hx0) > 0.5f) ? 0 : (hx1 - hx0) / (2 * e);
    *gz = (absf_(hz1 - hz0) > 0.5f) ? 0 : (hz1 - hz0) / (2 * e);
    *gx = clampf(*gx, -2.5f, 2.5f);
    *gz = clampf(*gz, -2.5f, 2.5f);
}

/* ------------------------------------------------------------- rails */
static V3 rail_point(const Rail *r, float t) { return vlerp(r->a, r->b, t); }

/* nearest rail the board centre p can lock onto; returns index or -1 */
static int rail_find(V3 p, float *tOut) {
    int best = -1; float bestD = 1e9f;
    for (int i = 0; i < g_nrail; i++) {
        const Rail *r = &g_rail[i];
        V3 ab = vsub(r->b, r->a);
        float len2 = ab.x * ab.x + ab.z * ab.z;
        if (len2 < 1e-4f) continue;
        float t = ((p.x - r->a.x) * ab.x + (p.z - r->a.z) * ab.z) / len2;
        if (t < 0.0f || t > 1.0f) continue;
        V3 q = rail_point(r, t);
        float dx = p.x - q.x, dz = p.z - q.z;
        float dh = sqrtf(dx * dx + dz * dz);
        float dy = p.y - q.y;
        if (dh < 0.42f && dy > -0.25f && dy < 0.45f) {
            float score = dh + absf_(dy) * 0.5f;
            if (score < bestD) { bestD = score; best = i; *tOut = t; }
        }
    }
    return best;
}

/* ---------------------------------------------------------- building */
static void level_init(void) {
    g_nbox = g_nramp = g_nrail = 0;
    const u32 CONC = 0x8b8e92, CONC2 = 0x777a7f, WOOD = 0xa07a4a, RED = 0xb6472f, TEAL = 0x2f8f94, YEL = 0xd6b43a;

    /* funbox in the middle: platform + 4 ramps + ledges */
    add_box(-3.0f, 0.0f, 3.0f, 5.0f, 0.75f, CONC);
    add_ramp(-3.0f, -3.5f, 3.0f, 0.0f, 1, 0.0f, 0.75f, 0, WOOD);     /* south bank */
    add_ramp(-6.0f, 0.0f, -3.0f, 5.0f, 0, 0.0f, 0.75f, 0, WOOD);     /* west bank  */
    add_ramp(3.0f, 0.0f, 6.0f, 5.0f, 0, 0.75f, 0.0f, 0, WOOD);       /* east bank  */
    add_rail(-3.0f, 0.75f, 5.0f, 3.0f, 0.75f, 5.0f, 1);              /* back ledge */
    add_rail(-3.0f, 0.75f, 0.0f, -3.0f, 0.75f, 5.0f, 1);
    add_rail(3.0f, 0.75f, 0.0f, 3.0f, 0.75f, 5.0f, 1);

    /* kicker + gap in front of the funbox */
    add_ramp(-2.0f, -12.0f, 2.0f, -9.5f, 1, 0.0f, 0.9f, 0, WOOD);
    /* manual pad */
    add_box(-9.0f, -9.0f, -4.5f, -6.5f, 0.22f, CONC2);

    /* mini ramp (two quarter pipes) on the left side */
    add_ramp(-28.0f, -4.0f, -23.0f, 9.0f, 0, 2.2f, 0.0f, 1, CONC);   /* west quarter (high at x0) */
    add_box(-30.0f, -4.0f, -28.0f, 9.0f, 2.2f, CONC2);
    add_ramp(-15.0f, -4.0f, -10.0f, 9.0f, 0, 0.0f, 2.2f, 1, CONC);   /* east quarter (high at x1) */
    add_box(-10.0f, -4.0f, -8.0f, 9.0f, 2.2f, CONC2);
    add_rail(-10.0f, 2.2f, -4.0f, -10.0f, 2.2f, 9.0f, 1);            /* coping/ledge along the deck edge */

    /* stair set with handrail on the right side */
    for (int i = 0; i < 6; i++)
        add_box(12.0f + 0.9f * i, 8.0f, 22.0f, 12.0f, 0.18f * (i + 1), (i & 1) ? CONC : CONC2);
    add_rail(12.0f, 0.18f + 0.85f, 12.35f, 17.4f, 1.08f + 0.85f, 12.35f, 0);   /* down-rail */
    add_rail(17.4f, 1.08f, 8.0f, 22.0f, 1.08f, 8.0f, 1);                      /* platform ledge */

    /* flat rails + hubba ledge */
    add_rail(4.0f, 0.55f, -7.0f, 14.0f, 0.55f, -7.0f, 0);
    add_rail(-1.0f, 0.55f, -18.0f, 9.0f, 0.55f, -18.0f, 0);
    add_box(5.0f, -4.0f, 15.0f, -3.0f, 0.5f, CONC);
    add_rail(5.0f, 0.5f, -3.0f, 15.0f, 0.5f, -3.0f, 1);
    add_rail(5.0f, 0.5f, -4.0f, 15.0f, 0.5f, -4.0f, 1);

    /* bank on the right wall */
    add_ramp(24.0f, -14.0f, 29.0f, -2.0f, 0, 0.0f, 1.6f, 0, TEAL);
    add_box(29.0f, -14.0f, 30.0f, -2.0f, 1.6f, CONC2);

    /* big box with ramps in the back */
    add_box(-3.0f, 12.0f, 3.0f, 16.0f, 1.1f, CONC);
    add_ramp(-3.0f, 8.5f, 3.0f, 12.0f, 1, 0.0f, 1.1f, 0, WOOD);
    add_ramp(3.0f, 12.0f, 6.0f, 16.0f, 0, 1.1f, 0.0f, 0, WOOD);
    add_rail(-3.0f, 1.1f, 16.0f, 3.0f, 1.1f, 16.0f, 1);

    /* upper platform (back-left) with long access ramp */
    add_box(-30.0f, 14.0f, -18.0f, 22.0f, 2.4f, CONC2);
    add_ramp(-18.0f, 15.0f, -10.0f, 20.0f, 0, 2.4f, 0.0f, 0, RED);
    add_rail(-18.0f, 2.4f, 14.0f, -18.0f, 2.4f, 22.0f, 1);
    add_rail(-30.0f, 2.4f, 14.0f, -18.0f, 2.4f, 14.0f, 1);

    /* a few extra low boxes for variety */
    add_box(18.0f, -10.0f, 21.0f, -7.0f, 0.4f, YEL);
}

/* ----------------------------------------------------------- drawing */
static void draw_ramp(const Ramp *r) {
    float ax0 = r->axis == 0 ? r->x0 : r->z0, ax1 = r->axis == 0 ? r->x1 : r->z1;
    float cx0 = r->axis == 0 ? r->z0 : r->x0, cx1 = r->axis == 0 ? r->z1 : r->x1;
    int segs = r->curved ? 10 : (int)ceilf((ax1 - ax0) / 2.0f);
    if (segs < 1) segs = 1;
    float hMax = r->hLo > r->hHi ? r->hLo : r->hHi;
    V3 centre = r->axis == 0 ? v3((r->x0 + r->x1) * 0.5f, hMax * 0.3f, (r->z0 + r->z1) * 0.5f)
                             : v3((r->x0 + r->x1) * 0.5f, hMax * 0.3f, (r->z0 + r->z1) * 0.5f);
    Orient up = or_dir(v3(0, 1, 0)), in = or_pt(centre);
    u32 side = (r->col >> 1) & 0x7f7f7f;
    for (int i = 0; i < segs; i++) {
        float s0 = ax0 + (ax1 - ax0) * i / segs, s1 = ax0 + (ax1 - ax0) * (i + 1) / segs;
        float h0, h1;
        if (r->axis == 0) { h0 = ramp_height(r, s0 + 1e-3f, (cx0 + cx1) * 0.5f); h1 = ramp_height(r, s1 - 1e-3f, (cx0 + cx1) * 0.5f); }
        else              { h0 = ramp_height(r, (cx0 + cx1) * 0.5f, s0 + 1e-3f); h1 = ramp_height(r, (cx0 + cx1) * 0.5f, s1 - 1e-3f); }
        #define RP(s, c, h) (r->axis == 0 ? v3((s), (h), (c)) : v3((c), (h), (s)))
        emit_quad(RP(s0, cx0, h0), RP(s1, cx0, h1), RP(s1, cx1, h1), RP(s0, cx1, h0), r->col, up, 2.0f);
        emit_quad(RP(s0, cx0, 0), RP(s1, cx0, 0), RP(s1, cx0, h1), RP(s0, cx0, h0), side, in, 3.0f);
        emit_quad(RP(s0, cx1, 0), RP(s1, cx1, 0), RP(s1, cx1, h1), RP(s0, cx1, h0), side, in, 3.0f);
        #undef RP
    }
    /* vertical back face at the high end */
    if (r->axis == 0) {
        float xe = r->hHi > r->hLo ? r->x1 : r->x0;
        emit_quad(v3(xe, 0, r->z0), v3(xe, 0, r->z1), v3(xe, hMax, r->z1), v3(xe, hMax, r->z0), side, in, 3.0f);
    } else {
        float ze = r->hHi > r->hLo ? r->z1 : r->z0;
        emit_quad(v3(r->x0, 0, ze), v3(r->x1, 0, ze), v3(r->x1, hMax, ze), v3(r->x0, hMax, ze), side, in, 3.0f);
    }
}

static void draw_rail(const Rail *r) {
    if (r->ledge) {                                    /* ledges get a bright coping stripe */
        emit_prism(r->a, r->b, 0.035f, 0.035f, 4, 0xe2c14b);
        return;
    }
    emit_prism(r->a, r->b, 0.045f, 0.045f, 6, 0xc9ced4);
    float len = sqrtf((r->b.x - r->a.x) * (r->b.x - r->a.x) + (r->b.z - r->a.z) * (r->b.z - r->a.z));
    int posts = (int)(len / 2.6f) + 2;
    for (int i = 0; i < posts; i++) {
        float t = (float)i / (posts - 1);
        V3 p = rail_point(r, t);
        emit_prism(v3(p.x, 0, p.z), p, 0.035f, 0.035f, 4, 0x8e949b);
    }
}

static void level_draw(void) {
    /* ---- background pass (unsorted): floor, walls, ceiling ---- */
    g_toBG = 1;
    Orient up = or_dir(v3(0, 1, 0));
    for (int ix = 0; ix < 20; ix++)
        for (int iz = 0; iz < 15; iz++) {
            float x0 = -ARENA_X + ix * 3.0f, z0 = -ARENA_Z + iz * 3.0f + 0.0f;
            if (iz == 14) continue;
            V3 c = v3(x0 + 1.5f, 0, z0 + 1.5f);
            if (!gfx_visible(c, 2.2f, 70.0f)) continue;
            u32 col = ((ix + iz) & 1) ? 0x6e7176 : 0x64676c;
            emit_quad(v3(x0, 0, z0), v3(x0 + 3, 0, z0), v3(x0 + 3, 0, z0 + 3), v3(x0, 0, z0 + 3), col, up, 99.0f);
        }
    /* last row to fill the 44 m depth (15th strip is 2 m) */
    for (int ix = 0; ix < 20; ix++) {
        float x0 = -ARENA_X + ix * 3.0f, z0 = ARENA_Z - 3.0f;
        V3 c = v3(x0 + 1.5f, 0, z0 + 1.5f);
        if (!gfx_visible(c, 2.2f, 70.0f)) continue;
        u32 col = (ix & 1) ? 0x6e7176 : 0x64676c;
        emit_quad(v3(x0, 0, z0), v3(x0 + 3, 0, z0), v3(x0 + 3, 0, z0 + 3), v3(x0, 0, z0 + 3), col, up, 99.0f);
    }
    /* walls (inner faces) */
    struct { V3 a, b; V3 n; } W[4] = {
        { v3(-ARENA_X, 0, -ARENA_Z), v3( ARENA_X, 0, -ARENA_Z), v3(0, 0,  1) },
        { v3( ARENA_X, 0,  ARENA_Z), v3(-ARENA_X, 0,  ARENA_Z), v3(0, 0, -1) },
        { v3(-ARENA_X, 0,  ARENA_Z), v3(-ARENA_X, 0, -ARENA_Z), v3(1, 0,  0) },
        { v3( ARENA_X, 0, -ARENA_Z), v3( ARENA_X, 0,  ARENA_Z), v3(-1, 0, 0) }
    };
    for (int w = 0; w < 4; w++) {
        int panels = (w < 2) ? 6 : 4;
        for (int p = 0; p < panels; p++) {
            V3 a = vlerp(W[w].a, W[w].b, (float)p / panels), b = vlerp(W[w].a, W[w].b, (float)(p + 1) / panels);
            V3 mid = vmul(vadd(a, b), 0.5f); mid.y = 4.0f;
            if (!gfx_visible(mid, 12.0f, 90.0f)) continue;
            u32 col = (p & 1) ? 0x4a4f57 : 0x555a63;
            emit_quad(a, b, v3(b.x, 5.0f, b.z), v3(a.x, 5.0f, a.z), col, or_dir(W[w].n), 99.0f);
            emit_quad(v3(a.x, 5.0f, a.z), v3(b.x, 5.0f, b.z), v3(b.x, ARENA_H, b.z), v3(a.x, ARENA_H, a.z), 0x3b3f47, or_dir(W[w].n), 99.0f);
            /* painted banner panel */
            V3 o = vmul(W[w].n, 0.03f);
            u32 bc = (p % 3 == 0) ? 0xb6472f : ((p % 3 == 1) ? 0x2f8f94 : 0xd6b43a);
            V3 a2 = vadd(vlerp(a, b, 0.2f), o), b2 = vadd(vlerp(a, b, 0.8f), o);
            emit_quad(v3(a2.x, 1.2f, a2.z), v3(b2.x, 1.2f, b2.z), v3(b2.x, 3.6f, b2.z), v3(a2.x, 3.6f, a2.z), bc, or_dir(W[w].n), 99.0f);
        }
    }
    /* ceiling with light panels */
    for (int ix = 0; ix < 6; ix++)
        for (int iz = 0; iz < 4; iz++) {
            float x0 = -ARENA_X + ix * 10.0f, z0 = -ARENA_Z + iz * 11.0f;
            V3 c = v3(x0 + 5, ARENA_H, z0 + 5.5f);
            if (!gfx_visible(c, 9.0f, 90.0f)) continue;
            emit_quad(v3(x0, ARENA_H, z0), v3(x0 + 10, ARENA_H, z0), v3(x0 + 10, ARENA_H, z0 + 11), v3(x0, ARENA_H, z0 + 11), 0x2b2e34, or_dir(v3(0, -1, 0)), 99.0f);
            g_unlit = 1;
            emit_quad(v3(x0 + 3, ARENA_H - 0.02f, z0 + 3.5f), v3(x0 + 7, ARENA_H - 0.02f, z0 + 3.5f), v3(x0 + 7, ARENA_H - 0.02f, z0 + 7.5f), v3(x0 + 3, ARENA_H - 0.02f, z0 + 7.5f), 0xf3ecc8, or_dir(v3(0, -1, 0)), 99.0f);
            g_unlit = 0;
        }
    g_toBG = 0;

    /* ---- foreground pass (sorted) ---- */
    for (int i = 0; i < g_nbox; i++) {
        const Box *b = &g_box[i];
        V3 c = v3((b->x0 + b->x1) * 0.5f, b->h * 0.5f, (b->z0 + b->z1) * 0.5f);
        float rad = 0.5f * sqrtf((b->x1 - b->x0) * (b->x1 - b->x0) + (b->z1 - b->z0) * (b->z1 - b->z0)) + b->h;
        if (!gfx_visible(c, rad, 62.0f)) continue;
        Xf id = xf_euler(c, 0, 0, 0);
        emit_box(&id, v3(0, 0, 0), v3((b->x1 - b->x0) * 0.5f, b->h * 0.5f, (b->z1 - b->z0) * 0.5f), b->col, 2.0f, 1);
    }
    for (int i = 0; i < g_nramp; i++) {
        const Ramp *r = &g_ramp[i];
        V3 c = v3((r->x0 + r->x1) * 0.5f, 1, (r->z0 + r->z1) * 0.5f);
        float rad = 0.5f * sqrtf((r->x1 - r->x0) * (r->x1 - r->x0) + (r->z1 - r->z0) * (r->z1 - r->z0)) + 2.5f;
        if (!gfx_visible(c, rad, 62.0f)) continue;
        draw_ramp(r);
    }
    for (int i = 0; i < g_nrail; i++) {
        V3 c = vlerp(g_rail[i].a, g_rail[i].b, 0.5f);
        if (!gfx_visible(c, vlen(vsub(g_rail[i].b, g_rail[i].a)) * 0.5f + 1, 55.0f)) continue;
        draw_rail(&g_rail[i]);
    }
}
#endif
