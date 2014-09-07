/* settings */

#ifndef PARAMS_H
#define PARAMS_H

class Params {
 public:
  /* BEGIN DECL */
  float anisotropy_ratio;
  int cos_max_gap;
  int curve_dist_threshold;
  int curve_score_min_dist;
  int dist_max_next;
  int dist_max_start;
  int incremental_index_gap;
  int incremental_length_lag;
  int inf_max;
  int inf_min;
  float length_penalty;
  int match_wait;
  int max_active_scenarios;
  int max_angle;
  int max_candidates;
  int max_turn_error1;
  int max_turn_error2;
  int max_turn_error3;
  int max_turn_index_gap;
  int min_turn_index_gap;
  int same_point_max_angle;
  float same_point_score;
  float score_pow;
  float sharp_turn_penalty;
  float slow_down_ratio;
  float small_segment_min_score;
  int turn_threshold;
  int turn_threshold2;
  float weight_cos;
  float weight_curve;
  float weight_curve2;
  float weight_distance;
  float weight_length;
  float weight_turn;

  /* END DECL */
  void toJson(QJsonObject &json) const;
  static Params fromJson(const QJsonObject &json);
};
#endif /* PARAMS_H */


#ifdef PARAMS_IMPL

static Params default_params = {
  /* BEGIN DEFAULT */
  1.5, // anisotropy_ratio
  100, // cos_max_gap
  85, // curve_dist_threshold
  50, // curve_score_min_dist
  100, // dist_max_next
  75, // dist_max_start
  5, // incremental_index_gap
  100, // incremental_length_lag
  120, // inf_max
  20, // inf_min
  0.001, // length_penalty
  7, // match_wait
  500, // max_active_scenarios
  45, // max_angle
  30, // max_candidates
  20, // max_turn_error1
  70, // max_turn_error2
  30, // max_turn_error3
  6, // max_turn_index_gap
  2, // min_turn_index_gap
  120, // same_point_max_angle
  0.1, // same_point_score
  1.0, // score_pow
  0.4, // sharp_turn_penalty
  1.5, // slow_down_ratio
  0.05, // small_segment_min_score
  75, // turn_threshold
  150, // turn_threshold2
  1.0, // weight_cos
  6.0, // weight_curve
  6.0, // weight_curve2
  3.0, // weight_distance
  1.0, // weight_length
  1.0, // weight_turn

  /* END DEFAULT */
};

void Params::toJson(QJsonObject &json) const {
  /* BEGIN TOJSON */
  json["anisotropy_ratio"] = anisotropy_ratio;
  json["cos_max_gap"] = cos_max_gap;
  json["curve_dist_threshold"] = curve_dist_threshold;
  json["curve_score_min_dist"] = curve_score_min_dist;
  json["dist_max_next"] = dist_max_next;
  json["dist_max_start"] = dist_max_start;
  json["incremental_index_gap"] = incremental_index_gap;
  json["incremental_length_lag"] = incremental_length_lag;
  json["inf_max"] = inf_max;
  json["inf_min"] = inf_min;
  json["length_penalty"] = length_penalty;
  json["match_wait"] = match_wait;
  json["max_active_scenarios"] = max_active_scenarios;
  json["max_angle"] = max_angle;
  json["max_candidates"] = max_candidates;
  json["max_turn_error1"] = max_turn_error1;
  json["max_turn_error2"] = max_turn_error2;
  json["max_turn_error3"] = max_turn_error3;
  json["max_turn_index_gap"] = max_turn_index_gap;
  json["min_turn_index_gap"] = min_turn_index_gap;
  json["same_point_max_angle"] = same_point_max_angle;
  json["same_point_score"] = same_point_score;
  json["score_pow"] = score_pow;
  json["sharp_turn_penalty"] = sharp_turn_penalty;
  json["slow_down_ratio"] = slow_down_ratio;
  json["small_segment_min_score"] = small_segment_min_score;
  json["turn_threshold"] = turn_threshold;
  json["turn_threshold2"] = turn_threshold2;
  json["weight_cos"] = weight_cos;
  json["weight_curve"] = weight_curve;
  json["weight_curve2"] = weight_curve2;
  json["weight_distance"] = weight_distance;
  json["weight_length"] = weight_length;
  json["weight_turn"] = weight_turn;

  /* END TOJSON */
}

Params Params::fromJson(const QJsonObject &json) {
  Params p;

  /* BEGIN FROMJSON */
  p.anisotropy_ratio = json["anisotropy_ratio"].toDouble();
  p.cos_max_gap = json["cos_max_gap"].toDouble();
  p.curve_dist_threshold = json["curve_dist_threshold"].toDouble();
  p.curve_score_min_dist = json["curve_score_min_dist"].toDouble();
  p.dist_max_next = json["dist_max_next"].toDouble();
  p.dist_max_start = json["dist_max_start"].toDouble();
  p.incremental_index_gap = json["incremental_index_gap"].toDouble();
  p.incremental_length_lag = json["incremental_length_lag"].toDouble();
  p.inf_max = json["inf_max"].toDouble();
  p.inf_min = json["inf_min"].toDouble();
  p.length_penalty = json["length_penalty"].toDouble();
  p.match_wait = json["match_wait"].toDouble();
  p.max_active_scenarios = json["max_active_scenarios"].toDouble();
  p.max_angle = json["max_angle"].toDouble();
  p.max_candidates = json["max_candidates"].toDouble();
  p.max_turn_error1 = json["max_turn_error1"].toDouble();
  p.max_turn_error2 = json["max_turn_error2"].toDouble();
  p.max_turn_error3 = json["max_turn_error3"].toDouble();
  p.max_turn_index_gap = json["max_turn_index_gap"].toDouble();
  p.min_turn_index_gap = json["min_turn_index_gap"].toDouble();
  p.same_point_max_angle = json["same_point_max_angle"].toDouble();
  p.same_point_score = json["same_point_score"].toDouble();
  p.score_pow = json["score_pow"].toDouble();
  p.sharp_turn_penalty = json["sharp_turn_penalty"].toDouble();
  p.slow_down_ratio = json["slow_down_ratio"].toDouble();
  p.small_segment_min_score = json["small_segment_min_score"].toDouble();
  p.turn_threshold = json["turn_threshold"].toDouble();
  p.turn_threshold2 = json["turn_threshold2"].toDouble();
  p.weight_cos = json["weight_cos"].toDouble();
  p.weight_curve = json["weight_curve"].toDouble();
  p.weight_curve2 = json["weight_curve2"].toDouble();
  p.weight_distance = json["weight_distance"].toDouble();
  p.weight_length = json["weight_length"].toDouble();
  p.weight_turn = json["weight_turn"].toDouble();

  /* END FROMJSON */

  return p;
}

#endif /* PARAMS_IMPL */

