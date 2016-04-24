#!/usr/bin/python3
# -*- coding: utf-8 -*-

""" List of all parameters with type and min/max values """

defparams = [
    [ "accel_gap", int, 2, 10 ],
    [ "accel_ratio", float, 0.2, 1.0 ],
    [ "accel_threshold1", int, 10, 500 ],
    [ "accel_threshold2", int, 10, 500 ],
    [ "aggressive_mode", float ],
    [ "anisotropy_ratio", float, 1, 2 ],
    [ "atp_excl_gap", int, 2, 10],
    [ "atp_max_pts", int, 3, 12],
    [ "atp_min_angle1", int, 10, 45],
    [ "atp_min_turn1", int, 5, 25],
    [ "atp_opt_gap", int, 1, 6],
    [ "atp_threshold", int, 1, 20],
    [ "bjr_min_turn", int, 90, 180],
    [ "cat_window", int ],  # no optim, larger is better (and slower)
    [ "cls_distance_max_ratio", float, 0, 2 ],
    [ "cls_enable", int ],
    [ "coef_distance", float, 0.01, 1 ],
    [ "coef_error", float ],  # more is better (at the moment ... unless i have better test cases)
    [ "cos_max_gap", int, 20, 150 ],
    [ "curve_dist_threshold", int, 30, 200 ],
    [ "curve_score_min_dist", int, 20, 100 ],
    [ "curve_surface_coef", float, 1, 50 ],
    [ "dist_max_next", int, 0, 150 ],
    [ "dist_max_start", int, 0, 150 ],
    [ "dst_x_add", int, 0, 50 ],
    [ "dst_x_max", int, 0, 100 ],
    [ "dst_y_add", int, 0, 50 ],
    [ "dst_y_max", int, 0, 200 ],
    [ "end_scenario_wait", int ],
    [ "error_correct", int ],
    [ "error_ignore_count", int, 3, 10 ],
    [ "final_coef_misc", float, 0.1, 5 ],
    [ "final_coef_turn", float, 0.1, 20 ],
    [ "final_coef_turn_exp", float, 0.01, 2 ],
    [ "final_distance_pow", float, 0.1, 5 ],
    [ "final_newdist_pow", float, 0.1, 10 ],
    [ "final_newdist_range", int, 5, 80 ],
    [ "final_score_v1_coef", float, 0, 5 ],
    [ "final_score_v1_threshold", float, 0, 1 ],
    [ "flat_max_angle", int, 5, 25 ],
    [ "flat_max_deviation", int, 10, 100 ],
    [ "flat_score", float, 0, 1 ],
    [ "incr_retry", int ],  # no optim => performance only
    [ "incremental_index_gap", int ],
    [ "incremental_length_lag", int ],  # no optimization
    [ "inf_max", int, 40, 180 ],
    [ "inf_min", int, 5, 40 ],
    [ "inter_pt_min_dist", int, 5, 80 ],
    [ "lazy_loop_bias", float, 0, 1 ],
    [ "length_penalty", float, -0.1, 0.1 ],
    [ "loop_penalty", float, 0, 1 ],
    [ "match_wait", int, 4, 12 ],
    [ "max_active_scenarios", int ],  # no optimization, larger is always better
    [ "max_angle", int, 10, 90 ],
    [ "max_candidates", int ],  # idem
    [ "max_segment_length", int ],
    [ "max_star_index", int, 3, 20 ],
    [ "max_turn_index_gap", int, 2, 10],
    [ "min_turn_index_gap", int, 1, 5],
    [ "multi_dot_threshold", int ],
    [ "multi_max_time_rewind", int ],  # impact is not continuous
    [ "newdist_c1", float, 0, 4 ],
    [ "newdist_c2", float, 0, 4 ],
    [ "newdist_c3", float, 0, 10 ],
    [ "newdist_c5", float, 0, 4 ],
    [ "newdist_ctip", float, 0, 4 ],
    [ "newdist_length_bias_pow", float, 0, 2],
    [ "newdist_pow", float, 0.1, 5 ],
    [ "newdist_speed", float, 0, 3 ],
    [ "rt2_count_nz", int, 1, 10 ],
    [ "rt2_count_z", int, 1, 10 ],
    [ "rt2_flat_max", int, 5, 40 ],
    [ "rt2_high", int, 5, 20 ],
    [ "rt2_low", int, 1, 10 ],
    [ "rt2_offcenter", int, 1, 10 ],
    [ "rt2_score_coef", float, 0, 1 ],
    [ "rt_score_coef", float, 0.01, 2 ],
    [ "rt_score_coef_tip", float, 0.01, 2 ],
    [ "rt_tip_gaps", int, 1, 5 ],
    [ "rt_turn_threshold", int, 1, 20 ],
    [ "same_point_max_angle", int, 45, 179 ],
    [ "same_point_score", float, 0, 1 ],
    [ "score_pow", float, 0.1, 10 ],
    [ "sharp_turn_penalty", float, 0, 1 ],
    [ "slow_down_ratio", float, 1, 2 ],
    [ "small_segment_min_score", float, 0.01, 0.9 ],
    [ "sp_bad", float, 0, 1 ],
    [ "speed_max_index_gap", int, 1, 6 ],
    [ "speed_min_angle", int, 1, 45 ],
    [ "speed_penalty", float, 0.01, 1 ],
    [ "speed_time_interval", int, 10, 300 ],
    [ "st2_ignore", int, 90, 150 ],
    [ "st2_max", int, 120, 179 ],
    [ "st2_min", int, 70, 160 ],
    [ "st5_score", float, 0, 1],
    [ "straight_max_total", int, 1, 30],
    [ "straight_max_turn", int, 1, 20],
    [ "straight_score1", float, 0, 1],
    [ "straight_score2", float, 0, 1],
    [ "straight_slope", float, 0, 1],
    [ "straight_threshold_high", float, 0.1, 4],
    [ "straight_threshold_low", float, 0.1, 4],
    [ "thumb_correction", int ],  # user decision, depends on style
    [ "tip_small_segment", float, 0, .5 ],
    [ "turn2_ignore_maxgap", int, 5, 30 ],
    [ "turn2_ignore_maxlen", int, 10, 80 ],
    [ "turn2_ignore_minlen", int, 50, 300 ],
    [ "turn2_ignore_score", float, 0, 1],
    [ "turn2_large_threshold", int, 181, 270 ],
    [ "turn2_large_y0", int, 10, 100 ],
    [ "turn2_min_y2", int, 1, 30 ],
    [ "turn2_powscale_tip", float, 0.1, 5 ],
    [ "turn2_score1", float, 0, 0.9 ],
    [ "turn2_score_pow", float, 0.2, 5],
    [ "turn2_xscale_tip", int, 150, 800 ],
    [ "turn2_yscale", int, 5, 80 ],
    [ "turn2_yscale_tip", int, 5, 80 ],
    [ "turn2_yscaleratio", float, 1, 4 ],
    [ "turn_distance_ratio", float, 0, 2 ],
    [ "turn_distance_score", float, 0, 2 ],
    [ "turn_distance_threshold", int, 10, 150 ],
    [ "turn_max_angle", int, 10, 80 ],
    [ "turn_max_transfer", int, 10, 90 ],
    [ "turn_min_angle", int, 1, 45 ],
    [ "turn_optim", int, 20, 150 ],
    [ "turn_score_unmatched", float, 0, 1],
    [ "turn_separation", int, 30, 300 ],
    [ "turn_threshold", int, 10, 85 ],
    [ "turn_threshold2", int, 120, 179 ],
    [ "turn_threshold3", int, 90, 145 ],
    [ "user_dict_learn", int ],
    [ "user_dict_size", int ],
    [ "ut_coef", float, 0.1, 0.9 ],
    [ "ut_score", float, 0, 1 ],
    [ "ut_total", int, 20, 90 ],
    [ "ut_turn", int, 5, 45 ],
    [ "weight_cos", float, 0.1, 10 ],
    [ "weight_curve", float, 0.1, 10 ],
    [ "weight_distance", float ],  # reference (=1)
    [ "weight_length", float, 0.1, 10 ],
    [ "weight_misc", float, 0.1, 10 ],
    [ "weight_turn", float, 0.1, 10 ],
    [ "turn2_ignore_zz_maxangle", int, 10, 90 ],
    [ "turn2_ignore_zz_maxlen", int, 10, 120 ],
    [ "turn2_ignore_zz_minangle", int, 90, 180 ],
]
