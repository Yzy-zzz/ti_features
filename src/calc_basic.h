#ifndef CALC_BASIC_H_
#define CALC_BASIC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// 基础特征计算模块
// 负责：包数、字节数、包长统计、Payload 统计等基础特征
// ============================================================================

// 更新基础计数器
void calc_basic_counters(flow_feature_state_t* state,
                         int direction,
                         unsigned int pkt_len,
                         unsigned int payload_len);

// 更新包长统计
void calc_packet_length_stats(flow_feature_state_t* state,
                               int direction,
                               unsigned int pkt_len);

// 更新 Payload 统计
void calc_payload_stats(flow_feature_state_t* state,
                        unsigned char* payload,
                        unsigned int payload_len);

// 检测 Payload 魔数/协议特征
void calc_payload_magic(flow_feature_state_t* state,
                        unsigned char* payload,
                        unsigned int payload_len);

// 计算派生特征（二次计算）
void calc_derived_basic(flow_feature_state_t* state,
                        cJSON* output);

#endif /* CALC_BASIC_H_ */
