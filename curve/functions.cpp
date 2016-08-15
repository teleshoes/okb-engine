#include "functions.h"

#include <QString>
#include <QRegExp>
#include <cmath>
#include <math.h>

/* --- common functions --- */
// NIH all over the place

float distance(float x1, float y1, float x2, float y2) {
  return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}
float distancep(const Point &p1, const Point &p2) {
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

float anglep(const Point &p1, const Point &p2) {
  return angle(p1.x, p1.y, p2.x, p2.y);
}

float dist_line_point(const Point &p1, const Point &p2, const Point &p) {
  float lp = distancep(p1, p2);

  float u = 1.0 * ((p.x - p1.x) * (p2.x - p1.x) + (p.y - p1.y) * (p2.y - p1.y)) / lp / lp;

  float projx = p1.x + u * (p2.x - p1.x);
  float projy = p1.y + u * (p2.y - p1.y);

  float result = distance(projx, projy, p.x, p.y);

  return result;
}


static float _surf0(const Point &p1, const Point &p2) {
  return (p1.y + p2.y) * (p1.x - p2.x) / 2.0;
}

float surface4(const Point &p1, const Point &p2, const Point &p3, const Point &p4) {
  float surf = abs(_surf0(p1, p2) +
		   _surf0(p2, p3) +
		   _surf0(p3, p4) +
		   _surf0(p4, p1));

  return surf;
}

float response2(float value, float middle) {
  if (value <= 0 || value >= 1) { return value; }

  return response(pow(value, log(middle) / log(0.5)));
}

float response(float value) {
  if (value <= 0 || value >= 1) { return value; }

  return (1 - cos(value * M_PI)) / 2;
}

// convert a key caption (possibly with diacritics) and return the matching ASCII letter (without diacritics)
// or return 0 if the String does not correspond to a letter
unsigned char caption2letter(QString value) {
  if (value.length() != 1) { return 0; }

  // https://stackoverflow.com/questions/12278448/removing-accents-from-a-qstring
  QString stringNormalized = value.normalized(QString::NormalizationForm_KD);
  stringNormalized.remove(QRegExp("[^a-zA-Z]"));
  return stringNormalized.toLower().at(0).cell();
}
