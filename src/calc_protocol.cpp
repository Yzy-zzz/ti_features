#include "calc_protocol.h"
#include "feature_config.h"
#include "utils/stats_utils.h"

// 判断是否为私有 IP
int is_private_ip(unsigned int ip)
{
    unsigned char* p = (unsigned char*)&ip;
    // 10.0.0.0/8
    if (p[0] == 10) return 1;
    // 172.16.0.0/12
    if (p[0] == 172 && (p[1] >= 16 && p[1] <= 31)) return 1;
    // 192.168.0.0/16
    if (p[0] == 192 && p[1] == 168) return 1;
    // 127.0.0.0/8
    if (p[0] == 127) return 1;
    return 0;
}

// 判断是否为知名端口
int is_well_known_port(unsigned short port)
{
    return (port < 1024) ? 1 : 0;
}

// 根据端口推断协议
const char* port_to_protocol(unsigned short port)
{
    switch (port) {
        case 20: return "FTP-DATA";
        case 21: return "FTP";
        case 22: return "SSH";
        case 23: return "TELNET";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 465: return "SMTPS";
        case 587: return "SUBMISSION";
        case 993: return "IMAPS";
        case 995: return "POP3S";
        case 3306: return "MYSQL";
        case 3389: return "RDP";
        case 5432: return "POSTGRESQL";
        case 6379: return "REDIS";
        case 8080: return "HTTP-ALT";
        case 8443: return "HTTPS-ALT";
        default: return "UNKNOWN";
    }
}

// 更新 TCP 标志统计
void calc_protocol_tcp_update(flow_feature_state_t* state,
                               struct tcphdr* tcph,
                               unsigned int payload_len,
                               unsigned int window,
                               int direction,
                               unsigned int tcp_seq,
                               unsigned long long ts_us)
{
    state->tcp_packets++;

    // 统计标志位
    if (tcph->syn) {
        state->tcp_syn_count++;
        if (!tcph->ack) {
            state->tcp_syn_only_count++;
        }
    }
    if (tcph->fin) {
        state->tcp_fin_count++;
    }
    if (tcph->rst) {
        state->tcp_rst_count++;
        if (state->last_rst_ts > 0 && ts_us > state->last_rst_ts) {
            state->rst_interval_sum_us += (ts_us - state->last_rst_ts);
            state->rst_interval_count++;
        }
        state->last_rst_ts = ts_us;
    }
    if (tcph->psh) {
        state->tcp_psh_count++;
    }
    if (tcph->ack) {
        state->tcp_ack_count++;
        // 纯 ACK 包（有 ACK 标志但无 payload 且无其他标志）
        if (!tcph->syn && !tcph->fin && !tcph->rst && !tcph->psh && payload_len == 0) {
            state->tcp_ack_only_count++;
        }
    }
    if (tcph->urg) {
        state->tcp_urg_count++;
    }

    // SYN-ACK 包
    if (tcph->syn && tcph->ack) {
        state->tcp_syn_ack_count++;
    }

    // 有 payload 的包
    if (payload_len > 0) {
        state->tcp_payload_packets++;
    }

    // TCP 窗口统计
    running_stats_update(&state->tcp_window_stats, (double)window);
    circular_buffer_push(&state->tcp_window_seq, &window);

    if (state->has_last_tcp_window) {
        if (tcph->ack && payload_len == 0 && window != state->last_tcp_window) {
            state->tcp_window_update_count++;
        }
    }
    state->last_tcp_window = window;
    state->has_last_tcp_window = 1;

    // 重传检测（简化：同方向TCP负载包seq回退或重复）
    if (payload_len > 0) {
        if (direction == DIR_FWD) {
            if (state->has_last_fwd_seq && tcp_seq <= state->last_fwd_seq) {
                state->tcp_retransmission_count++;
            }
            state->last_fwd_seq = tcp_seq;
            state->has_last_fwd_seq = 1;
        } else if (direction == DIR_BWD) {
            if (state->has_last_bwd_seq && tcp_seq <= state->last_bwd_seq) {
                state->tcp_retransmission_count++;
            }
            state->last_bwd_seq = tcp_seq;
            state->has_last_bwd_seq = 1;
        }
    }

    // 零窗口检测
    if (window == 0) {
        state->tcp_zero_window_count++;
    }
}

// 更新 IP 头部统计
void calc_protocol_ip_update(flow_feature_state_t* state,
                              struct iphdr* iph)
{
    unsigned int ttl = (unsigned int)iph->ttl;
    unsigned int tos = (unsigned int)iph->tos;

    // TTL 统计
    running_stats_update(&state->ip_ttl_stats, (double)ttl);
    circular_buffer_push(&state->ip_ttl_seq, &ttl);

    // ToS 统计
    running_stats_update(&state->ip_tos_stats, (double)tos);
    circular_buffer_push(&state->ip_tos_seq, &tos);

    // 源 IP
    state->src_ip = iph->saddr;
    state->src_ip_is_private = is_private_ip(iph->saddr);

    // 目标 IP
    state->dst_ip = iph->daddr;
    state->dst_ip_is_private = is_private_ip(iph->daddr);

    // 分片检测
    if (iph->frag_off & htons(0x1FFF)) {
        state->ip_frag_count++;
    }
}

// 更新端口统计
void calc_protocol_port_update(flow_feature_state_t* state,
                                unsigned short src_port,
                                unsigned short dst_port)
{
    state->src_port = ntohs(src_port);
    state->dst_port = ntohs(dst_port);
    state->unique_src_ports = (state->src_port > 0) ? 1 : 0;
    state->unique_dst_ports = (state->dst_port > 0) ? 1 : 0;

    if (is_well_known_port(state->src_port)) {
        state->well_known_src_count++;
    }
    if (is_well_known_port(state->dst_port)) {
        state->well_known_dst_count++;
    }
}

// 计算协议派生特征
void calc_derived_protocol(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    // === enable_protocol_tcp_flags: TCP 标志统计 (20 维) ===
    if (cfg->enable_protocol_tcp_flags) {
        cJSON_AddNumberToObject(output, "tcp_psh_count", state->tcp_psh_count);
        cJSON_AddNumberToObject(output, "tcp_urg_count", state->tcp_urg_count);
        cJSON_AddNumberToObject(output, "window_update_frequency", state->tcp_window_update_count);
        cJSON_AddNumberToObject(output, "tcp_syn_packets", state->tcp_syn_count);
        cJSON_AddNumberToObject(output, "tcp_syn_ack_packets", state->tcp_syn_ack_count);
        cJSON_AddNumberToObject(output, "tcp_ack_only_count", state->tcp_ack_only_count);
        cJSON_AddNumberToObject(output, "tcp_payload_packets", state->tcp_payload_packets);
        cJSON_AddNumberToObject(output, "tcp_zero_window_count", state->tcp_zero_window_count);
        cJSON_AddNumberToObject(output, "icmp_packet_count", state->icmp_packets);
        cJSON_AddNumberToObject(output, "l2_packets_count", (state->l2_packets > 0) ? state->l2_packets : state->total_packets);
        cJSON_AddNumberToObject(output, "header_anomaly_count", 0);
        cJSON_AddNumberToObject(output, "tcp_header_anomaly_count", 0);
        cJSON_AddNumberToObject(output, "header_anomaly_ratio", 0);

        // TCP 连接成功率
        if (state->tcp_syn_count > 0) {
            cJSON_AddNumberToObject(output, "tcp_connection_success_rate",
                (double)state->tcp_syn_ack_count / state->tcp_syn_count);
        }

        // TCP 标志比例
        if (state->tcp_packets > 0) {
            cJSON_AddNumberToObject(output, "tcp_syn_ratio",
                (double)state->tcp_syn_count / state->tcp_packets);
            cJSON_AddNumberToObject(output, "tcp_rst_ratio",
                (double)state->tcp_rst_count / state->tcp_packets);
            cJSON_AddNumberToObject(output, "tcp_ack_only_ratio",
                (double)state->tcp_ack_only_count / state->tcp_packets);
            cJSON_AddNumberToObject(output, "tcp_payload_packet_ratio",
                (double)state->tcp_payload_packets / state->tcp_packets);
            cJSON_AddNumberToObject(output, "tcp_syn_only_ratio",
                (double)state->tcp_syn_only_count / state->tcp_packets);
        }

        cJSON_AddNumberToObject(output, "tcp_half_open_ratio",
            (state->tcp_syn_count > 0) ?
            (double)(state->tcp_syn_count - state->tcp_syn_ack_count) / state->tcp_syn_count : 0);
    }

    // === enable_protocol_tcp_window: TCP 窗口统计 (9 维) ===
    if (cfg->enable_protocol_tcp_window) {
        cJSON_AddNumberToObject(output, "tcp_window_mean", state->tcp_window_stats.mean);
        cJSON_AddNumberToObject(output, "tcp_window_std", running_stats_std(&state->tcp_window_stats));
        cJSON_AddNumberToObject(output, "tcp_window_min", state->tcp_window_stats.min_val);
        cJSON_AddNumberToObject(output, "tcp_window_max", state->tcp_window_stats.max_val);
        cJSON_AddNumberToObject(output, "tcp_window_skew", running_stats_skew(&state->tcp_window_stats));
        cJSON_AddNumberToObject(output, "tcp_window_kurt", running_stats_kurt(&state->tcp_window_stats));

        unsigned int tcp_window_count = 0;
        unsigned int* tcp_windows = (unsigned int*)circular_buffer_get_array(&state->tcp_window_seq, &tcp_window_count);
        if (tcp_windows && tcp_window_count > 0) {
            unsigned int* sorted = copy_and_sort_uint(tcp_windows, tcp_window_count);
            if (sorted) {
                cJSON_AddNumberToObject(output, "tcp_window_median", calc_median_uint(sorted, tcp_window_count));
                cJSON_AddNumberToObject(output, "tcp_window_q1", calc_q1_uint(sorted, tcp_window_count));
                cJSON_AddNumberToObject(output, "tcp_window_q3", calc_q3_uint(sorted, tcp_window_count));
                free(sorted);
            }
            free(tcp_windows);
        }
    }

    // === enable_protocol_ip: IP TTL/ToS 统计 (22 维) ===
    if (cfg->enable_protocol_ip) {
        cJSON_AddNumberToObject(output, "ip_ttl_mean", state->ip_ttl_stats.mean);
        cJSON_AddNumberToObject(output, "ip_ttl_std", running_stats_std(&state->ip_ttl_stats));
        cJSON_AddNumberToObject(output, "ip_ttl_min", state->ip_ttl_stats.min_val);
        cJSON_AddNumberToObject(output, "ip_ttl_max", state->ip_ttl_stats.max_val);
        cJSON_AddNumberToObject(output, "ip_ttl_skew", running_stats_skew(&state->ip_ttl_stats));
        cJSON_AddNumberToObject(output, "ip_ttl_kurt", running_stats_kurt(&state->ip_ttl_stats));

        cJSON_AddNumberToObject(output, "ip_tos_mean", state->ip_tos_stats.mean);
        cJSON_AddNumberToObject(output, "ip_tos_std", running_stats_std(&state->ip_tos_stats));
        cJSON_AddNumberToObject(output, "ip_tos_min", state->ip_tos_stats.min_val);
        cJSON_AddNumberToObject(output, "ip_tos_max", state->ip_tos_stats.max_val);
        cJSON_AddNumberToObject(output, "ip_tos_skew", running_stats_skew(&state->ip_tos_stats));
        cJSON_AddNumberToObject(output, "ip_tos_kurt", running_stats_kurt(&state->ip_tos_stats));

        unsigned int ttl_count = 0;
        unsigned int* ttl_values = (unsigned int*)circular_buffer_get_array(&state->ip_ttl_seq, &ttl_count);
        if (ttl_values && ttl_count > 0) {
            unsigned int* sorted = copy_and_sort_uint(ttl_values, ttl_count);
            if (sorted) {
                cJSON_AddNumberToObject(output, "ip_ttl_median", calc_median_uint(sorted, ttl_count));
                cJSON_AddNumberToObject(output, "ip_ttl_q1", calc_q1_uint(sorted, ttl_count));
                cJSON_AddNumberToObject(output, "ip_ttl_q3", calc_q3_uint(sorted, ttl_count));
                free(sorted);
            }
            unsigned int ttl_eq_32 = 0, ttl_eq_64 = 0, ttl_eq_128 = 0, ttl_eq_255 = 0;
            for (unsigned int i = 0; i < ttl_count; i++) {
                if (ttl_values[i] == 32) ttl_eq_32++;
                else if (ttl_values[i] == 64) ttl_eq_64++;
                else if (ttl_values[i] == 128) ttl_eq_128++;
                else if (ttl_values[i] == 255) ttl_eq_255++;
            }
            cJSON_AddNumberToObject(output, "ip_ttl_eq_32_count", ttl_eq_32);
            cJSON_AddNumberToObject(output, "ip_ttl_eq_64_count", ttl_eq_64);
            cJSON_AddNumberToObject(output, "ip_ttl_eq_128_count", ttl_eq_128);
            cJSON_AddNumberToObject(output, "ip_ttl_eq_255_count", ttl_eq_255);
            free(ttl_values);
        }

        unsigned int tos_count = 0;
        unsigned int* tos_values = (unsigned int*)circular_buffer_get_array(&state->ip_tos_seq, &tos_count);
        if (tos_values && tos_count > 0) {
            unsigned int* sorted = copy_and_sort_uint(tos_values, tos_count);
            if (sorted) {
                cJSON_AddNumberToObject(output, "ip_tos_median", calc_median_uint(sorted, tos_count));
                cJSON_AddNumberToObject(output, "ip_tos_q1", calc_q1_uint(sorted, tos_count));
                cJSON_AddNumberToObject(output, "ip_tos_q3", calc_q3_uint(sorted, tos_count));
                free(sorted);
            }
            free(tos_values);
        }
    }

    // === enable_protocol_udp: UDP 长度统计 (9 维) ===
    if (cfg->enable_protocol_udp) {
        cJSON_AddNumberToObject(output, "udp_length_mean", state->udp_len_stats.mean);
        cJSON_AddNumberToObject(output, "udp_length_std", running_stats_std(&state->udp_len_stats));
        cJSON_AddNumberToObject(output, "udp_length_min", state->udp_len_stats.min_val);
        cJSON_AddNumberToObject(output, "udp_length_max", state->udp_len_stats.max_val);
        cJSON_AddNumberToObject(output, "udp_length_skew", running_stats_skew(&state->udp_len_stats));
        cJSON_AddNumberToObject(output, "udp_length_kurt", running_stats_kurt(&state->udp_len_stats));

        unsigned int udp_len_count = 0;
        unsigned int* udp_lens = (unsigned int*)circular_buffer_get_array(&state->udp_len_seq, &udp_len_count);
        if (udp_lens && udp_len_count > 0) {
            unsigned int* sorted = copy_and_sort_uint(udp_lens, udp_len_count);
            if (sorted) {
                cJSON_AddNumberToObject(output, "udp_length_median", calc_median_uint(sorted, udp_len_count));
                cJSON_AddNumberToObject(output, "udp_length_q1", calc_q1_uint(sorted, udp_len_count));
                cJSON_AddNumberToObject(output, "udp_length_q3", calc_q3_uint(sorted, udp_len_count));
                free(sorted);
            }
            free(udp_lens);
        }
    }

    // === enable_protocol_port: 端口/IP 特征 + 历史兼容 (25 维) ===
    if (cfg->enable_protocol_port) {
        // 重传率
        cJSON_AddNumberToObject(output, "avg_retransmission_rate",
            (state->tcp_payload_packets > 0) ?
            (double)state->tcp_retransmission_count / state->tcp_payload_packets : 0);
        cJSON_AddNumberToObject(output, "rst_trigger_interval",
            (state->rst_interval_count > 0) ?
            (double)state->rst_interval_sum_us / state->rst_interval_count / 1000000.0 : 0);

        // IP 特征
        cJSON_AddNumberToObject(output, "src_ip_is_private", state->src_ip_is_private);
        cJSON_AddNumberToObject(output, "dst_ip_is_private", state->dst_ip_is_private);
        cJSON_AddNumberToObject(output, "ip_fragmentation_flag",
            (state->ip_frag_count > 0) ? 1 : 0);

        // 端口特征
        cJSON_AddNumberToObject(output, "src_port_is_ephemeral",
            (state->src_port >= 1024 && state->src_port <= 65535) ? 1 : 0);
        cJSON_AddNumberToObject(output, "dst_port_is_ephemeral",
            (state->dst_port >= 1024 && state->dst_port <= 65535) ? 1 : 0);
        cJSON_AddNumberToObject(output, "well_known_dst_port_ratio",
            (state->total_packets > 0) ?
            (double)state->well_known_dst_count / state->total_packets : 0);
        cJSON_AddNumberToObject(output, "well_known_src_port_ratio",
            (state->total_packets > 0) ?
            (double)state->well_known_src_count / state->total_packets : 0);
        cJSON_AddNumberToObject(output, "unique_src_ports", state->unique_src_ports);
        cJSON_AddNumberToObject(output, "unique_dst_ports", state->unique_dst_ports);
        cJSON_AddNumberToObject(output, "is_common_service",
            (state->dst_port < 1024 || state->src_port < 1024) ? 1 : 0);
        cJSON_AddNumberToObject(output, "quic_usage_flag", (state->dst_port == 443) ? 1 : 0);
        cJSON_AddStringToObject(output, "src_port_protocol", port_to_protocol(state->src_port));

        const char* src_range = (state->src_port < 1024) ? "well_known" :
            ((state->src_port < 49152) ? "registered" : "dynamic");
        cJSON_AddStringToObject(output, "src_port_range", src_range);

        // 协议推断
        cJSON_AddStringToObject(output, "dst_port_protocol",
            port_to_protocol(state->dst_port));

        // 历史兼容字段（缺少底层语义时用近似或占位）
        cJSON_AddNumberToObject(output, "tcpackfaultcnt", 0);
        cJSON_AddNumberToObject(output, "tcpbflgtmx", 0);
        cJSON_AddNumberToObject(output, "tcpflwlssackrcvdbytes", 0);
        cJSON_AddNumberToObject(output, "tcpiseqn", state->last_fwd_seq);
        cJSON_AddNumberToObject(output, "tcpinitwinsz", (state->tcp_window_stats.count > 0) ? state->tcp_window_stats.mean : 0);
        cJSON_AddNumberToObject(output, "tcppackcnt", state->tcp_ack_count);
        cJSON_AddNumberToObject(output, "tcppseqcnt", state->tcp_packets);
        cJSON_AddNumberToObject(output, "tcpseqfaultcnt", state->tcp_retransmission_count);
        cJSON_AddNumberToObject(output, "tcpseqsntbytes", state->fwd_payload_bytes + state->bwd_payload_bytes);
    }
}
