#include "kb_distort.h"

#include <math.h>

void kb_distort_cancel(QHash<QString, Key> &keys) {
  QHash<QString, Key>::iterator it = keys.begin();
  for (it = keys.begin(); it != keys.end(); ++ it) {
    it.value().corrected_x = it.value().corrected_y = -1;
  }
}

void kb_distort(QHash<QString, Key> &keys, Params &params) {
  int minx = -1;
  int miny = -1;
  int maxx = -1;
  int maxy = -1;

  QHash<QString, Key>::iterator it = keys.begin();
  for (it = keys.begin(); it != keys.end(); ++ it) {
    Key key = it.value();
    if (key.x < minx || minx == -1) { minx = key.x; }
    if (key.x > maxx || maxx == -1) { maxx = key.x; }
    if (key.y < miny || miny == -1) { miny = key.y; }
    if (key.y > maxy || maxy == -1) { maxy = key.y; }
  }

  for (it = keys.begin(); it != keys.end(); ++ it) {
    int x = it.value().x;
    int y = it.value().y;

    if (x < minx + params.dst_x_max) {
      x +=  1.0 * params.dst_x_add * (params.dst_x_max + minx - x) / params.dst_x_max;
    }
    if (x > maxx - params.dst_x_max) {
      x -=  1.0 * params.dst_x_add * (x - maxx + params.dst_x_max) / params.dst_x_max;
    }

    if (y == miny && x < minx + params.dst_y_max) {
      y += 1.0 * params.dst_y_add * (params.dst_y_max + minx - x) / params.dst_y_max;
    }
    if (y == miny && x > maxx - params.dst_y_max) {
      y +=  1.0 * params.dst_y_add * (x - maxx + params.dst_y_max) / params.dst_y_max;
    }


    it.value().corrected_x = x;
    it.value().corrected_y = y;
  }
}
