/* math3d.h - tiny vector / transform helpers (single translation unit) */
#ifndef MATH3D_H
#define MATH3D_H
#include <math.h>

typedef struct { float x, y, z; } V3;
typedef struct { V3 o, r, u, f; } Xf;      /* origin + right/up/forward basis */

#define PI_F  3.14159265f
#define TAU_F 6.28318531f

static inline V3 v3(float x, float y, float z) { V3 v; v.x = x; v.y = y; v.z = z; return v; }
static inline V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 vsub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 vmul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vcross(V3 a, V3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float vlen(V3 a) { return sqrtf(vdot(a, a)); }
static inline V3 vnorm(V3 a) {
    float l = vlen(a);
    if (l < 1e-7f) return v3(0, 0, 0);
    return vmul(a, 1.0f / l);
}
static inline V3 vlerp(V3 a, V3 b, float t) { return vadd(a, vmul(vsub(b, a), t)); }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float absf_(float a) { return a < 0 ? -a : a; }
static inline float wrapPi(float a) {
    while (a > PI_F) a -= TAU_F;
    while (a < -PI_F) a += TAU_F;
    return a;
}
static inline float approachf(float v, float target, float step) {
    if (v < target) { v += step; if (v > target) v = target; }
    else            { v -= step; if (v < target) v = target; }
    return v;
}
static inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* rotations used to build bases */
static void rotZ_(V3 *v, float a) { float c = cosf(a), s = sinf(a); float x = v->x * c - v->y * s, y = v->x * s + v->y * c; v->x = x; v->y = y; }
static void rotX_(V3 *v, float a) { float c = cosf(a), s = sinf(a); float y = v->y * c - v->z * s, z = v->y * s + v->z * c; v->y = y; v->z = z; }
static void rotY_(V3 *v, float a) { float c = cosf(a), s = sinf(a); float x = v->x * c + v->z * s, z = -v->x * s + v->z * c; v->x = x; v->z = z; }

/* yaw: 0 faces +Z, positive turns toward +X.  pitch>0 = nose DOWN / lean forward.  roll = spin about forward axis */
static Xf xf_euler(V3 o, float yaw, float pitch, float roll) {
    Xf x; x.o = o; x.r = v3(1, 0, 0); x.u = v3(0, 1, 0); x.f = v3(0, 0, 1);
    rotZ_(&x.r, roll); rotZ_(&x.u, roll); rotZ_(&x.f, roll);
    rotX_(&x.r, pitch); rotX_(&x.u, pitch); rotX_(&x.f, pitch);
    rotY_(&x.r, yaw); rotY_(&x.u, yaw); rotY_(&x.f, yaw);
    return x;
}
static inline V3 xf_pt(const Xf *x, V3 p) {
    return vadd(x->o, vadd(vmul(x->r, p.x), vadd(vmul(x->u, p.y), vmul(x->f, p.z))));
}
static inline V3 xf_dir(const Xf *x, V3 d) {
    return vadd(vmul(x->r, d.x), vadd(vmul(x->u, d.y), vmul(x->f, d.z)));
}
static Xf xf_mul(const Xf *parent, const Xf *local) {
    Xf o;
    o.o = xf_pt(parent, local->o);
    o.r = xf_dir(parent, local->r);
    o.u = xf_dir(parent, local->u);
    o.f = xf_dir(parent, local->f);
    return o;
}
#endif
