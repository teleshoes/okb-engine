#include "curve_match.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTextStream>
#include <cmath> 
#include <iostream>
#include <math.h>

using namespace std;

/* --- parameters --- */
static Params default_params = {
  70, // dist_max_start
  100, // dist_max_next
  7, // match_wait
  45, // max_angle
  20, // max_turn_error1
  150, // max_turn_error2
  80, // max_turn_error3
  80, // length_score_scale
  50, // curve_score_min_dist
  2.0, // score_pow
  0.35, // weight_distance;
  2.0, // weight_cos;
  0.2, // weight_length;
  2.0, // weight_curve;
  0.1, // weight_curve2;
  1.0, // weight_turn;
  0.001, // length_penalty
  15, // turn_threshold;
  80, // turn_threshold2;
  5, // max_turn_index_gap;
  120, // curve_dist_threshold
  0.35, // small_segment_min_score
  1.25, // anisotropy_ratio
  0.4, // sharp_turn_penalty
  5, // curv_amin
  75, // curv_amax
  70, // curv_turnmax
  50, // max_active_scenarios
  30, // max_candidates
};

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
  json["turn_threshold2"] = turn_threshold2;	 
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
  p.turn_threshold2 = json["turn_threshold2"].toDouble();
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
  return p;
}

/* --- point --- */
Point::Point() {
  this -> x = this -> y = 0;
}

Point::Point(int x, int y) {
  this -> x = x;
  this -> y = y;
}

Point Point::operator-(const Point &other) {
  return Point(x - other.x, y - other.y);
}

Point Point::operator+(const Point &other) {
  return Point(x + other.x, y + other.y);
}

Point Point::operator*(const float &other) {
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
  key.label = json["k"].toString().toLocal8Bit().constData()[0];
  return key;
}

/* --- local functions --- */
// NIH all over the place
static float distance(float x1, float y1, float x2, float y2) {
  return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}
static float distancep(Point &p1, Point &p2) {
  return distance(p1.x, p1.y, p2.x, p2.y);
}

static float cos_angle(float x1, float y1, float x2, float y2) {
  return (x1 * x2 + y1 * y2) / (sqrt(pow(x1, 2) + pow(y1, 2)) * sqrt(pow(x2, 2) + pow(y2, 2)));
}

static float angle(float x1, float y1, float x2, float y2) {
  float cosa = cos_angle(x1, y1, x2, y2);
  float value = (cosa > 1)?0:acos(cosa); // for rounding errors
  if (x1*y2 - x2*y1 < 0) { value = -value; }
  return value;
}

static float anglep(Point p1, Point p2) {
  return angle(p1.x, p1.y, p2.x, p2.y);
}

static float dist_line_point(Point p1, Point p2, Point p) {
  float lp = distancep(p1, p2);
  
  float u = 1.0 * ((p.x - p1.x) * (p2.x - p1.x) + (p.y - p1.y) * (p2.y - p1.y)) / lp / lp;
  Point proj = Point(p1.x + u * (p2.x - p1.x), p1.y + u * (p2.y - p1.y));

  float result = distancep(proj, p);
  return result;
}    

template <typename T>
static QString qlist2str(QList<T> lst) {
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

/* --- scenario --- */
Scenario::Scenario(LetterTree *tree, QHash<unsigned char, Key> *keys, QList<CurvePoint> *curve, Params *params) {
  this -> node = tree -> getRoot();
  this -> keys = keys;
  this -> curve = curve;
  name = "";
  finished = false;
  count = 0;
  index = 0;
  this -> params = params;
  temp_score = 0;
  final_score = -1;
  last_fork = -1;
}

float Scenario::calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index) {
  Key k1 = (*keys)[prev_letter];
  Key k2 = (*keys)[letter];
  Point p1 = (*curve)[index];
  Point p2 = (*curve)[new_index];
  float cos = cos_angle(k2.x - k1.x, k2.y - k1.y, p2.x - p1.x, p2.y - p1.y);
  return 1 - acos(cos) / (params->max_angle * M_PI / 180);
}

float Scenario::calc_turn_score(unsigned char letter, int index) {
  /* score based on angle between two consecutive segments */

  if (count < 2) { return 0; }

  // curve index
  int i1 = index_history[count - 2].second;
  int i2 = index_history[count - 1].second;
  int i3 = index;

  // curve points
  Point p1 = (*curve)[i1];
  Point p2 = (*curve)[i2];
  Point p3 = (*curve)[i3];

  // letters
  unsigned char l1 = index_history[count - 2].first.getChar();
  unsigned char l2 = index_history[count - 1].first.getChar();
  unsigned char l3 = letter;  

  // key coordinates
  Point k1 = Point((*keys)[l1].x, (*keys)[l1].y);
  Point k2 = Point((*keys)[l2].x, (*keys)[l2].y);
  Point k3 = Point((*keys)[l3].x, (*keys)[l3].y);

  // compare angles
  float expected = anglep(k2 - k1, k3 - k2) * 180 / M_PI;
  float actual = anglep(p2 - p1, p3 - p2) * 180 / M_PI;

  float diff = abs(actual - expected);
  if (diff > 180) { diff = abs(diff - 360); }

  // thresholds
  int t1 = params->max_turn_error1;
  int t2 = params->max_turn_error2;
  int t3 = params->max_turn_error3;

  int t = (abs(expected)<90?t2:t3);
  if (t2 < t1 || t2 < t3) { return -1; } // in case the optimizer goes nuts

  float scale = t + (t - t1) * abs(sin(expected * M_PI / 180));

  if (expected * actual < 0 && abs(actual) > 5 && abs(expected) > 5) { diff *= 2; } // penalty for inverted angle

  float score = 1 - pow(diff / scale, 2);
  return score;
}

Point Scenario::expected_tangent(int index) {
  int i1 = index_history[index].second;
  Point d1, d2;
  Point origin(0, 0);
  if (index > 0) {
    int i0 = index_history[index - 1].second;  
    d1 = (*curve)[i1] - (*curve)[i0];
    d1 = d1 * (1000 / distancep(origin, d1)); // using integers was clearing a mistake
  }
  if (index < index_history.size() - 1) {
    int i2 = index_history[index + 1].second;
    d2 = (*curve)[i2] - (*curve)[i1];
    d2 = d2 * (1000 / distancep(origin, d2));
  }
  return d1 + d2;
}

float Scenario::begin_end_angle_score(bool end) {
  /* bonus score for curve ends pointing in the right direction */

  Point expected, actual;
  Point origin(0, 0);

  if (end) {
    int l = index_history.size();
    int nc = curve -> size();
    expected = expected_tangent(l - 1);
    actual = (*curve)[nc - 1] - (*curve)[nc - 3];
  } else {
    expected = expected_tangent(0);
    actual = (*curve)[2] - (*curve)[0];
  }

  float cosa = cos_angle(expected.x, expected.y, actual.x, actual.y);
  float sc = 0.5 - pow(acos(cosa) / (params->max_angle * M_PI / 180), 2) / 2; // this is a relative score

  if (distancep(actual, origin) < params->curve_score_min_dist) {
    // direction of a small segment may be irrelevant
    if (sc < 0) { sc = 0; }
  }
  
  return sc;
}

float Scenario::calc_curviness_score(int index) {
  /* score based on expected radius of curvature sign and derivative */
  int l = index_history.size();
  int i1 = index_history[index].second;
  int i2 = index_history[index + 1].second;

  Point p = (*curve)[i2] - (*curve)[i1];
  Point v1 = expected_tangent(index);
  Point v2 = expected_tangent(index + 1);
  float a1 = anglep(v1, p) * 180.0 / M_PI;
  float a2 = anglep(p, v2) * 180.0 / M_PI;
  
  int angle_threshold = 15;
  int s1 = (a1 > angle_threshold) - (a1 < -angle_threshold); // i found this on stackoverflow, it must be good
  int s2 = (a2 > angle_threshold) - (a2 < -angle_threshold);

  float result = 0;
  int max_inflection_point = 1;
  int expected_sign = 0;
  int sum_bad = -1;
  if ((*curve)[i1].turn_smooth * s1 < 0 && abs(a1) < params->curv_amax) {
    result = -1;
  } else if ((*curve)[i2].turn_smooth * s2 < 0 && abs(a2) < params->curv_amax) {
    result = -1;
  } else {
    if ((s1 * s2 == 1) || (s1 && index == l - 2) || (s2 && index == 0)) {
      max_inflection_point = 0;
      expected_sign = (s1 + s2 > 0?1:-1);
    }

    int infcount = 0;
    int sign = 0;
    sum_bad = 0;
    for (int i = i1; i <= i2; i++) {
      int value = (*curve)[i].turn_smooth;
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

  DBG("  [curviness score] %s[%d] i=%d:%d a=%.3f/%.3f s=%d/%d [maxpt=%d] score=%.3f (sum_bad=%d exp_sign=%d)", name.toLocal8Bit().constData(), index, i1, i2, a1, a2, s1, s2, max_inflection_point, result, sum_bad, expected_sign);

  return result;
}

float Scenario::calc_curve_score(unsigned char, unsigned char, int index, int new_index) {
  /* score based on curve "smoothness" and maximum distance from straight path */

  float max_dist = 0;
  int sharp_turn = 0;
  for (int i = index + 1; i < new_index - 1; i++) {
    float dist = dist_line_point((*curve)[index], (*curve)[new_index], (*curve)[i]);
    if (dist > max_dist) { max_dist = dist; }


    if (i > index + 2 && i < new_index - 2 && (*curve)[i].sharp_turn && i >= 2 && i < curve->size() - 2) { sharp_turn ++; }
  }

  float score = 1 - (max_dist / params->curve_dist_threshold) - params->sharp_turn_penalty * sharp_turn;

  return score;
}

float Scenario::calc_length_score(unsigned char prev_letter, unsigned char letter, int index, int new_index) {
  /* score based on distance between points for 2 successive keys */

  Key k1 = (*keys)[prev_letter];
  Key k2 = (*keys)[letter];

  float expected = distance(k1.x, k1.y, k2.x, k2.y);
  float actual = distancep((*curve)[index], (*curve)[new_index]);

  float scale = (float) params->length_score_scale;
  return 1 - pow((actual - expected) / scale / (1 + 0.2 * expected / scale), 2); // add 20% error margin for each "distance scale"
}

float Scenario::calc_distance_score(unsigned char letter, int index, int count) {
  /* score based on distance to from curve to key */

  float ratio = (count > 0)?params->dist_max_next:params->dist_max_start;
  Key k = (*keys)[letter];
  
  float cplus;
  float cminus; 
  float dx, dy;

  cplus = cminus = 1 / params->anisotropy_ratio;

  /* accept a bit of user laziness in curve beginning, end and sharp turns -> let's add some anisotropy */
  if (count == 0) {
    dx = (*curve)[1].x - (*curve)[0].x;
    dy = (*curve)[1].y - (*curve)[0].y;
  } else if (count == -1) {
    int idx = curve -> size() - 1;
    dx = (*curve)[idx].x - (*curve)[idx-1].x;
    dy = (*curve)[idx].y - (*curve)[idx-1].y;      
  } else if ((*curve)[index].sharp_turn) {
    dx = (*curve)[index].normalx * 100;
    dy = (*curve)[index].normaly * 100; // why did i use integers ?!
    cminus = 1;
  } else { // unremarkable point
    cplus = cminus = 0; // use standard formula
  }

  float px = k.x - (*curve)[index].x;
  float py = k.y - (*curve)[index].y;
  
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

float Scenario::get_next_key_match(unsigned char letter, int index, QList<int> &new_index_list) {
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

  while(1) {
    if (index >= (*curve).size()) { break; }
    
    float new_score = calc_distance_score(letter, index, this -> count);

    // @todo add a "more verbose" debug option
    // DBG("    get_next_key_match(%c, %d) index=%d score=%.3f)", letter, start_index, index, new_score); // QQQ

    if (new_score > max_score) {
      max_score = new_score;
      max_score_index = index;
    }
    
    if (new_score > score) {
      retry = 0;
    } else {
      retry ++;
      if (retry > params->match_wait && count > params->match_wait) { break; }
    }

    if ((*curve)[index].sharp_turn && ! last_turn_point && index > start_index) {
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

    count ++;
    index ++;
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
	if ((*curve)[i].sharp_turn) {
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
	name.toLocal8Bit().constData(), letter, start_index, max_score, max_score_index, last_turn_point_score, 
	last_turn_point, index, qlist2str(new_index_list).toLocal8Bit().constData());
    return max_score;
  } else {
    DBG("  [get_next_key_match] %s:%c[%d] max_score=%.3f[%d] last_turnpoint=%.3f[%d] last_index=%d -> FAIL",
	name.toLocal8Bit().constData(), letter, start_index, max_score, max_score_index, last_turn_point_score,
	last_turn_point, index);
    return -1;
  }

}

float Scenario::speedCoef(int index) {
  return sqrt(1000.0 / (*curve)[index].speed);
}

void Scenario::childScenario(LetterNode &childNode, bool endScenario, QList<Scenario> &result, int &st_fork) {
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

      int new_index = curve -> size() - 1;
      new_index_list << new_index;
      distance_score = calc_distance_score(letter, new_index, -1);

    } else {

      distance_score = get_next_key_match(letter, index, new_index_list);
      if (new_index_list.isEmpty()) {
	DBG("debug [%s:%c] %s =FAIL= {distance / turning point}", name.toLocal8Bit().constData(), letter,endScenario?"*":" ");
	return; 
      }

    }
  }
  
  if (new_index_list.size() >= 2) { st_fork ++; }

  bool first = true;
  foreach(int new_index, new_index_list) {
    
    score_t score = {0, 0, 0, 0, 0, 0}; // all scores are : <0 reject, 1 = best value
    score.distance_score = distance_score;

    if (count > 0) {
      float curve_advance = distance((*curve)[index].x, (*curve)[index].y, (*curve)[new_index].x, (*curve)[new_index].y);
      
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
	name.toLocal8Bit().constData(), letter, endScenario?"*":" ", first?"":" <FORK>", count, new_index, ok?"=OK=":"*FAIL*", 
	score.distance_score, score.curve_score, score.cos_score, score.length_score, score.turn_score);

    if (ok) {
      // create a new scenario for this child node
      Scenario new_scenario(*this); // use default copy constructor
      new_scenario.node = childNode;
      new_scenario.index = new_index;
      new_scenario.name.append(letter);
      new_scenario.scores.append(score);
      new_scenario.index_history.append(QPair<LetterNode, int>(childNode, new_index));
      new_scenario.count = count + 1;
      new_scenario.finished = endScenario;
      if (new_index_list.size() >= 2) new_scenario.last_fork = count + 1;

      // temp score is used only for simple filtering
      if (count == 0) {
	new_scenario.temp_score = temp_score + score.distance_score;
      } else {
	new_scenario.temp_score = temp_score + (score.distance_score * params->weight_distance 
						+ score.curve_score * params->weight_curve
						+ score.cos_score * params->weight_cos 
						+ score.turn_score * params->weight_turn
						+ score.length_score * params->weight_length);
      }

      result.append(new_scenario);
    }

    first = false;

  } // for (new_index)
}

void Scenario::nextKey(QList<Scenario> &result, int &st_fork) {
  foreach (LetterNode child, node.getChilds()) {
    if (child.hasPayload() && count >= 1) {      
      childScenario(child, true, result, st_fork);
    }
    childScenario(child, false, result, st_fork);
  }
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

bool Scenario::forkLast() {
  return last_fork == count;
}

void Scenario::postProcess() {
  
  // evaluate final score
  evalScore();

  float score_bak = final_score;

  // bonus for "curviness"
  int l = index_history.size();
  for(int i = 0; i < l - 1; i ++) {
    float sc = calc_curviness_score(i);
    if (i == 0) { sc += begin_end_angle_score(false); }
    if (i == l - 2) { sc += begin_end_angle_score(true); }

    scores[i + 1].curve_score2 = sc; // just for logging
    final_score += sc * params->weight_curve2;
    if (final_score < 0) { final_score = 0; /* this one is really bad */ }
  }

  DBG("  [postProcess] -> Final score [%s] = %.3f -> %.3f", name.toLocal8Bit().constData(), score_bak, final_score);
}

float Scenario::evalScore() {
  if (scores.size() < 2) { return 0; }
  int l = scores.size();
  float score = 0;

  // key scores
  float total = 0;
  float total_coef = 0;
  for (int i = 0; i < l; i++) {
    int idx = index_history[i].second;
    float coef = speedCoef(idx);
    total_coef += coef;
    total += pow(scores[i].distance_score, params->score_pow) * params->weight_distance;
  }
  score += total / total_coef;

  // segment scores
  total = 0;
  total_coef = 0;
  for (int i = 1; i < l; i++) {
    int idx = (index_history[i - 1].second + index_history[i].second) / 2;
    float coef = speedCoef(idx);
    total_coef += coef;
    total += coef * (pow(scores[i].cos_score, params->score_pow) * params->weight_cos +
		     pow(scores[i].turn_score, params->score_pow) * params->weight_turn +
		     pow(scores[i].curve_score, params->score_pow) * params->weight_curve +
		     pow(scores[i].length_score, params->score_pow) * params->weight_length)
      / (params->weight_distance + params->weight_length + params->weight_cos + params-> weight_curve + 
	 (i >= 2?params->weight_turn:0)); // normalize [0,1];

  }
  score += total / total_coef;

  score *= (1.0 +  params->length_penalty * scores.size());
  
  this -> final_score = score;
  return score;
}

void Scenario::toJson(QJsonObject &json) {
  json["name"] = name;
  json["finished"] = finished;
  json["score"] = getScore();

  QJsonArray json_score_array;
  for(int i = 0; i < count; i ++) {
    QJsonObject json_score;

    score_t* score = &(scores[i]);

    json_score["letter"] = QString(index_history[i].first.getChar());
    json_score["index"] = index_history[i].second;
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
}

void CurveMatch::curvePreprocess() {
  /* evaluate speed and find turning points */

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
  // avoid some side effects on curve_score (because there is often a delay before second point)
  curve[0].turn_angle = curve[1].turn_angle;
  curve[l-1].turn_angle = curve[l-2].turn_angle;
  
  for (int i = 1; i < l - 1 ; i ++) {
    curve[i].turn_smooth = int(0.5 * curve[i].turn_angle + 0.25 * curve[i-1].turn_angle + 0.25 * curve[i+1].turn_angle);
  }
  curve[0].turn_smooth = curve[1].turn_smooth;
  curve[l-1].turn_smooth = curve[l-2].turn_smooth;

  int index0 = -1;
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
      curve[i].speed = 1000 * dist / (curve[index2].t - curve[index1].t);
    } else {
      // compatibility with old test cases, should not happen often :-)
      if (i > 0) { curve[i].speed = curve[i - 1].speed; } else { curve[i].speed = 1000; }
    }

    /* rotation / turning points */
    if (abs(curve[i].turn_angle) > params.turn_threshold) {
      if (index0 == -1) { index0 = i; }
    } else if (index0 != -1) {
      float total = 0;
      float t_index = 0;
      for (int j = index0 ; j < i ; j ++) {
	total += curve[j].turn_angle;
	t_index += curve[j].turn_angle * j;
      }
      if (abs(total) > params.turn_threshold2) {
	int middle_index = int(0.5 + abs(t_index / total));
	if (middle_index >= 2 && middle_index < l - 2) {
	  curve[middle_index].sharp_turn = true;

	  // compute "normal" vector (really lame algorithm)
	  int i1 = middle_index - 1;
	  int i2 = middle_index + 1;
	  float x1 = curve[i1].x - curve[i1 - 1].x;
	  float y1 = curve[i1].y - curve[i1 - 1].y;
	  float x2 = curve[i2 + 1].x - curve[i2].x;
	  float y2 = curve[i2 + 1].y - curve[i2].y;
	  float l1 = sqrt(x1 * x1 + y1 * y1);
	  float l2 = sqrt(x2 * x2 + y2 * y2);	  
	  curve[middle_index].normalx = x1 / l1 - x2 / l2;
	  curve[middle_index].normaly = y1 / l1 - y2 / l2;
	}
      }
      index0 = -1;
    }
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
}

void CurveMatch::addPoint(Point point) {
  if (curve.isEmpty()) {
    startTime = QTime::currentTime();
  }
  QTime now = QTime::currentTime();
  curve << CurvePoint(point, startTime.msecsTo(now));
}

bool CurveMatch::loadTree(QString fileName) {
  bool status = wordtree.loadFromFile(fileName);
  loaded = status;
  this -> treeFile = fileName;
  qDebug("loadTree(%s): %d", fileName.toLocal8Bit().constData(), status);
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

void CurveMatch::scenarioFilter(QList<Scenario> &scenarios, float score_ratio, int min_size, int max_size) {
  float max_score = 0;

  qSort(scenarios.begin(), scenarios.end());

  foreach(Scenario s, scenarios) {
    float sc = s.getScore();
    if (sc > max_score) { max_score = sc; }
  }

  QHash<QString, int> dejavu;

  int i = 0;
  while (i < scenarios.size()) {
    float sc = scenarios[i].getTempScore();

    // remove scenarios with duplicate words (but not just after a scenario fork)
    if (! scenarios[i].forkLast()) {
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
      DBG("filtering(score): %s (%.3f/%.3f)", scenarios[i].getName().toLocal8Bit().constData(), sc, max_score);
      scenarios.takeAt(i);
    } else {
      i ++;
    }
  }

  // enforce max scenarios count
  if (max_size > 1) {
    while (scenarios.size() > max_size) {
      Scenario s = scenarios.takeAt(0);
      DBG("filtering(size): %s (%.3f/%.3f)", s.getName().toLocal8Bit().constData(), s.getScore(), max_score);
    }
  }
}

bool CurveMatch::match() {
  // @TODO make an incremental version (with persistent scenario list)
  scenarios.clear();
  candidates.clear();
 
  if (! loaded || ! keys.size() || ! curve.size()) { return false; }
  if (curve.size() < 3) { return false; }
 
  curvePreprocess();

  Scenario root = Scenario(&wordtree, &keys, &curve, &params);
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
      scenarioFilter(scenarios, 0.9 - 0.6 * (1.0 / (1.0 + (float) n)), 15, params.max_active_scenarios); // @todo add to parameter list
      DBG("Depth: %d - Scenarios: %d - Candidates: %d", n, scenarios.size(), candidates.size());      
    }
  }
    
  st_time = (int) timer.elapsed();
  st_count = count;
  scenarioFilter(candidates, 0.7, 10, params.max_candidates); // @todo add to parameter list
  
  qDebug("Candidate: %d (time=%d, nodes=%d, forks=%d, skim=%d)", candidates.size(), st_time, st_count, st_fork, st_skim);

  return candidates.size() > 0;
}

void CurveMatch::endCurve(int id) {
  this -> id = id;
  log(QString("IN: ") + toString());
  match();
  log(QString("OUT: ") + resultToString());
}

void CurveMatch::setParameters(QString jsonStr) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  params = Params::fromJson(doc.object());
}

void CurveMatch::useDefaultParameters() {
  params = default_params;
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
