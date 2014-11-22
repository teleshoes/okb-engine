/* settings */

#ifndef PARAMS_H
#define PARAMS_H

class Params {
 public:
  /* BEGIN DECL */
  float anisotropy_ratio;
  int atp_max_pts;
  int atp_min_angle1;
  int atp_min_turn1;
  int atp_opt_gap;
  int atp_threshold;
  int cat_window;
  float cls_distance_max_ratio;
  int cls_enable;
  float coef_distance;
  float coef_error;
  int cos_max_gap;
  int curve_dist_threshold;
  int curve_score_min_dist;
  float curve_surface_coef;
  int dist_max_next;
  int dist_max_start;
  int dst_x_add;
  int dst_x_max;
  int dst_y_add;
  int dst_y_max;
  int error_correct;
  int error_correct_gap;
  int incremental_index_gap;
  int incremental_length_lag;
  int inf_max;
  int inf_min;
  float lazy_loop_bias;
  float length_penalty;
  int match_wait;
  int max_active_scenarios;
  int max_angle;
  int max_candidates;
  int max_segment_length;
  int max_star_index;
  int max_turn_error1;
  int max_turn_error2;
  int max_turn_error3;
  int max_turn_index_gap;
  int min_turn_index_gap;
  float rt_score_coef;
  int rt_tip_gaps;
  int rt_total_threshold;
  int rt_turn_threshold;
  int same_point_max_angle;
  float same_point_score;
  float score_pow;
  float sharp_turn_penalty;
  float slow_down_min;
  float slow_down_ratio;
  float small_segment_min_score;
  int speed_max_index_gap;
  int speed_min_angle;
  float speed_penalty;
  int st2_max;
  int st2_min;
  float st5_score;
  int thumb_correction;
  float tip_small_segment;
  float turn_distance_ratio;
  float turn_distance_score;
  int turn_distance_threshold;
  int turn_max_angle;
  int turn_min_angle;
  int turn_optim;
  int turn_separation;
  int turn_threshold;
  int turn_threshold2;
  int turn_threshold3;
  int turn_tip_min_distance;
  float ut_coef;
  float ut_score;
  int ut_total;
  int ut_turn;
  float weight_cos;
  float weight_curve;
  float weight_distance;
  float weight_length;
  float weight_misc;
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
  8, // atp_max_pts
  30, // atp_min_angle1
  12, // atp_min_turn1
  4, // atp_opt_gap
  4, // atp_threshold
  12, // cat_window
  0.8, // cls_distance_max_ratio
  1, // cls_enable
  0.1, // coef_distance
  0.1, // coef_error
  100, // cos_max_gap
  85, // curve_dist_threshold
  50, // curve_score_min_dist
  20.0, // curve_surface_coef
  100, // dist_max_next
  75, // dist_max_start
  30, // dst_x_add
  70, // dst_x_max
  40, // dst_y_add
  120, // dst_y_max
  1, // error_correct
  4, // error_correct_gap
  5, // incremental_index_gap
  100, // incremental_length_lag
  120, // inf_max
  20, // inf_min
  0.005, // lazy_loop_bias
  0.001, // length_penalty
  7, // match_wait
  350, // max_active_scenarios
  45, // max_angle
  25, // max_candidates
  25, // max_segment_length
  8, // max_star_index
  30, // max_turn_error1
  70, // max_turn_error2
  50, // max_turn_error3
  6, // max_turn_index_gap
  2, // min_turn_index_gap
  1.0, // rt_score_coef
  3, // rt_tip_gaps
  20, // rt_total_threshold
  5, // rt_turn_threshold
  120, // same_point_max_angle
  0.1, // same_point_score
  1.0, // score_pow
  0.6, // sharp_turn_penalty
  0.6, // slow_down_min
  1.5, // slow_down_ratio
  0.2, // small_segment_min_score
  5, // speed_max_index_gap
  15, // speed_min_angle
  0.5, // speed_penalty
  170, // st2_max
  125, // st2_min
  0.01, // st5_score
  1, // thumb_correction
  0.05, // tip_small_segment
  0.65, // turn_distance_ratio
  1.0, // turn_distance_score
  60, // turn_distance_threshold
  20, // turn_max_angle
  10, // turn_min_angle
  120, // turn_optim
  120, // turn_separation
  75, // turn_threshold
  140, // turn_threshold2
  115, // turn_threshold3
  30, // turn_tip_min_distance
  0.45, // ut_coef
  0.5, // ut_score
  80, // ut_total
  15, // ut_turn
  2.0, // weight_cos
  3.0, // weight_curve
  2.0, // weight_distance
  1.0, // weight_length
  8.0, // weight_misc
  4.0, // weight_turn

  /* END DEFAULT */
};

void Params::toJson(QJsonObject &json) const {
  /* BEGIN TOJSON */
  json["anisotropy_ratio"] = anisotropy_ratio;
  json["atp_max_pts"] = atp_max_pts;
  json["atp_min_angle1"] = atp_min_angle1;
  json["atp_min_turn1"] = atp_min_turn1;
  json["atp_opt_gap"] = atp_opt_gap;
  json["atp_threshold"] = atp_threshold;
  json["cat_window"] = cat_window;
  json["cls_distance_max_ratio"] = cls_distance_max_ratio;
  json["cls_enable"] = cls_enable;
  json["coef_distance"] = coef_distance;
  json["coef_error"] = coef_error;
  json["cos_max_gap"] = cos_max_gap;
  json["curve_dist_threshold"] = curve_dist_threshold;
  json["curve_score_min_dist"] = curve_score_min_dist;
  json["curve_surface_coef"] = curve_surface_coef;
  json["dist_max_next"] = dist_max_next;
  json["dist_max_start"] = dist_max_start;
  json["dst_x_add"] = dst_x_add;
  json["dst_x_max"] = dst_x_max;
  json["dst_y_add"] = dst_y_add;
  json["dst_y_max"] = dst_y_max;
  json["error_correct"] = error_correct;
  json["error_correct_gap"] = error_correct_gap;
  json["incremental_index_gap"] = incremental_index_gap;
  json["incremental_length_lag"] = incremental_length_lag;
  json["inf_max"] = inf_max;
  json["inf_min"] = inf_min;
  json["lazy_loop_bias"] = lazy_loop_bias;
  json["length_penalty"] = length_penalty;
  json["match_wait"] = match_wait;
  json["max_active_scenarios"] = max_active_scenarios;
  json["max_angle"] = max_angle;
  json["max_candidates"] = max_candidates;
  json["max_segment_length"] = max_segment_length;
  json["max_star_index"] = max_star_index;
  json["max_turn_error1"] = max_turn_error1;
  json["max_turn_error2"] = max_turn_error2;
  json["max_turn_error3"] = max_turn_error3;
  json["max_turn_index_gap"] = max_turn_index_gap;
  json["min_turn_index_gap"] = min_turn_index_gap;
  json["rt_score_coef"] = rt_score_coef;
  json["rt_tip_gaps"] = rt_tip_gaps;
  json["rt_total_threshold"] = rt_total_threshold;
  json["rt_turn_threshold"] = rt_turn_threshold;
  json["same_point_max_angle"] = same_point_max_angle;
  json["same_point_score"] = same_point_score;
  json["score_pow"] = score_pow;
  json["sharp_turn_penalty"] = sharp_turn_penalty;
  json["slow_down_min"] = slow_down_min;
  json["slow_down_ratio"] = slow_down_ratio;
  json["small_segment_min_score"] = small_segment_min_score;
  json["speed_max_index_gap"] = speed_max_index_gap;
  json["speed_min_angle"] = speed_min_angle;
  json["speed_penalty"] = speed_penalty;
  json["st2_max"] = st2_max;
  json["st2_min"] = st2_min;
  json["st5_score"] = st5_score;
  json["thumb_correction"] = thumb_correction;
  json["tip_small_segment"] = tip_small_segment;
  json["turn_distance_ratio"] = turn_distance_ratio;
  json["turn_distance_score"] = turn_distance_score;
  json["turn_distance_threshold"] = turn_distance_threshold;
  json["turn_max_angle"] = turn_max_angle;
  json["turn_min_angle"] = turn_min_angle;
  json["turn_optim"] = turn_optim;
  json["turn_separation"] = turn_separation;
  json["turn_threshold"] = turn_threshold;
  json["turn_threshold2"] = turn_threshold2;
  json["turn_threshold3"] = turn_threshold3;
  json["turn_tip_min_distance"] = turn_tip_min_distance;
  json["ut_coef"] = ut_coef;
  json["ut_score"] = ut_score;
  json["ut_total"] = ut_total;
  json["ut_turn"] = ut_turn;
  json["weight_cos"] = weight_cos;
  json["weight_curve"] = weight_curve;
  json["weight_distance"] = weight_distance;
  json["weight_length"] = weight_length;
  json["weight_misc"] = weight_misc;
  json["weight_turn"] = weight_turn;

  /* END TOJSON */
}

Params Params::fromJson(const QJsonObject &json) {
  Params p;

  /* BEGIN FROMJSON */
  p.anisotropy_ratio = json["anisotropy_ratio"].toDouble();
  p.atp_max_pts = json["atp_max_pts"].toDouble();
  p.atp_min_angle1 = json["atp_min_angle1"].toDouble();
  p.atp_min_turn1 = json["atp_min_turn1"].toDouble();
  p.atp_opt_gap = json["atp_opt_gap"].toDouble();
  p.atp_threshold = json["atp_threshold"].toDouble();
  p.cat_window = json["cat_window"].toDouble();
  p.cls_distance_max_ratio = json["cls_distance_max_ratio"].toDouble();
  p.cls_enable = json["cls_enable"].toDouble();
  p.coef_distance = json["coef_distance"].toDouble();
  p.coef_error = json["coef_error"].toDouble();
  p.cos_max_gap = json["cos_max_gap"].toDouble();
  p.curve_dist_threshold = json["curve_dist_threshold"].toDouble();
  p.curve_score_min_dist = json["curve_score_min_dist"].toDouble();
  p.curve_surface_coef = json["curve_surface_coef"].toDouble();
  p.dist_max_next = json["dist_max_next"].toDouble();
  p.dist_max_start = json["dist_max_start"].toDouble();
  p.dst_x_add = json["dst_x_add"].toDouble();
  p.dst_x_max = json["dst_x_max"].toDouble();
  p.dst_y_add = json["dst_y_add"].toDouble();
  p.dst_y_max = json["dst_y_max"].toDouble();
  p.error_correct = json["error_correct"].toDouble();
  p.error_correct_gap = json["error_correct_gap"].toDouble();
  p.incremental_index_gap = json["incremental_index_gap"].toDouble();
  p.incremental_length_lag = json["incremental_length_lag"].toDouble();
  p.inf_max = json["inf_max"].toDouble();
  p.inf_min = json["inf_min"].toDouble();
  p.lazy_loop_bias = json["lazy_loop_bias"].toDouble();
  p.length_penalty = json["length_penalty"].toDouble();
  p.match_wait = json["match_wait"].toDouble();
  p.max_active_scenarios = json["max_active_scenarios"].toDouble();
  p.max_angle = json["max_angle"].toDouble();
  p.max_candidates = json["max_candidates"].toDouble();
  p.max_segment_length = json["max_segment_length"].toDouble();
  p.max_star_index = json["max_star_index"].toDouble();
  p.max_turn_error1 = json["max_turn_error1"].toDouble();
  p.max_turn_error2 = json["max_turn_error2"].toDouble();
  p.max_turn_error3 = json["max_turn_error3"].toDouble();
  p.max_turn_index_gap = json["max_turn_index_gap"].toDouble();
  p.min_turn_index_gap = json["min_turn_index_gap"].toDouble();
  p.rt_score_coef = json["rt_score_coef"].toDouble();
  p.rt_tip_gaps = json["rt_tip_gaps"].toDouble();
  p.rt_total_threshold = json["rt_total_threshold"].toDouble();
  p.rt_turn_threshold = json["rt_turn_threshold"].toDouble();
  p.same_point_max_angle = json["same_point_max_angle"].toDouble();
  p.same_point_score = json["same_point_score"].toDouble();
  p.score_pow = json["score_pow"].toDouble();
  p.sharp_turn_penalty = json["sharp_turn_penalty"].toDouble();
  p.slow_down_min = json["slow_down_min"].toDouble();
  p.slow_down_ratio = json["slow_down_ratio"].toDouble();
  p.small_segment_min_score = json["small_segment_min_score"].toDouble();
  p.speed_max_index_gap = json["speed_max_index_gap"].toDouble();
  p.speed_min_angle = json["speed_min_angle"].toDouble();
  p.speed_penalty = json["speed_penalty"].toDouble();
  p.st2_max = json["st2_max"].toDouble();
  p.st2_min = json["st2_min"].toDouble();
  p.st5_score = json["st5_score"].toDouble();
  p.thumb_correction = json["thumb_correction"].toDouble();
  p.tip_small_segment = json["tip_small_segment"].toDouble();
  p.turn_distance_ratio = json["turn_distance_ratio"].toDouble();
  p.turn_distance_score = json["turn_distance_score"].toDouble();
  p.turn_distance_threshold = json["turn_distance_threshold"].toDouble();
  p.turn_max_angle = json["turn_max_angle"].toDouble();
  p.turn_min_angle = json["turn_min_angle"].toDouble();
  p.turn_optim = json["turn_optim"].toDouble();
  p.turn_separation = json["turn_separation"].toDouble();
  p.turn_threshold = json["turn_threshold"].toDouble();
  p.turn_threshold2 = json["turn_threshold2"].toDouble();
  p.turn_threshold3 = json["turn_threshold3"].toDouble();
  p.turn_tip_min_distance = json["turn_tip_min_distance"].toDouble();
  p.ut_coef = json["ut_coef"].toDouble();
  p.ut_score = json["ut_score"].toDouble();
  p.ut_total = json["ut_total"].toDouble();
  p.ut_turn = json["ut_turn"].toDouble();
  p.weight_cos = json["weight_cos"].toDouble();
  p.weight_curve = json["weight_curve"].toDouble();
  p.weight_distance = json["weight_distance"].toDouble();
  p.weight_length = json["weight_length"].toDouble();
  p.weight_misc = json["weight_misc"].toDouble();
  p.weight_turn = json["weight_turn"].toDouble();

  /* END FROMJSON */

  return p;
}

#endif /* PARAMS_IMPL */

