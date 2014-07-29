#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "curve_match.h"

float distance(float x1, float y1, float x2, float y2);
float distancep(Point &p1, Point &p2);
float cos_angle(float x1, float y1, float x2, float y2);
float sin_angle(float x1, float y1, float x2, float y2);
float angle(float x1, float y1, float x2, float y2);
float anglep(Point p1, Point p2);
float dist_line_point(Point p1, Point p2, Point p);

#endif /* FUNCTIONS_H */
