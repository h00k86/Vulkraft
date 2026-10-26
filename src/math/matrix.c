#include "math/matrix.h"
#include <string.h>

mat4 mat4_identity() {
  mat4 res = {0};
  res.m[0] = 1.0f;
  res.m[5] = 1.0f;
  res.m[10] = 1.0f;
  res.m[15] = 1.0f;
  return res;
}

mat4 mat4_ortho(float left, float right, float bottom, float top, float near,
                float far) {
  mat4 res = mat4_identity();
  res.m[0] = 2.0f / (right - left);
  res.m[5] = 2.0f / (top - bottom);
  res.m[10] = -2.0f / (far - near);
  res.m[12] = -(right + left) / (right - left);
  res.m[13] = -(top + bottom) / (top - bottom);
  res.m[14] = -(far + near) / (far - near);
  return res;
}

mat4 mat4_ortho_world_z(float left, float right, float bottom, float top,
                        float zBack, float zFront) {
  mat4 res = mat4_identity();
  res.m[0] = 2.0f / (right - left);
  res.m[5] = 2.0f / (top - bottom);
  // depth = (zFront - z) / (zFront - zBack): z=zFront -> 0 (davanti),
  // z=zBack -> 1 (dietro). Vedi matrix.h.
  res.m[10] = -1.0f / (zFront - zBack);
  res.m[12] = -(right + left) / (right - left);
  res.m[13] = -(top + bottom) / (top - bottom);
  res.m[14] = zFront / (zFront - zBack);
  return res;
}

mat4 mat4_translate(mat4 m, float x, float y, float z) {
  mat4 res = m;
  res.m[12] += x;
  res.m[13] += y;
  res.m[14] += z;
  return res;
}

mat4 mat4_scale_around(float zoom, float centerX, float centerY) {
  // zoom > 1.0 deve far vedere PIÙ mondo (zoom out) — le distanze dal
  // centro vanno DIVISE per zoom (compresse verso il centro), non
  // moltiplicate. Il fattore di scala reale applicato ai vertici è
  // quindi 1/zoom, non zoom direttamente — prima era invertito: zoom=3
  // moltiplicava le distanze per 3 invece di dividerle, risultato
  // esattamente opposto (si avvicinava invece di allontanarsi).
  float s = 1.0f / zoom;
  mat4 res = mat4_identity();
  res.m[0] = s;
  res.m[5] = s;
  res.m[12] = centerX * (1.0f - s);
  res.m[13] = centerY * (1.0f - s);
  return res;
}

/* ---- MACROTASK Z2 ---- */
#include <math.h>

mat4 mat4_multiply(mat4 a, mat4 b) {
  mat4 r;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k)
        sum += a.m[k * 4 + row] * b.m[col * 4 + k];
      r.m[col * 4 + row] = sum;
    }
  }
  return r;
}

mat4 mat4_translation(float x, float y, float z) {
  mat4 r = mat4_identity();
  r.m[12] = x;
  r.m[13] = y;
  r.m[14] = z;
  return r;
}

mat4 mat4_scaling(float x, float y, float z) {
  mat4 r = mat4_identity();
  r.m[0] = x;
  r.m[5] = y;
  r.m[10] = z;
  return r;
}

static float deg_to_rad(float degrees) {
  return degrees * (3.14159265358979323846f / 180.0f);
}

mat4 mat4_rotation_x(float degrees) {
  float c = cosf(deg_to_rad(degrees)), s = sinf(deg_to_rad(degrees));
  mat4 r = mat4_identity();
  r.m[5] = c;
  r.m[6] = s;  /* col 1, row 2 */
  r.m[9] = -s; /* col 2, row 1 */
  r.m[10] = c;
  return r;
}

mat4 mat4_rotation_y(float degrees) {
  float c = cosf(deg_to_rad(degrees)), s = sinf(deg_to_rad(degrees));
  mat4 r = mat4_identity();
  r.m[0] = c;
  r.m[2] = -s; /* col 0, row 2 */
  r.m[8] = s;  /* col 2, row 0 */
  r.m[10] = c;
  return r;
}

mat4 mat4_rotation_z(float degrees) {
  float c = cosf(deg_to_rad(degrees)), s = sinf(deg_to_rad(degrees));
  mat4 r = mat4_identity();
  r.m[0] = c;
  r.m[1] = s;  /* col 0, row 1 */
  r.m[4] = -s; /* col 1, row 0 */
  r.m[5] = c;
  return r;
}

void mat4_transform_point(const mat4 *m, const float in[3], float out[3]) {
  float x = in[0], y = in[1], z = in[2];
  out[0] = m->m[0] * x + m->m[4] * y + m->m[8] * z + m->m[12];
  out[1] = m->m[1] * x + m->m[5] * y + m->m[9] * z + m->m[13];
  out[2] = m->m[2] * x + m->m[6] * y + m->m[10] * z + m->m[14];
}
