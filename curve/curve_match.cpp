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

  if (index == new_index) {
    Point tgt = actual_curve_tangent(index);
    p2 = p1 + tgt;
  }

  float a_sin = abs(sin_angle(k2.x - k1.x, k2.y - k1.y, p2.x - p1.x, p2.y - p1.y));  

  int max_gap = params -> cos_max_gap;
  float max_sin = sin(params -> max_angle * M_PI / 180);

  float len = distancep(k1, k2);
  float gap = len * a_sin;
  float score;
  if ((k2.x - k1.x)*(p2.x - p1.x) + (k2.y - k1.y)*(p2.y - p1.y) < 0) {
    score = -1;
  } else {
    score = 1 - max(gap / max_gap, a_sin / max_sin);
  }
  
  DBG("  [cos score] %s:%c i=%d:%d angle=%.2f/%.2f gap=%d/%d -> score=%.2f", 
      getNameCharPtr(), letter, index, new_index, a_sin, max_sin, (int) gap, (int) max_gap, score);
  
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
  /* score based on curve "smoothness" and maximum distance from straight path */
  Point pbegin = keys->get(prev_letter);
  Point pend = keys->get(letter);
  
  Point ptbegin = curve->point(index);
  Point ptend = curve->point(new_index);

  float dist_begin = dist_line_point(pbegin, pend, ptend);
  float dist_end = dist_line_point(pbegin, pend, ptbegin);
 
  float max_dist = 0;
  float total_dist = 0;
  int sharp_turns = 0;
  int c = 0;
  for (int i = index + 2; i < new_index - 1; i += 4) {
    Point p = curve->point(i);
    float dist = dist_line_point(pbegin, pend, p);

    if (dist > max_dist) { max_dist = dist; }
    total_dist += dist;
    c++;
  }
  float avg_dist = c?total_dist / c:0;

  for (int i = index + params->min_turn_index_gap + 1; i < new_index - params->min_turn_index_gap; i += 1) {
    int st = curve->getSpecialPoint(i);
    if (st && i >= 2 && i < curve->size() - 2) { sharp_turns ++; }
  }
  float s4 = sharp_turns * params->sharp_turn_penalty;

  float s1 = -1, s2 = -1, s3 = -1, score = -1;
  if (max_dist <= params->curve_dist_threshold) {
    
    s1 = max(dist_begin, dist_end) / params->curve_dist_threshold;
    s2 = 2 * avg_dist / params->curve_dist_threshold;
    s3 = max_dist / params->curve_dist_threshold;
    score = 1 - (s1 + min(s2, s3)) / 2 - s4; // @todo score bisounours
    if (score > 0) { score = pow(score, 0.25); }
  }

  DBG("  [curve score] %s:%c[%d->%d] sc=[%.3f:%.3f:%.3f:%.3f] max_dist=%d/%d -> score=%.3f",
      getNameCharPtr(), letter, index, new_index, s1, s2, s3, s4, (int) max_dist, params->curve_dist_threshold, score);

  return score;
}


float Scenario::calc_distance_score(unsigned char letter, int index, int count) {
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
      when we know which one is the better)

     The algorithm tries to uses information about special points (still called "sharp turn" 
     in the code):
     - 1: Sharp turns -> shortest distance to key must be near this point
     - 2: U-Turns --> mush exactly match a key
     - 3: Slow-down points --> treated as 1, but has les priority than type 1 & 2
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
	if (score <= 0) { failed = 1; break; }
	new_index_list.clear();
	new_index_list << index;
	if (max_score_index < index - 1) { new_index_list << max_score_index; }
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
    } else { // max_score_index > last_turn_point + max_turn_distance --> turn point was not matched 
      failed = 20;
    }
  }

  DBG("  [get_next_key_match] %s:%c[%d] %s max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> new_indexes=[%s] fail_code=%d", 
      getNameCharPtr(), letter, start_index, failed?"*FAIL*":"OK",
      max_score, max_score_index, last_turn_score, 
      last_turn_point, index, failed?"*FAIL*":QSTRING2PCHAR(qlist2str(new_index_list)), failed);
  
  return failed?-1:max_score;
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
      if (! st_found) { 
	new_index_list << new_index;
	distance_score = calc_distance_score(letter, new_index, -1);
      }

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
  int continue_count = 0;
  Scenario *first_scenario = NULL;

  foreach(int new_index, new_index_list) {
    if (count > 2 && new_index <= index_history[count - 2] + 1) {
      continue; // 3 consecutive letters are matched with the same curve point -> remove this scenario
    }

    score_t score = {NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE, NO_SCORE}; // all scores are : <0 reject, 1 = best value, ]0,1] OK, NO_SCORE = 0 = not computed
    score.distance_score = distance_score;

    if (count > 0) {
      if (new_index > index) {
	score.cos_score = calc_cos_score(prev_letter, letter, index, new_index);
	score.curve_score = calc_curve_score(prev_letter, letter, index, new_index);

      } else {	
	float sc = - 1;

	if (curve->getSharpTurn(new_index) != 2) { 
	  Point tgt = actual_curve_tangent(index);
	  Point k1 = keys->get(prev_letter);
	  Point k2 = keys->get(letter);
	  float a = anglep(k2 - k1, tgt) * 180 / M_PI;
	  if (abs(a) <= params->same_point_max_angle) { 
	    sc = calc_cos_score(prev_letter, letter, index, new_index);
	    if (sc < params->same_point_score) { sc = params->same_point_score; }
	  }
	}

	score.cos_score = sc;
	
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

void Scenario::calc_turn_score_all() {
  /* this score check if actual segmentation (deduced from user input curve) 
     is a good match for the expected one (curve which link all keys in the
     current scenario) */

  if (count < 2) { return; }

  float turn[2][count]; // 0: actual - 1:expected

  // thresholds
  int t1 = params->max_turn_error1;
  int t2 = params->max_turn_error2;

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
    if (i == 0) { segment_length[0] = distancep(p1, p2); } // piquets & intervalles

    if (i3 > i2 && i2 > i1) {
      float actual = anglep(p2 - p1, p3 - p2) * 180 / M_PI;
      for(int j = i_; j <= i; j++) {
	turn[0][j] = actual / (1 + i - i_);  // in case of "null-point" we spread the turn rate over all matches at the same position
      }

      i_ = i + 1; i1 = i2;
    }
  }
  for(int i = i_; i <= count - 1; i ++) {
    turn[0][i] = 0;
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

    float actual = turn[0][i];

    // a bit of cheating for U-turn (+180 is the same as -180, but it is
    // not handled by the code below
    if (abs(expected) >= 150 && abs(actual) > 150 && expected * actual < 0) {
      expected = 179 * ((actual > 0) - (actual < 0));
    }

    turn[1][i] = expected;
  }
  turn[1][count - 1] = 0;

  // match both sets
  // (a turn in a set must match a turn in the other one (even if they are not
  //  spread exactly on the same indexes)

  int min_angle = params->turn_min_angle;
  int max_angle = params->turn_max_angle;

  int cur_turn[2] = { 0, 0 };
  float cur_total[2];
  float other_total[2];
  int cur_matched[2];
  
  for(int i = 1; i <= count - 1; i ++) {
    float score = 1;
    int new_turn[2];
    for(int c = 0; c < 2; c ++) {     
      new_turn[c] = (turn[c][i] > min_angle) - (turn[c][i] < -min_angle);
    }
    bool match1 = false;
    for(int c = 0; c < 2; c ++) {     
      if (new_turn[c] != cur_turn[c]) {
	if (cur_turn[c]) {
	  // turn finished -> check matchin
	  if (abs(cur_total[c]) > max_angle && ! cur_matched[c]) {
	    DBG("  [turn score]    -> turn not matched: c=%d turn=%.2f", c, cur_total[c]);
	    score = -1; // @todo be more tolerant for small size segments
	  }
	  if (((new_turn[1 - c] != cur_turn[1 - c]) || (cur_matched[c] < i - 1)) && 
	      (abs(cur_total[c]) > max_angle || abs(other_total[c]) > max_angle) &&
	      cur_matched[c] && ! match1) {

	    float expected = c?cur_total[1]:other_total[0];
	    float actual = c?other_total[1]:cur_total[0];

	    float scale = t1 + (t2 - t1) * sin(min(90, abs(expected)) * M_PI / 180);
	    float diff = abs(actual - expected);
	    if (abs(expected) < 180) {
	      float sc0 = diff / scale;
	      score = min(1, 1.5 - sc0);
	    }

	    DBG("  [turn score]    -> turn matched: %.2f / %.2f (scale=%.2f score=%.2f)", actual, expected, scale, score);
	    match1 = true;
	  }
										    
	}
	cur_total[c] = 0;
	cur_matched[c] = 0;
	other_total[c] = 0;
	cur_turn[c] = new_turn[c];
      }
    }

    DBG("  [turn score] %s[%d:%c] act=%.2f->[%d,%.2f,%d] - exp=%.2f->[%d,%.2f,%d] - length=%.2f --> score=%.2f", 
	getNameCharPtr(), i, (i == count - 1)?'*':letter_history[i],
	turn[0][i], cur_turn[0], cur_total[0], (int) cur_matched[0],
	turn[1][i], cur_turn[1], cur_total[1], (int) cur_matched[1],
	segment_length[i], score);

    // there is no "i = count - 1" iteration, this is just to match ongoing turns 
    if (i == count - 1) { break; } 

    for(int c = 0; c < 2; c ++) {     
      if (cur_turn[c]) {
	cur_total[c] += turn[c][i];
      }
    }
    for(int c = 0; c < 2; c ++) {     
      if (cur_turn[c]) {
	if (cur_turn[1 - c] == cur_turn[c]) { 
	  cur_matched[c] = i;
	  other_total[c] = cur_total[1 - c];
	}
      }
    }
  }

}

bool Scenario::postProcess() {
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

  ScoreCounter sc;
  char* cols[] = { (char*) "angle", (char*) "curve", (char*) "curv2", (char*) "dist", (char*) "length", (char*) "turn", 0 };
  float weights[] = { params -> weight_cos, params -> weight_curve, params -> weight_curve2, params -> weight_distance, params -> weight_length, params -> weight_turn };
  sc.set_debug(debug); // will draw nice tables :-)
  sc.set_cols(cols);
  sc.set_weights(weights);
  sc.set_pow(params -> score_pow);

  bool bad = false;
  for(int i = 0; i < count; i++) {
    // key scores
    
    sc.start_line();
    sc.set_line_coef(1.0 / count);
    if (debug) { sc.line_label << "Key " << i << ":" << index_history[i] << ":" << QString(letter_history[i]); }
    sc.add_score(scores[i].distance_score, (char*) "dist");
    if (i > 0 && i < count - 1) {
      sc.add_score(scores[i + 1].turn_score, (char*) "turn");
      if (scores[i + 1].turn_score < 0) { bad = true; }
    } else if (i == 1 && count == 2) {
      sc.add_score(1, (char*) "turn");
      // avoid bias towards 3+ letters scenarios
    }
    sc.end_line();

    // segment scores
    if (i < count - 1) {
      sc.start_line();
      sc.set_line_coef(segment_length[i] / total_length);
      if (debug) { sc.line_label << "Segment " << i + 1 << " [" << index_history[i] << ":" << index_history[i + 1] << "]"; }
      sc.add_score(scores[i + 1].cos_score, (char*) "angle"); 
      sc.add_score(scores[i + 1].curve_score, (char*) "curve");
      sc.add_score(scores[i + 1].length_score, (char*) "length");
      sc.add_bonus(scores[i + 1].curve_score2, (char*) "curv2");
      sc.end_line();
    }
  }

  int samepoint = 0;
  for(int i = 0; i < count - 1; i ++) {
    samepoint += (index_history[i] == index_history[i+1]);
  }
  
  float score1 = sc.get_score();
  float score = score1 * (1.0 + params->length_penalty * count) * (count - samepoint) / count; // my scores have a bias towards number of letters
  
  if (bad) { score = -1; }

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
     - find sharp turns
     - normal vector calculation */
  
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

  // speed evaluation
  for(int i = start_idx(0, 4); i < l; i ++) {
    int i1 = i - 2, i2 = i + 2;
    if (i1 < 0) { i1 = 0; i2 = 4; }
    if (i2 > l - 1) { i1 = l - 5; i2 = l - 1; }
    float dist = 0;
    for (int j = i1; j < i2; j ++) {
      dist += distancep(curve[j], curve[j+1]);
    }
    if (curve[i2].t > curve[i1].t) {
      curve[i].speed = 1000.0 * dist / (curve[i2].t - curve[i1].t);
      if (curve[i].speed > max_speed) { max_speed = curve[i].speed; }
    } else {
      // compatibility with old test cases, should not happen often :-)
      if (i > 0) { curve[i].speed = curve[i - 1].speed; } else { curve[i].speed = 1; }
    }
  }

  /* rotation / turning points */
  int wait_for_next_st = 0;
  for(int i = start_idx(2, 8) ; i < l - 2; i ++) {
    float total = 0;
    float t_index = 0;
    for(int j = i - 1; j <= i + 1; j ++) {
      total += curve[j].turn_angle;
      t_index += curve[j].turn_angle * j;
    }
    
    if (abs(total) < last_total_turn && last_total_turn > params.turn_threshold && wait_for_next_st < 0) {
      if (sharp_turn_index >= 2 && sharp_turn_index < l - 2) {

	for(int j = i - 1; j <= i + 1; j ++) {
	  if (abs(curve[j].turn_angle) > params.turn_threshold2) { 
	    sharp_turn_index = j;
	  }
	}

	curve[sharp_turn_index].sharp_turn = 1 + (last_total_turn > params.turn_threshold2);
	wait_for_next_st = 2;
	
	// compute "normal" vector (really lame algorithm)
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

    if (abs(total) > params.turn_threshold) {
      sharp_turn_index = int(0.5 + abs(t_index / total));
    }
    
    last_total_turn = abs(total);
    wait_for_next_st --;
  }

  // slow down point search
  int maxd = params.max_turn_index_gap;
  for(int i = max(maxd, start_idx(0, 0)); i < l - maxd; i ++) {
    int spd0 = curve[i].speed;
    int ok = 0;
    for(int j = -maxd; j <= maxd; j ++) {
      int spd = curve[i + j].speed;
      if (spd < spd0 || curve[i + j].sharp_turn) { ok = 0; break; }
      if (spd > params.slow_down_ratio * spd0) { ok |= (1 << (j>0)); }
    }
    if (ok == 3) { curve[i].sharp_turn = 3; }
  }
  

  // lookup inflection points
  int max_inf = 0;
  int max_inf_idx = 0;
  for(int i = start_idx(3, 6); i < l - 3; i ++) {
    int s1 = curve[i - 3].turn_angle + curve[i - 2].turn_angle + curve[i - 1].turn_angle;
    int s2 = curve[i + 2].turn_angle + curve[i + 1].turn_angle + curve[i].turn_angle;
    bool st = false;
    for (int j = i - 3; j < i + 3; j++) { st |= (curve[j].sharp_turn != 0); }
    if (s1 * s2 < 0 && abs(s1) < params.inf_max && abs(s2) < params.inf_max &&
	abs(s1) > params.inf_min && abs(s2) > params.inf_min) {
      int cur = -s1 * s2;
      if (cur > max_inf) {
	max_inf = cur;
	max_inf_idx = i - (abs(s2) > abs(s1));
      }
    } else {
      if (max_inf) {
	curve[max_inf_idx].sharp_turn = 4;
	max_inf = 0;
      }
    }
  }

}

void CurveMatch::curvePreprocess2() {  
  /* curve preprocessing that can be deferred until final score calculation: 
     - nothing at the moment :-) */
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
  max_speed = 0;
  curve.clear();
  done = false;
}

void CurveMatch::addPoint(Point point, int timestamp) {
  QTime now = QTime::currentTime();
  if (curve.isEmpty()) {
    max_speed = 0;
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
      st_skim ++;
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
    st_skim ++;
    Scenario s = scenarios.takeAt(0);
    DBG("filtering(size): %s (%.3f/%.3f)", QSTRING2PCHAR(s.getName()), s.getScore(), max_score);
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
    
  st_time = (int) timer.elapsed();
  st_count = count;
  scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list
  
  logdebug("Candidates: %d (time=%d, nodes=%d, forks=%d, skim=%d)", candidates.size(), st_time, st_count, st_fork, st_skim);

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
