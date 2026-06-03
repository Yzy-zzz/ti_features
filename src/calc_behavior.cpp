#include "calc_behavior.h"
#include "feature_config.h"
#include "utils/stats_utils.h"

// 判断是否为交互式会话
int is_interactive_session(double avg_response_delay_ms)
{
    ti_feature_config_t* cfg = feat_config_get();
    return (avg_response_delay_ms < cfg->interactive_threshold_ms) ? 1 : 0;
}

// 判断是否为大流量传输
int is_bulk_transfer(double throughput_kbps)
{
    ti_feature_config_t* cfg = feat_config_get();
    return (throughput_kbps > cfg->bulk_transfer_kbps) ? 1 : 0;
}

// 判断是否为短连接
int is_short_connection(double duration_s)
{
    ti_feature_config_t* cfg = feat_config_get();
    return (duration_s < cfg->short_conn_threshold_s) ? 1 : 0;
}

// 计算流不对称比
double calc_flow_asymmetry_ratio(unsigned long fwd_bytes,
                                  unsigned long bwd_bytes)
{
    if (bwd_bytes == 0) {
        return (fwd_bytes == 0) ? 1 : INFINITY;
    }
    return (double)fwd_bytes / bwd_bytes;
}

// 获取流模式字符串
const char* get_flow_pattern(double asymmetry_ratio)
{
    if (asymmetry_ratio < 0.5) {
        return "download_heavy";
    } else if (asymmetry_ratio > 2.0) {
        return "upload_heavy";
    } else if (asymmetry_ratio >= 0.8 && asymmetry_ratio <= 1.25) {
        return "balanced";
    } else {
        return "asymmetric";
    }
}

// 检测周期性（基于 FFT 主频占比）
int has_strong_periodicity(double dominant_freq_ratio)
{
    // 阈值：主频能量占比 > 0.5 认为有强周期性
    return (dominant_freq_ratio > 0.5) ? 1 : 0;
}

// 判断是否有规律间隔
int has_regular_intervals(double cv)
{
    // 变异系数 < 0.5 认为有规律
    return (cv < 0.5) ? 1 : 0;
}

// 计算行为派生特征
void calc_derived_behavior(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    // === enable_behavior_basic: 基础行为 (6 维) ===
    if (cfg->enable_behavior_basic) {
        double duration_s = (double)(state->last_pkt_us - state->flow_start_us) / 1000000.0;
        cJSON_AddNumberToObject(output, "flow_duration", duration_s);

        if (duration_s > 0) {
            double avg_bitrate = (double)state->total_bytes * 8 / duration_s;
            cJSON_AddNumberToObject(output, "avg_bitrate", avg_bitrate);
            cJSON_AddNumberToObject(output, "avg_packet_rate",
                (double)state->total_packets / duration_s);
            cJSON_AddNumberToObject(output, "avg_throughput",
                (double)state->total_bytes / duration_s / 1024);

            cJSON_AddNumberToObject(output, "is_bulk_transfer",
                is_bulk_transfer((double)state->total_bytes / duration_s / 1024));

            cJSON_AddNumberToObject(output, "short_connection_ratio",
                is_short_connection(duration_s) ? 1 : 0);
        }
    }

    // === enable_behavior_pattern: 流模式 (8 维) ===
    if (cfg->enable_behavior_pattern) {
        double asymmetry = calc_flow_asymmetry_ratio(state->fwd_bytes, state->bwd_bytes);
        cJSON_AddNumberToObject(output, "flow_asymmetry_ratio", asymmetry);
        cJSON_AddStringToObject(output, "flow_pattern", get_flow_pattern(asymmetry));

        double iat_cv = running_stats_cv(&state->iat_stats);
        cJSON_AddNumberToObject(output, "has_regular_intervals",
            has_regular_intervals(iat_cv) ? 1 : 0);

        if (state->total_packets > 0) {
            cJSON_AddNumberToObject(output, "protocol_tcp_ratio",
                (double)state->tcp_packets / state->total_packets);
            cJSON_AddNumberToObject(output, "protocol_udp_ratio",
                (double)state->udp_packets / state->total_packets);
            cJSON_AddNumberToObject(output, "protocol_icmp_ratio",
                (double)state->icmp_packets / state->total_packets);
            cJSON_AddNumberToObject(output, "protocol_other_ratio",
                (double)(state->total_packets - state->tcp_packets - state->udp_packets - state->icmp_packets) /
                state->total_packets);
        }

        // peak_bitrate 也属于 pattern 模块
        unsigned int valid = 0;
        for (unsigned int i = 0; i < state->num_windows; i++) {
            if (state->window_pkt_counts[i] > 0) valid++;
        }
        if (valid > 0) {
            double max_v = 0;
            for (unsigned int i = 0; i < state->num_windows; i++) {
                if (state->window_pkt_counts[i] > 0) {
                    double v = (double)state->window_bitrate[i];
                    if (v > max_v) max_v = v;
                }
            }
            cJSON_AddNumberToObject(output, "peak_bitrate", max_v);
        }
    }

    // === enable_behavior_bitrate: 瞬时比特率统计 (15 维) ===
    if (cfg->enable_behavior_bitrate) {
        unsigned int valid = 0;
        for (unsigned int i = 0; i < state->num_windows; i++) {
            if (state->window_pkt_counts[i] > 0) {
                valid++;
            }
        }

        if (valid > 0) {
            double* bitrate_vals = (double*)malloc(valid * sizeof(double));
            if (bitrate_vals) {
                double sum = 0, sum_sq = 0;
                double min_v = 0, max_v = 0;
                unsigned int idx = 0;
                for (unsigned int i = 0; i < state->num_windows; i++) {
                    if (state->window_pkt_counts[i] > 0) {
                        double v = (double)state->window_bitrate[i];
                        bitrate_vals[idx++] = v;
                        sum += v;
                        sum_sq += v * v;
                        if (idx == 1) {
                            min_v = max_v = v;
                        } else {
                            if (v < min_v) min_v = v;
                            if (v > max_v) max_v = v;
                        }
                    }
                }

                double mean = sum / valid;
                double std = (valid > 1) ? sqrt((sum_sq - valid * mean * mean) / (valid - 1)) : 0;
                double* sorted = copy_and_sort_double(bitrate_vals, valid);

                cJSON_AddNumberToObject(output, "instant_bitrate_mean", mean);
                cJSON_AddNumberToObject(output, "instant_bitrate_std", std);
                cJSON_AddNumberToObject(output, "instant_bitrate_min", min_v);
                cJSON_AddNumberToObject(output, "instant_bitrate_max", max_v);

                if (sorted) {
                    cJSON_AddNumberToObject(output, "instant_bitrate_median", calc_median_double(sorted, valid));
                    cJSON_AddNumberToObject(output, "instant_bitrate_q1", calc_q1_double(sorted, valid));
                    cJSON_AddNumberToObject(output, "instant_bitrate_q3", calc_q3_double(sorted, valid));
                    cJSON_AddNumberToObject(output, "instant_bitrate_skew",
                        calc_skewness(bitrate_vals, valid, mean, std));
                    cJSON_AddNumberToObject(output, "instant_bitrate_kurt",
                        calc_kurtosis(bitrate_vals, valid, mean, std));

                    cJSON_AddNumberToObject(output, "bitrate_percentile_25",
                        calc_percentile_double(sorted, valid, 25.0));
                    cJSON_AddNumberToObject(output, "bitrate_percentile_50",
                        calc_percentile_double(sorted, valid, 50.0));
                    cJSON_AddNumberToObject(output, "bitrate_percentile_75",
                        calc_percentile_double(sorted, valid, 75.0));
                    cJSON_AddNumberToObject(output, "bitrate_percentile_90",
                        calc_percentile_double(sorted, valid, 90.0));
                    free(sorted);
                }

                free(bitrate_vals);
            }
        }

        // 流量熵时间序列
        {
            double total_bytes_windows = 0;
            unsigned int valid_windows = 0;
            for (unsigned int i = 0; i < state->num_windows; i++) {
                if (state->window_byte_counts[i] > 0) {
                    total_bytes_windows += state->window_byte_counts[i];
                    valid_windows++;
                }
            }

            if (valid_windows > 0 && total_bytes_windows > 0) {
                double ent_sum = 0;
                double ent_peak = 0;
                for (unsigned int i = 0; i < state->num_windows; i++) {
                    if (state->window_byte_counts[i] > 0) {
                        double p = state->window_byte_counts[i] / total_bytes_windows;
                        double e = -p * log2(p);
                        ent_sum += e;
                        if (e > ent_peak) ent_peak = e;
                    }
                }
                cJSON_AddNumberToObject(output, "traffic_entropy_ts_mean", ent_sum / valid_windows);
                cJSON_AddNumberToObject(output, "traffic_entropy_ts_peak", ent_peak);
            } else {
                cJSON_AddNumberToObject(output, "traffic_entropy_ts_mean", 0);
                cJSON_AddNumberToObject(output, "traffic_entropy_ts_peak", 0);
            }
        }
    }
}
