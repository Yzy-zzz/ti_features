#ifndef CALC_BURST_H_
#define CALC_BURST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// Burst 特征计算模块
// 负责：Burst 检测、Burst 统计、Burstiness 指数等
// ============================================================================

// 更新 Burst 状态
void calc_burst_update(flow_feature_state_t* state,
                       unsigned int pkt_len,
                       unsigned long long iat_us,
                       unsigned long long ts_us);

// 计算 Burst 统计特征
void calc_burst_statistics(flow_feature_state_t* state,
                           cJSON* output);

// 计算 Burstiness 指数
double calc_burstiness_index(flow_feature_state_t* state);

#endif /* CALC_BURST_H_ */
