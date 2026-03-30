#include "calc_burst.h"
#include "feature_config.h"

// 更新 Burst 状态
void calc_burst_update(flow_feature_state_t* state,
                       unsigned int pkt_len,
                       unsigned long long iat_us,
                       unsigned long long ts_us)
{
    ti_feature_config_t* cfg = feat_config_get();

    if (iat_us <= cfg->burst_iat_threshold_us) {
        // 当前 burst 继续
        state->burst_byte_count += pkt_len;
        state->burst_pkt_count++;
    } else {
        // burst 结束
        if (state->burst_pkt_count > 1) {
            unsigned int capacity = state->mem_pool ? state->mem_pool->actual_seq_len : cfg->max_seq_len;
            if (state->burst_count < capacity) {
                state->burst_sizes[state->burst_count] = state->burst_byte_count;
                state->burst_durations[state->burst_count] = ts_us - state->burst_start_us;
                state->burst_packet_counts[state->burst_count] = state->burst_pkt_count;
                if (state->burst_count > 0) {
                    state->burst_intervals[state->burst_count - 1] = iat_us;
                }
                state->burst_count++;
            }
        }
        // 新 burst 开始
        state->burst_start_us = ts_us;
        state->burst_byte_count = pkt_len;
        state->burst_pkt_count = 1;
    }
}

// 计算 Burst 统计特征
void calc_burst_statistics(flow_feature_state_t* state, cJSON* output)
{
    cJSON_AddNumberToObject(output, "burst_count", state->burst_count);

    if (state->burst_count == 0) {
        cJSON_AddNumberToObject(output, "avg_burst_duration", 0);
        cJSON_AddNumberToObject(output, "avg_burst_size", 0);
        cJSON_AddNumberToObject(output, "burst_size_std", 0);
        cJSON_AddNumberToObject(output, "burst_interval_mean", 0);
        cJSON_AddNumberToObject(output, "burst_packet_count_mean", 0);
        cJSON_AddNumberToObject(output, "burst_packet_rate_peak", 0);
        cJSON_AddNumberToObject(output, "burstiness_index", 0);
        return;
    }

    unsigned int valid_bursts = state->burst_count;

    // 平均 burst 大小与标准差
    double total_size = 0;
    double total_size_sq = 0;
    double total_duration_us = 0;
    double total_pkt_count = 0;
    double peak_pkt_rate = 0;
    for (unsigned int i = 0; i < valid_bursts; i++) {
        total_size += state->burst_sizes[i];
        total_size_sq += (double)state->burst_sizes[i] * state->burst_sizes[i];
        total_duration_us += state->burst_durations[i];
        total_pkt_count += state->burst_packet_counts[i];

        if (state->burst_durations[i] > 0) {
            double pkt_rate = (double)state->burst_packet_counts[i] * 1000000.0 / state->burst_durations[i];
            if (pkt_rate > peak_pkt_rate) {
                peak_pkt_rate = pkt_rate;
            }
        }
    }
    double avg_size = total_size / valid_bursts;
    double avg_duration_s = total_duration_us / valid_bursts / 1000000.0;
    double avg_pkt_count = total_pkt_count / valid_bursts;

    cJSON_AddNumberToObject(output, "avg_burst_size", avg_size);
    cJSON_AddNumberToObject(output, "avg_burst_duration", avg_duration_s);
    cJSON_AddNumberToObject(output, "burst_packet_count_mean", avg_pkt_count);
    cJSON_AddNumberToObject(output, "burst_packet_rate_peak", peak_pkt_rate);

    double size_std = 0;
    if (valid_bursts > 1) {
        size_std = sqrt((total_size_sq - valid_bursts * avg_size * avg_size) / (valid_bursts - 1));
    }
    cJSON_AddNumberToObject(output, "burst_size_std", size_std);

    // 平均 burst 间隔
    if (valid_bursts > 1) {
        double total_interval = 0;
        for (unsigned int i = 0; i < valid_bursts - 1; i++) {
            total_interval += state->burst_intervals[i];
        }
        cJSON_AddNumberToObject(output, "burst_interval_mean",
            (double)total_interval / (valid_bursts - 1) / 1000000.0);  // 转秒
    } else {
        cJSON_AddNumberToObject(output, "burst_interval_mean", 0);
    }

    // Burstiness 指数
    cJSON_AddNumberToObject(output, "burstiness_index", calc_burstiness_index(state));
}

// 计算 Burstiness 指数
double calc_burstiness_index(flow_feature_state_t* state)
{
    if (state->burst_count < 2) {
        return 0;
    }

    // 计算 burst 间隔的 CV
    double sum = 0, sum_sq = 0;
    for (unsigned int i = 0; i < state->burst_count - 1; i++) {
        double interval = (double)state->burst_intervals[i];
        sum += interval;
        sum_sq += interval * interval;
    }

    double n = state->burst_count - 1;
    double mean = sum / n;
    double variance = (sum_sq - n * mean * mean) / (n - 1);
    double std = sqrt(variance);

    return (mean > 0) ? (std / mean) : 0;
}
