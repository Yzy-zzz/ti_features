#ifndef CALC_WINDOW_H_
#define CALC_WINDOW_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// 窗口特征计算模块
// 负责：时间窗口内的包数、字节数、比特率统计
// ============================================================================

// 更新窗口统计
void calc_window_update(flow_feature_state_t* state,
                        unsigned int pkt_len,
                        unsigned long long ts_us);

// 计算窗口统计特征
void calc_derived_window(flow_feature_state_t* state,
                         cJSON* output);

#endif /* CALC_WINDOW_H_ */
