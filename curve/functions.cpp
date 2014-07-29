#include "functions.h"

#include <cmath> 
#include <math.h>

/* --- common functions --- */
// NIH all over the place

float distance(float x1, float y1, float x2, float y2) {
  return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}
float distancep(Point &p1, Point &p2) {
  return distance(p1.x, p1.y, p2.x, p2.y);
}

float cos_angle(float x1, float y1, float x2, float y2) {
  return (x1 * x2 + y1 * y2) / (sqrt(pow(x1, 2) + pow(y1, 2)) * sqrt(pow(x2, 2) + pow(y2, 2)));
}

float sin_angle(float x1, float y1, float x2, float y2) {
  return (x1 * y2 - x2 * y1) / (sqrt(pow(x1, 2) + pow(y1, 2)) * sqrt(pow(x2, 2) + pow(y2, 2)));
}

float angle(float x1, float y1, float x2, float y2) {
  float cosa = cos_angle(x1, y1, x2, y2);
  float value;
  if (cosa > 1) {
    value = 0;
  } else if (cosa < -1) {
    value = M_PI;
  } else {
    value = acos(cosa); // for rounding errors
  }
  if (x1*y2 - x2*y1 < 0) { value = -value; }
  return value;
}

float anglep(Point p1, Point p2) {
  return angle(p1.x, p1.y, p2.x, p2.y);
}

float dist_line_point(Point p1, Point p2, Point p) {
  float lp = distancep(p1, p2);
  
  float u = 1.0 * ((p.x - p1.x) * (p2.x - p1.x) + (p.y - p1.y) * (p2.y - p1.y)) / lp / lp;
  Point proj = Point(p1.x + u * (p2.x - p1.x), p1.y + u * (p2.y - p1.y));

  float result = distancep(proj, p);
  return result;
}    
