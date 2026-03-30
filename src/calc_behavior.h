#ifndef CALC_BEHAVIOR_H_
#define CALC_BEHAVIOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// 行为特征计算模块
// 负责：流模式、周期性检测、行为分类等
// ============================================================================

// 判断是否为交互式会话
int is_interactive_session(double avg_response_delay_ms);

// 判断是否为大流量传输
int is_bulk_transfer(double throughput_kbps);

// 判断是否为短连接
int is_short_connection(double duration_s);

// 计算流不对称比
double calc_flow_asymmetry_ratio(unsigned long fwd_bytes,
                                  unsigned long bwd_bytes);

// 获取流模式字符串
const char* get_flow_pattern(double asymmetry_ratio);

// 检测周期性（基于 FFT 主频占比）
int has_strong_periodicity(double dominant_freq_ratio);

// 判断是否有规律间隔
int has_regular_intervals(double cv);

// 计算行为派生特征
void calc_derived_behavior(flow_feature_state_t* state,
                           cJSON* output);

#endif /* CALC_BEHAVIOR_H_ */
