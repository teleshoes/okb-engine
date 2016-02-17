#include "curve_match.h"
#include "score.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTextStream>
#include <QStringList>
#include <cmath>
#include <iostream>
#include <math.h>
#include <string.h>
#include <stdio.h> // for rename()

#include "functions.h"

#include "params.h"

#include "kb_distort.h"

#define BUILD_TS (char*) (__DATE__ " " __TIME__)

#define MISC_ACCT(word, coef_name, coef_value, value) { if (abs(value) > 1E-5) { DBG("     [*] MISC %s %s %f %f", (word), (coef_name), (float) (coef_value), (float) (value)) } }

using namespace std;


template <typename T>
QString qlist2str(QList<T> lst) {
  QString str;
  if (lst.size() == 0) {
    str += "empty";
  } else {
    for(int i = 0; i < lst.size(); i++) {
      QTextStream(& str) << lst[i];
      if(i < lst.size()-1) { str +=  ","; }
    }
  }
  return str;
}

QString qlist_nextindex2str(QList<NextIndex> lst) {
  QString str;
  if (lst.size() == 0) {
    str += "empty";
  } else {
    for(int i = 0; i < lst.size(); i++) {
      QTextStream(& str) << lst[i].index << "[" << lst[i].score << "]";
      if(i < lst.size()-1) { str +=  ","; }
    }
  }
  return str;
}

/* --- optimized curve --- */
QuickCurve::QuickCurve() {
  count = -1;
}

QuickCurve::QuickCurve(QList<CurvePoint> &curve, int curve_id, int min_length) {
  count = -1;
  setCurve(curve, curve_id, min_length);
}

void QuickCurve::clearCurve() {
  if (count > 0) {
    delete[] x;
    delete[] y;
    delete[] turn;
    delete[] sharpturn;
    delete[] turnsmooth;
    delete[] normalx;
    delete[] normaly;
    delete[] speed;
    delete[] points;
    delete[] timestamp;
    delete[] length;
  }
  count = -1;
}

void QuickCurve::setCurve(QList<CurvePoint> &curve, int curve_id, int min_length) {
  clearCurve();
  finished = false;
  isDot = false;
  straight = -1;

  int cs = curve.size();
  if (! cs) { count = 0; return; }

  x = new int[cs];
  y = new int[cs];
  turn = new int[cs];
  turnsmooth = new int[cs];
  sharpturn = new int[cs];
  normalx = new int[cs];
  normaly = new int[cs];
  speed = new int[cs];
  points = new Point[cs];
  timestamp = new int[cs];
  length = new int[cs];

  int l = 0;
  int j = 0;
  for (int i = 0; i < cs; i++) {
    CurvePoint p = curve[i];
    if (p.curve_id != curve_id) { continue; }
    if (p.end_marker) { finished = true; break; }

    x[j] = p.x;
    y[j] = p.y;
    turn[j] = p.turn_angle;
    turnsmooth[j] = p.turn_smooth;
    sharpturn[j] = p.sharp_turn;
    normalx[j] = p.normalx;
    normaly[j] = p.normaly;
    speed[j] = p.speed;
    points[j].x = p.x;
    points[j].y = p.y;
    timestamp[j] = p.t;
    length[j] = l;

    if (j > 0) {
      l += distance(x[j - 1], y[j - 1], x[j], y[j]);
      length[j] = l;
    }

    j ++;
  }
  count = j;

  if (l <= min_length) {
    count = 1;
    isDot = true;
  }
}

QuickCurve::~QuickCurve() {
  clearCurve();
}

Point const& QuickCurve::point(int index) const { return points[index]; /* Point(x[index], y[index]);*/ }
int QuickCurve::getX(int index) { return x[index]; }
int QuickCurve::getY(int index) { return y[index]; }
int QuickCurve::getTurn(int index) { return turn[index]; }
int QuickCurve::getTurnSmooth(int index) { return turnsmooth[index]; }
int QuickCurve::getSharpTurn(int index) { int st = sharpturn[index]; return (st >= 3)?0:st; }
int QuickCurve::getSpecialPoint(int index) { return sharpturn[index]; }
Point QuickCurve::getNormal(int index) { return Point(normalx[index], normaly[index]); }
int QuickCurve::size() { return count; }
int QuickCurve::getNormalX(int index) { return normalx[index]; }
int QuickCurve::getNormalY(int index) { return normaly[index]; }
int QuickCurve::getSpeed(int index) { return speed[index]; }
int QuickCurve::getLength(int index) { return length[index]; }
int QuickCurve::getTimestamp(int index) { return timestamp[index]; }
int QuickCurve::getTotalLength() { return (count > 0)?length[count - 1]:0; }

/* optimized keys information */
QuickKeys::QuickKeys() {
  points = new Point[256];  // unsigned char won't overflow
  dim = new Point[256];
  points_raw = new Point[256];
}

QuickKeys::QuickKeys(QHash<unsigned char, Key> &keys) {
  points = new Point[256];
  dim = new Point[256];
  points_raw = new Point[256];
  setKeys(keys);
}

void QuickKeys::setKeys(QHash<unsigned char, Key> &keys) {
  foreach(unsigned char letter, keys.keys()) {
    points_raw[letter].x = keys[letter].x;
    points_raw[letter].y = keys[letter].y;
    if (keys[letter].corrected_x == -1) {
      points[letter].x = keys[letter].x;
      points[letter].y = keys[letter].y;
    } else {
      points[letter].x = keys[letter].corrected_x;
      points[letter].y = keys[letter].corrected_y;
    }
    dim[letter].x = keys[letter].height;
    dim[letter].y = keys[letter].width;
  }
}

QuickKeys::~QuickKeys() {
  delete[] points;
  delete[] dim;
  delete[] points_raw;
}

Point const& QuickKeys::get(unsigned char letter) const {
  return points[letter];
}

Point const& QuickKeys::get_raw(unsigned char letter) const {
  return points_raw[letter];
}

Point const& QuickKeys::size(unsigned char letter) const {
  return dim[letter];
}


/* --- point --- */
Point::Point() {
  this -> x = this -> y = 0;
}

Point::Point(int x, int y) {
  this -> x = x;
  this -> y = y;
}

Point const Point::operator-(const Point &other) const {
  return Point(x - other.x, y - other.y);
}

Point const Point::operator+(const Point &other) const {
  return Point(x + other.x, y + other.y);
}

Point const Point::operator*(const float &other) const {
  return Point(x * other, y * other);
}

CurvePoint::CurvePoint(Point p, int curve_id, int t, int l, bool dummy) : Point(p.x, p.y) {
  this -> t = t;
  this -> speed = 1;
  this -> sharp_turn = false;
  this -> turn_angle = 0;
  this -> turn_smooth = 0;
  this -> normalx = this -> normaly = 0;
  this -> length = l;
  this -> dummy = dummy;
  this -> curve_id = curve_id;
  this -> end_marker = false;
  this -> d2x = 0;
  this -> d2y = 0;
  this -> lac = 0;
}

bool CurvePoint::operator<(const CurvePoint &other) const {
  return this->t < other.t;
}

void CurvePoint::toJson(QJsonObject &json) const {
  json["curve_id"] = curve_id;
  if (end_marker) {
    json["end_marker"] = 1;
  } else {
    json["x"] = x;
    json["y"] = y;
    json["t"] = t;
    json["d2x"] = d2x;
    json["d2y"] = d2y;
    json["lac"] = lac;
    json["speed"] = speed;
    json["turn_angle"] = turn_angle;
    json["turn_smooth"] = turn_smooth;
    json["sharp_turn"] = sharp_turn;
    if (normalx || normaly) {
      json["normalx" ] = normalx;
      json["normaly" ] = normaly;
    }
    if (dummy) { json["dummy"] = 1; }
  }
}

CurvePoint CurvePoint::fromJson(const QJsonObject &json) {
  CurvePoint p(Point(), json["curve_id"].toDouble(), json["t"].toDouble());

  // these attributes are only output attributes and will be recomputed after loading curves from a JSON string
  p.speed = p.length = p.turn_angle = p.sharp_turn = p.normalx = p.normaly = 0;

  p.curve_id = json["curve_id"].toDouble();
  p.end_marker = json["end_marker"].toDouble();
  if (p.end_marker) { p.x = p.y = p.t = p.dummy = 0; return p; }

  p.x = json["x"].toDouble();
  p.y = json["y"].toDouble();
  p.t = json["t"].toDouble();
  p.dummy = json["dummy"].toDouble();
  return p;
}

/* --- key --- */
Key::Key(int x, int y, int width, int height, char label) {
  this -> x = x;
  this -> y = y;
  this -> width = width;
  this -> height = height;
  this -> label = label;
  this -> corrected_x = this -> corrected_y = -1;
}

Key::Key() {
  // needed by QHash, but who cares ?
}

void Key::toJson(QJsonObject &json) const {
  json["x"] = x;
  json["y"] = y;
  json["w"] = width;
  json["h"] = height;
  json["k"] = QString(label);
  if (corrected_x != -1) {
    json["corrected_x"] = corrected_x;
    json["corrected_y"] = corrected_y;
  }
}

Key Key::fromJson(const QJsonObject &json) {
  Key key;
  key.x = json["x"].toDouble(); // QT too old, no toInt() yet ?
  key.y = json["y"].toDouble();
  key.width = json["w"].toDouble();
  key.height = json["h"].toDouble();
  key.label = QSTRING2PCHAR(json["k"].toString())[0];
  key.corrected_x = key.corrected_y = -1;
  return key;
}

/* --- scenario --- */
static int RT_DIRECTION_UNKNOWN = 99;

Scenario::Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curve, Params *params) {
  if (tree) { this -> node = tree -> getRoot(); } // @todo remove
  this -> keys = keys;
  this -> curve = curve;
  finished = false;
  count = 0;
  index = 0;
  this -> params = params;
  temp_score = 0;
  final_score = -1;
  score_v1 = -1;
  last_fork = -1;
  result_class = -1;
  star = false;
  error_count = 0;
  min_total = 0;
  good_count = 0;

  dist = dist_sqr = 0;

  index_history = new unsigned char[1];
  letter_history = new unsigned char[1];
  scores = new score_t[1];

  letter_history[0] = 0;

  cache = false;

  new_dist = -1;
}

Scenario::Scenario(const Scenario &from) {
  // overriden copy to take care of dynamically allocated stuff
  copy_from(from);
}

Scenario& Scenario::operator=(const Scenario &from) {
  // overriden copy to take care of dynamically allocated stuff
  delete[] index_history;
  delete[] letter_history;
  delete[] scores;
  copy_from(from);
  return *this;
}

void Scenario::copy_from(const Scenario &from) {
  node = from.node;
  finished = from.finished;
  count = from.count;
  index = from.index;
  temp_score = from.temp_score;
  final_score = from.final_score;
  score_v1 = from.score_v1;
  last_fork = from.last_fork;
  debug = from.debug;
  min_total = from.min_total;

  keys = from.keys;
  curve = from.curve;
  params = from.params;

  result_class = from.result_class;
  star = from.star;
  error_count = from.error_count;

  dist = from.dist;
  dist_sqr = from.dist_sqr;

  index_history = new unsigned char[count + 1];
  letter_history = new unsigned char[count + 2]; // one more char to allow to keep a '\0' at the end (for use for display as a char*)
  scores = new score_t[count + 1];
  for (int i = 0; i < count; i ++) {
    index_history[i] = from.index_history[i];
    letter_history[i] = from.letter_history[i];
    scores[i] = from.scores[i];
  }
  letter_history[count] = 0;

  if (final_score != -1) {
    avg_score = from.avg_score;
    min_score = from.min_score;
  }

  good_count = from.good_count;

  cache = from.cache;
  if (cache) {
    cacheChilds = from.cacheChilds;
  }

  new_dist = from.new_dist;
}


Scenario::~Scenario() {
  delete[] index_history;
  delete[] letter_history;
  delete[] scores;
}

float Scenario::calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index) {

  Point k1 = keys->get(prev_letter);
  Point k2 = keys->get(letter);
  Point p1 = curve->point(index);
  Point p2 = curve->point(new_index);

  if (index == new_index) {
    return 0;
  }

  float a_sin = abs(sin_angle(k2.x - k1.x, k2.y - k1.y, p2.x - p1.x, p2.y - p1.y));

  int max_gap = params -> cos_max_gap;
  float max_sin = sin(params -> max_angle * M_PI / 180);

  float len = distancep(k1, k2);
  float gap = len * a_sin;
  float score;
  float coef_sin = min(1.0, distancep(p1, p2) / params->curve_score_min_dist / 2.0);
  if ((k2.x - k1.x)*(p2.x - p1.x) + (k2.y - k1.y)*(p2.y - p1.y) < 0) {
    score = -1;
  } else {
    score = 1 - max(gap / max_gap, coef_sin * a_sin / max_sin);
  }

  DBG("  [cos score] %s:%c i=%d:%d angle=%.2f/%.2f(%.2f) gap=%d/%d -> score=%.2f",
      getNameCharPtr(), letter, index, new_index, a_sin, max_sin, coef_sin, (int) gap, (int) max_gap, score);

  return score;
}


Point Scenario::computed_curve_tangent(int index) {
  int i1 = index_history[index];
  Point d1, d2;
  Point origin(0, 0);
  if (index > 0) {
    int i0 = index_history[index - 1];
    if (i0 < i1) {
      d1 = curve->point(i1) - curve->point(i0);
      d1 = d1 * (1000 / distancep(origin, d1)); // using integers was clearly a mistake
    }
  }
  if (index < count - 1) {
    int i2 = index_history[index + 1];
    if (i2 > i1) {
      d2 = curve->point(i2) - curve->point(i1);
      d2 = d2 * (1000 / distancep(origin, d2));
    }
  }
  return d1 + d2;
}

Point Scenario::actual_curve_tangent(int i) {
  // this does not work well on sharp turns
  if (i == 0) { i++; }
  if (i == curve->size() - 1) { i--; }
  return curve->point(i + 1) - curve->point(i - 1);
}


float Scenario::calc_curve_score(unsigned char prev_letter, unsigned char letter, int index, int new_index) {
  /* score based on maximum distance from straight path */

  Point pbegin = keys->get(prev_letter);
  Point pend = keys->get(letter);

  Point ptbegin = curve->point(index);
  Point ptend = curve->point(new_index);
  float surface = surface4(pbegin, ptbegin, ptend, pend);

  float max_dist = 0;
  float total_dist = 0;
  int sharp_turn = 0;
  int c = 0;
  for (int i = index + 2; i < new_index - 1; i += 4) {
    Point p = curve->point(i);
    float dist = dist_line_point(ptbegin, ptend, p);

    if (dist > max_dist) { max_dist = dist; }
    total_dist += dist;
    c++;

  }

  for (int i = index + 2; i < new_index - 2; i += 1) {
    if (curve->getSharpTurn(i)) { sharp_turn ++; }
  }

  float scale = params->curve_dist_threshold * min(1, 0.5 + distancep(pbegin, pend) / params->curve_dist_threshold / 4.0);
  float s1 = pow(max(max_dist, 2 * c?total_dist / c:0) / scale, 2);
  float s2 = params->curve_surface_coef * surface / 1E6;
  float s3 = params->sharp_turn_penalty * sharp_turn;

  float score = 1 - s1 - s2 - s3;
  DBG("  [curve score] %s:%c[%d->%d] sc=[%.3f:%.3f:%.3f] max_dist=%d surface=%d score=%.3f",
      getNameCharPtr(), letter, index, new_index, s1, s2, s3, (int) max_dist, (int) surface, score);

  return score;
}


float Scenario::calc_distance_score(unsigned char letter, int index, int count, float *return_distance) {
  /* score based on distance to from curve to key */

  float ratio = (count > 0)?params->dist_max_next:params->dist_max_start;
  Point k = keys->get(letter);

  float cplus;
  float cminus;
  float dx = 0, dy = 0;

  cplus = cminus = 1 / params->anisotropy_ratio;

  /* accept a bit of user laziness in curve beginning, end and sharp turns -> let's add some anisotropy */
  if (count == 0) {
    dx = curve->getX(1) - curve->getX(0);
    dy = curve->getY(1) - curve->getY(0);
  } else if (count == -1) {
    int idx = curve->size() - 1;
    dx = curve->getX(idx) - curve->getX(idx-1);
    dy = curve->getY(idx) - curve->getY(idx-1);
  } else if (curve->getSharpTurn(index)) {
    dx = curve->getNormalX(index);
    dy = curve->getNormalY(index); // why did i use integers ?!
    cminus = 1;
  } else { // unremarkable point
    cplus = cminus = 0; // use standard formula
  }

  float px = k.x - curve->getX(index);
  float py = k.y - curve->getY(index);

  float dist;
  float u = 0;
  float v = 0;
  if (cplus == 0 && cminus == 0) {
    // good old standard formula for distance
    dist = sqrt(px * px + py * py) / ratio;

  } else {
    // distance with a bit of anisotropy
    float d = sqrt(dx * dx + dy * dy);
    u = (px * dx + py * dy) / d;
    v = (px * dy - py * dx) / d;

    dist = sqrt(pow(u * (u > 0?cplus:cminus) / ratio, 2) +
		pow(v / ratio, 2));

    if (dist < 1 && count <= 0) {
      // for first and last point, be more tolerant if user draws a shorter or longer curve
      // (only for ranking, no impact on filtering bc "if (dist < 1)"
      dist = sqrt(pow(u * (u > 0?cplus:cminus) / ratio / 2, 2) +
		  pow(v / ratio, 2));

    }
  }

  /* score based on distance to from curve to key */
  float score = 1 - dist;

  /* return corrected distance */
  if (return_distance) {
    *return_distance = ratio * dist;
  }

  // @todo add a "more verbose" debug option
  //DBG("    distance_score[%c:%d] offset=%d:%d direction=%d:%d u=%.3f v=%.3f coefs=%.2f:%.2f dist=%.3f score=%.3f",
  //    letter, index, (int) px, (int) py, (int) dx, (int) dy, u, v, cplus, cminus, dist, score);

  return score;
}

float Scenario::get_next_key_match(unsigned char letter, int index, QList<NextIndex> &new_index_list, bool incremental, bool &overflow) {
  /* given an index on the curve, and a possible next letter key, evaluate the following:
     - if the next letter is a good candidate (i.e. the curve pass by the key)
     - compute a score based on curve to point distance
     - return the list on possible next_indexes (sometime more than one solution is possible,
       e.g. depending if we match the key with a near sharp turn or not)
     (multiple indexes for a same scenario will be removed on later iterations,
      when we know which one is the better)

     The algorithm tries to uses information about special points (still called "sharp turn"
     in the code):
     - 1: Sharp turns -> shortest distance to key must be near this point
     - 2: U-Turns --> mush exactly match a key
     - 3: Slow-down points --> treated as 1, but has les priority than type 1 & 2
     - 4: inflection points (*removed* this has proven useless or duplicating other checks)
     - 5: Small turn (or near curve tip) -> optionally matches a key
     - 6: (not-so) sharp turn (same as 1) but minimum distance point may be distinct from the turn point (e.g. used for loops)

     now score is returned for each possible index in NextIndex struct
     return value from this function is now less relevant (@todo remove it completely)
  */

  float score = -99999;
  int count = 0;
  int retry = 0;

  int start_index = index;

  float max_score = -99999;
  int max_score_index = -1;

  int max_turn_distance = params->max_turn_index_gap; // @todo this should be expressed as distance, not as a number of curve points
  int last_turn_point = 0;
  float last_turn_score = 0;

  new_index_list.clear();

  overflow = false;
  int failed = 0;
  bool finished = false;

  int start_st = curve->getSharpTurn(start_index);

  int step = 1;
  while(1) {
    if (index >= curve->size() - 4 && incremental) { overflow = true; break; }
    /* the magic value "4" is because the curvePreprocess1 (which find special points)
       does not work near the end of the curve, so this may be an issue in
       incremental mode */
    if (index >= curve->size() - 1) { break; }

    float new_score = calc_distance_score(letter, index, this -> count);

    // @todo add a "even more verbose" debug option
    // DBG("    get_next_key_match(%c, %d) index=%d score=%.3f)", letter, start_index, index, new_score);

    if (new_score > max_score) {
      max_score = new_score;
      max_score_index = index;
    }

    if (new_score > score) {
      retry = 0;
    } else {
      retry += step;
      if (retry > params->match_wait && count > params->match_wait) { break; }
    }
    score = new_score;

#define MANDATORY_TURN(st) ((st) == 1 || (st) == 2 || (st) == 6)

    // look for sharp turns
    int st = curve->getSpecialPoint(index);

    if (! MANDATORY_TURN(st) && last_turn_point) { st = 0; } // handle slow-down point and other types with less priority (type = 3)

    if (st > 0 && index > start_index) {

      if (last_turn_point && MANDATORY_TURN(curve->getSharpTurn(last_turn_point))) {
	// we have already encountered a sharp turn linked with next letter so we can safely ignore this one
	break;
      }

      last_turn_point = index;
      last_turn_score = score;

      if (st == 2) {
	new_index_list.clear();
	if (score > 0) { new_index_list << NextIndex(index, score); }
	if (max_score_index < index && max_score > 0) { new_index_list << NextIndex(max_score_index, max_score); }
	if (new_index_list.size() == 0) { failed = 1; }
	finished = true;
	break;
      }
    }

    step = 1;
    if (new_score < -1) {
      step = (- 0.5 - new_score) * params->dist_max_next / 20; // warning : this is adherent to score_distance implementation
      int i = 1;
      while (i < step && index + i < curve->size() - 1 && ! curve->getSharpTurn(index + i)) {
	i++;
      }
      step = i;
    }

    count += step;
    index += step;
  }

  if (max_score <= 0) { failed = 10; } // we didn't approach the target key

  bool use_st_score = false;
  if (finished || failed) {
    // nothing more to do
  } else {
    if (! last_turn_point) {
      new_index_list << NextIndex(max_score_index, max_score);
    } else if (max_score_index < last_turn_point - max_turn_distance) {
      new_index_list << NextIndex(max_score_index, max_score);
    } else if (max_score_index <= last_turn_point + max_turn_distance) {
      int last_st = curve->getSpecialPoint(last_turn_point);
      if (last_st == 3 || last_st == 6) {
	new_index_list << NextIndex(max_score_index, max_score);
      } else {
	if (last_turn_score > 0) { new_index_list << NextIndex(last_turn_point, last_turn_score); }

	int maxd = params->min_turn_index_gap;
	if ((max_score_index < last_turn_point - maxd) ||
	    (max_score_index > last_turn_point + maxd && start_st == 0 && start_index >= last_turn_point - max_turn_distance)) {
	  new_index_list << NextIndex(max_score_index, max_score);
	}
      }
      int st = curve->getSharpTurn(last_turn_point);

      /* particular case: simple turn (ST=1) shared between two keys */
      if (st == 1 && (start_index < last_turn_point &&
		      start_index >= last_turn_point - max_turn_distance &&
		      max_score_index <= last_turn_point + max_turn_distance)) {
	DBG("  [get_next_key_match] shared ST=1 point !");
      } else /* default case */ if (st < 3) { use_st_score = true; }

    } else { // max_score_index > last_turn_point + max_turn_distance --> turn point was not matched
      int last_st = curve->getSpecialPoint(last_turn_point);
      if (MANDATORY_TURN(last_st)) { failed = 20; }
      else { new_index_list << NextIndex(max_score_index, max_score); }
    }
  }

  DBG("  [get_next_key_match] %s:%c[%d] %s max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> new_indexes=[%s] fail_code=%d",
      getNameCharPtr(), letter, start_index, failed?"*FAIL*":"OK",
      max_score, max_score_index, last_turn_score,
      last_turn_point, index, failed?"*FAIL*":QSTRING2PCHAR(qlist_nextindex2str(new_index_list)), failed);

  if (failed) { return -1; }
  if (use_st_score) { return last_turn_score; } // in case of sharp turn, distance score is taken from the turn position, not the shortest curve-to-key distance
  return max_score;
}

bool Scenario::childScenario(LetterNode &childNode, QList<Scenario> &result, stats_t &st, int curve_id, bool incremental) {
  /* this function create childs scenarios from the current one (they represent a word with one more letter)
     based on a child letter nodes (i.e. next possible letter in word)
     it can return:
     - no scenario (the letter is not a valid candidat because it does not match the curve)
     - one scenario
     - multiple scenarios in case there are different points matching the key (shortest point, near sharp turn ...)
     it can fail only in the following case :
     we are running in incremental mode (incremental == true) and we are too close from the end of the curve
     (which means caller must call this function again later when there are more points, or when the curve is
     really finished)

     Now it automatically look for normal scenarios (middle of words) or end scenarios (last letter) or both
     based on the context. It should now be called only once for each scenario.
  */

  bool hasPayload = childNode.hasPayload();
  bool isLeaf = childNode.isLeaf();
  return childScenario(childNode, result, st, curve_id, incremental, hasPayload, isLeaf);
}

bool Scenario::childScenario(LetterNode &childNode, QList<Scenario> &result, stats_t &st, int curve_id, bool incremental, bool hasPayload, bool isLeaf) {
  /* this method allow to override hasPayload/isLeaf flags (used with multi-scenario) */

  if (curve_id > 0) { /* multi-touch not supported here */ }
  unsigned char letter = childNode.getChar();

  // cache management to allow better performance when reusing the same sub-scenarios in multiple multi-scenarios
  if (cache && result.size()) {
    cache = false;
    logdebug("ERROR: Scenario::childScenario requires an empty list as input when cache is activated!");
  }
  if (cache) {
    child_cache_t *cc = cacheChilds.data();
    if (cc && cc->contains(letter)) {
      // positive caching
      result.append((*cc)[letter]);
      st.st_cache_hit ++;
      return true;
    }
    /* no significant impact with current settings
    if (cacheMinLength.contains(letter)) {
      // negative caching
      int min_len = cacheMinLength[letter];
      if (curve->getTotalLength() <= min_len) {
	st.st_cache_hit ++;
	return false;
      }
    }
    */
  }

  st.st_cache_miss ++;

  bool partial = incremental && ! curve->finished; // curve is not complete yet
  bool isDot = curve->isDot;

  // step 1: find non-ending child scenarios
  if ((! isDot) && ((! isLeaf) || (partial && hasPayload))) {
    int len_before = result.size();
    if (! childScenarioInternal(childNode, result, st.st_fork, partial /* incremental */, false)) {
      // try again later
      /*
      if (cache) {
	cacheMinLength[letter] = curve->getTotalLength();
      }
      */
      return false;
    }
    int len_after = result.size();

    if (isLeaf) {
      // scenario we may have found will only be used to evaluate if curve is long enough to
      // check ending scenarios. We can safely discard them
      // (that's inefficient, but older code always did it also)

      if (len_after > len_before) { // (partial is true)

	int curve_index = 0;
	for(int i = len_before; i < len_after; i ++) {
	  curve_index = max(curve_index, result[i].getCurveIndex());
	}
	for(int i = len_before; i < len_after; i ++) {
	  result.removeLast();
	}

	if (curve->getTotalLength() < curve->getLength(curve_index) + params->end_scenario_wait) {
	  return false; // we must wait for curve to be longer to test end scenario
	}

	// continue to step 2

      } else {
	// no match, end-scenario is not possible
	return true;
      }
    }
  }

  // step 2: find ending scenario
  if (hasPayload && (count > 0 || isDot)) {
    childScenarioInternal(childNode, result, st.st_fork, partial /* incremental */, true); // result ignored (always true with endScenario == true)
  }

  // update cache
  if (cache) {
    child_cache_t *cc = cacheChilds.data();
    (*cc)[letter] = result;
  }

  return true;
}

bool Scenario::childScenarioInternal(LetterNode &childNode, QList<Scenario> &result, int &st_fork, bool incremental, bool endScenario) {
  unsigned char prev_letter = (count > 0)?letter_history[count - 1]:0;
  unsigned char letter = childNode.getChar();
  int index = this -> index;

  QList<NextIndex> new_index_list;

  DBG("==== %s:%c [end=%d, index=%d] ====", getNameCharPtr(), letter, endScenario, index);

  if (count == 0) {
    // this is the first letter
    float distance_score = calc_distance_score(letter, 0, count);
    new_index_list << NextIndex(0, distance_score);

  } else {
    if (endScenario) {

      bool st_found = false;
      int new_index = curve->size() - 1;
      for(int i = index + 1; i <= new_index; i ++) {
	st_found |= (curve->getSharpTurn(i) > 0);
      }
      if (st_found) {
	DBG("debug [%s:%c] * =FAIL= sharp turn found before end", getNameCharPtr(), letter);
      } else {
	float distance_score = calc_distance_score(letter, new_index, -1);
	new_index_list << NextIndex(new_index, distance_score);
      }

    } else {

      bool overflow = false;
      /* resultat ignored */ get_next_key_match(letter, index, new_index_list, incremental, overflow);
      if (incremental && overflow) {
	return false; // ask me again later
      }
      if (new_index_list.isEmpty()) {
	DBG("debug [%s:%c] %s =FAIL= {distance / turning point}", getNameCharPtr(), letter, endScenario?"*":" ");
	return true;
      }

    }
  }

  if (new_index_list.size() >= 2) { st_fork ++; }

  bool first = true;
  int continue_count = 0;
  Scenario *first_scenario = NULL;

  foreach(NextIndex nit, new_index_list) {
    int new_index = nit.index;
    float distance_score = nit.score;

    if (count > 2 && new_index <= index_history[count - 2] + 1) {
      continue; // 3 consecutive letters are matched with the same curve point -> remove this scenario
    }

    score_t score = {NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE}; // all scores are : <0 reject, 1 = best value, ]0,1] OK, NO_SCORE = 0 = not computed
    score.distance_score = distance_score;

    bool err = false;

    if (count > 0) {
      if (new_index > index) {
	score.cos_score = calc_cos_score(prev_letter, letter, index, new_index);
	score.curve_score = calc_curve_score(prev_letter, letter, index, new_index);
      } else {
	err = (curve->getSharpTurn(index) == 0);
      }
    }

    bool ok = (score.distance_score >= 0 && score.curve_score >= 0 && score.cos_score >= 0) && ! err;

    /* manage errors: we accept some user mistakes. In case there is no other candidate, this may
       be good enough solutions. This is especially useful for long (and boring!) words
       warning: this is known to increase CPU usage by more than 50% ! */
    bool error_ignore = false;
    if (params->error_correct && !ok && count >= 2 && (error_count < 1 || count >= params->error_ignore_count)) {
      if (score.distance_score > (endScenario?0:-0.45)) {
	if (score.cos_score >= 0 || score.curve_score >= 0 || count >= params->error_ignore_count) {
	  error_ignore = ok = true;
	}
      }
    }

    DBG("debug [%s:%c] %s%s %d:%d %s [d=%.2f cr=%.2f cs=%.2f l=%.2f t=%.2f]",
	getNameCharPtr(), letter, endScenario?"*":" ", first?"":" <FORK>", count, new_index, error_ignore?"ERROR[ignored]":(ok?"=OK=":"*FAIL*"),
	score.distance_score, score.curve_score, score.cos_score, score.length_score, score.turn_score);

    if (ok) {
      // create a new scenario for this child node
      Scenario new_scenario(*this); // use our copy constructor
      new_scenario.node = childNode;
      new_scenario.index = new_index;
      new_scenario.scores[count] = score;
      new_scenario.index_history[count] = new_index;
      new_scenario.letter_history[count] = letter;
      new_scenario.letter_history[count + 1] = 0; // letter_history is also a '\0' terminated string
      new_scenario.count = count + 1;
      new_scenario.finished = endScenario;
      new_scenario.error_count = error_count + error_ignore?1:0;

      if (cache) {
	new_scenario.cacheChilds = QSharedPointer<child_cache_t>(new child_cache_t); // do not inherit parent cache :-)
      }

      // number of keys actually crossed
      Point key = keys->get(letter);
      Point size = keys->size(letter);
      Point pt = curve->point(new_index);
      if (pt.x >= key.x - size.x / 2 && pt.x <= key.x + size.x / 2 &&
	  pt.y >= key.y - size.y / 2 && pt.y <= key.y + size.y / 2) {
	new_scenario.good_count ++;
      }

      // compute distance
      int dx = key.x - pt.x;
      int dy = key.y - pt.y;
      /* @todo try with corrected distance
      float distance;
      calc_distance_score(letter, new_index, count, &distance); // ignore result, we only need distance
      new_scenario.dist_sqr += distance * distance;
      */
      new_scenario.dist_sqr += dx * dx + dy * dy;
      new_scenario.dist = sqrt(new_scenario.dist_sqr / (count + 1));

      continue_count ++;
      if (continue_count >= 2) {
	if (first_scenario) {
	  first_scenario->last_fork = count + 1;
	  first_scenario = NULL;
	}
	new_scenario.last_fork = count + 1;

      } else {
	first_scenario = &new_scenario;
      }

      // temporary score is used only for simple filtering
      new_scenario.temp_score = 1.0 / (1.0 + new_scenario.dist / 30);
      /* @todo remove (old temp score)
      if (count == 0) {
	new_scenario.temp_score = temp_score + score.distance_score;
      } else {
	new_scenario.temp_score = temp_score + ((score.distance_score * params->weight_distance
						 + score.curve_score * params->weight_curve
						 + score.cos_score * params->weight_cos
						 + score.turn_score * params->weight_turn
						 + score.length_score * params->weight_length)
						/
						(params->weight_distance + params->weight_curve + params->weight_cos
						 + params->weight_turn + params->weight_length));
      }
      */

      result.append(new_scenario);
    }

    first = false;

  } // for (new_index)

  return true;
}

void Scenario::nextKey(QList<Scenario> &result, stats_t &st) {
  foreach (LetterNode child, node.getChilds()) {
    childScenario(child, result, st);
  }
}

QList<LetterNode> Scenario::getNextKeys() {
  return node.getChilds();
}

bool Scenario::operator<(const Scenario &other) const {
  return this -> getScore() < other.getScore();
}

QString Scenario::getWordList() {
  QPair<void*, int> payload = node.getPayload();
  return QString((char*) payload.first); // payload is always zero-terminated
}

float Scenario::getScore() const {
  if (! count) { return 0; }
  return (final_score == -1)?getTempScore():final_score;
}

float Scenario::getTempScore() const {
  return temp_score;
}

float Scenario::getCount() const {
  return count;
}

QString Scenario::getName() const {
  QString ret;
  ret.append((char*) getNameCharPtr());
  return ret;
}

unsigned char* Scenario::getNameCharPtr() const {
  return letter_history;
}


int Scenario::getCurveIndex() {
  return index;
}


bool Scenario::forkLast() {
  return (last_fork == count) || (last_fork && count <= 3); // don't resolve forks before the fourth letter
}

void Scenario::turn_transfer(int turn_count, turn_t *turn_detail) {
  // transfer "turn rate" between turns to enable users to cut through (users are lazy)

  int max_transfer = params->turn_max_transfer;

  for(int i = 0; i < turn_count; i++) {
    turn_t *d = &(turn_detail[i]);
    d->replace_expected = max(d -> actual - max_transfer, min(d -> actual + max_transfer, d->expected));
  }

  float give[turn_count * 2];

  bool change = true;
  while(change) {
    change = false;

    memset(give, 0, sizeof(give));

    for(int i = 0; i < turn_count; i++) {
      turn_t *d = &(turn_detail[i]);

      float nb_wants[2] = { 0.0, 0.0 };
      for(int ni = 0; ni <= 1; ni ++) {
	int j = i - 1 + 2 * ni;
	if (j < 0 || j >= turn_count) { continue; }
	turn_t *d2 = &(turn_detail[j]);
	// also transfer between same direction turn: if (d->replace_expected * d2->replace_expected >= 0) { continue; }

	int len = ni?d->length_after:d->length_before;
	if (len > params -> turn_optim) { continue; }

	nb_wants[ni] = d2->replace_expected - d2->corrected;
      }

      float i_want = d->replace_expected - d->corrected;
      float they_want = nb_wants[0] + nb_wants[1];
      int give_count = (nb_wants[0] != 0) + (nb_wants[1] != 0);
      if (! give_count) { continue; }

      int sgn = (they_want > 0) - (they_want < 0);
      if (fabs(they_want) <= fabs(i_want)) {
	give[i << 1] = - nb_wants[0];
	give[(i << 1) + 1] = - nb_wants[1];
      } else if (give_count <= 1) {
	give[i << 1] = sgn * (nb_wants[0] != 0) * fabs(i_want);
	give[(i << 1) + 1] = sgn * (nb_wants[1] != 0) * fabs(i_want);
      } else {
	float w1 = fabs(nb_wants[0]), w2 = fabs(nb_wants[1]);
	float g1 = 0, g2 = 0;
	float rest = fabs(i_want);
	float minw = min(w1, w2);
	if (rest < minw * 2) {
	  g1 += rest / 2;
	  g2 += rest / 2;
	} else {
	  g1 += minw;
	  g2 += minw;
	  rest -= minw * 2;
	  if (g1 > g2) { g1 += rest; } else { g2 += rest; }
	}
	give[i << 1] = sgn * g1;
	give[(i << 1) + 1] = sgn * g2;
      }
      /*
      DBG("Turn transfer debug: #%d: %d=>%d/%d  nb_want=%d:%d i_want=%d they_want=%d give=%d:%d count=%d",
	  i, (int) d->actual, (int) d->corrected, (int) d->expected,
    	  (int) nb_wants[0], (int) nb_wants[1], (int) i_want, (int) they_want, (int) give[i<<1], (int) give[(i<<1)+1], give_count);
      */
    }

    for(int i = 0; i < turn_count; i++) {
      turn_t *d = &(turn_detail[i]);

      for(int ni = 0; ni <= 1; ni ++) {
	int j = i - 1 + 2 * ni;
	if (i > j) { continue; }
	if (j < 0 || j >= turn_count) { continue; }
	turn_t *d2 = &(turn_detail[j]);

	float t_ij = give[(i << 1) + ni];
	float t_ji = give[(j << 1) + (1 - ni)];

	float absv = min(fabs(t_ij), fabs(t_ji));
	if (absv <= 2) { continue; }

	int diff = d->replace_expected - d->corrected;
	int sens = (diff > 0)?1:-1;

	d->corrected += sens * absv;
	d2->corrected -= sens * absv;
	DBG("Turn transfer #%d->#%d: %d", i, j, (int) (sens * absv));
	change = true;
      }
    }
  }
}

void Scenario::calc_turn_score_all(turn_t *turn_detail, int *turn_count_return) {
  /* this score check if actual segmentation (deduced from user input curve)
     is a good match for the expected one (curve which link all keys in the
     current scenario)

     note: this is far to much complicated for a small reward in term of scoring
     but it helps the classifier by providing a new kind of score independant
     of the others */

  if (count < 2) { return; }


  float a_actual[count];
  float a_expected[count];
  float a_expected_real[count];
  bool  a_same[count];
  memset(a_same, 0, sizeof(a_same));

  // compute actual turn rate
  float segment_length[count];

  if (count > 0 && index_history[0] == index_history[1]) { a_same[0] = true; }

  int i1 = 0;
  int i_ = 1;
  for(int i = 1; i < count - 1; i ++) {
    // curve index
    int i2 = index_history[i];
    int i3 = index_history[i + 1];

    // curve points
    Point p1 = curve->point(i1);
    Point p2 = curve->point(i2);
    Point p3 = curve->point(i3);

    segment_length[i] = distancep(p2, p3);
    if (i == 1) { segment_length[0] = distancep(p1, p2); } // piquets & intervalles

    a_same[i] = (i2 == i3);

    if (i3 > i2 && i2 > i1) {
      float actual = anglep(p2 - p1, p3 - p2) * 180 / M_PI;

      for(int j = i_; j <= i; j++) {
	a_actual[j] = actual / (1 + i - i_);  // in case of "null-point" we spread the turn rate over all matches at the same position
      }

      i_ = i + 1; i1 = i2;
    }
  }
  for(int i = i_; i <= count - 1; i ++) {
    a_actual[i] = 0;
  }

  // compute expected turn rate
  for(int i = 1; i < count - 1; i ++) {
    // letters
    unsigned char l1 = letter_history[i - 1];
    unsigned char l2 = letter_history[i];
    unsigned char l3 = letter_history[i + 1];

    // key coordinates
    Point k1 = keys->get_raw(l1);
    Point k2 = keys->get_raw(l2);
    Point k3 = keys->get_raw(l3);

    // corrected key coordinates
    Point ck1 = keys->get(l1);
    Point ck2 = keys->get(l2);
    Point ck3 = keys->get(l3);

    // actual/expected angles
    float expected = anglep(k2 - k1, k3 - k2) * 180 / M_PI;
    float c_expected = anglep(ck2 - ck1, ck3 - ck2) * 180 / M_PI; // corrected

    // a bit of cheating for U-turn (+180 is the same as -180, but it is
    // not handled by the code below)
    float actual = a_actual[i];
    if (abs(expected) >= 130 && abs(actual) > 130 && expected * actual < 0) {
      expected = expected - 360 * ((expected > 0) - (expected < 0));
    }
    if (abs(c_expected) >= 130 && abs(actual) > 130 && c_expected * actual < 0) {
      c_expected = c_expected - 360 * ((c_expected > 0) - (c_expected < 0));
    }

    // choose between displayed key coordinates and corrected ones
    // warning : this may make turn score less discriminating (especially with dynamic key position adjustment)
    float new_expected = expected;

    if ((c_expected * expected < 0) || (abs(expected - actual) > 100)) {
      // extreme cases we decide not to handle

    } else {

      if ((actual - expected) * (actual - c_expected) < 0) {
	// actual is between expected & c_expected
	new_expected = actual;
      } else if (abs(actual - c_expected) < abs(actual - expected)) {
	// actual is nearer from c_expected than expected
	new_expected = c_expected;
      }

      if (new_expected != expected) {
	DBG("Expected turn updated(#%d): a:%d / e:%d (corrected:%d) --> new expected = %d",
	    i, (int) actual, (int) expected, (int) c_expected, (int) new_expected);
      }
    }

    a_expected[i] = new_expected;
    a_expected_real[i] = expected;
  }
  a_expected[count - 1] = 0;

  /* in case of merged match-points, spread overall expected turn angle over both letters */
  for(int i = 1; i < count - 1; i ++) {
    if (a_same[i] && ! a_same[i + 1]) {
      a_expected[i] = a_expected[i + 1] = 0.5 * (a_expected[i] + a_expected[i + 1]);
      a_expected_real[i] = a_expected_real[i + 1] = 0.5 * (a_expected_real[i] + a_expected_real[i + 1]);
    }
  }

  // another version of the U-turn test above for when two keys are matched
  // at the same point
  for(int i = 1; i < count - 1; i ++) {
    if (i > 1 && a_same[i - 1]) { continue; } // processed during previous iterations

    float actual = 0, expected = 0;
    int j = i;
    while(1) {
      actual += a_actual[j];
      expected += a_expected[j];
      if (! a_same[j]) { break; }
      j ++;
      if (j >= count - 1) { break; }
    }
    if (i == j) { continue; }
    if (j > i + 1) { continue; } // don't manage more than two keys matched at the same point

    // check if curve tangent at the matching point is consistent with letter order
    Point k1 = keys->get_raw(letter_history[i]);
    Point k2 = keys->get_raw(letter_history[j]);
    Point tgt = actual_curve_tangent(index_history[i]);
    if ((k2.x - k1.x) * tgt.x + (k2.y - k1.y) * tgt.y < 0 &&
	curve -> getSpecialPoint(index_history[i]) != 2) { continue; }

    float new_expected = expected - 360 * ((expected > 0) - (expected < 0));
    if (abs(expected) >= 100 && abs(actual) > 100 && expected * actual < 0 &&
	abs(new_expected - actual) < abs(expected - actual) &&
	abs(actual) < 360) {
      DBG("Reversing expected angles for shared match point [%d:%d] %d->%d", i, j, (int) expected, (int) new_expected);
      expected = new_expected;
      for (int k = i; k <= j; k ++) {
	a_expected[k] = expected / (j - i + 1);
      }
    }
  }

  /*
  for(int i = 1; i < count - 1; i ++) {
    DBG("Turn detail #%d: '%c' %d/%d", i, letter_history[i], (int) a_actual[i], (int) a_expected[i]);
  }
  */

  // match both sets
  // (a turn in a set must match a turn in the other one (even if they are not
  //  spread exactly on the same indexes)
  int min_angle = params->turn_min_angle;
  int max_angle = params->turn_max_angle;

  memset(turn_detail, 0, sizeof(turn_t) * count);
  int turn_count = 0;

  int i0 = 1;
  int typ_exp[count];
  int typ_act[count];
  memset(&typ_exp, 0, sizeof(typ_exp));
  memset(&typ_act, 0, sizeof(typ_act));

  for(int i = 1; i < count - 1; i ++) {
    // i is the index where the turn occurs (between segment i-1 and segment i)

    if ((segment_length[i] > params -> turn_separation) || (i == count - 2)) {
      // we've got a block [i0, i1] (turns can not overlap across blocks)
      DBG("  [score turn] === block[%d:%d]", i0, i)

      /* step 1 : find obvious matches (same direction turn or high angle turns) */
      for(int j = i0; j <= i; j++) {
	if (abs(a_expected[j]) > 130 && abs(a_actual[j]) > 130) {
	  typ_exp[j] = typ_act[j] = 2; // inderterminate (for now ...)
	} else if (abs(a_expected[j]) > min_angle && abs(a_actual[j]) > min_angle && a_expected[j] * a_actual[j] > 0) {
	  int direction = (a_actual[j] > 0)?1:-1;
	  typ_exp[j] = typ_act[j] = direction;
	}
      }

      /* step 1.5 :  "half soft turns */
      for(int j = i0; j <= i; j++) {
	if ((abs(a_expected[j]) > min_angle || abs(a_actual[j]) > min_angle) && a_expected[j] * a_actual[j] > 0) {
	  int direction = (a_actual[j] > 0)?1:-1;
	  typ_exp[j] = typ_act[j] = direction;
	}
      }

      /* for(int j = i0; j <= i; j ++) {
	DBG("Turn detail: %d %d[%d] / %d[%d] %d-%d", j, (int) a_actual[j], typ_act[j], (int) a_expected[j], typ_exp[j], index_history[j + 1], index_history[j + 2]);
      } */

      /* step pre-2 : merge close turns when user cutting through produce bad turn matching
	 (this is very rare: only one test case involved but i like it) */
      for(int j = i0; j < i; j ++) {
	if ((typ_act[j] == 0) && (typ_exp[j] == 0) &&
	    (typ_act[j + 1] == 0) && (typ_exp[j + 1] == 0) &&
	    ((abs(a_expected[j]) > min_angle && abs(a_actual[j]) < min_angle && abs(a_expected[j + 1]) < min_angle && abs(a_actual[j + 1]) > min_angle && a_expected[j] * a_actual[j + 1] > 0) ||
	     (abs(a_expected[j]) < min_angle && abs(a_actual[j]) > min_angle && abs(a_expected[j + 1]) > min_angle && abs(a_actual[j + 1]) < min_angle && a_actual[j] * a_expected[j + 1] > 0)) &&
	    (index_history[j + 2] - index_history[j + 1]) < params->max_turn_index_gap /* @todo: use a dedicated parameted? */
	    ) {
	  DBG("===> merging two close and unmatched turns (index=%d)", j);
	  a_actual[j] = a_actual[j + 1] = (a_actual[j] + a_actual[j + 1]) / 2;
	  a_expected[j] = a_expected[j + 1] = (a_expected[j] + a_expected[j + 1]) / 2;
	  typ_act[j] = typ_exp[j] = typ_act[j + 1] = typ_exp[j + 1] = (a_actual[j] > 0)?1:-1;
	}
      }


      /* step 2 : try to fill the gaps */
      for(int j = i0; j <= i; j++) {
	if ((typ_exp[j] == 1 || typ_exp[j] == -1) && (typ_exp[j] == typ_act[j])) {
	  int direction = typ_exp[j];
	  for(int incr = -1 ; incr <= 1; incr += 2) {
	    int k = j + incr;
	    if (k < i0 || k > i) { continue; }
	    if ((! typ_exp[k]) && (a_expected[k] * direction > 0)) {
	      typ_exp[k] = direction;
	    }
	    if ((! typ_act[k]) && (a_actual[k] * direction > 0)) {
	      typ_act[k] = direction;
	    }
	  }
	}
      }

      /* step 3 : fill the gaps for indeterminate turns */
      for(int j = i0; j <= i; j++) {
	if (typ_exp[j] == 2  && typ_act[j] == 2) {
	  int direction = 0;
	  int k0 = j;
	  for(int incr = -1 ; incr <= 1; incr += 2) {
	    int k = k0;
	    k += incr;
	    if (k < i0 || k > i) { continue; }
	    if ((! typ_exp[k]) && abs(a_expected[k]) > min_angle) {
	      int new_direction = (a_expected[k] > 0)?1:-1;
	      if (! direction || new_direction == direction) {
		direction = new_direction;
		typ_exp[k] = new_direction;
	      }
	    }
	    if ((! typ_act[k]) && abs(a_actual[k]) > min_angle) {
	      int new_direction = (a_actual[k] > 0)?1:-1;
	      if (! direction || new_direction == direction) {
		direction = new_direction;
		typ_act[k] = new_direction;
	      }
	    }
	  }
	  if (! direction) {
	    direction = (a_actual[k0] + a_expected[k0] > 0)?1:-1;
	  }
	  typ_act[k0] = typ_exp[k0] = direction;
	}
      }


      for(int j = i0; j <= i; j ++) {
	DBG("  [score turn] > \"%s\" turn point #%d:%c %.2f[%d] %.2f[%d] length=(%d,%d)",
	    getNameCharPtr(), j, letter_history[j], a_actual[j], typ_act[j], a_expected[j], typ_exp[j],
	    (int) segment_length[j - 1], (int) segment_length[j]);
      }

      /* step 4 : put everything together, calculate turns list and find unmatched turns */
      int current_turn = -1;
      int current_dir = 0;
      for(int j = i0; j <= i; j++) {
	if (typ_act[j] == 2 || typ_exp[j] == 2) {
	  DBG("  [score turn] *** indeterminate turn (should not happen) #%d: %.2f/%.2f", j, a_actual[j], a_expected[j]);
	  scores[j].turn_score = -1;
	  return;
	}
	bool bad_turn = false;
	if (typ_act[j] == 0 && abs(a_actual[j]) > max_angle) {
	  DBG("  [score turn] *** unmatched actual turn #%d: %.2f", j, a_actual[j]);
	  bad_turn = true;
	}
	if (typ_exp[j] == 0 && abs(a_expected[j]) > max_angle) {
	  DBG("  [score turn] *** unmatched expected turn #%d: %.2f", j, a_expected[j]);

	  /*
	    When there is a quick succession of : turn > 120°, normal turn, turn > 120°
	    the middle one is difficult to do when typing quickly (especially one-handed)
	    At the moment i've noticed this with only one word ("bonjour", for the "J" turn)
	    I hope i haven't implemented a rule just for one word :-)
	    This causes no significant regression on existing test cases
	  */
	  float min_turn = params->bjr_min_turn;

	  int f1 = 0, f2 = 0;
	  if (j > i0 && abs(a_expected[j - 1]) > min_turn) {
	    f1 = 2;
	  } else if (j == i0 && i0 > 0) {
	    f1 = 1;
	  }

	  if (j < i && abs(a_expected[j + 1]) > min_turn) {
	    f2 = 2;
	  } else if (j == i && i < count - 2) {
	    f2 = 1;
	  }

	  if (f1 + f2 > 2 && min_turn > 0 && abs(a_expected[j]) < min_turn) {
	      DBG("  [score turn] bad turn ignored #%d (\"bonjour\"-like)", j);
	      if (scores[j].cos_score < 0) { error_count --; }
	      scores[j].cos_score = 0; // invalidate turn angle score because it's not relevant in this case
	  } else {
	    bad_turn = true;
	  }
	}

	if (bad_turn) {
	  current_turn = (turn_count ++);
	  turn_detail[current_turn].direction = bad_turn;
	  turn_detail[current_turn].index = j;
	  turn_detail[current_turn].start_index = j;
	  turn_detail[current_turn].expected = a_actual[j];
	  turn_detail[current_turn].actual = a_expected[j];
	  turn_detail[current_turn].unmatched = true;
	  current_turn = -1;
	  current_dir = 0;
	  continue;
	}

	bool p1 = (typ_exp[j] == 1 || typ_act[j] == 1);
	bool p_1 = (typ_exp[j] == -1 || typ_act[j] == -1);

	int new_dir = 0;
	bool overlap = false;
	bool finish = false;

	if ((! current_dir) && (p1 || p_1)) {
	  // a new turn begins
	  new_dir = (p1?1:-1);

	} else if ((p1 && current_dir == -1) || (p_1 && current_dir == 1)) {
	  // turns in opposite direction
	  new_dir = (p1?1:-1);
	  overlap = (p1 && p_1);
	  finish = true;

	} else if (current_dir && ! (p1 || p_1)) {
	  // end turn
	  finish = true;
	}

	if (finish) {
	  if (overlap) {
	    turn_detail[current_turn].index = j;
	    if (typ_act[j] == current_dir) { turn_detail[current_turn].actual += a_actual[j]; }
	    if (typ_exp[j] == current_dir) { turn_detail[current_turn].expected += a_expected[j]; }
	  }
	  current_dir = 0;
	}

	if (new_dir) {
	  current_dir = new_dir;
	  current_turn = (turn_count ++);
	  turn_detail[current_turn].direction = new_dir;
	  turn_detail[current_turn].start_index = j;
	  turn_detail[current_turn].expected = 0;
	  turn_detail[current_turn].actual = 0;
	}

	if (current_dir) {
	  turn_detail[current_turn].index = j;
	  if (typ_act[j] == current_dir) { turn_detail[current_turn].actual += a_actual[j]; }
	  if (typ_exp[j] == current_dir) { turn_detail[current_turn].expected += a_expected[j]; }
	}
      }

      /* let's go to next block */
      i0 = i + 1;

    } /* new block */

  } /* for(i) */

  /* step 5: compute length */
  if (turn_count > 0) {
    for (int j = 0; j < turn_count; j ++) {
      turn_detail[j].corrected = turn_detail[j].actual;
      float total = 0;
      for (int k = turn_detail[j].start_index; k <= turn_detail[j].index; k ++) {
	total += a_expected_real[k];
      }
      turn_detail[j].expected_real = total;
    }

    for (int i = 0; i <= turn_count; i ++) {
      int i1 = (i > 0)?turn_detail[i - 1].index:0;
      int i2 = (i < turn_count)?turn_detail[i].start_index:(count - 1);

      int l = 0;
      for(int k = i1; k < i2; k ++) {
	l += segment_length[k];
      }
      if (i > 0) { turn_detail[i - 1].length_after = l; }
      if (i < turn_count) { turn_detail[i].length_before = l; }
    }
  }

  if (turn_count_return) { *turn_count_return = turn_count; }

  /* step 6: scoring */
  if (turn_count) {
    turn_transfer(turn_count, turn_detail);

    // thresholds
    int t1 = params->max_turn_error1;
    int t2 = params->max_turn_error2;
    int t3 = params->max_turn_error3;

    for(int i = 0; i < turn_count; i++) {
      turn_t *d = &(turn_detail[i]);

      int length = 0;
      for (int k = d->start_index; k < d->index; k++) {
	int i1 = index_history[k];
	int i2 = index_history[k + 1];
	length += distancep(curve->point(i1), curve->point(i2));
      }
      d->length = length;

      float actual = d -> corrected;
      float expected = d -> expected;
      float score = 1;
      float scale = -1, scale2 = 0;

      if (abs(expected) > params->turn_threshold_ignore) {
	score = 1; // too large turn, no check
      } else {
	float t = (abs(expected) < 90)?t1:t3;
	float add_score = 0;

	if (abs(expected) > 180) {
	  scale = t3 * params -> turn_scale_ut;
	  scale2 = params->turn_scale2_ut;
	} else {
	  scale = t + (t2 - t) * sin(min(abs(actual), abs(expected)) * M_PI / 180);
	}

	int tip = 0;

	if ((((d->start_index == 1) && (d->length_before < params->turn_tip_verysmall)) ||
	     ((d->index == count - 2) && (d->length_after < params->turn_tip_verysmall))) && turn_count > 0) {
	  // no negative scoring for very small tips

	} else {

	  if ((d->start_index == 1) && (d->length_before < params->turn_tip_min_distance)) { tip |= 1; }
	  if ((d->index == count - 2) && (d->length_after < params->turn_tip_min_distance)) { tip |= 2; }
	  if (tip) {
	    scale *= params->turn_tip_scale_ratio;
	    scale2 = params->turn_scale2_tip;
	    if (tip == 3 && fabs(turn_detail[0].expected) > 180) { scale2 = params->turn_scale2_tip2; }
	    add_score = - params -> turn_tip_len_penalty;
	  }

	  scale = t + (t2 - t) * sin(min(abs(actual), abs(expected)) * M_PI / 180);

	  float diff = abs(actual - expected);
	  float sc0 = (scale2 > scale - 0.1)?0:max(0, diff - scale2) / (scale - scale2);
	  score = 1 - pow(sc0, params->turn_diff_pow) + add_score;

	}

      }

      if (d->unmatched) { score -= params->turn_score_unmatched; }

      if (score < 0) { score = 0.01; } // we can keep this scenario in case other are even worse

      int index = d -> index;
      float trn = (d->corrected - d->actual) * ((d->expected > 0)?1:-1);
      DBG("  [score turn] \"%s\" turn #%d/%d: %.2f[%.2f] / %.2f trn=%.2f length[%d:%d:%d] index=[%d:%d]->[%c:%c]->[%d:%d] (scale=%.2f:%.2f) ---> score=%.2f %s",
	  getNameCharPtr(), i, turn_count, d->actual, d->corrected, d->expected, trn,
	  (int) d->length_before, int(length), (int) d->length_after, d->start_index, index,
	  letter_history[d->start_index], letter_history[index],
	  index_history[d->start_index], index_history[index], scale, scale2, score,
	  d->unmatched?"*unmatched*":"");

      if (scores[index + 1].turn_score >= 0) { scores[index + 1].turn_score = score; }
    }

    // check for expected U-turn vs. ST=2 special points
    for (int i = 1; i < count - 1; i ++) {
      int curve_index = index_history[i];
      int st = curve->getSpecialPoint(curve_index);

      float fail = 0;
      float expected = a_expected[i];

      if (a_same[i] || a_same[i - 1]) { continue; } // case is rare enough to ignore it

      if (abs(expected) > params->st2_max && st != 2 && st != 5) {
	if (st == 1 && abs(getLocalTurn(curve_index)) > 135) {
	  DBG("  [score turn] --- U-turn with no ST=2 (curve_index=%d expected=%.2f) --> ST=1 OK", curve_index, expected);
	} else {
	  DBG("  [score turn] *** U-turn with no ST=2 (curve_index=%d expected=%.2f)", curve_index, expected);
	  fail = (st == 1)?.2:1;
	}
      } else if (abs(expected) < params->st2_min && st == 2) {
	int reason = 0;

	int local_turn = getLocalTurn(curve_index);
	if (abs(local_turn) < params->st2_ignore) {
	  // accidental ST=2 (user is shaking?)
	  DBG("Accidental ST=2 (turn=%d, curve_index=%d)", local_turn, curve_index);
	  reason = 1;
	} else for(int j = 0; j < turn_count; j ++) {
	  turn_t *d = &(turn_detail[j]);
	  int length = d->length;
	  if (abs(d->expected) >= params->st2_min &&
	      abs(d->expected) <= 540 - 2 *  params->st2_min &&
	      i >= d->start_index && i <= d->index &&
	      length < params-> curve_score_min_dist) {
	    reason = 10 + j;
	    break;
	  } else if (i >= d->start_index && i <= d->index && d->start_index < d->index) {
	    // we don't handle long turns
	    reason = 2;
	    break;
	  }
	}
	if (reason) {
	  DBG("  [score turn] --- ST=2 w/o U-turn (curve_index=%d expected=%.2f) --> OK (reason=%d)", curve_index, expected, reason);
	} else {
	  DBG("  [score turn] *** ST=2 w/o U-turn (curve_index=%d expected=%.2f)", curve_index, expected);
	  fail = 1;
	}
      }
      if (fail) {
	float *psc = &(scores[i + 1].turn_score);
	if (! *psc) { *psc = 1; }
	*psc -= fail * params->sharp_turn_penalty;
      }
    } /* for(i) */

  } else {
    // no turn = always a perfect match
    for (int i = 1; i < count - 1; i ++) {
      if (scores[i + 1].turn_score >= 0) { scores[i + 1].turn_score = 1;}
    }
  }

  // "reverse turn score" check the actual curve turn rates match the theoretical turns computed above
  if (! turn_count) {
    check_reverse_turn(0, count - 1, 0, 0);
  } else {
    for(int i = 0; i < turn_count; i++) {
      turn_t *d = &(turn_detail[i]);

      // corrected_direction takes loops into account
      d->corrected_direction = d->direction;

      if (abs(d->actual) > 140 || abs(d->expected) > 140) { // @todo set as parameter
	if (d->start_index == d->index) {
	  int t = curve->getTurnSmooth(index_history[d->index]);
	  d->corrected_direction = (t>0) - (t<0);
	} else if (d->start_index + 1 == d->index && d->length > params->turn_separation / 2) {
	  int t1 = curve->getTurnSmooth(index_history[d->start_index]);
	  int t2 = curve->getTurnSmooth(index_history[d->index]);
	  if (t1 && t2 && t1 * t2 < 0) {
	    DBG("Turn #%d [%d:%d] : partial loop in a multi-letter turns [%d:%d] -> not handled (will pass all checks)",
		d->start_index, d->index, i, t1, t2);
	    d->corrected_direction = RT_DIRECTION_UNKNOWN;
	  }
	}
      }
    }
    for(int i = 0; i < turn_count; i++) {
      turn_t *d1 = &(turn_detail[i]);
      if (i == 0) {
	// check before first turn
	check_reverse_turn(0, d1->start_index, 0, d1->corrected_direction);
      }

      if (d1->index > d1->start_index) {
	// check inside multi-letter turn
	check_reverse_turn(d1->start_index, d1->index, d1->corrected_direction, d1->corrected_direction);
      }

      if (i < turn_count - 1) {
	// check between two turns
	turn_t *d2 = &(turn_detail[i + 1]);
	check_reverse_turn(d1->index, d2->start_index, d1->corrected_direction, d2->corrected_direction);
      } else {
	// check after last turn
	check_reverse_turn(d1->index, this->count - 1, d1->corrected_direction, 0);
      }
    }
  }

  // check for "locally-flat" strokes
  // (if a segment is between curve tips and/or straigt or 180° turns it should be mostly flat)
#define IS_FLAT(a) (abs(a) < params->flat_max_angle || abs(abs(a) - 180) < params->flat_max_angle)

  for(int i = 0; i < count - 1; i ++) {
    if ((i == 0 || IS_FLAT(a_expected[i])) &&
	(i == count - 2 || IS_FLAT(a_expected[i + 1]))) {
      int i1 = index_history[i];
      int i2 = index_history[i + 1];
      Point pt1 = curve->point(i1);
      Point pt2 = curve->point(i2);
      float max_dist = 0;
      for(int j = i1 + 1; j < i2 - 1; j += 2) {
	Point cur_pt = curve->point(j);
	float distance = dist_line_point(pt1, pt2, cur_pt);
	if (distance > max_dist) { max_dist = distance; }
      }
      int max_err = params->flat_max_deviation;
      float score = max(0, max_dist / max_err - 1);
      if (! score) { continue; }
      DBG("[%s] Flat segment not matched: turn #%d->#%d max_dist=%d --> score=%.2f", getNameCharPtr(), i, i + 1, (int) max_dist, score);
      MISC_ACCT(getNameCharPtr(), "flat_score", params->flat_score, -1);
      scores[i].misc_score -= 0.5 * params->flat_score * score;
      scores[i + 1].misc_score -= 0.5 * params->flat_score * score;
    }
  }

}

int Scenario::getLocalTurn(int index) {
  int turn0 = curve->getTurnSmooth(index);
  int total = turn0;
  for(int i = -1 ; i <= 1; i += 2) {
    for (int j = 1; j <= 4; j ++) {
      int turn = curve-> getTurnSmooth(index + i * j);
      if (turn * turn0 < 0) { break; }
      total += turn;
    }
  }
  return total;
}

int Scenario::get_turn_kind(int index) {
  // returns :
  //  1 = normal turn (round)
  //  0 = pointy turn
  // -1 = normal reverse turn (loop)

  int i = index_history[index];

  if (abs(curve->getTurn(i)) > 120 ||
      (abs(curve->getTurn(i - 1) + curve->getTurn(i) + curve->getTurn(i + 1)) > 150)) {
    return 0;
  }

  Point tg_expected = computed_curve_tangent(index);
  Point tg_actual = actual_curve_tangent(i);

  if (tg_expected.x * tg_actual.x + tg_expected.y * tg_actual.y < 0) {
    return -1;
  } else {
    return 1;
  }
}

void Scenario::check_reverse_turn(int index1, int index2, int direction1, int direction2) {
  if (direction1 == RT_DIRECTION_UNKNOWN ||
      direction2 == RT_DIRECTION_UNKNOWN) { return; /* not handled case */ }

  int i1 = index_history[index1];
  int i2 = index_history[index2];

  int threshold = params->rt_turn_threshold;
  float coef_score = (direction1 && direction2)?params->rt_score_coef:params->rt_score_coef_tip;
  int tip_gap = params->rt_tip_gaps;

  int bad = 0;

  int kind1 = 0, kind2 = 0;
  if (curve->getSharpTurn(i1) == 2) {
    kind1 = get_turn_kind(index1);
    if (kind1 == 0) {
      DBG(" [check_reverse_turn] [%d:%d]->[%d:%d] direction=%d:%d --> skipped due to ST=2 point [begin: %d]", index1, index2, i1, i2, direction1, direction2, index1);
      return;
    }
    if (kind1 < 0) { direction1 = -direction1; }
  }
  if (curve->getSharpTurn(i2) == 2) {
    kind2 = get_turn_kind(index2);
    if (kind2 == 0) {
      DBG(" [check_reverse_turn] [%d:%d]->[%d:%d] direction=%d:%d --> skipped due to ST=2 point [end: %d]", index1, index2, i1, i2, direction1, direction2, index2);
      return;
    }
    if (kind2 < 0) { direction2 = -direction2; }
  }

  int direction = direction1;
  int total = 0;
  for(int i = max(tip_gap, i1); i <= min(i2, curve->size() - 1 - tip_gap); i++) {
    int turn = int(0.5 * curve->getTurnSmooth(i) + 0.25 * curve->getTurnSmooth(i - 1) + 0.25 * curve->getTurnSmooth(i));

    total += turn;

    if ((abs(turn) > threshold) && (turn * direction < 0 || ! direction)) {
      if (direction2 != direction1 && direction == direction1 && turn * direction2 >= 0) { direction = direction2; i --; continue; }
      bad ++;
    }
  }

  float score = 1.0 * bad / (i2 - i1 + 1);
  for(int i = index1; i <= index2; i ++) {
    scores[i].misc_score -= coef_score * score / (index2 - index1 + 1);
    MISC_ACCT(getNameCharPtr(), (direction1 && direction2)?"rt_score_coef":"rt_score_coef_tip", coef_score, - score / (index2 - index1 + 1));
  }
  DBG(" [check_reverse_turn] [%d:%d]->[%d:%d] kind=[%d:%d] direction=%d:%d --> score=%.2f", index1, index2, i1, i2, kind1, kind2, direction1, direction2, score);
}

#define ALIGN(a) (min(abs(a), abs(abs(a) - 180)))

float Scenario::calc_score_misc(int i) {
  /* misc scoring heuristic, i is in [0, count - 1] */
  float score = 0;

  /* lower score for small segment at curve begin / end
     -> this means the user has entered a too short curve and missed 2 letters */
  if ((i == 0 && index_history[1] <= 1) ||
      (i == count - 1 && index_history[count - 1] - index_history[count - 2] <= 1)) {
    DBG("  [score misc] small segment at tip (%s)", (i?"end":"begin"));
    score -= params->tip_small_segment;
    MISC_ACCT(getNameCharPtr(), "tip_small_segment", params->tip_small_segment, -1);
  }

  /* speed -> slow down points (ST=3) must be matched with a key
     also reduce score for missed optional points (ST=5) */
  if (i > 0) {
    int i0 = i - 1;
    int i1 = i;
    int maxgap = params->speed_max_index_gap;
    for (int j = index_history[i - 1] + maxgap + 1;
	 j < index_history[i] - maxgap;
	 j ++) {
      int st = curve->getSpecialPoint(j);

      if ((st == 3 || st == 5) && abs(curve->getTurnSmooth(j)) < params->speed_min_angle) {
	DBG("  [score misc] unmatched special point (point=%d, type=%d, index=%d:%d)", j, st, i0, i1);

	float value = (st==3)?params->speed_penalty:params->st5_score;
	score -= value;
	MISC_ACCT(getNameCharPtr(), (st == 3)?"speed_penalty":"st5_score", value, -1);
      }
    }
  }

  /* detect suspect turn rate maxima in the middle of segments */
  if (i < count - 1) {
    int i1 = index_history[i];
    int i2 = index_history[i + 1];
    int max_turn = -1;
    bool peak = false;
    for(int j = i1 + 3 ; j <= i2 - 3; j ++) {
      int turn = abs(curve->getTurnSmooth(j));
      if (turn >= max_turn) {
	max_turn = turn;
	peak = false;
      } else if (! peak) {
	if (max_turn > params->ut_turn) {
	  int i0 = j - 1;
	  char found = 0;
	  int total = curve->getTurn(i0);
	  for(int k = 1; k <= 3; k ++) {
	    total += curve->getTurnSmooth(i0 - k) + curve->getTurnSmooth(i0 + k);

	    int a1 = max(abs(curve->getTurn(i0 - k)), abs(curve->getTurn(i0 - k - 1)));
	    int a2 = max(abs(curve->getTurn(i0 + k)), abs(curve->getTurn(i0 - k + 1)));
	    if (a1 < max_turn * params->ut_coef) { found |= 1; }
	    if (a2 < max_turn * params->ut_coef) { found |= 2; }
	  }
	  if (abs(total) > params->ut_total) {
	    if (found == 3) {
	      DBG("  [score misc] suspect turn rate maxima [%d:%d] index=%d max_turn=%d total=%d", i, i + 1, i0, max_turn, abs(total));
	      score -= params->ut_score;
	      MISC_ACCT(getNameCharPtr(), "ut_score", params->ut_score, -1);
	    }
	  }
	}
	peak = true;
      }
    }
  }

  // find bad tangents at curve tips
  // (usefull for small turn that would be overlooked by turn score)
  int cl = curve->size();
  if (cl > 4 && count > 1) {
    Point tg_act, tg_exp;
    bool proceed = true;
    if (i == 0) {
      tg_act = curve->point(2) - curve->point(0);
      tg_exp = keys->get(letter_history[1]) - keys->get(letter_history[0]);
    } else if (i == count - 1) {
      tg_act = curve->point(cl - 1) - curve->point(cl - 3);
      tg_exp = keys->get(letter_history[count - 1]) - keys->get(letter_history[count - 2]);
    } else {
      proceed = false;
    }

    if (proceed) {
      float acos = cos(anglep(tg_act, tg_exp));
      if (acos < 0) {
	DBG("  [score misc] bad %s tip tangent : acos()=%.2f", (i == 0)?"begin":"end", acos);
	score += .2 * acos; // hardcoded because I did not find any case where value is important
	MISC_ACCT(getNameCharPtr(), "None", .2, acos);
      }
    }
  }

  return score;
}

void Scenario::calc_straight_score_all(turn_t *turn_detail, int turn_count, float straight_score) {
  float result = 0;

  int real_turn_count = 0;
  for (int i = 0; i < turn_count; i ++) {
    turn_t *d = &(turn_detail[i]);
    if ((abs(d->expected_real) < 10) ||
	((d->length_after < 45 || d->length_before < 45) && (abs(d->expected_real) < 30))) { // @todo use parameters
      // not "significant" turn
    } else {
      real_turn_count ++;
    }
  }

  if (straight_score < 1) {
    /* user has drawn a straight line (score is 0 for a perfect stroke)
       we should not accept scenarios with turns (or they must be very small or near small tips) */
    float coef = 1 - straight_score;
    if (real_turn_count > 0) {
      result = - params->straight_score1 * coef * real_turn_count;
      DBG(" [score_straight] (1) bad=%d score=%.2f", real_turn_count, result);
    }

    /* check if line orientation is OK */
    float a_sin;
    if (letter_history[0] == letter_history[count - 1]) {
      a_sin = 1;
    } else {
      Point k1 = keys->get_raw(letter_history[0]);
      Point k2 = keys->get_raw(letter_history[count - 1]);
      Point p1 = curve->point(0);
      Point p2 = curve->point(index);
      a_sin = abs(sin_angle(k2.x - k1.x, k2.y - k1.y, p2.x - p1.x, p2.y - p1.y));
    }

    result += params->straight_slope * (sqrt(1 - a_sin * a_sin) - 1);


  } else {
    /* other curve: penalty for straight candidates (geometrically speaking) */
    if (! real_turn_count) {
      result = - params->straight_score2 * min(1, straight_score - 1);
      DBG(" [score_straight] (2) score=%.2f", result);
    }
  }

  // spread score as this is a global score
  for(int i = 0; i < count; i ++) {
    MISC_ACCT(getNameCharPtr(), "straight", 1, result / count);
    scores[i].misc_score += result / count;
  }

}

void Scenario::calc_loop_score_all(turn_t *turn_detail, int turn_count) {
  for (int i = 0; i < turn_count; i ++) {
    turn_t *d = &(turn_detail[i]);

    for(int j = d->start_index; j <= d->index; j++) {
      if (get_turn_kind(j) == -1) {
	int exp = d->expected;
	bool ok = false;

	if (abs(exp) > 170) {
	  ok = true;
	} else if (abs(exp) > 120 && i > 0 && i < turn_count - 1) {
	  int exp1 = turn_detail[i - 1].expected;
	  int exp2 = turn_detail[i + 1].expected;
	  if ((exp1 * exp < 0 && abs(exp1) > 60) ||
	      (exp2 * exp < 0 && abs(exp2) > 60)) {
	    ok = true;
	  }
	}

	DBG("Loop detected: turn #%d (index %d, letter '%c')  OK=%d", i, j, letter_history[j], (int) ok);
	if (! ok) {
	  scores[j].misc_score -= params->loop_penalty;
	  MISC_ACCT(getNameCharPtr(), "loop_penalty", params->loop_penalty, -1);
	}
      }
    }

  }
}

void Scenario::newDistance() {
  /* new distance between candidates and expected words
     it aims at better accuracy (more likely to find the right word), and if it
     fails, the expected word is likely to have an higher distance but with a
     small difference */
  float dist_exp = 0;
  QString str;
  QTextStream qs(& str);

  float coef[10] = { 1., params->newdist_c1, params->newdist_c2, params->newdist_c3, 0., params->newdist_c5,
		     0., 0., 0., params->newdist_ctip };
  float exposant = params->newdist_pow;

  // display match-point/key distance
  float ctotal = 0;
  for (int i = 0; i < count; i ++) {
    Point key = keys->get(letter_history[i]);
    Point pt = curve->point(index_history[i]);
    int speed = curve->getSpeed(index_history[i]);
    float dist;

    // @todo retry my good old "anisotropic distance"
    // /* score ignored */ calc_distance_score(letter_history[i], index_history[i], (i == count - 1)?-1:i, &dist);

    int st = curve->getSpecialPoint(index_history[i]);

    if (st == 5 && i > 0 && i < count - 1) {
      Point v1 = curve->point(index_history[i + 1]) - curve->point(index_history[i - 1]);
      Point v2 = key - pt;
      dist = abs(v1.x * v2.y - v1.y * v2.x) / distancep(Point(0, 0), v1);
    } else {
      dist = distancep(key, pt);
    }

    int st2 = (i == 0 || i == count - 1)?9:st; /* tip */

    float c = coef[st2] / (1. + params->newdist_speed * speed / 1000.);
    ctotal += c;

    dist_exp += c * pow(dist, exposant);

    qs << "#" << i << "[" << (char) letter_history[i] << "," << st2 << "," << speed << "]=" << int(dist) << " ";
  }
  float new_dist = pow(getCount(), params->newdist_length_bias_pow) * pow(dist_exp / ctotal, 1 / exposant);

  qs << "=> " << int(new_dist);

  DBG("New distance(%s): %s", getNameCharPtr(), QSTRING2PCHAR(str));
  this->new_dist = new_dist;
}

bool Scenario::postProcess() {
  DBG("==== Postprocess: %s", getNameCharPtr());

  newDistance(); // evaluate improved distance

  QString str;
  QTextStream qs(& str);

  // display match-point/key distance
  for (int i = 0; i < count; i ++) {
      Point key = keys->get(letter_history[i]);
      Point pt = curve->point(index_history[i]);
      int dx = key.x - pt.x;
      int dy = key.y - pt.y;
      float dist = sqrt(dx * dx + dy * dy);
      qs << "#" << i << ": " << int(dist) << " ";
  }
  qs << "=> " << int(this->dist);
  DBG("Distance(%s): %s", getNameCharPtr(), QSTRING2PCHAR(str));

  if (count == 1) {
    // special case: curve is just a click (so it always has a perfect score)
    evalScore();
    return true;
  }

  for(int i = 0; i < count; i++) {
    scores[i].misc_score = calc_score_misc(i);
  }

  turn_t turn_detail[count];
  int turn_count;
  calc_turn_score_all(turn_detail, &turn_count);

  calc_straight_score_all(turn_detail, turn_count, curve->straight);

  calc_loop_score_all(turn_detail, turn_count);

  /* particular case: simple turn (ST=1) shared between two keys
     this is an ugly workaround: curve_score works really badly in this case
     fortunately, this is very rare
  */
  int gap = params->max_turn_index_gap;
  for(int i = 0; i < count - 1; i++) {
    int i1 = index_history[i], i2 = index_history[i + 1];
    for(int j = i1 + 1; j < i2 - 1; j ++) {
      if (curve->getSharpTurn(j) == 1 && j - i1 <= gap && i2 - j <= gap) {
	if (scores[i + 1].curve_score < 0) { error_count --; /* we've just canceled an error */ }
	scores[i + 1].curve_score = 0;
	DBG("  workaround: %s shared turn point (ST=1) %c:%c", getNameCharPtr(), letter_history[i], letter_history[i + 1]);
	float len = distancep(curve->point(i1), curve->point(i2));
	float dlp = dist_line_point(curve->point(i1), curve->point(i2), curve->point(j));
	if (dlp > len) {
	  scores[i + 1].misc_score -= params->sp_bad;
	  MISC_ACCT(getNameCharPtr(), "sp_bad", params->sp_bad, -1);
	}
      }
    }
  }

  return (evalScore() > 0);
}

float Scenario::evalScore() {
  DBG("==== Evaluating final score: %s", getNameCharPtr());
  this -> score_v1 = 0;

  float segment_length[count];
  float total_length = 0;
  for(int i = 0; i < count - 1; i ++) {
    Point k1 = keys->get(letter_history[i]);
    Point k2 = keys->get(letter_history[i + 1]);
    float d = distancep(k1, k2);
    segment_length[i] = d;
    total_length += d;
  }

  // columns named with strings is inefficient, but we don't care because this is done only at the end for the few remaining candidates
  char* cols[] = { (char*) "angle", (char*) "curve", (char*) "misc", (char*) "dist", (char*) "length", (char*) "turn", 0 };
  float weights[] = { params -> weight_cos, params -> weight_curve, - params -> weight_misc, params -> weight_distance, - params -> weight_length, params -> weight_turn };

  ScoreCounter sc;
  sc.set_debug(debug); // will draw nice tables :-)
  sc.set_cols(cols);
  sc.set_weights(weights);
  sc.set_pow(params -> score_pow);

  for(int i = 0; i < count; i++) {
    // key scores

    sc.start_line();
    sc.set_line_coef(1.0 / count);
    if (debug) { sc.line_label << "Key " << i << ":" << index_history[i] << ":" << QString(letter_history[i]); }
    sc.add_score(scores[i].distance_score, (char*) "dist");
    if (i > 0 && i < count - 1) {
      sc.add_score(scores[i + 1].turn_score, (char*) "turn");
    } else if (i == 1 && count == 2) {
      sc.add_score(1, (char*) "turn");
      // avoid bias towards 3+ letters scenarios
    } else if (i == 0 && count == 1) {
      sc.add_score(1, (char*) "turn");
      sc.add_score(1, (char*) "angle");
      sc.add_score(1, (char*) "curve");
      // single click curve has always perfect scores :-)
      // (except maybe for distance)
    }
    sc.end_line();

    // segment scores
    sc.start_line();
    if (i < count - 1) {
      sc.set_line_coef(segment_length[i] / total_length);
      if (debug) { sc.line_label << "Segment " << i + 1 << " [" << index_history[i] << ":" << index_history[i + 1] << "]"; }
      sc.add_score(scores[i + 1].cos_score, (char*) "angle");
      sc.add_score(scores[i + 1].curve_score, (char*) "curve");
      sc.add_score(scores[i + 1].length_score, (char*) "length");
    }
    sc.add_bonus(scores[i].misc_score, (char*) "misc");
    sc.end_line();
  }

  float score1 = sc.get_score();
  float score = score1 * (1.0 + params->length_penalty * count); // my scores have a bias towards number of letters

  /* store column score */
  avg_score.distance_score = sc.get_column_score((char*) "dist");
  avg_score.turn_score = sc.get_column_score((char*) "turn");
  avg_score.cos_score = sc.get_column_score((char*) "angle");
  avg_score.curve_score = sc.get_column_score((char*) "curve");
  avg_score.length_score = sc.get_column_score((char*) "length");
  avg_score.misc_score = sc.get_column_score((char*) "misc");

  min_score.distance_score = sc.get_column_min_score((char*) "dist");
  min_score.turn_score = sc.get_column_min_score((char*) "turn");
  min_score.cos_score = sc.get_column_min_score((char*) "angle");
  min_score.curve_score = sc.get_column_min_score((char*) "curve");
  min_score.length_score = sc.get_column_min_score((char*) "length");
  min_score.misc_score = sc.get_column_min_score((char*) "misc");

  DBG("==== %s --> Score: %.3f --> Final Score: %.3f", getNameCharPtr(), score1, score);

  min_total = sc.get_min_score();

  this -> score_v1 = score; // used only for logging
  return score;
}

static void scoreToJson(QJsonObject &json_obj, score_t &score) {
  json_obj["score_distance"] = score.distance_score;
  json_obj["score_cos"] = score.cos_score;
  json_obj["score_turn"] = score.turn_score;
  json_obj["score_curve"] = score.curve_score;
  json_obj["score_misc"] = score.misc_score;
  json_obj["score_length"] = score.length_score;
}

void Scenario::toJson(QJsonObject &json) {
  json["name"] = getName();
  json["finished"] = finished;
  json["score"] = getScore();
  json["score_std"] = -1;
  json["score_v1"] = score_v1;
  json["distance"] = (int) distance();
  json["class"] = result_class;
  json["star"] = star;
  json["min_total"] = min_total;
  json["error"] = error_count;
  json["good"] = good_count;
  json["words"] = getWordList();
  json["new_dist"] = (int) new_dist;

  QJsonArray json_score_array;
  for(int i = 0; i < count; i ++) {
    QJsonObject json_score;
    json_score["letter"] = QString(letter_history[i]);
    json_score["index"] = index_history[i];
    scoreToJson(json_score, scores[i]);
    json_score_array.append(json_score);
  }
  json["detail"] = json_score_array;

  QJsonObject json_min, json_avg;
  scoreToJson(json_min, min_score);
  scoreToJson(json_avg, avg_score);
  json["avg_score"] = json_avg;
  json["min_score"] = json_min;
}

QString Scenario::toString(bool indent) {
  QJsonObject json;
  toJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

void Scenario::setDebug(bool debug) {
  this -> debug = debug;
}

void Scenario::getDetailedScores(score_t &avg, score_t &min) const {
  avg = avg_score;
  min = min_score;
}

float Scenario::distance() const {
  return dist;
}

int Scenario::getTimestamp() const {
  return index?curve->getTimestamp(index - 1):curve->getTimestamp(0);
}

score_t Scenario::getScoreIndex(int i) {
  return scores[i];
}

void Scenario::sortCandidates(QList<Scenario*> candidates, Params &params, int debug) {
  /* try to find the most likely candidates by combining multiple methods :
     - score (linear combination of multiple scores)
     - distance (sum of square distance between expected keys and curve), as calculated by Scenario::distance()
     - error count
  */

  if (candidates.size() == 0) { return; }

  QElapsedTimer timer;
  timer.start();

  int n = candidates.size();

  float min_dist = 0;
  float max_score_v1 = 0;
  for(int i = 0; i < n; i ++) {
    if (candidates[i]->getNewDistance() < min_dist || i == 0) {
      min_dist = candidates[i]->getNewDistance();
    }
    if (candidates[i]->getScoreV1() > max_score_v1) {
      max_score_v1 = candidates[i]->getScoreV1();
    }
  }

  float quality = 0;
  for(int i = 0; i < n; i ++) {
    float new_score = candidates[i]->getScoreV1()
      - params.coef_error * min(2, candidates[i]->getErrorCount());
    if (new_score > quality) { quality = new_score; }
  }
  DBG("Quality index %.3f", quality)

  float tmpsc[n];
  float max_score = 0;
  for(int i = 0; i < n; i ++) {
    score_t sc = candidates[i]->getScores();
    float new_score = (params.final_coef_misc * sc.misc_score
		       + params.final_coef_turn * pow(max(0, sc.turn_score), params.final_coef_turn_exp)
		       - params.final_score_v1_coef * max(0, max_score_v1 - params.final_score_v1_threshold - candidates[i]->getScoreV1())
		       /* old distance: - 0.1 * signpow(0.1 * (candidates[i]->distance() - min_dist), params.final_distance_pow) */
		       - 0.1 * pow((candidates[i]->getNewDistance() - min_dist) / params.final_newdist_range, params.final_newdist_pow)
		       ) / (1 + params.final_coef_turn)
      - params.coef_error * candidates[i]->getErrorCount();
    tmpsc[i] = new_score;
    if (new_score > max_score) { max_score = new_score; }
  }
  DBG("Quality index %.3f", max_score)
  for(int i = 0; i < n; i ++) {
    candidates[i]->setScore(quality + tmpsc[i] - max_score);
  }

  /* @todo classifier replacement ? */

  int elapsed = timer.elapsed();
  DBG("Sort time: %d", elapsed);

  if (debug) {
    logdebug("SORT>                                     | ----------[average]---------- | ------------[min]------------ |     |       |");
    logdebug("SORT> ## =word======= =score =min E G dst | =cos curv dist =len misc turn | =cos curv dist =len misc turn | cls | final |");
    for(int i = n - 1; i >=0 && i >= n - params.max_candidates; i --) {
      Scenario *candidate = candidates[i];
      score_t smin, savg;
      candidate->getDetailedScores(savg, smin);
      float score = candidate->getScore();
      float min_score = candidate->getMinScore();
      int error_count = candidate->getErrorCount();
      int good_count = candidate->getGoodCount();
      int dist = (int) candidate->distance();
      unsigned char *name = candidate->getNameCharPtr();
      logdebug("SORT> %2d %12s %5.2f %5.2f %1d %1d%4d |%5.2f%5.2f%5.2f%5.2f%5.2f%5.2f |%5.2f%5.2f%5.2f%5.2f%5.2f%5.2f |%3d%c |%6.3f |",
	       i, name, score, min_score, error_count, good_count, dist,
	       savg.cos_score, savg.curve_score, savg.distance_score, savg.length_score, savg.misc_score, savg.turn_score,
	       smin.cos_score, smin.curve_score, smin.distance_score, smin.length_score, smin.misc_score, smin.turn_score,
	       candidate->getClass(), candidate->getStar()?'*':' ', score);
    }
  }
}

bool Scenario::nextLength(unsigned char next_letter, int curve_id, int &min_length, int &max_length) {
  /* compute minimal curve for evaluating a possible child scenario */

  if (curve_id > 0) { return false; }
  if (! count) { min_length = max_length = 1; return true; }

  unsigned char last_letter = letter_history[count - 1];
  int last_length = curve->getLength(index_history[count - 1]);

  if (last_letter == next_letter) { max_length = min_length = last_length; return true; }

  Point lk = keys->get(last_letter);
  Point nk = keys->get(next_letter);

  int dist = distancep(lk, nk);

  /*
    compute minimal curve length to evaluate this scenario
    (currently it's value is last key curve length + distance between the two keys + 2 * max distance error
    which means, that matching will always ~200 pixels late (with current settings)
    max_length is a pessimistic guess (it prevent retries and uses the less cpu time)
    min_length is an optimistic guess
  */

  max_length = last_length  + (1.0 + (float)dist / params->dist_max_next / 20) * (params->incremental_length_lag + dist);
  min_length = last_length + max(0, dist - params->incremental_length_lag / 2);

  return true;
}

void Scenario::setCache(bool value) {
  cache = value;
  if (cache) {
    cacheChilds = QSharedPointer<child_cache_t>(new child_cache_t);
  }
}

