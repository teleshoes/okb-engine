#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "curve_match.h"

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

float distance(float x1, float y1, float x2, float y2);
float distancep(const Point &p1, const Point &p2);
float cos_angle(float x1, float y1, float x2, float y2);
float sin_angle(float x1, float y1, float x2, float y2);
float angle(float x1, float y1, float x2, float y2);
float anglep(const Point &p1, const Point &p2);
float dist_line_point(const Point &p1, const Point &p2, const Point &p);
float surface4(const Point &p1, const Point &p2, const Point &p3, const Point &p4);

float response2(float value, float middle);
float response(float value);

#endif /* FUNCTIONS_H */
