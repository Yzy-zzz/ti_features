#ifndef CALC_PAYLOAD_H_
#define CALC_PAYLOAD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// Payload 特征计算模块
// 负责：载荷熵、字节分布、协议检测、压缩率等
// ============================================================================

// 计算字节熵（香农熵）
double calc_byte_entropy(unsigned char* data, int len);

// 计算马尔可夫熵
double calc_markov_entropy(unsigned char* data, int len);

// 计算卡方统计量
double calc_chi_square(unsigned char* data, int len);

// 检测是否像 HTTP
int payload_looks_like_http(unsigned char* data, int len);

// 检测是否像 JSON
int payload_looks_like_json(unsigned char* data, int len);

// 检测是否像 XML
int payload_looks_like_xml(unsigned char* data, int len);

// 检测是否像 TLS
int payload_looks_like_tls(unsigned char* data, int len);

// 检测是否像 SSH
int payload_looks_like_ssh(unsigned char* data, int len);

// 计算压缩率
double calc_compression_ratio(unsigned char* data, int len);

// 计算 Payload 派生特征
void calc_derived_payload(flow_feature_state_t* state,
                          cJSON* output);

#endif /* CALC_PAYLOAD_H_ */
