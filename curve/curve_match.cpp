#include "curve_match.h"
#include "score.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTextStream>
#include <cmath> 
#include <iostream>
#include <math.h>

#include "default_params.h"
#include "functions.h"

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

void Params::toJson(QJsonObject &json) const {
  json["dist_max_start"] = dist_max_start;
  json["dist_max_next"] = dist_max_next;
  json["match_wait"] = match_wait;
  json["max_angle"] = max_angle;
  json["max_turn_error1"] = max_turn_error1;
  json["max_turn_error2"] = max_turn_error2;
  json["max_turn_error3"] = max_turn_error3;
  json["length_score_scale"] = length_score_scale;
  json["curve_score_min_dist"] = curve_score_min_dist;
  json["score_pow"] = score_pow;
  json["weight_distance" ] = weight_distance;
  json["weight_cos"] = weight_cos;
  json["weight_length"] = weight_length;
  json["weight_curve"] = weight_curve;
  json["weight_curve2"] = weight_curve2;
  json["weight_turn"] = weight_turn;
  json["length_penalty"] = length_penalty;
  json["turn_threshold"] = turn_threshold;     
  json["max_turn_index_gap"] = max_turn_index_gap;
  json["curve_dist_threshold"] = curve_dist_threshold;
  json["small_segment_min_score"] = small_segment_min_score;
  json["anisotropy_ratio"] = anisotropy_ratio;
  json["sharp_turn_penalty"] = sharp_turn_penalty;
  json["curv_amin"] = curv_amin;
  json["curv_amax"] = curv_amax;
  json["curv_turnmax"] = curv_turnmax;
  json["max_active_scenarios"] = max_active_scenarios;
  json["max_candidates"] = max_candidates;
  json["score_coef_speed"] = score_coef_speed;
  json["angle_dist_range"] = angle_dist_range;
  json["incremental_length_lag"] = incremental_length_lag;
  json["incremental_index_gap"] = incremental_index_gap;
}

Params Params::fromJson(const QJsonObject &json) {
  Params p;
  p.dist_max_start = json["dist_max_start"].toDouble();
  p.dist_max_next = json["dist_max_next"].toDouble();
  p.match_wait = json["match_wait"].toDouble();
  p.max_angle = json["max_angle"].toDouble();
  p.max_turn_error1 = json["max_turn_error1"].toDouble();  
  p.max_turn_error2 = json["max_turn_error2"].toDouble();  
  p.max_turn_error3 = json["max_turn_error3"].toDouble();  
  p.length_score_scale = json["length_score_scale"].toDouble();
  p.curve_score_min_dist = json["curve_score_min_dist"].toDouble();
  p.score_pow = json["score_pow"].toDouble();
  p.weight_distance = json["weight_distance"].toDouble();
  p.weight_cos = json["weight_cos"].toDouble();
  p.weight_length = json["weight_length"].toDouble();
  p.weight_curve = json["weight_curve"].toDouble();
  p.weight_curve2 = json["weight_curve2"].toDouble();
  p.weight_turn = json["weight_turn"].toDouble();
  p.length_penalty = json["length_penalty"].toDouble();
  p.turn_threshold = json["turn_threshold"].toDouble();
  p.max_turn_index_gap = json["max_turn_index_gap"].toDouble();
  p.curve_dist_threshold = json["curve_dist_threshold"].toDouble();
  p.small_segment_min_score = json["small_segment_min_score"].toDouble();
  p.anisotropy_ratio = json["anisotropy_ratio"].toDouble();
  p.sharp_turn_penalty = json["sharp_turn_penalty"].toDouble();
  p.curv_amin = json["curv_amin"].toDouble();
  p.curv_amax = json["curv_amax"].toDouble();
  p.curv_turnmax = json["curv_turnmax"].toDouble();
  p.max_active_scenarios = json["max_active_scenarios"].toDouble();
  p.max_candidates = json["max_candidates"].toDouble();
  p.score_coef_speed = json["score_coef_speed"].toDouble();
  p.angle_dist_range = json["angle_dist_range"].toDouble();
  p.incremental_length_lag = json["incremental_length_lag"].toDouble();
  p.incremental_index_gap = json["incremental_index_gap"].toDouble();

  return p;
}

/* --- optimized curve --- */
QuickCurve::QuickCurve() {
  count = -1;
}

QuickCurve::QuickCurve(QList<CurvePoint> &curve) {
  count = -1;
  setCurve(curve);
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
  }
  count = -1;
}

void QuickCurve::setCurve(QList<CurvePoint> &curve) {
  clearCurve();  
  count = curve.size();
  if (! count) { return; }

  x = new int[count];
  y = new int[count];
  turn = new int[count];
  turnsmooth = new int[count];
  sharpturn = new int[count];
  normalx = new int[count];
  normaly = new int[count];
  speed = new int[count];
  points = new Point[count];
  for (int i = 0; i < count; i++) {
    CurvePoint p = curve[i];
    x[i] = p.x;
    y[i] = p.y;
    turn[i] = p.turn_angle;
    turnsmooth[i] = p.turn_smooth;
    sharpturn[i] = p.sharp_turn;
    normalx[i] = p.normalx;
    normaly[i] = p.normaly;
    speed[i] = p.speed;
    points[i].x = p.x;
    points[i].y = p.y;
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
int QuickCurve::getSharpTurn(int index) { return sharpturn[index]; }
Point QuickCurve::getNormal(int index) { return Point(normalx[index], normaly[index]); }
int QuickCurve::size() { return count; }
int QuickCurve::getNormalX(int index) { return normalx[index]; }
int QuickCurve::getNormalY(int index) { return normaly[index]; }
int QuickCurve::getSpeed(int index) { return speed[index]; }

/* optimized keys information */
QuickKeys::QuickKeys() {
  points = new Point[256];  // unsigned char won't overflow
}

QuickKeys::QuickKeys(QHash<unsigned char, Key> &keys) {
  points = new Point[256]; 
  setKeys(keys);
}

void QuickKeys::setKeys(QHash<unsigned char, Key> &keys) {
  foreach(unsigned char letter, keys.keys()) {
    points[letter].x = keys[letter].x;
    points[letter].y = keys[letter].y;
  }
}

QuickKeys::~QuickKeys() {
  delete[] points;
}

Point const& QuickKeys::get(unsigned char letter) const {
  return points[letter];
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

CurvePoint::CurvePoint(Point p, int t) : Point(p.x, p.y) {
  this -> t = t;
  this -> speed = 1;
  this -> sharp_turn = false;
  this -> turn_angle = 0;
  this -> turn_smooth = 0;
  this -> normalx = this -> normaly = 0;
}

/* --- key --- */
Key::Key(int x, int y, int width, int height, char label) {
  this -> x = x;
  this -> y = y;
  this -> width = width;
  this -> height = height;
  this -> label = label;
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
}

Key Key::fromJson(const QJsonObject &json) {
  Key key;
  key.x = json["x"].toDouble(); // QT too old, no toInt() yet ?
  key.y = json["y"].toDouble();
  key.width = json["w"].toDouble();
  key.height = json["h"].toDouble();
  key.label = QSTRING2PCHAR(json["k"].toString())[0];
  return key;
}

/* --- scenario --- */
Scenario::Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curve, Params *params) {
  this -> node = tree -> getRoot();
  this -> keys = keys;
  this -> curve = curve;
  finished = false;
  count = 0;
  index = 0;
  this -> params = params;
  temp_score = 0;
  final_score = -1;
  last_fork = -1;

  index_history = new unsigned char[1];
  letter_history = new unsigned char[1];
  scores = new score_t[1];

  letter_history[0] = 0;
}

Scenario::Scenario(const Scenario &from) {
  // overriden copy to take care of dynamically allocated stuff
  copy_from(from);
}

Scenario& Scenario::operator=( const Scenario &from) {
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
  last_fork = from.last_fork;
  debug = from.debug;
  
  keys = from.keys;
  curve = from.curve;
  params = from.params;

  index_history = new unsigned char[count + 1];
  letter_history = new unsigned char[count + 2]; // one more char to allow to keep a '\0' at the end (for use for display as a char*)
  scores = new score_t[count + 1];
  for (int i = 0; i < count; i ++) {
    index_history[i] = from.index_history[i];
    letter_history[i] = from.letter_history[i];
    scores[i] = from.scores[i];
  }
  letter_history[count] = 0;
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
  float a_sin = abs(sin_angle(k2.x - k1.x, k2.y - k1.y, p2.x - p1.x, p2.y - p1.y));

  float len = distancep(p1, p2);
  float len_coef = len / params -> angle_dist_range; // reusing another parameter, but fine adjusting can be done with max_angle
  if (len_coef < 1) { len_coef = 1; }
  float max_sin = sin(params -> max_angle * M_PI / 180) / len_coef;
  
  float score = 1 - a_sin / max_sin;

  DBG("  [cos score] %s:%c i=%d:%d len_coef=%.2f angle=%.2f/%.2f -> score=%.2f", 
      getNameCharPtr(), letter, index, new_index, len_coef, a_sin, max_sin, score);
  
  return score;
}

float Scenario::calc_turn_score(unsigned char letter, int index) {
  /* score based on angle between two consecutive segments */

  if (count < 2) { return 0; }

  // curve index
  int i1 = index_history[count - 2];
  int i2 = index_history[count - 1];
  int i3 = index;

  if ((i1 == i2) || (i2 == i3)) {
    // two letters matched at the same point -> we can't compute this score
    return 0;
  }

  // curve points
  Point p1 = curve->point(i1);
  Point p2 = curve->point(i2);
  Point p3 = curve->point(i3);

  // letters
  unsigned char l1 = letter_history[count - 2];
  unsigned char l2 = letter_history[count - 1];
  unsigned char l3 = letter;  

  // key coordinates
  Point k1 = keys->get(l1);
  Point k2 = keys->get(l2);
  Point k3 = keys->get(l3);

  // compare angles
  float expected = anglep(k2 - k1, k3 - k2) * 180 / M_PI;
  float actual = anglep(p2 - p1, p3 - p2) * 180 / M_PI;

  float diff = abs(actual - expected);
  if (diff > 180) { diff = abs(diff - 360); }

  /* @todo adjust for distance
  float dist1 = distancep(k1, k2);
  float dist2 = distancep(k2, k3);
  float dist = (dist1 < dist2)?dist1:dist2;
  float dist_coef = 1;
  if (dist > params -> angle_dist_range) {
    dist_coef = dist / params -> angle_dist_range; // really simplified
  }
  */

  // thresholds
  int t1 = params->max_turn_error1;
  int t2 = params->max_turn_error2;
  int t3 = params->max_turn_error3;

  int t = (abs(expected)<90?t2:t3);
  if (t2 < t1 || t2 < t3) { return -1; } // in case the optimizer goes nuts

  float scale = t + (t - t1) * abs(sin(expected * M_PI / 180));

  if (expected * actual < 0 && abs(actual) > 5 && abs(expected) > 5) { diff *= 2; } // penalty for inverted angle

  float score = 1 - pow(diff / scale, 2);

  DBG("  [turn score] %s:%c i=%d:%d:%d angle exp/act=%.2f/%.2f -> score=%.2f", 
      getNameCharPtr(), letter, i1, i2, i3, expected, actual, score)

  return score;
}

Point Scenario::curve_tangent(int index) {
  int i1 = index_history[index];
  Point d1, d2;
  Point origin(0, 0);
  if (index > 0) {
    int i0 = index_history[index - 1];
    d1 = curve->point(i1) - curve->point(i0);
    d1 = d1 * (1000 / distancep(origin, d1)); // using integers was clearly a mistake
  }
  if (index < count - 1) {
    int i2 = index_history[index + 1];
    d2 = curve->point(i2) - curve->point(i1);
    d2 = d2 * (1000 / distancep(origin, d2));
  }
  return d1 + d2;
}

float Scenario::begin_end_angle_score(bool end) {
  /* bonus score for curve ends pointing in the right direction */

  Point expected, actual;
  Point origin(0, 0);

  if (end) {
    int nc = curve->size();

    if (index_history[count - 2] > nc - 2) { return 0; }

    Point k1 = keys->get(letter_history[count - 2]);
    Point k2 = keys->get(letter_history[count - 1]);
    expected = k2 - k1;

    actual = curve->point(nc - 1) - curve->point(nc - 3);

  } else {
    if (index_history[1] < 2) { return 0; }

    Point k1 = keys->get(letter_history[0]);
    Point k2 = keys->get(letter_history[1]);
    expected = k2 - k1;

    actual = curve->point(2) - curve->point(0);
  }

  float cosa = cos_angle(expected.x, expected.y, actual.x, actual.y);
  float sc = 0.5 - pow(acos(cosa) / (params->max_angle * M_PI / 180), 2) / 2; // this is a relative score

  if (distancep(actual, origin) < params->curve_score_min_dist) {
    // direction of a small segment may be irrelevant
    if (sc < 0) { sc = 0; }
  }
  
  DBG("  [begin_end_angle_score] %s end=%d -> score=%.2f", getNameCharPtr(), end, sc);

  return sc;
}

float Scenario::calc_curviness2_score(int index) {
  int i1 = index_history[index];
  int i2 = index_history[index + 1];
  
  if (i2 <= i1 + 1) { return 0; }

  float total = 0;
  int maxv = 0;
  for (int i = i1; i <= i2; i ++) {
    float coef = cos(M_PI / 180 * i / (i2 - i1));
    float abv = abs(curve->getTurnSmooth(i));
    total += coef * abv;
    if (abv > maxv) { maxv = abv; }
  }
  return 1.0 * total / (i2 - i1 + 1) / maxv;
}
    

float Scenario::calc_curviness_score(int index) {
  /* score based on expected radius of curvature sign and derivative */
  int i1 = index_history[index];
  int i2 = index_history[index + 1];

  if ((index > 0 && index_history[index - 1] == i1) ||
      (index < count - 1 && index_history[index + 1] == i2)) {
    // could not compute tangents, so just skip this
    return 0;
  }

  Point p = curve->point(i2) - curve->point(i1);
  Point v1 = curve_tangent(index);
  Point v2 = curve_tangent(index + 1);

  float a1 = anglep(v1, p) * 180.0 / M_PI;
  float a2 = anglep(p, v2) * 180.0 / M_PI;
  
  int angle_threshold = 15;
  int s1 = (a1 > angle_threshold) - (a1 < -angle_threshold); // i found this on stackoverflow, it must be good
  int s2 = (a2 > angle_threshold) - (a2 < -angle_threshold);

  float result = 0;
  int max_inflection_point = 1;
  int expected_sign = 0;
  int sum_bad = -1;
  if (curve->getTurnSmooth(i1) * s1 < 0 && abs(a1) < params->curv_amax) {
    result = -1;
  } else if (curve->getTurnSmooth(i2) * s2 < 0 && abs(a2) < params->curv_amax) {
    result = -1;
  } else {
    if ((s1 * s2 == 1) || (s1 && index == count - 2) || (s2 && index == 0)) {
      max_inflection_point = 0;
      expected_sign = (s1 + s2 > 0?1:-1);
    }

    int infcount = 0;
    int sign = 0;
    sum_bad = 0;
    for (int i = i1; i <= i2; i++) {
      int value = curve->getTurnSmooth(i);
      if (abs(value) > params->curv_amin && value * sign <= 0) {
	if (sign) { infcount += 1; }
	sign = (value > 0)?1:-1;
      }
      if (value * expected_sign < 0) {
	sum_bad += value;
      }
    }
    if (infcount > max_inflection_point) {
      if (max_inflection_point > 0) {
	result = -0.5 * (infcount - max_inflection_point);
      } else {
	result = - pow((float) sum_bad / params->curv_turnmax, 2);
      }
    }
  }

  DBG("  [curviness score] %s[%d] i=%d:%d a=%.3f/%.3f s=%d/%d [maxpt=%d] score=%.3f (sum_bad=%d exp_sign=%d)", 
      getNameCharPtr(), index, i1, i2, a1, a2, s1, s2, max_inflection_point, result, sum_bad, expected_sign);

  return result;
}

float Scenario::calc_curve_score(unsigned char, unsigned char letter, int index, int new_index) {
  /* score based on curve "smoothness" and maximum distance from straight path */

  float max_dist = 0;
  float max_distp = 0;
  int sharp_turn = 0;

  Point pbegin = curve->point(index);
  Point pend =  curve->point(new_index);

  int length = max(distancep(pbegin, pend), params->dist_max_next);
  

  Point middle((pbegin.x + pend.x) / 2, (pbegin.y + pend.y) / 2);

  for (int i = index + 2; i < new_index - 1; i += 4) {
    Point p = curve->point(i);
    float dist = dist_line_point(pbegin, pend, p);
    if (dist > max_dist) { max_dist = dist; }

    float distp = distancep(middle, p);
    if (distp > max_distp) { max_distp = distp; }
  }

  for (int i = index + 1; i < new_index; i += 1) {
    if (i > index + 2 && i < new_index - 2 && curve->getSharpTurn(i) && i >= 2 && i < curve->size() - 2) { sharp_turn ++; }
  }

  float s1 = max_dist / params->curve_dist_threshold;
  float s2 = params->sharp_turn_penalty * sharp_turn;
  float s3 = max(0.0, 1.0 * (max_distp - length) / length);

  float score = 1 - s1 - s2 - s3;
  DBG("  [curve score] %s:%c[%d->%d] sc=[%.3f:%.3f:%.3f] max_dist=%d max_distp=%d length=%d score=%.3f",
      getNameCharPtr(), letter, index, new_index, s1, s2, s3, (int) max_dist, (int) max_distp, length, score);

  return score;
}

float Scenario::calc_length_score(unsigned char prev_letter, unsigned char letter, int index, int new_index) {
  /* score based on distance between points for 2 successive keys */

  Point k1 = keys->get(prev_letter);
  Point k2 = keys->get(letter);

  float expected = distancep(k1, k2);
  float actual = distance(curve->getX(index), curve->getY(index), curve->getX(new_index), curve->getY(new_index));

  float scale = (float) params->length_score_scale;
  return 1 - pow((actual - expected) / scale / (1 + 0.2 * expected / scale), 2); // add 20% error margin for each "distance scale"
}

float Scenario::calc_distance_score(unsigned char letter, int index, int count) {
  /* score based on distance to from curve to key */

  float ratio = (count > 0)?params->dist_max_next:params->dist_max_start;
  Point k = keys->get(letter);
  
  float cplus;
  float cminus; 
  float dx, dy;

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
    dx = curve->getNormalX(index) * 100;
    dy = curve->getNormalY(index) * 100; // why did i use integers ?!
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
  }
  
  float score = 1 - dist;

  // @todo add a "more verbose" debug option
  //DBG("    distance_score[%c:%d] offset=%d:%d direction=%d:%d u=%.3f v=%.3f coefs=%.2f:%.2f dist=%.3f score=%.3f",
  //    letter, index, (int) px, (int) py, (int) dx, (int) dy, u, v, cplus, cminus, dist, score);

  return score;
}

float Scenario::get_next_key_match(unsigned char letter, int index, QList<int> &new_index_list, bool &overflow) {
  /* given an index on the curve, and a possible next letter key, evaluate the following:
     - if the next letter is a good candidate (i.e. the curve pass by the key)
     - compute a score based on curve to point distance
     - return the list on possible next_indexes (sometime more than one solution is possible, 
       e.g. depending if we match the key with a near sharp turn or not)
     (multiple indexes for a same scenario will be removed on later iterations,
      when we know which one is the better) */

  float score = -99999;
  int count = 0;
  int retry = 0;

  int start_index = index;
  
  float max_score = -99999;
  int max_score_index = -1;

  int max_turn_distance = params->max_turn_index_gap;
  int last_turn_point = 0;
  float last_turn_point_score = 0;

  float max_score_before_turn = 0;
  int max_score_index_before_turn = -1;

  new_index_list.clear();

  overflow = false;

  int step = 1;
  while(1) {
    if (index >= curve->size() - 1) { overflow = true; break; }
    
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

    if (curve->getSharpTurn(index) && ! last_turn_point && index > start_index) {
      // sharp turn
      last_turn_point = index;
      last_turn_point_score = new_score;
    }
    if (last_turn_point && index > last_turn_point + max_turn_distance && last_turn_point_score > 0) { break; }
    
    score = new_score;

    if (! last_turn_point) {
      max_score_before_turn = max_score; 
      max_score_index_before_turn = max_score_index;
    }

    step = 1;
    if (new_score < -1) {
      step = (- 0.5 - new_score) * params->dist_max_next / 20;
      int i = 1;
      while (i < step && index + i < curve->size() - 1 && ! curve->getSharpTurn(index + i)) {
	i++;
      }
      step = i;
    }

    count += step;
    index += step;
  }

  if (max_score > 0) {
    if (last_turn_point) {
      if (last_turn_point_score > 0 || last_turn_point < max_score_index) {
	new_index_list << last_turn_point;
      }
      if (max_score > last_turn_point_score) {
	// if sharp turn score is less than max_score we can return more than one possible indexes
	new_index_list << max_score_index;
      }
      if (max_score_before_turn > 0) {
	// add a candidate index before the sharp turn (in worst case we return 3 indexes)
	new_index_list << max_score_index_before_turn;
      }
    } else {
      // look for a near sharp turn (and add it as a candidate)
      bool found = false;
      for (int i = index ; i < curve->size() && i < index + max_turn_distance; i++) {
	if (i >= curve->size() - 1) { overflow = true; }
	if (curve->getSharpTurn(i)) {
	  new_index_list << i;
	  float score_turn = calc_distance_score(letter, i, this -> count);

	  if (score_turn < max_score) {
	    // if sharp turn score is less than max_score we can return 2 possible indexes
	    new_index_list << max_score_index;
	  }
	  found = true;
	  break;
	}
      }
      if (! found) { new_index_list << max_score_index; }
    }
    DBG("  [get_next_key_match] %s:%c[%d] max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> new_indexes=[%s]", 
	getNameCharPtr(), letter, start_index, max_score, max_score_index, last_turn_point_score, 
	last_turn_point, index, QSTRING2PCHAR(qlist2str(new_index_list)));
    return max_score;
  } else {
    DBG("  [get_next_key_match] %s:%c[%d] max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> FAIL",
	getNameCharPtr(), letter, start_index, max_score, max_score_index, last_turn_point_score,
	last_turn_point, index);
    return -1;
  }

}

bool Scenario::childScenario(LetterNode &childNode, bool endScenario, QList<Scenario> &result, int &st_fork, bool incremental) {
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
  */
  unsigned char prev_letter = childNode.getParentChar();
  unsigned char letter = childNode.getChar();
  int index = this -> index;

  QList<int> new_index_list;
  float distance_score;

  if (count == 0) {
    // this is the first letter
    distance_score = calc_distance_score(letter, 0, count);
    new_index_list << 0;

  } else {
    if (endScenario) {

      int new_index = curve->size() - 1;
      new_index_list << new_index;
      distance_score = calc_distance_score(letter, new_index, -1);

    } else {
      
      bool overflow = false;
      distance_score = get_next_key_match(letter, index, new_index_list, overflow);
      if (incremental && overflow) {
	return false; // ask me again later
      }
      if (new_index_list.isEmpty()) {
	DBG("debug [%s:%c] %s =FAIL= {distance / turning point}", getNameCharPtr(), letter,endScenario?"*":" ");
	return true;
      }

    }
  }
  
  if (new_index_list.size() >= 2) { st_fork ++; }

  bool first = true;
  foreach(int new_index, new_index_list) {

    score_t score = {NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE}; // all scores are : <0 reject, 1 = best value, ]0,1] OK, NO_SCORE = 0 = not computed
    score.distance_score = distance_score;

    if (count > 0) {
      float curve_advance = distance(curve->getX(index), curve->getY(index), curve->getX(new_index), curve->getY(new_index));
      
      if (new_index > index) {
	score.cos_score = calc_cos_score(prev_letter, letter, index, new_index);
	score.turn_score = calc_turn_score(letter, new_index);
	score.length_score = calc_length_score(prev_letter, letter, index, new_index);
	if (curve_advance < params->curve_score_min_dist) {
	  float min_score = params->small_segment_min_score;
	  if (score.cos_score < min_score) { score.cos_score = min_score; } // travel too short to estimate direction -> lets choose an "average" score
	  if (score.length_score < min_score) { score.length_score = min_score; } // itou
	}
	score.curve_score = calc_curve_score(prev_letter, letter, index, new_index);
      }
      
    }

    bool ok = (score.distance_score >= 0 && score.curve_score >= 0 && score.cos_score >= 0 && score.length_score >= 0 && score.turn_score >= 0);
  
    DBG("debug [%s:%c] %s%s %d:%d %s [%.2f, %.2f, %.2f, %.2f, %.2f]", 
	getNameCharPtr(), letter, endScenario?"*":" ", first?"":" <FORK>", count, new_index, ok?"=OK=":"*FAIL*", 
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
      if (new_index_list.size() >= 2) new_scenario.last_fork = count + 1;

      // temp score is used only for simple filtering
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

      result.append(new_scenario);
    }

    first = false;

  } // for (new_index)

  return true;
}

void Scenario::nextKey(QList<Scenario> &result, int &st_fork) {
  foreach (LetterNode child, node.getChilds()) {
    if (child.hasPayload() && count >= 1) {      
      childScenario(child, true, result, st_fork);
    }
    childScenario(child, false, result, st_fork);
  }
}

QList<LetterNode> Scenario::getNextKeys() {
  return node.getChilds();
}

bool Scenario::operator<(const Scenario &other) const {
  if (this -> getScore() == -1 || other.getScore() == -1) {
    return this -> getTempScore() < other.getTempScore();
  } else {
    return this -> getScore() < other.getScore();
  }
}

QString Scenario::getWordList() {
  QPair<void*, int> payload = node.getPayload();
  return QString((char*) payload.first); // payload is always zero-terminated
}

float Scenario::getScore() const {
  return (final_score == -1)?getTempScore():final_score;
}

float Scenario::getTempScore() const {
  return count?temp_score / count:0;
}

float Scenario::getCount() {
  return count;
}

QString Scenario::getName() {
  QString ret;
  ret.append((char*) getNameCharPtr());
  return ret;
}

unsigned char* Scenario::getNameCharPtr() {
  return letter_history;
}


int Scenario::getCurveIndex() {
  return index;
}
       

bool Scenario::forkLast() {
  return last_fork == count;
}

void Scenario::postProcess() {
  // bonus for "curviness"
  for(int i = 0; i < count - 1; i ++) {
    float sc = (count>2)?calc_curviness_score(i):0;
    if (i == 0) { sc += begin_end_angle_score(false); }
    if (i == count - 2) { sc += begin_end_angle_score(true); }

    /* not yet used at the moment : sc += (l>2)?calc_curviness2_score(i):0; */

    scores[i + 1].curve_score2 = sc;
  }

  evalScore();
}

float Scenario::speedCoef(int i) {
  return 1.0 + params -> score_coef_speed * (curve->getSpeed(i) - 1000) / 1000;
}

float Scenario::evalScore() {
  DBG("==== Evaluating final score: %s", getNameCharPtr());
  this -> final_score = 0;
  if (count < 2) { return 0; }

  ScoreCounter sc;
  char* cols[] = { (char*) "angle", (char*) "curve", (char*) "curve2", (char*) "distance", (char*) "length", (char*) "turn", 0 };
  sc.set_debug(debug); // will draw nice tables :-)
  sc.set_cols(cols);
  sc.set_pow(params -> score_pow);

  for(int i = 0; i < count; i++) {
    // key scores
    sc.start_line();
    sc.set_line_coef(speedCoef(index_history[i]));
    if (debug) { sc.line_label << "Key " << i << ":" << index_history[i] << ":" << QString(letter_history[i]); }
    sc.add_score(scores[i].distance_score, params -> weight_distance, (char*) "distance");
    if (i > 0 && i < count - 1) {
      sc.add_score(scores[i + 1].turn_score, params -> weight_turn, (char*) "turn");
    }
    sc.end_line();

    // segment scores
    if (i < count - 1) {
      sc.start_line();
      sc.set_line_coef((speedCoef(index_history[i]) + 
			speedCoef(index_history[i + 1])) / 2);
      if (debug) { sc.line_label << "Segment " << i + 1 << " [" << index_history[i] << ":" << index_history[i + 1] << "]"; }
      sc.add_score(scores[i + 1].cos_score, params -> weight_cos, (char*) "angle"); 
      sc.add_score(scores[i + 1].curve_score, params -> weight_curve, (char*) "curve");
      sc.add_score(scores[i + 1].length_score, params -> weight_length, (char*) "length");
      sc.add_bonus(scores[i + 1].curve_score2, params -> weight_curve2, (char*) "curve2");
      sc.end_line();
    }
  }

  float score1 = sc.get_score();
  float score = score1 * (1.0 + params->length_penalty * count); // my scores have a bias towards number of letters
  
  DBG("==== %s --> Score: %.3f --> Final Score: %.3f", getNameCharPtr(), score1, score);
  
  this -> final_score = score;
  return score;
}

void Scenario::toJson(QJsonObject &json) {
  json["name"] = getName();
  json["finished"] = finished;
  json["score"] = getScore();

  QJsonArray json_score_array;
  for(int i = 0; i < count; i ++) {
    QJsonObject json_score;

    score_t* score = &(scores[i]);

    json_score["letter"] = QString(letter_history[i]);
    json_score["index"] = index_history[i];
    json_score["score_distance"] = score->distance_score;
    json_score["score_cos"] = score->cos_score;
    json_score["score_turn"] = score->turn_score;
    json_score["score_curve"] = score->curve_score;
    json_score["score_curve2"] = score->curve_score2;
    json_score["score_length"] = score->length_score;
    json_score_array.append(json_score);
  }
  json["detail"] = json_score_array;
}

void Scenario::setDebug(bool debug) {
  this -> debug = debug;
}

/* --- main class for curve matching ---*/
CurveMatch::CurveMatch() {
  loaded = false;
  params = default_params;
  debug = false;
  done = false;
}

#define start_idx(i0, o) (last_curve_index - (o) > (i0)?(last_curve_index - (o)):(i0))

void CurveMatch::curvePreprocess1(int last_curve_index) {
  /* curve preprocessing that can be evaluated incrementally :
     - evaluate turn rate
     - find sharp turns */
  
  int l = curve.size();
  if (l < 8) {
    return; // too small, probably a simple 2-letter words
  }
  if (last_curve_index < 8) { last_curve_index = 0; }
  
  for (int i = start_idx(1, 2); i < l - 1; i ++) {
    curve[i].turn_angle = (int) int(angle(curve[i].x - curve[i-1].x,
					  curve[i].y - curve[i-1].y,
					  curve[i+1].x - curve[i].x,
					  curve[i+1].y - curve[i].y) * 180 / M_PI + 0.5); // degrees
  }
  // avoid some side effects on curve_score (because there is often a delay before the second point)
  if (last_curve_index == 0) { curve[0].turn_angle = curve[1].turn_angle; }
  curve[l-1].turn_angle = curve[l-2].turn_angle;
  
  for (int i = start_idx(1, 2); i < l - 1 ; i ++) {
    curve[i].turn_smooth = int(0.5 * curve[i].turn_angle + 0.25 * curve[i-1].turn_angle + 0.25 * curve[i+1].turn_angle);
  }
  if (last_curve_index == 0) { curve[0].turn_smooth = curve[1].turn_smooth; }
  curve[l-1].turn_smooth = curve[l-2].turn_smooth;
  
  int sharp_turn_index = -1;
  int last_total_turn = -1;
  
  /* rotation / turning points */
  for(int i = start_idx(2, 8) ; i < l - 2; i ++) {
    float total = 0;
    float t_index = 0;
    for(int j = i - 1; j <= i + 1; j ++) {
      total += curve[j].turn_angle;
      t_index += curve[j].turn_angle * j;
    }
    
    if (abs(total) < last_total_turn && last_total_turn > params.turn_threshold) {
      if (sharp_turn_index >= 2 && sharp_turn_index < l - 2) {
	curve[sharp_turn_index].sharp_turn = true;
	
	// compute "normal" vector (really lame algorithm)
	int i1 = sharp_turn_index - 1;
	int i2 = sharp_turn_index + 1;
	float x1 = curve[i1].x - curve[i1 - 1].x;
	float y1 = curve[i1].y - curve[i1 - 1].y;
	float x2 = curve[i2 + 1].x - curve[i2].x;
	float y2 = curve[i2 + 1].y - curve[i2].y;
	float l1 = sqrt(x1 * x1 + y1 * y1);
	float l2 = sqrt(x2 * x2 + y2 * y2);	  
	curve[sharp_turn_index].normalx = x1 / l1 - x2 / l2;
	curve[sharp_turn_index].normaly = y1 / l1 - y2 / l2;
      }
    }
    
    if (abs(total) > params.turn_threshold) {
      sharp_turn_index = int(0.5 + abs(t_index / total));
    }
    
    last_total_turn = abs(total);
  }
}

void CurveMatch::curvePreprocess2() {  
  /* curve preprocessing that can be deferred until final score calculation: 
     - normalize speed */

  int l = curve.size();
  if (l < 8) {
    return; // too small, probably a simple 2-letter words
  }

  float speed[l];
  float total_speed = 0;

  int index1 = 2;
  int index2 = 6;
  for(int i = 0; i < l; i ++) {
    /* speed */
    if (i > (index1 + index2) / 2 && index2 < l - 1) {
      index1 ++; index2 ++;
    }
    float dist = 0;
    for (int j = index1; j < index2; j ++) {
      dist += distancep(curve[j], curve[j+1]);
    }
    if (curve[index2].t > curve[index1].t) {
      speed[i] = dist / (curve[index2].t - curve[index1].t);
    } else {
      // compatibility with old test cases, should not happen often :-)
      if (i > 0) { speed[i] = speed[i - 1]; } else { speed[i] = 1.0; }
    }
    total_speed += speed[i];
  }

  for(int i = 0; i < l; i ++) {
    curve[i].speed = 1000 * (speed[i] / (total_speed / l));
  }

}

void CurveMatch::setDebug(bool debug) {
  this -> debug = debug;
}
void CurveMatch::clearKeys() {
  keys.clear();
}

void CurveMatch::addKey(Key key) {
  if (key.label >= 'a' && key.label <= 'z') {
    keys[key.label] = key;
    DBG("Key: '%c' %d,%d %d,%d", key.label, key.x, key.y, key.width, key.height);
  }
}

void CurveMatch::clearCurve() {
  curve.clear();
  done = false;
}

void CurveMatch::addPoint(Point point, int timestamp) {
  QTime now = QTime::currentTime();
  if (curve.isEmpty()) {
    startTime = now;
  }
  curve << CurvePoint(point, (timestamp >= 0)?timestamp:startTime.msecsTo(now));
}

bool CurveMatch::loadTree(QString fileName) {
  /* load a .tre (word tree) file */
  bool status = wordtree.loadFromFile(fileName);
  loaded = status;
  this -> treeFile = fileName;
  logdebug("loadTree(%s): %d", QSTRING2PCHAR(fileName), status);
  return status;
}

void CurveMatch::log(QString txt) {
  if (! logFile.isNull()) {
    QFile file(logFile);
    if (file.open(QIODevice::Append)) {
      QTextStream out(&file);
      out << txt << "\n";
    }
  }
}

void CurveMatch::setLogFile(QString fileName) {
  logFile = fileName;
}

QList<Scenario> CurveMatch::getCandidates() {
  return candidates;
}

void CurveMatch::scenarioFilter(QList<Scenario> &scenarios, float score_ratio, int min_size, int max_size, bool finished) {
  /* "skim" any scenario list based on number and/or score 

     This method is awfully inefficient, but as the target is to use only the incremental
     implementation (class IncrementalMatch), it'll stay as is */

  float max_score = 0;

  qSort(scenarios.begin(), scenarios.end());  // <- 25% of the whole CPU usage, ouch!

  foreach(Scenario s, scenarios) {
    float sc = s.getScore();
    if (sc > max_score) { max_score = sc; }
  }

  QHash<QString, int> dejavu;

  int i = 0;
  while (i < scenarios.size()) {
    float sc = scenarios[i].getTempScore();

    // remove scenarios with duplicate words (but not just after a scenario fork)
    if (finished || ! scenarios[i].forkLast()) {
      QString name = scenarios[i].getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	float s0 = scenarios[i0].getTempScore();
	if (sc > s0) {
	  scenarios.takeAt(i0);
	  foreach(QString n, dejavu.keys()) {
	    if (dejavu[n] > i0 && dejavu[n] < i) {
	      dejavu[n] --;
	    }
	  }
	  i --;
	  dejavu.remove(name); // will be added again on next iteration
	} else {
	  scenarios.takeAt(i);	  
	}
	continue; // retry iteration
      } else {
	dejavu.insert(name, i);
      }
    }

    // remove scenarios with lowest scores
    if (sc < max_score * score_ratio && scenarios.size() > min_size) {
      st_skim ++;
      DBG("filtering(score): %s (%.3f/%.3f)", QSTRING2PCHAR(scenarios[i].getName()), sc, max_score);
      scenarios.takeAt(i);
    } else {
      i ++;
    }
  }

  // enforce max scenarios count
  if (max_size > 1) {
    while (scenarios.size() > max_size) {
      st_skim ++;
      Scenario s = scenarios.takeAt(0);
      DBG("filtering(size): %s (%.3f/%.3f)", QSTRING2PCHAR(s.getName()), s.getScore(), max_score);
    }
  }
}

bool CurveMatch::match() {
  /* run the full "one-shot algorithm */
  scenarios.clear();
  candidates.clear();
 
  if (! loaded || ! keys.size() || ! curve.size()) { return false; }
  if (curve.size() < 3) { return false; }
 
  curvePreprocess1();
  curvePreprocess2();

  QuickCurve quickCurve(curve);
  QuickKeys quickKeys(keys);

  Scenario root = Scenario(&wordtree, &quickKeys, &quickCurve, &params);
  root.setDebug(debug);
  scenarios.append(root);

  QElapsedTimer timer;
  timer.start();

  int count = 0;
  
  st_fork = 0;
  st_skim = 0;

  int n = 0;
  while (scenarios.size()) {
    QList<Scenario> new_scenarios;
    foreach(Scenario scenario, scenarios) {
   
      QList<Scenario> childs;
      scenario.nextKey(childs, st_fork);
      foreach(Scenario child, childs) {
	if (child.isFinished()) {
	  child.postProcess();
	  candidates.append(child);
	} else {
	  new_scenarios.append(child);
	}
	count += 1;
      }

    }
    n += 1;
    scenarios = new_scenarios; // remember QT collections have intelligent copy-on-write (hope it works)
    if (n >= 3) {
      scenarioFilter(scenarios, 0, 15, params.max_active_scenarios, false); // @todo add to parameter list
      DBG("Depth: %d - Scenarios: %d - Candidates: %d", n, scenarios.size(), candidates.size());      
    }
  }
    
  st_time = (int) timer.elapsed();
  st_count = count;
  scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list
  
  logdebug("Candidate: %d (time=%d, nodes=%d, forks=%d, skim=%d)", candidates.size(), st_time, st_count, st_fork, st_skim);

  done = true;
  
  return candidates.size() > 0;
}

void CurveMatch::endCurve(int id) {
  this -> id = id;
  log(QString("IN: ") + toString());
  if (! done) { match(); }
  log(QString("OUT: ") + resultToString());
}

void CurveMatch::setParameters(QString jsonStr) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  params = Params::fromJson(doc.object());
}

void CurveMatch::useDefaultParameters() {
  params = default_params;
}

QList<CurvePoint> CurveMatch::getCurve() {
  return curve;
}

void CurveMatch::toJson(QJsonObject &json) {
  json["id"] = id;

  // parameters
  QJsonObject json_params;
  params.toJson(json_params); 
  json["params"] = json_params;

  // keys
  QJsonArray json_keys;
  QHash<unsigned char, Key>::iterator i;
  for (i = keys.begin(); i != keys.end(); ++i) {
    Key k = i.value();
    QJsonObject json_key;
    k.toJson(json_key);
    json_keys.append(json_key);
  }
  json["keys"] = json_keys;

  // curve
  QJsonArray json_curve;
  foreach(CurvePoint p, curve) {
    QJsonObject json_point;
    json_point["x"] = p.x;
    json_point["y"] = p.y;
    json_point["t"] = p.t;
    json_point["speed"] = p.speed;
    json_point["turn_angle"] = p.turn_angle;
    json_point["turn_smooth"] = p.turn_smooth;
    json_point["sharp_turn"] = p.sharp_turn;
    json_point["normalx" ] = p.normalx;
    json_point["normaly" ] = p.normaly;
    json_curve.append(json_point);
  }
  json["curve"] = json_curve;

  // other
  json["treefile"] = treeFile;
  json["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void CurveMatch::fromJson(const QJsonObject &json) {

  QJsonValue input = json["input"];
  if (input != QJsonValue::Undefined) {
    fromJson(input.toObject());
    return;
  }

  // parameters
  params = Params::fromJson(json["params"].toObject());

  // keys
  QJsonArray json_keys = json["keys"].toArray();
  keys.clear();
  foreach(QJsonValue json_key, json_keys) {
    Key k = Key::fromJson(json_key.toObject());
    keys[k.label] = k;
  }

  // curve    
  QJsonArray json_curve = json["curve"].toArray();
  curve.clear();
  foreach(QJsonValue json_point, json_curve) {
    QJsonObject o = json_point.toObject();
    CurvePoint p = CurvePoint(Point(o["x"].toDouble(), o["y"].toDouble()), o["t"].toDouble());
    curve.append(p);
  }
}

void CurveMatch::fromString(const QString &jsonStr) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  fromJson(doc.object());
}

QString CurveMatch::toString(bool indent) {
  QJsonObject json;
  toJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

void CurveMatch::resultToJson(QJsonObject &json) {
  json["id"] = id;

  QJsonObject json_input;
  toJson(json_input);
  json["input"] = json_input;

  QJsonArray json_candidates;
  QList<Scenario> candidates = getCandidates();
  qSort(candidates.begin(), candidates.end());
  foreach (Scenario c, candidates) {
    QJsonObject json_scenario;
    c.toJson(json_scenario);
    json_candidates.append(json_scenario);
  }
  json["candidates"] = json_candidates;
  json["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  QJsonObject json_stats;
  json_stats["time"] = st_time;
  json_stats["count"] = st_count;
  json_stats["fork"] = st_fork;
  json_stats["skim"] = st_skim;
  json["stats"] = json_stats;

  QJsonObject json_params;
  default_params.toJson(json_params); 
  json["default_params"] = json_params;
}

QString CurveMatch::resultToString(bool indent) {
  QJsonObject json;
  resultToJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}
