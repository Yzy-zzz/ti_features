#ifndef CALC_SEQUENCE_H_
#define CALC_SEQUENCE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// 序列特征计算模块
// 负责：自相关、复杂度分析、游程统计、Hurst 指数等
// ============================================================================

// 计算包长序列自相关（滞后 1-N）
void calc_autocorr_packet_length(flow_feature_state_t* state,
                                  cJSON* output,
                                  int max_lag);

// 计算 IAT 序列自相关
void calc_autocorr_iat(flow_feature_state_t* state,
                       cJSON* output,
                       int max_lag);

// 计算游程统计（连续相同长度包）
void calc_run_length(flow_feature_state_t* state,
                     cJSON* output);

// 计算序列复杂度（Lempel-Ziv）
double calc_lz78_complexity(unsigned int* seq, int n);

// 计算 Hurst 指数（R/S 分析）
double calc_hurst_exponent(unsigned long long* iat_seq, int n);

// 计算方向序列熵
double calc_direction_entropy(flow_feature_state_t* state);

// 计算长度 - 方向相关系数
double calc_length_direction_correlation(flow_feature_state_t* state);

// 计算长度 - IAT 相关系数
double calc_length_iat_correlation(flow_feature_state_t* state);

// 计算序列派生特征
void calc_derived_sequence(flow_feature_state_t* state,
                           cJSON* output);

#endif /* CALC_SEQUENCE_H_ */
