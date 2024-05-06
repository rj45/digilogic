#include <assert.h>

#include "sokol_gfx.h"
#include "sokol_gp.h"

static inline sgp_vec2 mat2x3_vec2_mul(const sgp_mat2x3 *m, const sgp_vec2 *v) {
  sgp_vec2 u = {
    m->v[0][0] * v->x + m->v[0][1] * v->y + m->v[0][2],
    m->v[1][0] * v->x + m->v[1][1] * v->y + m->v[1][2]};
  return u;
}

static inline float mat2x3_det(const sgp_mat2x3 *m) {
  return m->v[0][0] * m->v[1][1] - m->v[0][1] * m->v[1][0];
}

static inline sgp_mat2x3 mat2x3_invert(const sgp_mat2x3 *m) {
  float det = mat2x3_det(m);
  assert(det != 0.0f); // "sgp_mat2x3_invert: matrix is not invertible"

  sgp_mat2x3 inv = {
    .v =
      {
        {
          m->v[1][1] / det,
          -m->v[0][1] / det,
          (-m->v[1][1] * m->v[0][2] + m->v[0][1] * m->v[1][2]) / det,
        },
        {
          -m->v[1][0] / det,
          m->v[0][0] / det,
          (m->v[1][0] * m->v[0][2] - m->v[0][0] * m->v[1][2]) / det,
        },
      },
  };

  return inv;
}