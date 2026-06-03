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

    // 特征模块总开关 (0=关闭, 1=开启, 默认全部开启)
    unsigned int enable_basic;               // 基础模块
    unsigned int enable_iat;                 // IAT 模块
    // enable_burst 已在下方定义
    unsigned int enable_protocol;            // 协议模块
    unsigned int enable_payload;             // Payload 模块
    unsigned int enable_sequence;            // 序列模块
    unsigned int enable_fft;                 // FFT 频域模块
    unsigned int enable_window;              // 窗口模块
    unsigned int enable_behavior;            // 行为模块

    // 特征模块子开关 (0=关闭, 1=开启, 默认全部开启)

    // 基础模块子开关
    unsigned int enable_basic_ratios;        // 基础比率 (fwd/bwd 比、平均包长)
    unsigned int enable_basic_payload_dir;   // 前/后向 payload 统计
    unsigned int enable_basic_first_n;       // 前 N 包长度统计
    unsigned int enable_basic_pkt_length;    // 包长统计 (方向包长、分位数)

    // IAT 模块子开关
    unsigned int enable_iat_stats;           // IAT 基本统计 + 熵/自相关 + 序列统计
    unsigned int enable_iat_fwd_bwd;         // 前向/后向 IAT 统计
    unsigned int enable_iat_active;          // 活跃/空闲时间
    unsigned int enable_iat_response;        // 响应延迟统计

    // Burst 模块 (保持不变)
    unsigned int enable_burst;               // 突发检测

    // 协议模块子开关
    unsigned int enable_protocol_tcp_flags;  // TCP 标志统计
    unsigned int enable_protocol_tcp_window; // TCP 窗口统计
    unsigned int enable_protocol_ip;         // IP TTL/ToS 统计
    unsigned int enable_protocol_udp;        // UDP 长度统计
    unsigned int enable_protocol_port;       // 端口/IP 特征 + 历史兼容

    // Payload 模块子开关
    unsigned int enable_payload_size;        // 载荷大小统计
    unsigned int enable_payload_content;     // 内容特征 (可打印比、字母数字比)
    unsigned int enable_payload_hist;        // 字节直方图 + 字节统计
    unsigned int enable_payload_magic;       // 魔数/协议检测
    unsigned int enable_payload_stats;       // 每包高级统计 (熵/压缩率)

    // 序列模块子开关
    unsigned int enable_sequence_stats;      // 序列统计特征 (自相关、游程、熵等)
    unsigned int enable_raw_sequences;       // 原始序列输出 (length/iat/dir/composite)
    unsigned int enable_chunk_sequences;     // chunk 序列输出
    unsigned int enable_rate_sequences;      // 速率序列输出

    // FFT 模块子开关
    unsigned int enable_fft_global;          // 全局 FFT 特征
    unsigned int enable_fft_fwd;             // 前向 FFT 特征
    unsigned int enable_fft_bwd;             // 后向 FFT 特征

    // Window 模块子开关
    unsigned int enable_window_pkt;          // 窗口包数统计
    unsigned int enable_window_byte;         // 窗口字节数统计

    // Behavior 模块子开关
    unsigned int enable_behavior_basic;      // 基础行为 (持续时间、比特率)
    unsigned int enable_behavior_pattern;    // 流模式 (短连接、不对称比)
    unsigned int enable_behavior_bitrate;    // 瞬时比特率统计

    // === 复合依赖标志（自动计算，非用户配置）===
    // 以下标志由多个子开关聚合而成，在 feat_config_read() 末尾自动计算。
    // 用于 per-packet 阶段跳过不需要的数据采集，节省 CPU 开销。
    unsigned int need_pkt_len_seq;           // 需要 pkt_len_seq: fft_global | sequence_stats | raw_seq | chunk_seq | rate_seq
    unsigned int need_dir_seq;               // 需要 dir_seq: sequence_stats | raw_seq | chunk_seq | rate_seq
    unsigned int need_ts_seq;                // 需要 ts_seq: chunk_seq | rate_seq
    unsigned int need_l3_l4_payload_seq;     // 需要 l3/l4/payload_len_seq: raw_sequences
    unsigned int need_fwd_bwd_pkt_lens;      // 需要 fwd_pkt_lens/bwd_pkt_lens: basic_pkt_length | fft_fwd | fft_bwd
    unsigned int need_first_n;               // 需要 first_n_lens 采集: enable_basic && enable_basic_first_n
    unsigned int need_payload_dir;           // 需要 payload dir 采集: enable_basic && enable_basic_payload_dir
    unsigned int need_fwd_bwd_iats;          // 需要 fwd/bwd IAT 数组: enable_iat && enable_iat_fwd_bwd
    unsigned int need_window_update;         // 需要 window_update: window_pkt | window_byte | behavior_pattern | behavior_bitrate
    unsigned int need_iat_seq;               // 需要 iat_seq: iat_stats | sequence_stats | raw_sequences

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
