/* settings */

#ifndef PARAMS_H
#define PARAMS_H

class Params {
 public:
  /* BEGIN DECL */
  int accel_gap;
  float accel_ratio;
  int accel_threshold1;
  int accel_threshold2;
  float aggressive_mode;
  float anisotropy_ratio;
  int atp_excl_gap;
  int atp_max_pts;
  int atp_min_angle1;
  int atp_min_turn1;
  int atp_opt_gap;
  float atp_pt_61;
  int atp_threshold;
  float bad_tangent_score;
  int bjr_min_turn;
  int cat_window;
  float cls_distance_max_ratio;
  int cls_enable;
  float coef_distance;
  float coef_error;
  float coef_error_tmp;
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
  int end_scenario_wait;
  int error_correct;
  int error_ignore_count;
  int fallback_max_candidates;
  int fallback_min_length;
  int fallback_snapshot_queue;
  int fallback_start_count;
  int fallback_timeout;
  float final_coef_misc;
  float final_coef_turn;
  float final_coef_turn_exp;
  float final_distance_pow;
  float final_newdist_pow;
  int final_newdist_range;
  float final_score_v1_coef;
  float final_score_v1_threshold;
  int flat2_max_height;
  int flat2_min_height;
  float flat2_score_max;
  float flat2_score_min;
  int flat_max_angle;
  int flat_max_deviation;
  float flat_score;
  int incr_retry;
  int incremental_index_gap;
  int incremental_length_lag;
  int inf_max;
  int inf_min;
  int inter_pt_min_dist;
  float key_shift_ema_coef;
  int key_shift_enable;
  float key_shift_ratio;
  float lazy_loop_bias;
  float length_penalty;
  float loop_penalty;
  int loop_recover_max_len;
  int loop_threshold1;
  int loop_threshold2;
  int loop_threshold3;
  int match_wait;
  int max_active_scenarios;
  int max_active_scenarios2;
  int max_angle;
  int max_candidates;
  int max_segment_length;
  int max_star_index;
  int max_turn_index_gap;
  int min_turn_index_gap;
  int min_turn_index_gap_st;
  int multi_dot_threshold;
  int multi_max_time_rewind;
  float newdist_c1;
  float newdist_c2;
  float newdist_c3;
  float newdist_c5;
  float newdist_c6;
  float newdist_ctip;
  float newdist_length_bias_pow;
  float newdist_pow;
  float newdist_speed;
  int rt2_count_nz;
  int rt2_count_z;
  int rt2_flat_max;
  int rt2_high;
  int rt2_low;
  int rt2_offcenter;
  float rt2_score_coef;
  float rt_score_coef;
  float rt_score_coef_tip;
  int rt_tip_gaps;
  int rt_turn_threshold;
  int same_point_max_angle;
  float same_point_score;
  float score_pow;
  float sharp_turn_penalty;
  int slow_down_max_turn;
  float slow_down_ratio;
  float small_segment_min_score;
  int smooth;
  float sp_bad;
  int speed_max_index_gap;
  int speed_min_angle;
  float speed_penalty;
  int speed_time_interval;
  int st2_ignore;
  int st2_max;
  int st2_min;
  float st5_score;
  int straight_max_total;
  int straight_max_turn;
  float straight_score1;
  float straight_score2;
  float straight_slope;
  float straight_threshold_high;
  float straight_threshold_low;
  int straight_tip;
  int thumb_correction;
  float tip_small_segment;
  int turn2_ignore_maxgap;
  int turn2_ignore_maxlen;
  int turn2_ignore_minlen;
  float turn2_ignore_score;
  int turn2_ignore_zz_maxangle;
  int turn2_ignore_zz_maxlen;
  int turn2_ignore_zz_minangle;
  int turn2_large_threshold;
  int turn2_large_y0;
  int turn2_min_y2;
  float turn2_powscale_tip;
  float turn2_score1;
  float turn2_score_pow;
  int turn2_xscale_tip;
  int turn2_yscale;
  int turn2_yscale_tip;
  float turn2_yscaleratio;
  float turn_distance_ratio;
  float turn_distance_score;
  int turn_distance_threshold;
  int turn_max_angle;
  int turn_max_transfer;
  int turn_min_angle;
  int turn_min_gap;
  int turn_optim;
  float turn_score_unmatched;
  int turn_separation;
  int turn_threshold;
  int turn_threshold2;
  int turn_threshold3;
  int turn_threshold_st6;
  int user_dict_learn;
  int user_dict_size;
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
  5, // accel_gap
  0.9, // accel_ratio
  120, // accel_threshold1
  500, // accel_threshold2
  0.0, // aggressive_mode
  1.5, // anisotropy_ratio
  8, // atp_excl_gap
  5, // atp_max_pts
  34, // atp_min_angle1
  14, // atp_min_turn1
  4, // atp_opt_gap
  0.79, // atp_pt_61
  8, // atp_threshold
  0.03, // bad_tangent_score
  120, // bjr_min_turn
  12, // cat_window
  0.8, // cls_distance_max_ratio
  1, // cls_enable
  0.1, // coef_distance
  0.05, // coef_error
  0.05, // coef_error_tmp
  120, // cos_max_gap
  95, // curve_dist_threshold
  50, // curve_score_min_dist
  20.0, // curve_surface_coef
  100, // dist_max_next
  75, // dist_max_start
  22, // dst_x_add
  97, // dst_x_max
  40, // dst_y_add
  124, // dst_y_max
  100, // end_scenario_wait
  1, // error_correct
  5, // error_ignore_count
  80, // fallback_max_candidates
  4, // fallback_min_length
  3, // fallback_snapshot_queue
  5, // fallback_start_count
  100, // fallback_timeout
  1.0, // final_coef_misc
  29.0, // final_coef_turn
  0.33, // final_coef_turn_exp
  0.5, // final_distance_pow
  1.0, // final_newdist_pow
  40, // final_newdist_range
  0.0, // final_score_v1_coef
  0.12, // final_score_v1_threshold
  50, // flat2_max_height
  100, // flat2_min_height
  0.03, // flat2_score_max
  0.0, // flat2_score_min
  24, // flat_max_angle
  29, // flat_max_deviation
  0.06, // flat_score
  50, // incr_retry
  5, // incremental_index_gap
  100, // incremental_length_lag
  120, // inf_max
  20, // inf_min
  20, // inter_pt_min_dist
  0.005, // key_shift_ema_coef
  0, // key_shift_enable
  1.0, // key_shift_ratio
  0.02, // lazy_loop_bias
  0.001, // length_penalty
  0.01, // loop_penalty
  50, // loop_recover_max_len
  155, // loop_threshold1
  120, // loop_threshold2
  60, // loop_threshold3
  7, // match_wait
  105, // max_active_scenarios
  205, // max_active_scenarios2
  64, // max_angle
  50, // max_candidates
  25, // max_segment_length
  8, // max_star_index
  7, // max_turn_index_gap
  2, // min_turn_index_gap
  3, // min_turn_index_gap_st
  25, // multi_dot_threshold
  300, // multi_max_time_rewind
  0.43, // newdist_c1
  0.58, // newdist_c2
  2.63, // newdist_c3
  0.67, // newdist_c5
  1.62, // newdist_c6
  0.71, // newdist_ctip
  0.5, // newdist_length_bias_pow
  2.0, // newdist_pow
  3.12, // newdist_speed
  4, // rt2_count_nz
  5, // rt2_count_z
  37, // rt2_flat_max
  8, // rt2_high
  5, // rt2_low
  4, // rt2_offcenter
  0.2, // rt2_score_coef
  0.08, // rt_score_coef
  0.32, // rt_score_coef_tip
  3, // rt_tip_gaps
  5, // rt_turn_threshold
  120, // same_point_max_angle
  0.1, // same_point_score
  1.0, // score_pow
  0.6, // sharp_turn_penalty
  3, // slow_down_max_turn
  2.5, // slow_down_ratio
  0.2, // small_segment_min_score
  1, // smooth
  0.2, // sp_bad
  4, // speed_max_index_gap
  45, // speed_min_angle
  0.05, // speed_penalty
  58, // speed_time_interval
  120, // st2_ignore
  170, // st2_max
  70, // st2_min
  0.02, // st5_score
  15, // straight_max_total
  8, // straight_max_turn
  0.5, // straight_score1
  0.2, // straight_score2
  0.5, // straight_slope
  1.2, // straight_threshold_high
  0.6, // straight_threshold_low
  6, // straight_tip
  1, // thumb_correction
  0.02, // tip_small_segment
  20, // turn2_ignore_maxgap
  50, // turn2_ignore_maxlen
  100, // turn2_ignore_minlen
  0.97, // turn2_ignore_score
  40, // turn2_ignore_zz_maxangle
  70, // turn2_ignore_zz_maxlen
  120, // turn2_ignore_zz_minangle
  185, // turn2_large_threshold
  96, // turn2_large_y0
  5, // turn2_min_y2
  0.1, // turn2_powscale_tip
  0.014, // turn2_score1
  2.0, // turn2_score_pow
  160, // turn2_xscale_tip
  30, // turn2_yscale
  35, // turn2_yscale_tip
  2.8, // turn2_yscaleratio
  0.65, // turn_distance_ratio
  0.01, // turn_distance_score
  60, // turn_distance_threshold
  10, // turn_max_angle
  55, // turn_max_transfer
  10, // turn_min_angle
  4, // turn_min_gap
  160, // turn_optim
  0.3, // turn_score_unmatched
  184, // turn_separation
  75, // turn_threshold
  128, // turn_threshold2
  95, // turn_threshold3
  0, // turn_threshold_st6
  1, // user_dict_learn
  2000, // user_dict_size
  0.45, // ut_coef
  0.08, // ut_score
  80, // ut_total
  15, // ut_turn
  2.0, // weight_cos
  6.0, // weight_curve
  2.0, // weight_distance
  1.0, // weight_length
  1.5, // weight_misc
  8.0, // weight_turn

  /* END DEFAULT */
};

void Params::toJson(QJsonObject &json) const {
  /* BEGIN TOJSON */
  json["accel_gap"] = accel_gap;
  json["accel_ratio"] = accel_ratio;
  json["accel_threshold1"] = accel_threshold1;
  json["accel_threshold2"] = accel_threshold2;
  json["aggressive_mode"] = aggressive_mode;
  json["anisotropy_ratio"] = anisotropy_ratio;
  json["atp_excl_gap"] = atp_excl_gap;
  json["atp_max_pts"] = atp_max_pts;
  json["atp_min_angle1"] = atp_min_angle1;
  json["atp_min_turn1"] = atp_min_turn1;
  json["atp_opt_gap"] = atp_opt_gap;
  json["atp_pt_61"] = atp_pt_61;
  json["atp_threshold"] = atp_threshold;
  json["bad_tangent_score"] = bad_tangent_score;
  json["bjr_min_turn"] = bjr_min_turn;
  json["cat_window"] = cat_window;
  json["cls_distance_max_ratio"] = cls_distance_max_ratio;
  json["cls_enable"] = cls_enable;
  json["coef_distance"] = coef_distance;
  json["coef_error"] = coef_error;
  json["coef_error_tmp"] = coef_error_tmp;
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
  json["end_scenario_wait"] = end_scenario_wait;
  json["error_correct"] = error_correct;
  json["error_ignore_count"] = error_ignore_count;
  json["fallback_max_candidates"] = fallback_max_candidates;
  json["fallback_min_length"] = fallback_min_length;
  json["fallback_snapshot_queue"] = fallback_snapshot_queue;
  json["fallback_start_count"] = fallback_start_count;
  json["fallback_timeout"] = fallback_timeout;
  json["final_coef_misc"] = final_coef_misc;
  json["final_coef_turn"] = final_coef_turn;
  json["final_coef_turn_exp"] = final_coef_turn_exp;
  json["final_distance_pow"] = final_distance_pow;
  json["final_newdist_pow"] = final_newdist_pow;
  json["final_newdist_range"] = final_newdist_range;
  json["final_score_v1_coef"] = final_score_v1_coef;
  json["final_score_v1_threshold"] = final_score_v1_threshold;
  json["flat2_max_height"] = flat2_max_height;
  json["flat2_min_height"] = flat2_min_height;
  json["flat2_score_max"] = flat2_score_max;
  json["flat2_score_min"] = flat2_score_min;
  json["flat_max_angle"] = flat_max_angle;
  json["flat_max_deviation"] = flat_max_deviation;
  json["flat_score"] = flat_score;
  json["incr_retry"] = incr_retry;
  json["incremental_index_gap"] = incremental_index_gap;
  json["incremental_length_lag"] = incremental_length_lag;
  json["inf_max"] = inf_max;
  json["inf_min"] = inf_min;
  json["inter_pt_min_dist"] = inter_pt_min_dist;
  json["key_shift_ema_coef"] = key_shift_ema_coef;
  json["key_shift_enable"] = key_shift_enable;
  json["key_shift_ratio"] = key_shift_ratio;
  json["lazy_loop_bias"] = lazy_loop_bias;
  json["length_penalty"] = length_penalty;
  json["loop_penalty"] = loop_penalty;
  json["loop_recover_max_len"] = loop_recover_max_len;
  json["loop_threshold1"] = loop_threshold1;
  json["loop_threshold2"] = loop_threshold2;
  json["loop_threshold3"] = loop_threshold3;
  json["match_wait"] = match_wait;
  json["max_active_scenarios"] = max_active_scenarios;
  json["max_active_scenarios2"] = max_active_scenarios2;
  json["max_angle"] = max_angle;
  json["max_candidates"] = max_candidates;
  json["max_segment_length"] = max_segment_length;
  json["max_star_index"] = max_star_index;
  json["max_turn_index_gap"] = max_turn_index_gap;
  json["min_turn_index_gap"] = min_turn_index_gap;
  json["min_turn_index_gap_st"] = min_turn_index_gap_st;
  json["multi_dot_threshold"] = multi_dot_threshold;
  json["multi_max_time_rewind"] = multi_max_time_rewind;
  json["newdist_c1"] = newdist_c1;
  json["newdist_c2"] = newdist_c2;
  json["newdist_c3"] = newdist_c3;
  json["newdist_c5"] = newdist_c5;
  json["newdist_c6"] = newdist_c6;
  json["newdist_ctip"] = newdist_ctip;
  json["newdist_length_bias_pow"] = newdist_length_bias_pow;
  json["newdist_pow"] = newdist_pow;
  json["newdist_speed"] = newdist_speed;
  json["rt2_count_nz"] = rt2_count_nz;
  json["rt2_count_z"] = rt2_count_z;
  json["rt2_flat_max"] = rt2_flat_max;
  json["rt2_high"] = rt2_high;
  json["rt2_low"] = rt2_low;
  json["rt2_offcenter"] = rt2_offcenter;
  json["rt2_score_coef"] = rt2_score_coef;
  json["rt_score_coef"] = rt_score_coef;
  json["rt_score_coef_tip"] = rt_score_coef_tip;
  json["rt_tip_gaps"] = rt_tip_gaps;
  json["rt_turn_threshold"] = rt_turn_threshold;
  json["same_point_max_angle"] = same_point_max_angle;
  json["same_point_score"] = same_point_score;
  json["score_pow"] = score_pow;
  json["sharp_turn_penalty"] = sharp_turn_penalty;
  json["slow_down_max_turn"] = slow_down_max_turn;
  json["slow_down_ratio"] = slow_down_ratio;
  json["small_segment_min_score"] = small_segment_min_score;
  json["smooth"] = smooth;
  json["sp_bad"] = sp_bad;
  json["speed_max_index_gap"] = speed_max_index_gap;
  json["speed_min_angle"] = speed_min_angle;
  json["speed_penalty"] = speed_penalty;
  json["speed_time_interval"] = speed_time_interval;
  json["st2_ignore"] = st2_ignore;
  json["st2_max"] = st2_max;
  json["st2_min"] = st2_min;
  json["st5_score"] = st5_score;
  json["straight_max_total"] = straight_max_total;
  json["straight_max_turn"] = straight_max_turn;
  json["straight_score1"] = straight_score1;
  json["straight_score2"] = straight_score2;
  json["straight_slope"] = straight_slope;
  json["straight_threshold_high"] = straight_threshold_high;
  json["straight_threshold_low"] = straight_threshold_low;
  json["straight_tip"] = straight_tip;
  json["thumb_correction"] = thumb_correction;
  json["tip_small_segment"] = tip_small_segment;
  json["turn2_ignore_maxgap"] = turn2_ignore_maxgap;
  json["turn2_ignore_maxlen"] = turn2_ignore_maxlen;
  json["turn2_ignore_minlen"] = turn2_ignore_minlen;
  json["turn2_ignore_score"] = turn2_ignore_score;
  json["turn2_ignore_zz_maxangle"] = turn2_ignore_zz_maxangle;
  json["turn2_ignore_zz_maxlen"] = turn2_ignore_zz_maxlen;
  json["turn2_ignore_zz_minangle"] = turn2_ignore_zz_minangle;
  json["turn2_large_threshold"] = turn2_large_threshold;
  json["turn2_large_y0"] = turn2_large_y0;
  json["turn2_min_y2"] = turn2_min_y2;
  json["turn2_powscale_tip"] = turn2_powscale_tip;
  json["turn2_score1"] = turn2_score1;
  json["turn2_score_pow"] = turn2_score_pow;
  json["turn2_xscale_tip"] = turn2_xscale_tip;
  json["turn2_yscale"] = turn2_yscale;
  json["turn2_yscale_tip"] = turn2_yscale_tip;
  json["turn2_yscaleratio"] = turn2_yscaleratio;
  json["turn_distance_ratio"] = turn_distance_ratio;
  json["turn_distance_score"] = turn_distance_score;
  json["turn_distance_threshold"] = turn_distance_threshold;
  json["turn_max_angle"] = turn_max_angle;
  json["turn_max_transfer"] = turn_max_transfer;
  json["turn_min_angle"] = turn_min_angle;
  json["turn_min_gap"] = turn_min_gap;
  json["turn_optim"] = turn_optim;
  json["turn_score_unmatched"] = turn_score_unmatched;
  json["turn_separation"] = turn_separation;
  json["turn_threshold"] = turn_threshold;
  json["turn_threshold2"] = turn_threshold2;
  json["turn_threshold3"] = turn_threshold3;
  json["turn_threshold_st6"] = turn_threshold_st6;
  json["user_dict_learn"] = user_dict_learn;
  json["user_dict_size"] = user_dict_size;
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
  p.accel_gap = json["accel_gap"].toDouble();
  p.accel_ratio = json["accel_ratio"].toDouble();
  p.accel_threshold1 = json["accel_threshold1"].toDouble();
  p.accel_threshold2 = json["accel_threshold2"].toDouble();
  p.aggressive_mode = json["aggressive_mode"].toDouble();
  p.anisotropy_ratio = json["anisotropy_ratio"].toDouble();
  p.atp_excl_gap = json["atp_excl_gap"].toDouble();
  p.atp_max_pts = json["atp_max_pts"].toDouble();
  p.atp_min_angle1 = json["atp_min_angle1"].toDouble();
  p.atp_min_turn1 = json["atp_min_turn1"].toDouble();
  p.atp_opt_gap = json["atp_opt_gap"].toDouble();
  p.atp_pt_61 = json["atp_pt_61"].toDouble();
  p.atp_threshold = json["atp_threshold"].toDouble();
  p.bad_tangent_score = json["bad_tangent_score"].toDouble();
  p.bjr_min_turn = json["bjr_min_turn"].toDouble();
  p.cat_window = json["cat_window"].toDouble();
  p.cls_distance_max_ratio = json["cls_distance_max_ratio"].toDouble();
  p.cls_enable = json["cls_enable"].toDouble();
  p.coef_distance = json["coef_distance"].toDouble();
  p.coef_error = json["coef_error"].toDouble();
  p.coef_error_tmp = json["coef_error_tmp"].toDouble();
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
  p.end_scenario_wait = json["end_scenario_wait"].toDouble();
  p.error_correct = json["error_correct"].toDouble();
  p.error_ignore_count = json["error_ignore_count"].toDouble();
  p.fallback_max_candidates = json["fallback_max_candidates"].toDouble();
  p.fallback_min_length = json["fallback_min_length"].toDouble();
  p.fallback_snapshot_queue = json["fallback_snapshot_queue"].toDouble();
  p.fallback_start_count = json["fallback_start_count"].toDouble();
  p.fallback_timeout = json["fallback_timeout"].toDouble();
  p.final_coef_misc = json["final_coef_misc"].toDouble();
  p.final_coef_turn = json["final_coef_turn"].toDouble();
  p.final_coef_turn_exp = json["final_coef_turn_exp"].toDouble();
  p.final_distance_pow = json["final_distance_pow"].toDouble();
  p.final_newdist_pow = json["final_newdist_pow"].toDouble();
  p.final_newdist_range = json["final_newdist_range"].toDouble();
  p.final_score_v1_coef = json["final_score_v1_coef"].toDouble();
  p.final_score_v1_threshold = json["final_score_v1_threshold"].toDouble();
  p.flat2_max_height = json["flat2_max_height"].toDouble();
  p.flat2_min_height = json["flat2_min_height"].toDouble();
  p.flat2_score_max = json["flat2_score_max"].toDouble();
  p.flat2_score_min = json["flat2_score_min"].toDouble();
  p.flat_max_angle = json["flat_max_angle"].toDouble();
  p.flat_max_deviation = json["flat_max_deviation"].toDouble();
  p.flat_score = json["flat_score"].toDouble();
  p.incr_retry = json["incr_retry"].toDouble();
  p.incremental_index_gap = json["incremental_index_gap"].toDouble();
  p.incremental_length_lag = json["incremental_length_lag"].toDouble();
  p.inf_max = json["inf_max"].toDouble();
  p.inf_min = json["inf_min"].toDouble();
  p.inter_pt_min_dist = json["inter_pt_min_dist"].toDouble();
  p.key_shift_ema_coef = json["key_shift_ema_coef"].toDouble();
  p.key_shift_enable = json["key_shift_enable"].toDouble();
  p.key_shift_ratio = json["key_shift_ratio"].toDouble();
  p.lazy_loop_bias = json["lazy_loop_bias"].toDouble();
  p.length_penalty = json["length_penalty"].toDouble();
  p.loop_penalty = json["loop_penalty"].toDouble();
  p.loop_recover_max_len = json["loop_recover_max_len"].toDouble();
  p.loop_threshold1 = json["loop_threshold1"].toDouble();
  p.loop_threshold2 = json["loop_threshold2"].toDouble();
  p.loop_threshold3 = json["loop_threshold3"].toDouble();
  p.match_wait = json["match_wait"].toDouble();
  p.max_active_scenarios = json["max_active_scenarios"].toDouble();
  p.max_active_scenarios2 = json["max_active_scenarios2"].toDouble();
  p.max_angle = json["max_angle"].toDouble();
  p.max_candidates = json["max_candidates"].toDouble();
  p.max_segment_length = json["max_segment_length"].toDouble();
  p.max_star_index = json["max_star_index"].toDouble();
  p.max_turn_index_gap = json["max_turn_index_gap"].toDouble();
  p.min_turn_index_gap = json["min_turn_index_gap"].toDouble();
  p.min_turn_index_gap_st = json["min_turn_index_gap_st"].toDouble();
  p.multi_dot_threshold = json["multi_dot_threshold"].toDouble();
  p.multi_max_time_rewind = json["multi_max_time_rewind"].toDouble();
  p.newdist_c1 = json["newdist_c1"].toDouble();
  p.newdist_c2 = json["newdist_c2"].toDouble();
  p.newdist_c3 = json["newdist_c3"].toDouble();
  p.newdist_c5 = json["newdist_c5"].toDouble();
  p.newdist_c6 = json["newdist_c6"].toDouble();
  p.newdist_ctip = json["newdist_ctip"].toDouble();
  p.newdist_length_bias_pow = json["newdist_length_bias_pow"].toDouble();
  p.newdist_pow = json["newdist_pow"].toDouble();
  p.newdist_speed = json["newdist_speed"].toDouble();
  p.rt2_count_nz = json["rt2_count_nz"].toDouble();
  p.rt2_count_z = json["rt2_count_z"].toDouble();
  p.rt2_flat_max = json["rt2_flat_max"].toDouble();
  p.rt2_high = json["rt2_high"].toDouble();
  p.rt2_low = json["rt2_low"].toDouble();
  p.rt2_offcenter = json["rt2_offcenter"].toDouble();
  p.rt2_score_coef = json["rt2_score_coef"].toDouble();
  p.rt_score_coef = json["rt_score_coef"].toDouble();
  p.rt_score_coef_tip = json["rt_score_coef_tip"].toDouble();
  p.rt_tip_gaps = json["rt_tip_gaps"].toDouble();
  p.rt_turn_threshold = json["rt_turn_threshold"].toDouble();
  p.same_point_max_angle = json["same_point_max_angle"].toDouble();
  p.same_point_score = json["same_point_score"].toDouble();
  p.score_pow = json["score_pow"].toDouble();
  p.sharp_turn_penalty = json["sharp_turn_penalty"].toDouble();
  p.slow_down_max_turn = json["slow_down_max_turn"].toDouble();
  p.slow_down_ratio = json["slow_down_ratio"].toDouble();
  p.small_segment_min_score = json["small_segment_min_score"].toDouble();
  p.smooth = json["smooth"].toDouble();
  p.sp_bad = json["sp_bad"].toDouble();
  p.speed_max_index_gap = json["speed_max_index_gap"].toDouble();
  p.speed_min_angle = json["speed_min_angle"].toDouble();
  p.speed_penalty = json["speed_penalty"].toDouble();
  p.speed_time_interval = json["speed_time_interval"].toDouble();
  p.st2_ignore = json["st2_ignore"].toDouble();
  p.st2_max = json["st2_max"].toDouble();
  p.st2_min = json["st2_min"].toDouble();
  p.st5_score = json["st5_score"].toDouble();
  p.straight_max_total = json["straight_max_total"].toDouble();
  p.straight_max_turn = json["straight_max_turn"].toDouble();
  p.straight_score1 = json["straight_score1"].toDouble();
  p.straight_score2 = json["straight_score2"].toDouble();
  p.straight_slope = json["straight_slope"].toDouble();
  p.straight_threshold_high = json["straight_threshold_high"].toDouble();
  p.straight_threshold_low = json["straight_threshold_low"].toDouble();
  p.straight_tip = json["straight_tip"].toDouble();
  p.thumb_correction = json["thumb_correction"].toDouble();
  p.tip_small_segment = json["tip_small_segment"].toDouble();
  p.turn2_ignore_maxgap = json["turn2_ignore_maxgap"].toDouble();
  p.turn2_ignore_maxlen = json["turn2_ignore_maxlen"].toDouble();
  p.turn2_ignore_minlen = json["turn2_ignore_minlen"].toDouble();
  p.turn2_ignore_score = json["turn2_ignore_score"].toDouble();
  p.turn2_ignore_zz_maxangle = json["turn2_ignore_zz_maxangle"].toDouble();
  p.turn2_ignore_zz_maxlen = json["turn2_ignore_zz_maxlen"].toDouble();
  p.turn2_ignore_zz_minangle = json["turn2_ignore_zz_minangle"].toDouble();
  p.turn2_large_threshold = json["turn2_large_threshold"].toDouble();
  p.turn2_large_y0 = json["turn2_large_y0"].toDouble();
  p.turn2_min_y2 = json["turn2_min_y2"].toDouble();
  p.turn2_powscale_tip = json["turn2_powscale_tip"].toDouble();
  p.turn2_score1 = json["turn2_score1"].toDouble();
  p.turn2_score_pow = json["turn2_score_pow"].toDouble();
  p.turn2_xscale_tip = json["turn2_xscale_tip"].toDouble();
  p.turn2_yscale = json["turn2_yscale"].toDouble();
  p.turn2_yscale_tip = json["turn2_yscale_tip"].toDouble();
  p.turn2_yscaleratio = json["turn2_yscaleratio"].toDouble();
  p.turn_distance_ratio = json["turn_distance_ratio"].toDouble();
  p.turn_distance_score = json["turn_distance_score"].toDouble();
  p.turn_distance_threshold = json["turn_distance_threshold"].toDouble();
  p.turn_max_angle = json["turn_max_angle"].toDouble();
  p.turn_max_transfer = json["turn_max_transfer"].toDouble();
  p.turn_min_angle = json["turn_min_angle"].toDouble();
  p.turn_min_gap = json["turn_min_gap"].toDouble();
  p.turn_optim = json["turn_optim"].toDouble();
  p.turn_score_unmatched = json["turn_score_unmatched"].toDouble();
  p.turn_separation = json["turn_separation"].toDouble();
  p.turn_threshold = json["turn_threshold"].toDouble();
  p.turn_threshold2 = json["turn_threshold2"].toDouble();
  p.turn_threshold3 = json["turn_threshold3"].toDouble();
  p.turn_threshold_st6 = json["turn_threshold_st6"].toDouble();
  p.user_dict_learn = json["user_dict_learn"].toDouble();
  p.user_dict_size = json["user_dict_size"].toDouble();
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

