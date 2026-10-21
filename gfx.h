/* gfx.h - small software 3D pipeline on top of gsKit (flat/gouraud triangles + sprites)
 *
 * Everything is transformed on the EE and handed to the GS as 2D triangles that are
 * sorted back-to-front (painter's algorithm), so no Z-buffer is needed.            */
#ifndef GFX_H
#define GFX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "math3d.h"

#ifdef GSKIT_OLD_GORAUD   /* very old gsKit spelled it "goraud" */
#define gsKit_prim_triangle_gouraud gsKit_prim_triangle_goraud
#endif

#define NEAR_Z   0.30f
#define MAX_FG   7000
#define MAX_BG   2800
#define GUARD    120.0f

typedef struct { float x[3], y[3]; u32 rgb; float key; } STri;
typedef struct { int dirMode; V3 v; } Orient;    /* dirMode 0: v is a point on the "inside"; 1: v is wanted normal */

static GSGLOBAL *g_gs;
static STri g_fg[MAX_FG], g_bg[MAX_BG];
static STri *g_ord[MAX_FG];
static int g_nfg, g_nbg, g_toBG, g_unlit;
static V3 g_cam, g_cr, g_cu, g_cf;
static float g_focal;
static int g_sw = 640, g_sh = 448;
static V3 g_light;
static u32 g_bgRGB = 0x15181d;
static float g_fogA = 34.0f, g_fogB = 80.0f;

static inline Orient or_pt(V3 p) { Orient o; o.dirMode = 0; o.v = p; return o; }
static inline Orient or_dir(V3 d) { Orient o; o.dirMode = 1; o.v = d; return o; }

static inline u64 gfx_col(u32 rgb) {
    return GS_SETREG_RGBAQ((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, 0x80, 0x00);
}

/* ------------------------------------------------------------------ camera */
static void gfx_camera(V3 eye, V3 target, float fovDeg) {
    g_cam = eye;
    g_cf = vnorm(vsub(target, eye));
    g_cr = vnorm(vcross(v3(0, 1, 0), g_cf));
    g_cu = vcross(g_cf, g_cr);
    g_focal = (g_sw * 0.5f) / tanf(fovDeg * 0.5f * PI_F / 180.0f);
}

static void gfx_begin_scene(void) {
    g_nfg = 0; g_nbg = 0; g_toBG = 0; g_unlit = 0;
    g_light = vnorm(v3(0.45f, 0.85f, 0.30f));
}

static int gfx_visible(V3 c, float radius, float maxDist) {
    V3 d = vsub(c, g_cam);
    float z = vdot(d, g_cf);
    if (z < -radius) return 0;
    if (vlen(d) - radius > maxDist) return 0;
    return 1;
}

/* ------------------------------------------------------------- 2D clipping */
typedef struct { float x, y; } P2;

static int clip_axis(const P2 *in, int n, P2 *out, int axis, float val, int keepGreater) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        P2 a = in[i], b = in[(i + 1) % n];
        float va = axis ? a.y : a.x, vb = axis ? b.y : b.x;
        int ia = keepGreater ? (va >= val) : (va <= val);
        int ib = keepGreater ? (vb >= val) : (vb <= val);
        if (ia) out[m++] = a;
        if (ia != ib) {
            float t = (val - va) / (vb - va);
            P2 p; p.x = a.x + (b.x - a.x) * t; p.y = a.y + (b.y - a.y) * t;
            out[m++] = p;
        }
        if (m > 12) break;
    }
    return m;
}

static void push_tri2d(P2 a, P2 b, P2 c, u32 rgb, float key) {
    STri *t;
    if (g_toBG) { if (g_nbg >= MAX_BG) return; t = &g_bg[g_nbg++]; }
    else        { if (g_nfg >= MAX_FG) return; t = &g_fg[g_nfg++]; }
    t->x[0] = a.x; t->y[0] = a.y; t->x[1] = b.x; t->y[1] = b.y; t->x[2] = c.x; t->y[2] = c.y;
    t->rgb = rgb; t->key = key;
}

static void submit2d(P2 a, P2 b, P2 c, u32 rgb, float key) {
    float minx = a.x < b.x ? (a.x < c.x ? a.x : c.x) : (b.x < c.x ? b.x : c.x);
    float maxx = a.x > b.x ? (a.x > c.x ? a.x : c.x) : (b.x > c.x ? b.x : c.x);
    float miny = a.y < b.y ? (a.y < c.y ? a.y : c.y) : (b.y < c.y ? b.y : c.y);
    float maxy = a.y > b.y ? (a.y > c.y ? a.y : c.y) : (b.y > c.y ? b.y : c.y);
    if (maxx < 0 || minx > g_sw || maxy < 0 || miny > g_sh) return;      /* fully off-screen */
    if (minx >= -GUARD && maxx <= g_sw + GUARD && miny >= -GUARD && maxy <= g_sh + GUARD) {
        push_tri2d(a, b, c, rgb, key);
        return;
    }
    P2 p0[16], p1[16];
    p0[0] = a; p0[1] = b; p0[2] = c;
    int n = 3;
    n = clip_axis(p0, n, p1, 0, -GUARD, 1);              if (n < 3) return;
    n = clip_axis(p1, n, p0, 0, g_sw + GUARD, 0);        if (n < 3) return;
    n = clip_axis(p0, n, p1, 1, -GUARD, 1);              if (n < 3) return;
    n = clip_axis(p1, n, p0, 1, g_sh + GUARD, 0);        if (n < 3) return;
    for (int i = 1; i + 1 < n; i++) push_tri2d(p0[0], p0[i], p0[i + 1], rgb, key);
}

static P2 proj(V3 v) {
    P2 p;
    float iz = 1.0f / v.z;
    p.x = g_sw * 0.5f + v.x * g_focal * iz;
    p.y = g_sh * 0.5f - v.y * g_focal * iz;
    return p;
}

/* view-space triangle -> near clip -> project -> submit */
static void submit_view(V3 a, V3 b, V3 c, u32 rgb, float key) {
    V3 in[3]; V3 poly[5]; int n = 0;
    in[0] = a; in[1] = b; in[2] = c;
    for (int i = 0; i < 3; i++) {
        V3 p = in[i], q = in[(i + 1) % 3];
        int ia = p.z >= NEAR_Z, ib = q.z >= NEAR_Z;
        if (ia) poly[n++] = p;
        if (ia != ib) {
            float t = (NEAR_Z - p.z) / (q.z - p.z);
            poly[n++] = vlerp(p, q, t);
        }
    }
    if (n < 3) return;
    for (int i = 1; i + 1 < n; i++)
        submit2d(proj(poly[0]), proj(poly[i]), proj(poly[i + 1]), rgb, key);
}

static inline V3 to_view(V3 w) {
    V3 d = vsub(w, g_cam);
    return v3(vdot(d, g_cr), vdot(d, g_cu), vdot(d, g_cf));
}

/* ------------------------------------------------------------ triangle emit */
static u32 shade_rgb(u32 rgb, float sh, float fog) {
    float r = ((rgb >> 16) & 255) * sh, g = ((rgb >> 8) & 255) * sh, b = (rgb & 255) * sh;
    float fr = (g_bgRGB >> 16) & 255, fg = (g_bgRGB >> 8) & 255, fb = g_bgRGB & 255;
    r = r + (fr - r) * fog; g = g + (fg - g) * fog; b = b + (fb - b) * fog;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static void emit_tri(V3 a, V3 b, V3 c, u32 rgb, Orient o) {
    V3 n = vcross(vsub(b, a), vsub(c, a));
    float nl = vlen(n);
    if (nl < 1e-9f) return;
    n = vmul(n, 1.0f / nl);
    V3 cen = vmul(vadd(a, vadd(b, c)), 1.0f / 3.0f);
    float side = o.dirMode ? vdot(n, o.v) : vdot(n, vsub(cen, o.v));
    if (side < 0) n = vmul(n, -1.0f);
    V3 toCam = vsub(g_cam, cen);
    if (vdot(n, toCam) <= 0) return;                         /* back-face */
    float z = vdot(vsub(cen, g_cam), g_cf);
    if (z < -3.0f) return;
    float sh = 1.0f;
    if (!g_unlit) {
        float d = vdot(n, g_light);
        if (d < 0) d = 0;
        sh = 0.46f + 0.62f * d;
    }
    float fog = clampf((z - g_fogA) / (g_fogB - g_fogA), 0, 1);
    u32 col = shade_rgb(rgb, sh, fog);
    submit_view(to_view(a), to_view(b), to_view(c), col, z);
}

/* quad a-b-c-d (in loop order), subdivided into <= cell sized pieces */
static void emit_quad(V3 a, V3 b, V3 c, V3 d, u32 rgb, Orient o, float cell) {
    int n1 = (int)ceilf(vlen(vsub(b, a)) / cell); if (n1 < 1) n1 = 1; if (n1 > 14) n1 = 14;
    int n2 = (int)ceilf(vlen(vsub(d, a)) / cell); if (n2 < 1) n2 = 1; if (n2 > 14) n2 = 14;
    for (int j = 0; j < n2; j++) {
        float v0 = (float)j / n2, v1 = (float)(j + 1) / n2;
        for (int i = 0; i < n1; i++) {
            float u0 = (float)i / n1, u1 = (float)(i + 1) / n1;
            V3 p00 = vlerp(vlerp(a, b, u0), vlerp(d, c, u0), v0);
            V3 p10 = vlerp(vlerp(a, b, u1), vlerp(d, c, u1), v0);
            V3 p11 = vlerp(vlerp(a, b, u1), vlerp(d, c, u1), v1);
            V3 p01 = vlerp(vlerp(a, b, u0), vlerp(d, c, u0), v1);
            emit_tri(p00, p10, p11, rgb, o);
            emit_tri(p00, p11, p01, rgb, o);
        }
    }
}

/* oriented box. c = local centre, h = half extents */
static void emit_box(const Xf *x, V3 c, V3 h, u32 rgb, float cell, int skipBottom) {
    #define BP(sx, sy, sz) xf_pt(x, v3(c.x + (sx) * h.x, c.y + (sy) * h.y, c.z + (sz) * h.z))
    Orient o = or_pt(xf_pt(x, c));
    emit_quad(BP(1,-1,-1), BP(1,1,-1), BP(1,1,1), BP(1,-1,1), rgb, o, cell);
    emit_quad(BP(-1,-1,-1), BP(-1,1,-1), BP(-1,1,1), BP(-1,-1,1), rgb, o, cell);
    emit_quad(BP(-1,1,-1), BP(1,1,-1), BP(1,1,1), BP(-1,1,1), rgb, o, cell);
    if (!skipBottom) emit_quad(BP(-1,-1,-1), BP(1,-1,-1), BP(1,-1,1), BP(-1,-1,1), rgb, o, cell);
    emit_quad(BP(-1,-1,1), BP(1,-1,1), BP(1,1,1), BP(-1,1,1), rgb, o, cell);
    emit_quad(BP(-1,-1,-1), BP(1,-1,-1), BP(1,1,-1), BP(-1,1,-1), rgb, o, cell);
    #undef BP
}

/* tapered prism between a and b */
static void emit_prism(V3 a, V3 b, float ra, float rb, int n, u32 rgb) {
    V3 d = vnorm(vsub(b, a));
    if (vlen(d) < 0.5f) return;
    V3 ref = absf_(d.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    V3 e1 = vnorm(vcross(d, ref)), e2 = vcross(d, e1);
    V3 pa[12], pb[12];
    if (n > 12) n = 12;
    for (int i = 0; i < n; i++) {
        float ang = TAU_F * i / n, cs = cosf(ang), sn = sinf(ang);
        V3 dir = vadd(vmul(e1, cs), vmul(e2, sn));
        pa[i] = vadd(a, vmul(dir, ra));
        pb[i] = vadd(b, vmul(dir, rb));
    }
    Orient o = or_pt(vmul(vadd(a, b), 0.5f));
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        emit_tri(pa[i], pa[j], pb[j], rgb, o);
        emit_tri(pa[i], pb[j], pb[i], rgb, o);
        emit_tri(a, pa[i], pa[j], rgb, o);
        emit_tri(b, pb[i], pb[j], rgb, o);
    }
}

/* ellipsoid in a local frame */
static void emit_ell(const Xf *x, V3 lc, V3 rad, u32 rgb, int slices, int stacks) {
    V3 pts[9][13];
    if (slices > 12) slices = 12;
    if (stacks > 8) stacks = 8;
    for (int s = 0; s <= stacks; s++) {
        float ph = PI_F * s / stacks;
        for (int l = 0; l <= slices; l++) {
            float th = TAU_F * l / slices;
            V3 p = v3(lc.x + rad.x * sinf(ph) * cosf(th), lc.y + rad.y * cosf(ph), lc.z + rad.z * sinf(ph) * sinf(th));
            pts[s][l] = xf_pt(x, p);
        }
    }
    Orient o = or_pt(xf_pt(x, lc));
    for (int s = 0; s < stacks; s++)
        for (int l = 0; l < slices; l++) {
            emit_tri(pts[s][l], pts[s + 1][l], pts[s + 1][l + 1], rgb, o);
            emit_tri(pts[s][l], pts[s + 1][l + 1], pts[s][l + 1], rgb, o);
        }
}

/* ------------------------------------------------------------ frame output */
static int cmp_far_first(const void *a, const void *b) {
    float ka = (*(STri *const *)a)->key, kb = (*(STri *const *)b)->key;
    return (ka < kb) - (ka > kb);
}

static inline void draw_stri(const STri *t) {
    u64 c = gfx_col(t->rgb);
    gsKit_prim_triangle_gouraud(g_gs, t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], 1, c, c, c);
}

static void gfx_end_scene(void) {
    for (int i = 0; i < g_nbg; i++) draw_stri(&g_bg[i]);
    for (int i = 0; i < g_nfg; i++) g_ord[i] = &g_fg[i];
    qsort(g_ord, g_nfg, sizeof(STri *), cmp_far_first);
    for (int i = 0; i < g_nfg; i++) draw_stri(g_ord[i]);
}

/* ------------------------------------------------------------------- 2D */
static void gfx_rect(float x0, float y0, float x1, float y1, u32 rgb) {
    gsKit_prim_sprite(g_gs, x0, y0, x1, y1, 2, gfx_col(rgb));
}

static const u8 FONT5X7[59][5] = {
 {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
 {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x05,0x03,0x00,0x00},
 {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
 {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
 {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
 {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
 {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},
 {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},
 {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
 {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
 {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
 {0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
 {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43}
};

static float gfx_text_w(const char *s, float sc) { return (float)strlen(s) * 6.0f * sc; }

static void gfx_text_raw(float x, float y, float sc, u32 rgb, const char *s) {
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 90) c = '?';
        const u8 *g = FONT5X7[c - 32];
        for (int col = 0; col < 5; col++) {
            u8 bits = g[col];
            int row = 0;
            while (row < 7) {
                if (bits & (1 << row)) {
                    int r0 = row;
                    while (row < 7 && (bits & (1 << row))) row++;
                    gfx_rect(x + col * sc, y + r0 * sc, x + (col + 1) * sc, y + row * sc, rgb);
                } else row++;
            }
        }
        x += 6.0f * sc;
    }
}

static void gfx_text(float x, float y, float sc, u32 rgb, const char *s) {
    gfx_text_raw(x + sc, y + sc, sc, 0x000000, s);       /* drop shadow */
    gfx_text_raw(x, y, sc, rgb, s);
}
static void gfx_text_c(float cx, float y, float sc, u32 rgb, const char *s) {
    gfx_text(cx - gfx_text_w(s, sc) * 0.5f, y, sc, rgb, s);
}
#endif
