#include "curve_match.h"
#include "score.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTextStream>
#include <cmath>
#include <iostream>
#include <math.h>

#include "functions.h"

#define PARAMS_IMPL
#include "params.h"

#include "kb_distort.h"

#define BUILD_TS (char*) (__DATE__ " " __TIME__)

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
int QuickCurve::getSharpTurn(int index) { int st = sharpturn[index]; return (st >= 3)?0:st; }
int QuickCurve::getSpecialPoint(int index) { return sharpturn[index]; }
Point QuickCurve::getNormal(int index) { return Point(normalx[index], normaly[index]); }
int QuickCurve::size() { return count; }
int QuickCurve::getNormalX(int index) { return normalx[index]; }
int QuickCurve::getNormalY(int index) { return normaly[index]; }
int QuickCurve::getSpeed(int index) { return speed[index]; }

/* optimized keys information */
QuickKeys::QuickKeys() {
  points = new Point[256];  // unsigned char won't overflow
  dim = new Point[256];
}

QuickKeys::QuickKeys(QHash<unsigned char, Key> &keys) {
  points = new Point[256];
  dim = new Point[256];
  setKeys(keys);
}

void QuickKeys::setKeys(QHash<unsigned char, Key> &keys) {
  foreach(unsigned char letter, keys.keys()) {
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
}

Point const& QuickKeys::get(unsigned char letter) const {
  return points[letter];
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

CurvePoint::CurvePoint(Point p, int t, int l, bool dummy) : Point(p.x, p.y) {
  this -> t = t;
  this -> speed = 1;
  this -> sharp_turn = false;
  this -> turn_angle = 0;
  this -> turn_smooth = 0;
  this -> normalx = this -> normaly = 0;
  this -> length = l;
  this -> dummy = dummy;
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
  final_score2 = -1;
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
  final_score2 = from.final_score2;
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
    *return_distance = params->dist_max_next * dist;
  }

  // @todo add a "more verbose" debug option
  //DBG("    distance_score[%c:%d] offset=%d:%d direction=%d:%d u=%.3f v=%.3f coefs=%.2f:%.2f dist=%.3f score=%.3f",
  //    letter, index, (int) px, (int) py, (int) dx, (int) dy, u, v, cplus, cminus, dist, score);

  return score;
}

float Scenario::get_next_key_match(unsigned char letter, int index, QList<int> &new_index_list, bool incremental, bool &overflow) {
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
     - 5: Small turn -> optimally matches a key
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

    // look for sharp turns
    int st = curve->getSharpTurn(index);

    if (st >= 3 && last_turn_point) { st = 0; } // handle slow-down point with less priority (type = 3)

    if (st > 0 && index > start_index) {

      if (last_turn_point && curve->getSharpTurn(last_turn_point) < 3) {
	// we have already encountered a sharp turn linked with next letter so we can safely ignore this one
	break;
      }

      last_turn_point = index;
      last_turn_score = score;

      if (st == 2) {
	new_index_list.clear();
	if (score > 0) { new_index_list << index; }
	if (max_score_index < index && max_score > 0) { new_index_list << max_score_index; }
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
      new_index_list << max_score_index;
    } else if (max_score_index < last_turn_point - max_turn_distance) {
      new_index_list << max_score_index;
    } else if (max_score_index <= last_turn_point + max_turn_distance) {
      if (curve->getSharpTurn(last_turn_point) == 3) {
	new_index_list << max_score_index;
      } else {
	if (last_turn_score > 0) { new_index_list << last_turn_point; }

	int maxd = params->min_turn_index_gap;
	if ((max_score_index < last_turn_point - maxd) ||
	    (max_score_index > last_turn_point + maxd && start_st == 0 && start_index >= last_turn_point - max_turn_distance)) {
	  new_index_list << max_score_index;
	}
      }
      if (curve->getSharpTurn(last_turn_point) < 3) { use_st_score = true; }
    } else { // max_score_index > last_turn_point + max_turn_distance --> turn point was not matched
      failed = 20;
    }
  }

  DBG("  [get_next_key_match] %s:%c[%d] %s max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> new_indexes=[%s] fail_code=%d",
      getNameCharPtr(), letter, start_index, failed?"*FAIL*":"OK",
      max_score, max_score_index, last_turn_score,
      last_turn_point, index, failed?"*FAIL*":QSTRING2PCHAR(qlist2str(new_index_list)), failed);

  if (failed) { return -1; }
  if (use_st_score) { return last_turn_score; } // in case of sharp turn, distance score is taken from the turn position, not the shortest curve-to-key distance
  return max_score;
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

  DBG("==== %s:%c [end=%d] ====", getNameCharPtr(), letter, endScenario);

  if (count == 0) {
    // this is the first letter
    distance_score = calc_distance_score(letter, 0, count);
    new_index_list << 0;

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
	new_index_list << new_index;
	distance_score = calc_distance_score(letter, new_index, -1);
      }

    } else {

      bool overflow = false;
      distance_score = get_next_key_match(letter, index, new_index_list, incremental, overflow);
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

  foreach(int new_index, new_index_list) {
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
    if (params->error_correct && !ok && count > 2 && ! endScenario
	&& error_count < 1 + (count - 1) / params->error_correct_gap) {
      if (score.distance_score > -0.45) {
	if (score.cos_score >= 0 || score.curve_score >= 0) {
	  error_ignore = ok = true;
	}
      }
    }

    DBG("debug [%s:%c] %s%s %d:%d %s [%.2f, %.2f, %.2f, %.2f, %.2f]",
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
      new_scenario.dist = sqrt(new_scenario.dist_sqr);

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
    // @todo remove temporary score (was: "return this -> getTempScore() < other.getTempScore();")
    return this -> distance() / sqrt(this -> getCount()) > other.distance() / sqrt(other.getCount());
  } else {
    return this -> getScore() < other.getScore();
  }
}

QString Scenario::getWordList() {
  QPair<void*, int> payload = node.getPayload();
  return QString((char*) payload.first); // payload is always zero-terminated
}

float Scenario::getScore() const {
  if (final_score2 != -1) { return final_score2; }
  return (final_score == -1)?getTempScore():final_score;
}

float Scenario::getTempScore() const {
  return count?temp_score / count:0;
}

float Scenario::getCount() const {
  return count;
}

QString Scenario::getName() {
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
  return last_fork == count;
}

typedef struct {
  int direction; // was char, but char to int conversion seems to handle these as unsigned chars (didn't investigate)
  int corrected_direction;
  float expected;
  float actual;
  float corrected;
  float length_before;
  float length_after;
  int index;
  int start_index;
  bool unmatched;
  int length;
} turn_t;

void Scenario::calc_turn_score_all() {
  /* this score check if actual segmentation (deduced from user input curve)
     is a good match for the expected one (curve which link all keys in the
     current scenario)

     note: this is far to much complicated for a small reward in term of scoring
     but it helps the classifier by providing a new kind of score independant
     of the others */

  if (count < 2) { return; }


  float a_actual[count];
  float a_expected[count];
  bool  a_same[count];
  memset(a_same, 0, sizeof(a_same));

  // compute actual turn rate
  float segment_length[count];

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
    Point k1 = keys->get(l1);
    Point k2 = keys->get(l2);
    Point k3 = keys->get(l3);

    // actual/expected angles
    float expected = anglep(k2 - k1, k3 - k2) * 180 / M_PI;

    if (a_same[i - 1] &&
	(abs(a_expected[i - 1]) > 165 || abs(expected) > 165) &&
    	(abs(a_expected[i - 1]) < 80 || abs(expected) < 80) && // @todo set as parameters
	curve->getSharpTurn(index_history[i - 1])) {
      expected += a_expected[i - 1];
      if (abs(expected) > 180) {
	expected = expected - 360 * ((expected > 0) - (expected < 0));
      }
      expected /= 2;
      a_expected[i - 1] = a_expected[i] = expected;
    }

    // a bit of cheating for U-turn (+180 is the same as -180, but it is
    // not handled by the code below)
    float actual = a_actual[i];
    if (abs(expected) >= 130 && abs(actual) > 130 && expected * actual < 0) {
      expected = expected - 360 * ((expected > 0) - (expected < 0));
    }

    a_expected[i] = expected;
  }
  a_expected[count - 1] = 0;

  // match both sets
  // (a turn in a set must match a turn in the other one (even if they are not
  //  spread exactly on the same indexes)
  int min_angle = params->turn_min_angle;
  int max_angle = params->turn_max_angle;

  turn_t turn_detail[count];
  memset(turn_detail, 0, sizeof(turn_detail));
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
	DBG("  [score turn]  > turn point #%d:%c %.2f[%d] %.2f[%d]", j, letter_history[j], a_actual[j], typ_act[j], a_expected[j], typ_exp[j]);
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
	bool bad_turn = 0;
	if (typ_act[j] == 0 && abs(a_actual[j]) > max_angle) {
	  DBG("  [score turn] *** unmatched actual turn #%d: %.2f", j, a_actual[j]);
	  bad_turn = (a_actual[j] > 0)?1:-1;
	}
	if (typ_exp[j] == 0 && abs(a_expected[j]) > max_angle) {
	  DBG("  [score turn] *** unmatched expected turn #%d: %.2f", j, a_expected[j]);
	  bad_turn = (a_expected[j] > 0)?1:-1;
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

      /* step 5: compute length */
      if (turn_count > 0) {
	for (int j = 0; j < turn_count; j ++) {
	  turn_detail[j].corrected = turn_detail[j].actual;
	}

	int l = 0;
	for(int k = 0; k < turn_detail[0].start_index; k ++) {
	  l += segment_length[k];
	}
	turn_detail[0].length_before = l;

	for (int j = 1; j < turn_count; j ++) {
	  l = 0;
	  for (int k = turn_detail[j-1].index; k <= turn_detail[j].start_index - 1; k ++) {
	    l += segment_length[k];
	  }
	  turn_detail[j].length_before = turn_detail[j-1].length_after = l;
	}

	int j = turn_count - 1;
	l = 0;
	for(int k = turn_detail[j].index; k <= count - 1 ; k ++) {
	  l += segment_length[k];
	}
	turn_detail[j].length_after = l;
      }

      /* let's go to next block */
      i0 = i + 1;

    } /* new block */

  } /* for(i) */

  if (turn_count) {
    // thresholds
    int t1 = params->max_turn_error1;
    int t2 = params->max_turn_error2;
    int t3 = params->max_turn_error3;

    for(int i = 0; i < turn_count; i++) {
      turn_t *d = &(turn_detail[i]);

      if (i < turn_count - 1) {
	turn_t *d2 = &(turn_detail[i  + 1]);
	// transfer "turn rate" between turns to enable users to cut through (users are lazy)
	if (d -> length_after < params -> turn_optim  && d -> expected * d2 -> expected < 0) {
	  if (abs(d -> actual) < abs(d -> expected) && abs(d2 -> actual) < abs(d2 -> expected)) {
	    float diff1 = d -> expected - d -> actual;
	    float diff2 = d2 -> expected - d2 -> actual;
	    if (diff1 * diff2 < 0) {
	      float diff = min(abs(diff1), abs(diff2)); // maybe we should add some limit here ?
	      float sign = (diff1 > 0) - (diff1 < 0);
	      d -> corrected += diff * sign;
	      d2 -> corrected -= diff * sign;
	    }
	  }
	}
      }

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
      float scale = -1;
      if (((d->start_index == 1) && (d->length_before < params->turn_tip_min_distance)) ||
	  ((d->index == count - 2) && (d->length_after < params->turn_tip_min_distance))) {
	score = 1; // for very small begin/end segments, tip angle is not that mush important
      } else if (abs(expected) < 181) {
	float t = (abs(expected) < 90)?t1:t3;
	scale = t + (t2 - t) * sin(min(abs(actual), abs(expected)) * M_PI / 180);
	float diff = abs(actual - expected);
	float sc0 = diff / scale;
	score = 1 - pow(sc0, 2);
      } else {
	score = min(abs(actual) / 180, 1); // @todo add as a parameter
      }

      if (score < 0) { score = 0.01; } // we can keep this scenario in case other are even worse

      int index = d -> index;
      DBG("  [score turn]  turn #%d: %.2f[%.2f] / %.2f length[%d:%d:%d] index=[%d:%d]->[%c:%c]->[%d:%d] (scale=%.2f) ---> score=%.2f",
	  i, d->actual, d->corrected, d->expected, (int) d->length_before, int(length), (int) d->length_after, d->start_index, index,
	  letter_history[d->start_index], letter_history[index],
	  index_history[d->start_index], index_history[index], scale, score);

      if (scores[index + 1].turn_score >= 0) { scores[index + 1].turn_score = score; }
    }

    // check for expected U-turn vs. ST=2 special points
    for (int i = 1; i < count - 1; i ++) {
      int curve_index = index_history[i];
      int st = curve->getSpecialPoint(curve_index);

      float fail = 0;
      float expected = a_expected[i];

      if (a_same[i] || a_same[i - 1]) { continue; } // case is rare enough to ignore it

      if (abs(expected) > params->st2_max && st != 2) {
	DBG("  [score turn] *** U-turn with no ST=2 (curve_index=%d expected=%.2f)", curve_index, expected);
	fail = (st == 1)?.2:1;
      } else if (abs(expected) < params->st2_min && st == 2) {
	bool found = false;
	for(int j = 0; j < turn_count; j ++) {
	  turn_t *d = &(turn_detail[j]);
	  int length = d->length;
	  if (abs(d->expected) >= params->st2_min &&
	      abs(d->expected) <= 540 - 2 *  params->st2_min &&
	      i >= d->start_index && i <= d->index &&
	      length < params-> curve_score_min_dist) {
	    found = true;
	    break;
	  }
	}
	if (found) {
	  DBG("  [score turn] --- ST=2 w/o U-turn (curve_index=%d expected=%.2f) --> match turn #%d OK", curve_index, expected, found);
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

      if ((abs(d->actual) > 140 || abs(d->expected) > 140) && d->start_index == d->index) { // @todo set as parameter
	int t = curve->getTurnSmooth(index_history[d->index]);
	d->corrected_direction = (t>0) - (t<0);
      }
    }
    for(int i = 0; i < turn_count; i++) {
      turn_t *d1 = &(turn_detail[i]);
      if (i == 0) {
	check_reverse_turn(0, d1->start_index, 0, d1->corrected_direction);
      }

      if (d1->index > d1->start_index) {
	check_reverse_turn(d1->start_index, d1->index, d1->corrected_direction, d1->corrected_direction);
      }

      if (i < turn_count - 1) {
	turn_t *d2 = &(turn_detail[i + 1]);
	check_reverse_turn(d1->index, d2->start_index, d1->corrected_direction, d2->corrected_direction);
      } else {
	check_reverse_turn(d1->index, this->count - 1, d1->corrected_direction, 0);
      }
    }
  }
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

  if (tg_expected.x * tg_expected.x + tg_actual.y * tg_actual.y < 0) {
    return -1;
  } else {
    return 1;
  }
}

void Scenario::check_reverse_turn(int index1, int index2, int direction1, int direction2) {
  int i1 = index_history[index1];
  int i2 = index_history[index2];

  int threshold = params->rt_turn_threshold;
  int total_threshold = params->rt_total_threshold;
  float coef_score = params->rt_score_coef;
  int tip_gap = params->rt_tip_gaps;

  int direction = direction1;
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

  int total = 0;
  for(int i = max(tip_gap, i1); i <= min(i2, curve->size() - 1 - tip_gap); i++) {
    int turn = int(0.5 * curve->getTurnSmooth(i) + 0.25 * curve->getTurnSmooth(i - 1) + 0.25 * curve->getTurnSmooth(i));

    total += turn;

    if ((abs(turn) > threshold) && (turn * direction < 0 || ! direction)) {
      if (direction2 != direction1 && direction == direction1 && turn * direction2 >= 0) { direction = direction2; i --; continue; }
      bad ++;
    } else if ((abs(total) > total_threshold) && (total * direction < 0 || ! direction)) {
      if (direction2 != direction1 && direction == direction1 && total * direction2 >= 0) { direction = direction2; i --; continue; }
      bad ++;
    }
  }

  float score = 1.0 * bad / (i2 - i1 + 1);
  for(int i = index1; i <= index2; i ++) {
    scores[i].misc_score -= coef_score * score / (index2 - index1 + 1);
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
  }

  /* check turn relative position */
  if (i > 0 && i < count - 1) {
    Point k0 = keys->get(letter_history[i - 1]);
    Point k1 = keys->get(letter_history[i]);
    Point k2 = keys->get(letter_history[i + 1]);
    float k_angle = anglep(k1 - k0, k2 - k1) * 180 / M_PI;
    float dk01 = distancep(k0,k1);
    float dk12 = distancep(k1,k2);
    if (abs(k_angle) > /* 150 */ 115 &&
	dk01 > 2 * params->curve_score_min_dist &&
	dk12 > 2 * params->curve_score_min_dist) {
      int i1 = -1;
      for(int d = 0; d <= params->max_turn_index_gap && i1 == -1; d ++) {
	if (curve->getSharpTurn(index_history[i] - d)) {
	  i1 = index_history[i] - d;
	} else if (curve->getSharpTurn(index_history[i] + d)) {
	  i1 = index_history[i] + d;
	}
      }
      if (i1 == -1) {
	/* turn point not found: this may happen when user draw very round curves :-) */
	DBG("  [score misc] %s[%d:%c] turn distance -> turn not found (this is not an error)",getNameCharPtr(), i, letter_history[i])
      } else {
	// @todo if curvature at i_h[i] is roughly the same as at index i1, replace i1 with i_h[i] for better score
	Point normal = (k1 - k0) * (10000 / dk01) - (k2 - k1) * (10000 / dk12);
	Point p = curve->point(i1);
	float d = distancep(Point(), normal);
	float dx = normal.x;
	float dy = normal.y;
	float px = p.x - k1.x;
	float py = p.y - k1.y;
	float u = (px * dx + py * dy) / d;
	float v = (px * dy - py * dx) / d;

	int threshold = params->turn_distance_threshold;
	float sc0 = -params->turn_distance_score * max(0, (abs(v) + max(0, u * params->turn_distance_ratio)) / threshold - 1);
	float sc1 = -params->lazy_loop_bias * u / params->turn_distance_threshold; // this is just intended as a small bias when i've got close choices
	DBG("  [score misc] %s[%d:%c] turn distance u=%.2f v=%.2f score=%.3f/%.3f",getNameCharPtr(), i, letter_history[i], u, v, sc0, sc1);
	score += sc0 + sc1;
      }


    }
  }

  /* speed -> slow down points (ST=3) must be matched with a key */
  if (i > 0) {
    int i0 = i - 1;
    int i1 = i;
    int maxgap = params->speed_max_index_gap;
    for (int j = index_history[i - 1] + maxgap + 1;
	 j < index_history[i] - maxgap;
	 j ++) {
      if (curve->getSpecialPoint(j) == 3 && abs(curve->getTurnSmooth(j)) < params->speed_min_angle) {
	DBG("  [score misc] unmatched slow down point (point=%d, index=%d:%d)", j, i0, i1);
	score -= params->speed_penalty;
      }
    }
  }

  /* optional turn matching (positive bias if matched)
     (this is better than trying to guestimate near tips turns using tangent direction) */
  int l = curve->size();
  if (i > 0 && i < count - 1) {
    for(int j = 0; j < l; j++) {
      if (curve->getSpecialPoint(j) == 5) {
	int max_turn_distance = params->max_turn_index_gap;
	int st = curve->getSpecialPoint(index_history[i]);

	if (abs(index_history[i] - j) <= max_turn_distance && st != 1 && st != 2) {
	  DBG("  [score misc] optional turn matched: index=%d -> key #%d:'%c'", j, i, letter_history[i]);
	  score += params->st5_score;
	}
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
	    }
	  }
	}
	peak = true;
      }
    }
  }

  return score;
}

bool Scenario::postProcess() {
  for(int i = 0; i < count; i++) {
    scores[i].misc_score = calc_score_misc(i);
  }
  calc_turn_score_all();

  return (evalScore() > 0);
}

float Scenario::evalScore() {
  DBG("==== Evaluating final score: %s", getNameCharPtr());
  this -> final_score = 0;
  if (count < 2) { return 0; }

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

  this -> final_score = score;
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
  json["score_std"] = getScoreOrig();
  json["distance"] = distance();
  json["class"] = result_class;
  json["star"] = star;
  json["min_total"] = min_total;
  json["error"] = error_count;
  json["good"] = good_count;

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


/* --- main class for curve matching ---*/
CurveMatch::CurveMatch() {
  loaded = false;
  params = default_params;
  debug = false;
  done = false;
  kb_preprocess = true;
}

void CurveMatch::curvePreprocess1(int /* unused parmeter for the moment */) {
  /* curve preprocessing that can be evaluated incrementally :
     - evaluate turn rate
     - find sharp turns
     - normal vector calculation */

  int l = curve.size();
  if (l < 8) {
    return; // too small, probably a simple 2-letter words
  }

  for (int i = 1; i < l - 1; i ++) {
    curve[i].turn_angle = (int) int(angle(curve[i].x - curve[i-1].x,
					  curve[i].y - curve[i-1].y,
					  curve[i+1].x - curve[i].x,
					  curve[i+1].y - curve[i].y) * 180 / M_PI + 0.5); // degrees


  }
  // avoid some side effects on curve_score (because there is often a delay before the second point)
  curve[0].turn_angle = 0;
  curve[l-1].turn_angle = 0;

  for (int i = 1; i < l - 1 ; i ++) {
    int t1 = curve[i-1].turn_angle;
    int t2 = curve[i].turn_angle;
    int t3 = curve[i+1].turn_angle;

    if (abs(t2) > 160 && t1 * t2 < 0 && t2 * t3 < 0) {
      curve[i].turn_angle += 360 * ((t2 < 0) - (t2 > 0));
    }
  }

  for (int i = 1; i < l - 1 ; i ++) {
    curve[i].turn_smooth = int(0.5 * curve[i].turn_angle + 0.25 * curve[i-1].turn_angle + 0.25 * curve[i+1].turn_angle);
  }
  curve[0].turn_smooth = curve[1].turn_angle / 4;
  curve[l-1].turn_smooth = curve[l-2].turn_angle / 4;

  // speed evaluation
  int max_speed = 0;
  for(int i = 0; i < l; i ++) {
    int i1 = i - 2, i2 = i + 2;
    if (i1 < 1) { i1 = 1; i2 = 5; }
    if (i2 > l - 1) { i1 = l - 5; i2 = l - 1; }

    while(curve[i1].t - curve[0].t < 50 && i2 < l - 1) { i1 ++; i2 ++; }
    while(curve[i2].t - curve[i1].t < 40 && i2 < l - 1 && i1 > 1 ) { i2 ++; i1 --; }

    float dist = 0;
    for (int j = i1; j < i2; j ++) {
      dist += distancep(curve[j], curve[j+1]);
    }
    if (curve[i2].t > curve[i1].t) {
      curve[i].speed = 1000.0 * dist / (curve[i2].t - curve[i1].t);
    } else {
      // compatibility with old test cases, should not happen often :-)
      if (i > 0) { curve[i].speed = curve[i - 1].speed; } else { curve[i].speed = 1; }
    }
    if (curve[i].speed > max_speed) { max_speed = curve[i].speed; }
  }

  for(int i = 0 ; i < l; i ++) {
    curve[i].sharp_turn = 0;
  }

  /* rotation / turning points */
  int sharp_turn_index = -1;
  int last_total_turn = -1;
  int last_turn_index = -100;
  int range = 1;
  for(int i = 2 ; i < l - 2; i ++) {
    float total = 0;
    float t_index = 0;
    for(int j = i - range; j <= i + range; j ++) {
      total += curve[j].turn_angle;
      t_index += curve[j].turn_angle * j;
    }

    if (abs(total) < last_total_turn && last_total_turn > params.turn_threshold) {
      if (sharp_turn_index >= 2 && sharp_turn_index < l - 2) {

  	for(int j = i - range; j <= i + range; j ++) {
  	  if (abs(curve[j].turn_angle) > params.turn_threshold2) {
  	    sharp_turn_index = j;
  	  }
  	}

	int st_value = 1 + (last_total_turn > params.turn_threshold2 ||
			    abs(curve[sharp_turn_index].turn_angle) > params.turn_threshold3);

  	int diff = sharp_turn_index - last_turn_index;
  	if (diff <= 1) {
  	  sharp_turn_index = -1;
  	} else if (diff == 2) {
  	  sharp_turn_index --;
  	  curve[sharp_turn_index - 1].sharp_turn = 0;
  	} else if (diff <= 4) {
	  bool remove_old = false, remove_new = false;
	  int old_value = curve[last_turn_index].sharp_turn;

	  if (old_value < st_value) { remove_old = true; }
	  else if (old_value > st_value) { remove_new = true; }
	  else if (st_value == 1) {
	    remove_new = (abs(curve[sharp_turn_index].turn_smooth) < abs(curve[last_turn_index].turn_smooth));
	    remove_old = ! remove_new;
	  }

	  DBG("Sharp-turns too close: %d[%d] -> %d[%d] ---> remove_old=%d remove_new=%d",
	      last_turn_index, old_value, sharp_turn_index, st_value, remove_old, remove_new);

	  if (remove_old) { curve[last_turn_index].sharp_turn = 0; }
	  if (remove_new) { sharp_turn_index = -1; }
	}

  	if (sharp_turn_index >= 0) {
  	  curve[sharp_turn_index].sharp_turn = st_value;

  	  last_turn_index = sharp_turn_index;

  	  DBG("Special point[%d]=%d", sharp_turn_index, curve[sharp_turn_index].sharp_turn);

	  sharp_turn_index = -1;
  	}
      }
    }

    if (abs(total) > params.turn_threshold) {
      sharp_turn_index = int(0.5 + abs(t_index / total));
    }

    last_total_turn = abs(total);
  }

  // alternate way of finding turning points
  int cur = 0;
  int total, start_index, max_turn, max_index;
  int threshold = params.atp_threshold;
  int min_angle1 = params.atp_min_angle1;
  int min_turn1 = params.atp_min_turn1;
  int max_pts = params.atp_max_pts;
  int tip_gap = 0;
  int opt_gap = params.atp_opt_gap;
  bool st_found = false;
  for(int i = tip_gap; i < l - tip_gap; i ++) {
    int turn = curve[i].turn_smooth; // take care of jagged curves
    if (cur) {
      st_found |= (curve[i].sharp_turn != 0);
      if (abs(turn) >= threshold && cur * turn > 0 && i < l - 1 - tip_gap) {
	// turn continues
	total += turn;
	if (abs(turn) > abs(max_turn)) {
	  max_turn = turn;
	  max_index = i;
	}
      } else {
	// turn end
	int end_index = i;
	if (i == l - 1 - tip_gap) { total += turn; }
	for(int j = 1; j <= 2; j ++) {
	  if (start_index - j > 0) { st_found |= (curve[start_index - j].sharp_turn != 0); }
	  if (end_index + j < l) { st_found |= (curve[end_index + j].sharp_turn != 0); }
	}
	DBG("New turn %d->%d total=%d max_index=%d st_found=%d", start_index, end_index, total, max_index, st_found);
	if (abs(total) > min_angle1 &&
	    abs(curve[max_index].turn_smooth) >= min_turn1 &&
	    end_index - start_index <= max_pts &&
	    ! st_found) {
	  int value = 1;
	  if (start_index <= opt_gap || end_index >= l - opt_gap) { value = 5; }
	  DBG("Special point[%d]=%d (try 2)", max_index, value);
	  curve[max_index].sharp_turn = value;
	}
	cur = 0;
      }
    } else if (abs(turn) > threshold) {
      cur = (turn > 0) - (turn < 0);
      total = turn;
      start_index = i;
      max_index = i;
      max_turn = turn;
      st_found = (curve[i].sharp_turn != 0);
    }
  }

  // compute "normal" vector for turns (really lame algorithm)
  for(int i = 2; i < l - 2; i ++) {
    if (curve[i].sharp_turn) {
      int sharp_turn_index = i;

      int i1 = sharp_turn_index - 1;
      int i2 = sharp_turn_index + 1;
      float x1 = curve[i1].x - curve[i1 - 1].x;
      float y1 = curve[i1].y - curve[i1 - 1].y;
      float x2 = curve[i2 + 1].x - curve[i2].x;
      float y2 = curve[i2 + 1].y - curve[i2].y;
      float l1 = sqrt(x1 * x1 + y1 * y1);
      float l2 = sqrt(x2 * x2 + y2 * y2);
      curve[sharp_turn_index].normalx = 100 * (x1 / l1 - x2 / l2); // integer vs. float snafu -> don't loose too much precision
      curve[sharp_turn_index].normaly = 100 * (y1 / l1 - y2 / l2);
    }
  }


  // slow down point search
  int maxd = params.max_turn_index_gap;
  for(int i = max(maxd, 0); i < l - maxd; i ++) {
    int spd0 = curve[i].speed;
    if (spd0 < max_speed * params.slow_down_min) { continue; }
    int ok = 0;
    for(int j = -maxd; j <= maxd; j ++) {
      int spd = curve[i + j].speed;
      if (spd < spd0 || curve[i + j].sharp_turn) { ok = 0; break; }
      if (spd > params.slow_down_ratio * spd0) { ok |= (1 << (j>0)); }
    }
    if (ok == 3) {
      curve[i].sharp_turn = 3;
      DBG("Special point[%d]=3", i);
    }
  }

}

void CurveMatch::curvePreprocess2() {
  /* curve preprocessing that can be deferred until final score calculation:
     - only statistics at the moment :-) */

  st.st_speed = st.st_special = 0;
  int total_speed = 0;
  for(int i = 0; i < curve.size(); i ++) {
    st.st_special += (curve[i].sharp_turn > 0);
    total_speed += curve[i].speed;
  }
  st.st_speed = total_speed / curve.size();
}

void CurveMatch::setDebug(bool debug) {
  this -> debug = debug;
}
void CurveMatch::clearKeys() {
  keys.clear();
}

void CurveMatch::addKey(Key key) {
  kb_preprocess = true;
  if (key.label >= 'a' && key.label <= 'z') {
    keys[key.label] = key;
    DBG("Key: '%c' %d,%d %d,%d", key.label, key.x, key.y, key.width, key.height);
  }
}

void CurveMatch::clearCurve() {
  curve.clear();
  done = false;
  memset(&st, 0, sizeof(st));
}

void CurveMatch::addPoint(Point point, int timestamp) {
  QTime now = QTime::currentTime();

  if (kb_preprocess && params.thumb_correction) {
    // we must apply keyboard biases before feeding the curve
    // in case of incremental processing
    kb_preprocess = false;
    kb_distort(keys, params);
    if (debug) {
      DBG("Keys adjustments:");
      QHashIterator<unsigned char, Key> ki(keys);
      while (ki.hasNext()) {
	ki.next();
	Key key = ki.value();
	DBG("Key '%c' %d,%d -> %d,%d", key.label, key.x, key.y, key.corrected_x, key.corrected_y);
      }
    }
  }

  if (curve.isEmpty()) {
    startTime = now;
    curve_length = 0;
  }

  int ts = (timestamp >= 0)?timestamp:startTime.msecsTo(now);

  if (curve.size() > 0) {
    if (ts < curve.last().t) {
      /* This is a workaround for a bug that produced bad test cases with
	 time going back. This should be eventually removed */
      for(int i = 0; i < curve.size(); i++) {
	curve[i].t = ts;
      }
    }

    /* Current implementation is dependant on good spacing between points
       (which is a design fault).
       Last SailfishOS has introduced some "micro-lag" (unnoticeable by
       user but it leads to "spaces" in the curve (... or maybe it's just
       some placebo effect).
       This is a simple workaround for this problem. */
    CurvePoint lastPoint = curve.last();
    float dist = distancep(lastPoint, point);

    int max_length = params.max_segment_length;
    if (dist > max_length) {
      Point dp = point - lastPoint;
      int n = dist / max_length;
      for(int i = 0; i < n; i ++) {
	float coef = (i + 1.0) / (n + 1);
	curve << CurvePoint(lastPoint + dp * coef, lastPoint.t + coef * (ts - lastPoint.t), curve_length + coef * dist, true /* "dummy" point */);
      }
    }

    curve_length += dist;
  }
  curve << CurvePoint(point, ts, curve_length);
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
    file.close();
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
  // (in incremental mode it's only used to sort candidates, so it's not a problem)

  foreach(Scenario s, scenarios) {
    float sc = s.getScore();
    if (sc > max_score) { max_score = sc; }
  }

  QHash<QString, int> dejavu;

  int i = 0;
  while (i < scenarios.size()) {
    float sc = scenarios[i].getScore();

    if (sc < max_score * score_ratio && scenarios.size() > min_size) {
      // remove scenarios with lowest scores
      st.st_skim ++;
      DBG("filtering(score): %s (%.3f/%.3f)", QSTRING2PCHAR(scenarios[i].getName()), sc, max_score);
      scenarios.takeAt(i);

    } else if (finished || ! scenarios[i].forkLast()) {
      // remove scenarios with duplicate words (but not just after a scenario fork)

      QString name = scenarios[i].getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	scenarios.takeAt(i0);
	foreach(QString n, dejavu.keys()) {
	  if (dejavu[n] > i0) {
	    dejavu[n] --;
	  }
	}
	i --;
	dejavu.remove(name); // will be added again on next iteration
      } else {
	dejavu.insert(name, i);
	i++;
      }

    } else {
      i++;

    }

  }

  // enforce max scenarios count
  while (max_size > 1 && scenarios.size() > max_size) {
    st.st_skim ++;
    Scenario s = scenarios.takeAt(0);
    DBG("filtering(size): %s (%.3f/%.3f)", QSTRING2PCHAR(s.getName()), s.getScore(), max_score);
  }

}

static int scenarioDistanceLessThan(Scenario &s1, Scenario &s2) {
  return s1.distance() < s2.distance();
}

int CurveMatch::compare_scenario(Scenario *s1, Scenario *s2, bool reverse) {
  // compare two scenarios :
  // - positive if s1 > s2
  // - absolute value > 5 if distance gap is greater than elimination threshold

  score_t avg1, min1, avg2, min2;
  s1->getDetailedScores(avg1, min1);
  s2->getDetailedScores(avg2, min2);

  // check if s1 must be eliminated
  int result1 = 0;
  if (reverse) {
    result1 = - compare_scenario(s2, s1, false);
  }

  // check if s2 must be eliminated
  int result = 0;

  float d1 = s1->distance(), d2 = s2 -> distance();
  float ratio = (d2 - d1) / min(50, (d1 + d2) / 2);

  int st = int((avg1.turn_score - avg2.turn_score + min1.turn_score - min2.turn_score) / 0.03);
  int sc = int((avg1.curve_score - avg2.curve_score + min1.curve_score - min2.curve_score) / 0.03);
  int sm = int((avg1.misc_score - avg2.misc_score + min1.misc_score - min2.misc_score) / 0.01);

  DBG("           %s-->%s (%.2f-->%.2f) st=%d sc=%d sm=%d", s1->getNameCharPtr(), s2->getNameCharPtr(), s1->getScoreOrig(), s2->getScoreOrig(), st, sc, sm);

  if (ratio > params.cls_distance_max_ratio) {
    result = 10;
  } else if (ratio > 0) {
    result = 1;
  } else if ((sm >= 0) ||
	     (min(sm, min(st, sc)) >= -1) ||
	     (st + sc > 5) ||
	     (st + sc + sm >= -3) ||
	     (s1->getScoreOrig() - s2->getScoreOrig() > -0.02)) {
    result = 1;
  }

  if (reverse) {
    DBG("compare scenario: %s <-> %s = %d + %d (score: %.2f <-> %.2f - st=%d sc=%d sm=%d)",
	s1->getNameCharPtr(), s2->getNameCharPtr(), result, result1,
	s1->getScoreOrig(), s2->getScoreOrig(), st, sc, sm);
  }

  return result + result1;
}

void CurveMatch::sortCandidates() {
  /* try to find the most likely candidates by combining multiple methods :
     - score (linear combination of multiple scores)
     - classifier: compare relative scores in pairs, scenario "worse" than another one can be eliminated
     - distance (sum of square distance between expected keys and curve), as calculated by Scenario::distance()
  */

  if (candidates.size() < 2) { return; }

  QElapsedTimer timer;
  timer.start();

  scenarioFilter(candidates, 0.5, 10, 3 * params.max_candidates, true); // @todo add to parameter list

  int n = candidates.size();

  /* new composite score */
  float min_dist = 0;
  for(int i = 0; i < n; i ++) {
    if (candidates[i].distance() < min_dist || ! min_dist) {
      min_dist = candidates[i].distance();
    }
  }
  for(int i = 0; i < n; i ++) {
    float new_score = candidates[i].getScore()
      - params.coef_distance * (candidates[i].distance() - min_dist) / max(15, min_dist)
      - params.coef_error * candidates[i].getErrorCount();
    candidates[i].setScore(new_score);
  }

  // classifier V3 (much simpler)
  qSort(candidates.begin(), candidates.end(), scenarioDistanceLessThan);
  int cls[n];
  memset(&cls, 0, sizeof(cls));

  for(int i = 0; i < n; i ++) {
    Scenario *s1 = &candidates[i];

    int remove = 0;
    int cmp[n];
    bool done = false;
    for(int j = i + 1; j < n; j ++) {
      Scenario *s2 = &candidates[j];
      if (cls[j] < 0) { continue; }
      if (done) { cls[j] = -50; continue; }

      int result = cmp[j] = compare_scenario(s1, s2, true);

      if (result > 5) {
	done = true;
	cls[j] = -50;
	continue;
      }

      if (result < 0) {
	remove = max(remove, -result);
      }
    }

    if (remove) {
      cls[i] = -remove;
    }

    for(int j = i + 1; j < n; j ++) {
      if (cls[j] < 0) { continue; }

      if (cmp[j] > 0) {
	cls[j] = -cmp[j];
      }
    }

  } /* for i */

  for(int i = 0; i < n; i ++) {
    candidates[i].setClass(cls[i]); // obsolete
    candidates[i].setStar((cls[i] >= 0));
  }


  scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list

  int elapsed = timer.elapsed();

  if (debug) {
    DBG("SORT> [time:%4d]                         | ----------[average]---------- | ------------[min]------------ |     |       |", elapsed);
    DBG("SORT> ## =word======= =score =min E G dst | =cos curv dist =len misc turn | =cos curv dist =len misc turn | cls | final |");
    for(int i = candidates.size() - 1; i >=0; i --) {
      Scenario candidate = candidates[i];
      score_t smin, savg;
      candidate.getDetailedScores(savg, smin);
      float score = candidate.getScoreOrig();
      float min_score = candidate.getMinScore();
      int error_count = candidate.getErrorCount();
      int good_count = candidate.getGoodCount();
      int dist = (int) candidate.distance();
      float new_score = candidate.getScore();
      unsigned char *name = candidate.getNameCharPtr();
      DBG("SORT> %2d %12s %5.2f %5.2f %1d %1d%4d |%5.2f%5.2f%5.2f%5.2f%5.2f%5.2f |%5.2f%5.2f%5.2f%5.2f%5.2f%5.2f |%3d%c |%6.3f |",
	  i, name, score, min_score, error_count, good_count, dist,
	  savg.cos_score, savg.curve_score, savg.distance_score, savg.length_score, savg.misc_score, savg.turn_score,
	  smin.cos_score, smin.curve_score, smin.distance_score, smin.length_score, smin.misc_score, smin.turn_score,
	  candidate.getClass(), candidate.getStar()?'*':' ', new_score);
    }
  }
}

bool CurveMatch::match() {
  /* run the full "one-shot algorithm */
  scenarios.clear();
  candidates.clear();

  if (! loaded || ! keys.size() || ! curve.size()) { return false; }
  if (curve.size() < 3) { return false; }

  memset(& st, 0, sizeof(st));

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

  int n = 0;
  while (scenarios.size()) {
    QList<Scenario> new_scenarios;
    foreach(Scenario scenario, scenarios) {

      QList<Scenario> childs;
      scenario.nextKey(childs, st.st_fork);
      foreach(Scenario child, childs) {
	if (child.isFinished()) {
	  if (child.postProcess()) {
	    candidates.append(child);
	  }
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

  st.st_time = (int) timer.elapsed();
  st.st_count = count;

  sortCandidates();

  logdebug("Candidates: %d (time=%d, nodes=%d, forks=%d, skim=%d, speed=%d, special=%d)", candidates.size(), st.st_time, st.st_count, st.st_fork, st.st_skim, st.st_speed, st.st_special);

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
    json_point["dummy"] = p.dummy?1:0;
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
    if (! o["dummy"].toDouble()) {
      CurvePoint p = CurvePoint(Point(o["x"].toDouble(), o["y"].toDouble()), o["t"].toDouble());
      curve.append(p);
    }
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
  json["build"] = QString(BUILD_TS);

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
  json_stats["time"] = st.st_time;
  json_stats["count"] = st.st_count;
  json_stats["fork"] = st.st_fork;
  json_stats["skim"] = st.st_skim;
  json_stats["special"] = st.st_special;
  json_stats["speed"] = st.st_speed;
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
