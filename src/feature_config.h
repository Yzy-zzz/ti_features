#ifndef FEATURE_CONFIG_H_
#define FEATURE_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"

// 配置参数结构
typedef struct _ti_feature_config
{
    // 日志相关
    void* log_handle;
    // char log_path[MAX_PATH_LEN];
    // short log_level;

    // 序列长度限制
    unsigned int max_seq_len;
    unsigned int first_n_packets;         // 前 N 包统计窗口大小

    // 时间来源模式：0=包时间戳，1=平台当前时间
    unsigned int run_mode;

    // 包大小阈值
    unsigned int small_pkt_threshold;      // 小包阈值 (默认 64B)
    unsigned int large_pkt_threshold;      // 大包阈值 (默认 1200B)

    // Burst 检测阈值
    unsigned int burst_iat_threshold_us;   // Burst IAT 阈值 (微秒)

    // 活跃/空闲时间阈值
    unsigned int active_iat_threshold_us;  // 活跃时间 IAT 阈值 (微秒)

    // 时间窗口大小
    unsigned int window_size_ms;           // 窗口大小 (毫秒)
    unsigned int instant_bitrate_window_ms; // 瞬时比特率窗口 (毫秒)

    // 行为特征阈值
    unsigned int interactive_threshold_ms;  // 交互式会话响应延迟阈值
    unsigned int bulk_transfer_kbps;        // 大流量传输阈值 (KB/s)
    unsigned int short_conn_threshold_s;    // 短连接阈值 (秒)

    // Kafka 输出配置
    unsigned int output_to_log;
    unsigned int send_kafka_flag;
    char kafka_brokers[MAX_PATH_LEN];
    char topic_name[MAX_DOMAIN_LEN];
    KafkaProducer* kafka_producer;

    // FEATURE 桥接配置
    short feature_bridge_flag;
    int feature_bridge_id;
    char feature_bridge_name[MAX_DOMAIN_LEN];


    // SNI 桥接配置
    short sni_bridge_flag;
    int sni_bridge_id;
    char sni_bridge_name[MAX_DOMAIN_LEN];

    // 输出原始序列标志
    unsigned int output_raw_seq;

    // 特征模块开关 (0=关闭, 1=开启, 默认全部开启)
    unsigned int enable_basic;         // 基础计数+包长
    unsigned int enable_iat;           // 到达间隔
    unsigned int enable_burst;         // 突发检测
    unsigned int enable_protocol;      // 协议头
    unsigned int enable_payload;       // 载荷分析
    unsigned int enable_sequence;      // 序列特征
    unsigned int enable_fft;           // 频域特征
    unsigned int enable_window;        // 时间窗
    unsigned int enable_behavior;      // 行为特征

    // 每包阶段子开关
    unsigned int enable_payload_stats; // 每包熵/压缩率计算 (关闭可大幅提升性能)

} ti_feature_config_t;

// 全局配置实例
extern ti_feature_config_t g_feat_config;

// 配置读取函数
int feat_config_read(const char* filename);

// 配置初始化
int feat_config_init(void);

// 配置销毁
void feat_config_destroy(void);

// 获取配置实例
ti_feature_config_t* feat_config_get(void);

#endif /* FEATURE_CONFIG_H_ */
