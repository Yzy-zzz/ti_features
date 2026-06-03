#include "calc_window.h"
#include "feature_config.h"
#include "utils/stats_utils.h"
#include <math.h>

// 更新窗口统计
void calc_window_update(flow_feature_state_t* state,
                        unsigned int pkt_len,
                        unsigned long long ts_us)
{
    if (state->window_size_ms == 0) return;

    // 计算当前窗口索引
    unsigned long long window_size_us = (unsigned long long)state->window_size_ms * 1000ULL;

    if (state->window_start_us == 0) {
        // 初始化第一个窗口
        state->window_start_us = ts_us;
        state->current_window = 0;
    }

    // 计算当前应该在哪个窗口
    unsigned long long elapsed = ts_us - state->window_start_us;
    unsigned int target_window = (unsigned int)(elapsed / window_size_us);

    // 如果跨越了多个窗口，需要填充中间的窗口
    while (state->current_window <= target_window && state->current_window < state->num_windows) {
        // 计算当前窗口的比特率 (bps)
        if (state->current_window < state->num_windows) {
            unsigned long long window_duration_us = window_size_us;
            unsigned int window_bytes = state->window_byte_counts[state->current_window];

            // 比特率 = 字节数 * 8 / 时间 (秒)
            if (window_duration_us > 0) {
                state->window_bitrate[state->current_window] =
                    (unsigned int)((unsigned long long)window_bytes * 8 * 1000000ULL / window_duration_us);
            }
        }
        state->current_window++;
    }

    // 更新当前窗口的计数
    if (target_window < state->num_windows) {
        state->window_pkt_counts[target_window]++;
        state->window_byte_counts[target_window] += pkt_len;
    }
}

// 计算窗口统计特征
void calc_derived_window(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();
    unsigned int num_windows = state->num_windows;

    // 只统计有数据的窗口
    unsigned int valid_windows = 0;
    for (unsigned int i = 0; i < num_windows; i++) {
        if (state->window_pkt_counts[i] > 0) {
            valid_windows++;
        }
    }

    if (valid_windows == 0) {
        return;
    }

    // === enable_window_pkt: 窗口包数统计 (9 维) ===
    if (!cfg->enable_window_pkt && !cfg->enable_window_byte) return;

    if (cfg->enable_window_pkt) {
        double sum = 0, sum_sq = 0;
        unsigned int min_val = state->window_pkt_counts[0];
        unsigned int max_val = 0;

        // 复制到数组用于排序
        unsigned int* pkt_counts = (unsigned int*)malloc(valid_windows * sizeof(unsigned int));
        if (!pkt_counts) return;

        int idx = 0;
        for (unsigned int i = 0; i < num_windows; i++) {
            if (state->window_pkt_counts[i] > 0) {
                pkt_counts[idx++] = state->window_pkt_counts[i];
                sum += state->window_pkt_counts[i];
                sum_sq += state->window_pkt_counts[i] * state->window_pkt_counts[i];
                if (state->window_pkt_counts[i] < min_val) min_val = state->window_pkt_counts[i];
                if (state->window_pkt_counts[i] > max_val) max_val = state->window_pkt_counts[i];
            }
        }

        double mean = sum / valid_windows;
        double std = (valid_windows > 1) ?
            sqrt((sum_sq - valid_windows * mean * mean) / (valid_windows - 1)) : 0;

        cJSON_AddNumberToObject(output, "window_packet_count_mean", mean);
        cJSON_AddNumberToObject(output, "window_packet_count_std", std);
        cJSON_AddNumberToObject(output, "window_packet_count_min", min_val);
        cJSON_AddNumberToObject(output, "window_packet_count_max", max_val);

        // 分位数
        unsigned int* sorted = copy_and_sort_uint(pkt_counts, valid_windows);
        if (sorted) {
            cJSON_AddNumberToObject(output, "window_packet_count_median",
                calc_median_uint(sorted, valid_windows));
            cJSON_AddNumberToObject(output, "window_packet_count_q1",
                calc_q1_uint(sorted, valid_windows));
            cJSON_AddNumberToObject(output, "window_packet_count_q3",
                calc_q3_uint(sorted, valid_windows));

            // 偏度和峰度
            double* double_arr = (double*)malloc(valid_windows * sizeof(double));
            if (double_arr) {
                for (unsigned int i = 0; i < valid_windows; i++) {
                    double_arr[i] = (double)pkt_counts[i];
                }
                cJSON_AddNumberToObject(output, "window_packet_count_skew",
                    calc_skewness(double_arr, valid_windows, mean, std));
                cJSON_AddNumberToObject(output, "window_packet_count_kurt",
                    calc_kurtosis(double_arr, valid_windows, mean, std));
                free(double_arr);
            }
            free(sorted);
        }
        free(pkt_counts);
    }

    // === enable_window_byte: 窗口字节数统计 (9 维) ===
    if (cfg->enable_window_byte) {
        double sum = 0, sum_sq = 0;
        unsigned int min_val = state->window_byte_counts[0];
        unsigned int max_val = 0;

        // 复制到数组用于排序
        unsigned int* byte_counts = (unsigned int*)malloc(valid_windows * sizeof(unsigned int));
        if (!byte_counts) return;

        int idx = 0;
        for (unsigned int i = 0; i < num_windows; i++) {
            if (state->window_byte_counts[i] > 0) {
                byte_counts[idx++] = state->window_byte_counts[i];
                sum += state->window_byte_counts[i];
                sum_sq += state->window_byte_counts[i] * state->window_byte_counts[i];
                if (state->window_byte_counts[i] < min_val) min_val = state->window_byte_counts[i];
                if (state->window_byte_counts[i] > max_val) max_val = state->window_byte_counts[i];
            }
        }

        double mean = sum / valid_windows;
        double std = (valid_windows > 1) ?
            sqrt((sum_sq - valid_windows * mean * mean) / (valid_windows - 1)) : 0;

        cJSON_AddNumberToObject(output, "window_byte_count_mean", mean);
        cJSON_AddNumberToObject(output, "window_byte_count_std", std);
        cJSON_AddNumberToObject(output, "window_byte_count_min", min_val);
        cJSON_AddNumberToObject(output, "window_byte_count_max", max_val);

        // 分位数
        unsigned int* sorted = copy_and_sort_uint(byte_counts, valid_windows);
        if (sorted) {
            cJSON_AddNumberToObject(output, "window_byte_count_median",
                calc_median_uint(sorted, valid_windows));
            cJSON_AddNumberToObject(output, "window_byte_count_q1",
                calc_q1_uint(sorted, valid_windows));
            cJSON_AddNumberToObject(output, "window_byte_count_q3",
                calc_q3_uint(sorted, valid_windows));

            // 偏度和峰度
            double* double_arr = (double*)malloc(valid_windows * sizeof(double));
            if (double_arr) {
                for (unsigned int i = 0; i < valid_windows; i++) {
                    double_arr[i] = (double)byte_counts[i];
                }
                cJSON_AddNumberToObject(output, "window_byte_count_skew",
                    calc_skewness(double_arr, valid_windows, mean, std));
                cJSON_AddNumberToObject(output, "window_byte_count_kurt",
                    calc_kurtosis(double_arr, valid_windows, mean, std));
                free(double_arr);
            }
            free(sorted);
        }
        free(byte_counts);
    }

    // // === 窗口比特率统计 ===
    // {
    //     double sum = 0, sum_sq = 0;
    //     unsigned int min_val = 0;
    //     unsigned int max_val = 0;
    //     int valid_bitrate_windows = 0;

    //     // 先找到有效窗口数
    //     for (unsigned int i = 0; i < num_windows; i++) {
    //         if (state->window_pkt_counts[i] > 0) {
    //             if (valid_bitrate_windows == 0) {
    //                 min_val = state->window_bitrate[i];
    //                 max_val = state->window_bitrate[i];
    //             } else {
    //                 if (state->window_bitrate[i] < min_val) min_val = state->window_bitrate[i];
    //                 if (state->window_bitrate[i] > max_val) max_val = state->window_bitrate[i];
    //             }
    //             sum += state->window_bitrate[i];
    //             sum_sq += state->window_bitrate[i] * state->window_bitrate[i];
    //             valid_bitrate_windows++;
    //         }
    //     }

    //     if (valid_bitrate_windows > 0) {
    //         double mean = sum / valid_bitrate_windows;
    //         double std = (valid_bitrate_windows > 1) ?
    //             sqrt((sum_sq - valid_bitrate_windows * mean * mean) / (valid_bitrate_windows - 1)) : 0;

    //         cJSON_AddNumberToObject(output, "window_bitrate_mean", mean);
    //         cJSON_AddNumberToObject(output, "window_bitrate_std", std);
    //         cJSON_AddNumberToObject(output, "window_bitrate_min", min_val);
    //         cJSON_AddNumberToObject(output, "window_bitrate_max", max_val);
    //     }
    // }
}
