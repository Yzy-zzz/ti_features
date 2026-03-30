#ifndef FEAT_STATE_H_
#define FEAT_STATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ti_features.h"
#include "feature_stats.h"
#include "utils/circular_buffer.h"
#include "utils/fft.h"
#include "utils/memory_pool.h"

// ============================================================================
// 每流状态结构 - 存储所有特征计算所需的状态
// ============================================================================

typedef struct _flow_feature_state
{
    // === 基础信息 ===
    unsigned long long flow_start_us;      // 流开始时间（微秒）
    unsigned long long last_pkt_us;        // 最后一个包时间
    unsigned long long last_fwd_us;        // 最后前向包时间
    unsigned long long last_bwd_us;        // 最后后向包时间

    // === 基础计数器 ===
    unsigned long total_packets;
    unsigned long fwd_packets;
    unsigned long bwd_packets;
    unsigned long total_bytes;
    unsigned long fwd_bytes;
    unsigned long bwd_bytes;
    unsigned long fwd_payload_bytes;
    unsigned long bwd_payload_bytes;
    unsigned long tcp_packets;
    unsigned long udp_packets;
    unsigned long icmp_packets;
    unsigned long l2_packets;

    // === TCP 标志统计 ===
    unsigned int tcp_syn_count;
    unsigned int tcp_fin_count;
    unsigned int tcp_rst_count;
    unsigned int tcp_psh_count;
    unsigned int tcp_ack_count;
    unsigned int tcp_urg_count;
    unsigned int tcp_syn_only_count;      // 仅 SYN
    unsigned int tcp_syn_ack_count;       // SYN+ACK
    unsigned int tcp_ack_only_count;      // 仅 ACK（无 payload）
    unsigned int tcp_payload_packets;     // 有 payload 的 TCP 包
    unsigned int tcp_window_update_count;
    unsigned int tcp_zero_window_count;
    unsigned int tcp_retransmission_count;
    unsigned int has_last_tcp_window;
    unsigned int last_tcp_window;
    unsigned long long last_rst_ts;
    unsigned long long rst_interval_sum_us;
    unsigned int rst_interval_count;
    unsigned int has_last_fwd_seq;
    unsigned int has_last_bwd_seq;
    unsigned int last_fwd_seq;
    unsigned int last_bwd_seq;

    // === 运行统计（在线计算） ===
    running_stats_t pkt_len_stats;           // 包长统计
    directional_running_stats_t fwd_pkt_len; // 前向包长
    directional_running_stats_t bwd_pkt_len; // 后向包长
    running_stats_t iat_stats;               // IAT 统计
    directional_running_stats_t fwd_iat;     // 前向 IAT
    directional_running_stats_t bwd_iat;     // 后向 IAT
    running_stats_t ip_ttl_stats;            // IP TTL 统计
    running_stats_t ip_tos_stats;            // IP ToS 统计
    running_stats_t tcp_window_stats;        // TCP 窗口统计
    running_stats_t udp_len_stats;           // UDP 长度统计

    // === 方向过滤的包长数组（用于计算分位数） ===
    unsigned int fwd_pkt_len_count;
    unsigned int bwd_pkt_len_count;
    unsigned int fwd_pkt_len_capacity;
    unsigned int bwd_pkt_len_capacity;
    unsigned int* fwd_pkt_lens;
    unsigned int* bwd_pkt_lens;

    // === 方向过滤的 IAT 数组（用于计算分位数） ===
    unsigned int fwd_iat_count;
    unsigned int bwd_iat_count;
    unsigned int fwd_iat_capacity;
    unsigned int bwd_iat_capacity;
    unsigned long long* fwd_iats;
    unsigned long long* bwd_iats;

    // === Payload 统计 ===
    running_stats_t payload_size_stats;
    running_stats_t fwd_payload_stats;
    running_stats_t bwd_payload_stats;
    running_stats_t payload_byte_stats;
    unsigned int payload_byte_hist[16];      // 字节值直方图（16 个 bin）
    unsigned int payload_byte_hist_full[256];
    unsigned int payload_total_bytes;
    unsigned int payload_non_empty_count;
    unsigned int payload_printable_count;
    unsigned int payload_alnum_count;
    unsigned char payload_first_bytes[256];
    unsigned int payload_first_bytes_count;

    unsigned int fwd_payload_count;
    unsigned int bwd_payload_count;
    unsigned int fwd_payload_capacity;
    unsigned int bwd_payload_capacity;
    unsigned int* fwd_payload_sizes;
    unsigned int* bwd_payload_sizes;

    // === Payload 大小 mod 统计 ===
    running_stats_t payload_mod[8];          // mod 2,4,8,16,32,64,128,256

    // === 端口统计 ===
    unsigned short src_port;
    unsigned short dst_port;
    unsigned int unique_src_ports;           // 需要使用集合或 Bloom filter
    unsigned int unique_dst_ports;
    unsigned int well_known_src_count;       // 知名端口计数
    unsigned int well_known_dst_count;

    // === IP 特征 ===
    unsigned int src_ip;
    unsigned int dst_ip;
    int src_ip_is_private;   
    int dst_ip_is_private;
    int ip_frag_count;

    // === 时间窗口统计 ===
    unsigned int window_pkt_counts_offset;   // 各窗口包数（内存池中的偏移）
    unsigned int window_byte_counts_offset;  // 各窗口字节数
    unsigned int window_bitrate_offset;      // 各窗口比特率
    unsigned int* window_pkt_counts;
    unsigned int* window_byte_counts;
    unsigned int* window_bitrate;
    unsigned int window_size_ms;
    unsigned int num_windows;
    unsigned int current_window;
    unsigned long long window_start_us;

    // === Burst 检测状态 ===
    unsigned long long burst_start_us;
    unsigned int burst_byte_count;
    unsigned int burst_pkt_count;
    unsigned int burst_count;
    unsigned int burst_sizes_offset;
    unsigned long long burst_durations_offset;
    unsigned long long burst_intervals_offset;
    unsigned int burst_packet_counts_offset;
    unsigned int* burst_sizes;
    unsigned long long* burst_durations;
    unsigned long long* burst_intervals;
    unsigned int* burst_packet_counts;

    // === 响应延迟状态 ===
    unsigned long long last_req_ts;          // 最后请求时间
    unsigned long long last_resp_ts;         // 最后响应时间
    unsigned int resp_delays_offset;         // 延迟采样数组（内存池偏移）
    unsigned int resp_delay_count;
    unsigned int resp_delay_capacity;
    double* resp_delays;

    // === 序列数据（环形 buffer，数据内嵌在内存池中） ===
    circular_buffer_t pkt_len_seq;           // 包长序列
    circular_buffer_t iat_seq;               // IAT 序列
    circular_buffer_t dir_seq;               // 方向序列
    circular_buffer_t ts_seq;                // 时间戳序列
    circular_buffer_t tcp_window_seq;        // TCP 窗口序列
    circular_buffer_t udp_len_seq;           // UDP 长度序列
    circular_buffer_t ip_ttl_seq;            // IP TTL 序列
    circular_buffer_t ip_tos_seq;            // IP ToS 序列
    circular_buffer_t payload_size_seq;      // Payload 大小序列
    circular_buffer_t payload_size_diff_seq; // Payload 大小差分序列
    circular_buffer_t l3_len_seq;            // L3 长度序列（字节）
    circular_buffer_t l4_len_seq;            // L4 长度序列（字节）
    circular_buffer_t payload_len_seq;       // Payload 长度序列（字节）

    // === 内存池管理（统一管理所有动态数组） ===
    flow_mem_pool_t* mem_pool;               // 内存池指针

    // === 临时计算 buffer 指针（特征计算时复用） ===
    double* temp_double_buffer;              // 临时 double 数组
    unsigned int temp_uint_buffer_size;      // 临时 uint 缓冲区大小
    double* temp_double_buffer_2;            // 第二个临时 double 数组

    // === Payload 细粒度统计状态 ===
    running_stats_t payload_entropy_stats;
    running_stats_t payload_markov_entropy_stats;
    running_stats_t payload_compression_ratio_stats;
    running_stats_t payload_autocorr_lag1_stats;
    running_stats_t payload_size_diff_stats;
    unsigned int last_payload_len;
    int has_last_payload_len;
    int payload_looks_like_http;
    int payload_looks_like_json;
    int payload_looks_like_xml;
    int payload_looks_like_tls;
    int payload_looks_like_ssh;

    // === 序列衍生长度 ===
    unsigned int last_dl_chunk_count;
    unsigned int last_uplink_rate_seq_len;
    unsigned int last_downlink_rate_seq_len;

    // === Bigram 统计 ===
    bigram_stats_t bigram_stats;
    pkt_size_class last_pkt_size_class;

    // === IAT 相关状态 ===
    unsigned long long last_arrival_us;      // 上一个包到达时间
    unsigned long long active_time_us;       // 活跃时间累加
    unsigned long long idle_time_us;         // 空闲时间累加

    // === 第一个包信息 ===
    int first_10_count;                      // 前 10 包计数
    unsigned int first_10_lens[10];          // 前 10 包长度

    // === 协议魔数检测 ===
    int payload_magic_HTTP_GET;
    int payload_magic_HTTP_POST;
    int payload_magic_TLS_1_0;
    int payload_magic_TLS_1_1;
    int payload_magic_TLS_1_2;
    int payload_magic_TLS_1_3;
    int payload_magic_SSH;
    int payload_magic_JPEG;
    int payload_magic_PNG;
    int payload_magic_GIF;

    // === 输出特征（流结束时填充） ===
    cJSON* output_json;

    // === 已发送快速突发标志 ===
    int send_fast_burst_flag;

} flow_feature_state_t;

// ============================================================================
// 函数声明
// ============================================================================

// 初始化流状态
int flow_state_init(flow_feature_state_t* state);

// 释放流状态
void flow_state_destroy(flow_feature_state_t* state);

// 重置流状态
void flow_state_reset(flow_feature_state_t* state);

// 更新流状态（每个包调用）
int flow_state_update(flow_feature_state_t* state,
                      struct streaminfo* a_stream,
                      void* a_packet,
                      int direction,
                      unsigned int pkt_len,
                      unsigned long long ts_us);

// 计算所有特征并输出
cJSON* flow_state_compute_features(flow_feature_state_t* state,
                                    struct streaminfo* a_stream,
                                    int thread_seq,
                                    stream_type a_stream_type);

#endif /* FEAT_STATE_H_ */
