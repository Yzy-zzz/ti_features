#ifndef CALC_PROTOCOL_H_
#define CALC_PROTOCOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include "ti_features.h"
#include "feat_state.h"

// ============================================================================
// 协议头特征计算模块
// 负责：TCP 标志、IP 头部字段、端口统计等
// ============================================================================

// 更新 TCP 标志统计
void calc_protocol_tcp_update(flow_feature_state_t* state,
                               struct tcphdr* tcph,
                               unsigned int payload_len,
                               unsigned int window,
                               int direction,
                               unsigned int tcp_seq,
                               unsigned long long ts_us);

// 更新 IP 头部统计
void calc_protocol_ip_update(flow_feature_state_t* state,
                              struct iphdr* iph);

// 更新端口统计
void calc_protocol_port_update(flow_feature_state_t* state,
                                unsigned short src_port,
                                unsigned short dst_port);

// 判断是否为私有 IP
int is_private_ip(unsigned int ip);

// 判断是否为知名端口
int is_well_known_port(unsigned short port);

// 根据端口推断协议
const char* port_to_protocol(unsigned short port);

// 计算协议派生特征
void calc_derived_protocol(flow_feature_state_t* state,
                           cJSON* output);

#endif /* CALC_PROTOCOL_H_ */
