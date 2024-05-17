/*
Based on PolyLine2D github.com/CrushedPixel/Polyline2D

Copyright © 2019 Marius Metzger (CrushedPixel)
Copyright © 2024 Ryan "rj45" Sanche

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the “Software”), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "polyline.h"
#include "handmade_math.h"
#include "stb_ds.h"

#include "sokol_gfx.h"
#include "sokol_gp.h"

#include <assert.h>
#include <stdbool.h>

typedef struct LineSegment {
  HMM_Vec2 a;
  HMM_Vec2 b;
} LineSegment;

typedef struct PolySegment {
  LineSegment center;
  LineSegment edge1;
  LineSegment edge2;
} PolySegment;

typedef struct PolyLiner {
  float thickness;
  float screenScale;
  JointStyle jointStyle;
  CapStyle capStyle;
  PolySegment *segments;
  HMM_Vec2 pen;
} PolyLiner;

LineSegment line_segment_add(LineSegment seg, HMM_Vec2 toAdd) {
  return (LineSegment){
    .a = HMM_AddV2(seg.a, toAdd),
    .b = HMM_AddV2(seg.b, toAdd),
  };
}

LineSegment line_segment_sub(LineSegment seg, HMM_Vec2 toSub) {
  return (LineSegment){
    .a = HMM_SubV2(seg.a, toSub),
    .b = HMM_SubV2(seg.b, toSub),
  };
}

HMM_Vec2 line_segment_dir_norm(LineSegment seg) {
  return HMM_NormV2(HMM_SubV2(seg.b, seg.a));
}

HMM_Vec2 line_segment_dir(LineSegment seg) { return HMM_SubV2(seg.b, seg.a); }

HMM_Vec2 line_segment_normal(LineSegment seg) {
  HMM_Vec2 dir = line_segment_dir_norm(seg);
  return HMM_V2(-dir.Y, dir.X);
}

float HMM_CrossV2(HMM_Vec2 a, HMM_Vec2 b) { return a.X * b.Y - a.Y * b.X; }

bool line_segment_intersection(
  LineSegment a, LineSegment b, HMM_Vec2 *point, bool infiniteLines) {
  HMM_Vec2 r = line_segment_dir(a);
  HMM_Vec2 s = line_segment_dir(b);

  HMM_Vec2 originDist = HMM_SubV2(b.a, a.a);

  float uNumerator = HMM_CrossV2(originDist, r);
  float denominator = HMM_CrossV2(r, s);

  if (fabsf(denominator) < 0.0001f) {
    // lines are parallel
    return false;
  }

  float u = uNumerator / denominator;
  float t = HMM_CrossV2(originDist, s) / denominator;

  if (!infiniteLines && (t < 0 || t > 1 || u < 0 || u > 1)) {
    // intersection point is outside of the line segments
    return false;
  }

  *point = HMM_AddV2(a.a, HMM_MulV2F(r, t));
  return true;
}

void pl_add_polysegment(PolyLiner *pl, LineSegment center) {
  HMM_Vec2 offset = HMM_MulV2F(line_segment_normal(center), pl->thickness);
  PolySegment seg = {
    .center = center,
    .edge1 = line_segment_add(center, offset),
    .edge2 = line_segment_sub(center, offset),
  };
  arrput(pl->segments, seg);
}

float HMM_AngleV2(HMM_Vec2 a, HMM_Vec2 b) {
  return HMM_ACosF(HMM_DotV2(a, b) / (HMM_LenV2(a) * HMM_LenV2(b)));
}

// min angle for round joint's triangles
// #define ROUND_MIN_ANGLE (0.174533f) // 10 degrees
#define ROUND_MIN_ANGLE (0.349066f) // 20 degrees

#define ROUND_MIN_LENGTH (1.5f)

void pl_create_triangle_fan(
  PolyLiner *pl, HMM_Vec2 connectTo, HMM_Vec2 origin, HMM_Vec2 start,
  HMM_Vec2 end, bool clockwise) {
  HMM_Vec2 point1 = HMM_SubV2(start, origin);
  HMM_Vec2 point2 = HMM_SubV2(end, origin);

  // calculate the angle between the two points
  float angle1 = atan2f(point1.Y, point1.X);
  float angle2 = atan2f(point2.Y, point2.X);

  // ensure the outer angle is calculated
  if (clockwise) {
    if (angle2 > angle1) {
      angle2 = angle2 - 2 * HMM_PI32;
    }
  } else {
    if (angle1 > angle2) {
      angle1 = angle1 - 2 * HMM_PI32;
    }
  }

  float jointAngle = angle2 - angle1;

  // calculate the arc length, pl->thickness is the radius
  float arcLength = pl->thickness * HMM_ABS(jointAngle);

  float arcThresh = ROUND_MIN_LENGTH / pl->screenScale;

  // calculate the amount of triangles to use for the joint
  int numAngleTriangles =
    HMM_MAX(1, (int)floorf(HMM_ABS(jointAngle) / ROUND_MIN_ANGLE));
  int numLengthTriangles = HMM_MAX(1, (int)floorf(arcLength / arcThresh));
  int numTriangles = HMM_MIN(numAngleTriangles, numLengthTriangles);

  // calculate the angle for each triangle
  float triAngle = jointAngle / numTriangles;

  HMM_Vec2 startPoint = start;
  HMM_Vec2 endPoint;
  for (int t = 0; t < numTriangles; t++) {
    if (t == numTriangles - 1) {
      // it's the last triangle - ensure it perfectly
      // connects to the next line
      endPoint = end;
    } else {
      float rot = (t + 1) * triAngle;

      // rotate the original point around the origin
      endPoint.X = cosf(rot) * point1.X - sinf(rot) * point1.Y;
      endPoint.Y = sinf(rot) * point1.X + cosf(rot) * point1.Y;

      // re-add the rotation origin to the target point
      endPoint = HMM_AddV2(endPoint, origin);
    }

    // emit the triangle
    sgp_draw_filled_triangle(
      startPoint.X, startPoint.Y, endPoint.X, endPoint.Y, connectTo.X,
      connectTo.Y);

    startPoint = endPoint;
  }
}

// threshold for miter joints, if the angle is smaller than this, a bevel joint
// will be created instead
#define MITER_MIN_ANGLE (0.349066f) // 20 degrees

void pl_create_joint(
  PolyLiner *pl, PolySegment *segment1, PolySegment *segment2, HMM_Vec2 *end1,
  HMM_Vec2 *end2, HMM_Vec2 *nextStart1, HMM_Vec2 *nextStart2) {
  // calculate angle between two segments
  HMM_Vec2 dir1 = line_segment_dir_norm(segment1->center);
  HMM_Vec2 dir2 = line_segment_dir_norm(segment2->center);

  float angle = HMM_AngleV2(dir1, dir2);

  float wrappedAngle = angle;
  if (wrappedAngle > (HMM_PI32 / 2.0f)) {
    wrappedAngle = HMM_PI32 - wrappedAngle;
  }

  if (pl->jointStyle == LJ_MITER && wrappedAngle < MITER_MIN_ANGLE) {
    pl->jointStyle = LJ_BEVEL;
  }

  if (pl->jointStyle == LJ_MITER) {
    // calculate each edge's intersection point
    HMM_Vec2 sec1, sec2;
    if (line_segment_intersection(
          segment1->edge1, segment2->edge1, &sec1, true)) {
      *end1 = sec1;
    } else {
      *end1 = segment1->edge1.b;
    }
    if (line_segment_intersection(
          segment1->edge2, segment2->edge2, &sec2, true)) {
      *end2 = sec2;
    } else {
      *end2 = segment1->edge2.b;
    }

    *nextStart1 = *end1;
    *nextStart2 = *end2;
  } else {
    // either bevel or round

    // find out which are the inner edges for this joint
    float x1 = dir1.X;
    float x2 = dir2.X;
    float y1 = dir1.Y;
    float y2 = dir2.Y;

    bool clockwise = (x1 * y2 - x2 * y1) < 0;

    LineSegment *outer1, *outer2, *inner1, *inner2;
    if (clockwise) {
      outer1 = &segment1->edge1;
      outer2 = &segment2->edge1;
      inner1 = &segment1->edge2;
      inner2 = &segment2->edge2;
    } else {
      outer1 = &segment1->edge2;
      outer2 = &segment2->edge2;
      inner1 = &segment1->edge1;
      inner2 = &segment2->edge1;
    }

    HMM_Vec2 innerSec;
    bool hasIntersection =
      line_segment_intersection(*inner1, *inner2, &innerSec, false);
    if (!hasIntersection) {
      // for parallel lines, simply connect them directly
      innerSec = inner1->b;
    }

    // if there's no inner intersection, flip
    // the next start position for near-180° turns
    HMM_Vec2 innerStart;
    if (hasIntersection) {
      innerStart = innerSec;
    } else if (angle > HMM_PI32 / 2) {
      innerStart = outer1->b;
    } else {
      innerStart = inner1->b;
    }

    if (clockwise) {
      *end1 = outer1->b;
      *end2 = innerSec;

      *nextStart1 = outer2->a;
      *nextStart2 = innerStart;

    } else {
      *end1 = innerSec;
      *end2 = outer1->b;

      *nextStart1 = innerStart;
      *nextStart2 = outer2->a;
    }

    // connect the intersection points according to the joint style
    if (pl->jointStyle == LJ_BEVEL) {
      // simply connect the intersection points
      sgp_draw_filled_triangle(
        outer1->b.X, outer1->b.Y, outer2->a.X, outer2->a.Y, innerSec.X,
        innerSec.Y);
    } else if (pl->jointStyle == LJ_ROUND) {
      // draw a circle between the ends of the outer edges,
      // centered at the actual point
      // with half the line thickness as the radius

      pl_create_triangle_fan(
        pl, innerSec, segment1->center.b, outer1->b, outer2->a, clockwise);
    } else {
      assert(0);
    }
  }
}

PolyLiner *pl_create() {
  PolyLiner *pl = malloc(sizeof(PolyLiner));
  *pl = (PolyLiner){0};
  pl_reset(pl);
  return pl;
}

void pl_free(PolyLiner *pl) {
  arrfree(pl->segments);
  free(pl);
}

void pl_reset(PolyLiner *pl) {
  pl->screenScale = 1.0f;
  pl->thickness = 0.5f;
  pl->jointStyle = LJ_MITER;
  pl->capStyle = LC_SQUARE;
  arrsetlen(pl->segments, 0);
}

void pl_joint_style(PolyLiner *pl, JointStyle jointStyle) {
  pl->jointStyle = jointStyle;
}

void pl_cap_style(PolyLiner *pl, CapStyle capStyle) { pl->capStyle = capStyle; }

void pl_thickness(PolyLiner *pl, float thickness) {
  // operate on half the thickness to make things easier
  pl->thickness = thickness / 2.0f;
}

void pl_start(PolyLiner *pl, HMM_Vec2 pos) {
  arrsetlen(pl->segments, 0);
  pl->pen = pos;
}

void pl_lineto(PolyLiner *pl, HMM_Vec2 pos) {
  if (pl->pen.X == pos.X && pl->pen.Y == pos.Y) {
    return;
  }
  pl_add_polysegment(pl, (LineSegment){pl->pen, pos});
  pl->pen = pos;
}

void pl_draw_path(PolyLiner *pl, HMM_Vec2 *pts, int numPts) {
  arrsetlen(pl->segments, 0);
  for (int i = 0; (i + 1) < numPts; i++) {
    HMM_Vec2 p0 = pts[i];
    HMM_Vec2 p1 = pts[i + 1];

    if (p0.X == p1.X && p0.Y == p1.Y) {
      continue;
    }

    pl_add_polysegment(pl, (LineSegment){p0, p1});
  }
  pl_finish(pl);
}

void pl_finish(PolyLiner *pl) {
  sgp_mat2x3 xform = sgp_query_state()->transform;
  float scaleX = HMM_LenV2(HMM_V2(xform.v[0][0], xform.v[1][0]));
  float scaleY = HMM_LenV2(HMM_V2(xform.v[0][1], xform.v[1][1]));
  pl->screenScale = (scaleX + scaleY) / 2.0f;

  if (arrlen(pl->segments) == 0) {
    // nothing to draw
    return;
  }

  if (pl->capStyle == LC_JOINT) {
    // close the polyline
    HMM_Vec2 p0 = pl->segments[arrlen(pl->segments) - 1].center.b;
    HMM_Vec2 p1 = pl->segments[0].center.a;
    if (p0.X != p1.X || p0.Y != p1.Y) {
      pl_add_polysegment(pl, (LineSegment){p0, p1});
    }
  }

  HMM_Vec2 nextStart1 = {0};
  HMM_Vec2 nextStart2 = {0};
  HMM_Vec2 start1 = {0};
  HMM_Vec2 start2 = {0};
  HMM_Vec2 end1 = {0};
  HMM_Vec2 end2 = {0};

  PolySegment *firstSegment = &pl->segments[0];
  PolySegment *lastSegment = &pl->segments[arrlen(pl->segments) - 1];

  HMM_Vec2 pathStart1 = firstSegment->edge1.a;
  HMM_Vec2 pathStart2 = firstSegment->edge2.a;
  HMM_Vec2 pathEnd1 = lastSegment->edge1.b;
  HMM_Vec2 pathEnd2 = lastSegment->edge2.b;

  switch (pl->capStyle) {
  case LC_BUTT:
    break;
  case LC_SQUARE:
    // extend the first and last segment by half the thickness
    {
      HMM_Vec2 off1 =
        HMM_MulV2F(line_segment_dir_norm(firstSegment->edge1), pl->thickness);
      HMM_Vec2 off2 =
        HMM_MulV2F(line_segment_dir_norm(firstSegment->edge2), pl->thickness);
      pathStart1 = HMM_SubV2(pathStart1, off1);
      pathStart2 = HMM_SubV2(pathStart2, off2);
      pathEnd1 = HMM_AddV2(pathEnd1, off1);
      pathEnd2 = HMM_AddV2(pathEnd2, off2);
    }
    break;
  case LC_ROUND:
    // create half circle triangle fans at the start and end
    pl_create_triangle_fan(
      pl, firstSegment->center.a, firstSegment->center.a, firstSegment->edge1.a,
      firstSegment->edge2.a, false);
    pl_create_triangle_fan(
      pl, lastSegment->center.b, lastSegment->center.b, lastSegment->edge1.b,
      lastSegment->edge2.b, true);
    break;
  case LC_JOINT:
    // create a joint at the start and end
    pl_create_joint(
      pl, lastSegment, firstSegment, &pathEnd1, &pathEnd2, &pathStart1,
      &pathStart2);
    break;
  }

  // generate mesh data for segments
  for (size_t i = 0; i < arrlen(pl->segments); i++) {
    PolySegment *seg = &pl->segments[i];

    if (i == 0) {
      start1 = pathStart1;
      start2 = pathStart2;
    }

    if (i == (arrlen(pl->segments) - 1)) {
      end1 = pathEnd1;
      end2 = pathEnd2;
    } else {
      pl_create_joint(
        pl, seg, &pl->segments[i + 1], &end1, &end2, &nextStart1, &nextStart2);
    }

    sgp_draw_filled_triangle(
      start1.X, start1.Y, start2.X, start2.Y, end1.X, end1.Y);
    sgp_draw_filled_triangle(
      end1.X, end1.Y, start2.X, start2.Y, end2.X, end2.Y);

    start1 = nextStart1;
    start2 = nextStart2;
  }

  return;
}
