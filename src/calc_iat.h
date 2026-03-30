#ifndef CALC_IAT_H_
#define CALC_IAT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// IAT（Inter-Arrival Time）特征计算模块
// 负责：IAT 统计、活跃/空闲时间、响应延迟等
// ============================================================================

// 更新 IAT 统计
void calc_iat_update(flow_feature_state_t* state,
                     unsigned long long ts_us,
                     int direction);

// 更新活跃/空闲时间
void calc_active_idle_time(flow_feature_state_t* state,
                            unsigned long long iat_us);

// 更新响应延迟（请求 - 响应配对）
void calc_response_delay(flow_feature_state_t* state,
                         unsigned long long ts_us,
                         int direction);

// 计算 IAT 派生特征
void calc_derived_iat(flow_feature_state_t* state,
                      cJSON* output);

#endif /* CALC_IAT_H_ */
