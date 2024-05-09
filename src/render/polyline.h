/*
Based on PolyLine2D github.com/CrushedPixel/Polyline2D

Copyright © 2019 Marius Metzger (CrushedPixel)
Copyright © 2019 Ryan "rj45" Sanche

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

#ifndef POLYLINE_H
#define POLYLINE_H

#include <stdbool.h>

#include "handmade_math.h"

typedef enum JointStyle {
  LJ_MITER, // cornered or sharp joint
  LJ_BEVEL, // flat joint
  LJ_ROUND, // rounded joint
} JointStyle;

typedef enum CapStyle {
  LC_BUTT,   // No cap
  LC_SQUARE, // Squared off cap
  LC_ROUND,  // Rounded cap
  LC_JOINT,  // Join end to beginning (close the loop)
} CapStyle;

typedef struct PolyLiner PolyLiner;

PolyLiner *pl_create();
void pl_free(PolyLiner *pl);

// reset settings back to defaults
void pl_reset(PolyLiner *pl);

// set joint style
void pl_joint_style(PolyLiner *pl, JointStyle js);

// set cap style
void pl_cap_style(PolyLiner *pl, CapStyle cs);

// set the thickness of the line
void pl_thickness(PolyLiner *pl, float thickness);

// draw a line point by point
void pl_start(PolyLiner *pl, HMM_Vec2 pos);
void pl_lineto(PolyLiner *pl, HMM_Vec2 pos);
void pl_finish(PolyLiner *pl);

// draw a polyline all at once
void pl_draw_lines(PolyLiner *pl, HMM_Vec2 *pts, int numPts);

#endif // POLYLINE_H