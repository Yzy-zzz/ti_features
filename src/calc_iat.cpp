#include "calc_iat.h"
#include "feature_config.h"
#include "calc_behavior.h"
#include "utils/stats_utils.h"

// static double calc_iat_autocorr(unsigned long long* seq, unsigned int n, int lag)
// {
//     if (!seq || n <= (unsigned int)lag || lag <= 0) return 0;

//     double mean = 0;
//     for (unsigned int i = 0; i < n; i++) {
//         mean += (double)seq[i];
//     }
//     mean /= n;

//     double num = 0, den = 0;
//     for (unsigned int i = 0; i < n - (unsigned int)lag; i++) {
//         double d1 = (double)seq[i] - mean;
//         double d2 = (double)seq[i + lag] - mean;
//         num += d1 * d2;
//         den += d1 * d1;
//     }
//     return (den > 0) ? (num / den) : 0;
// }

// static double calc_iat_entropy(unsigned long long* seq, unsigned int n)
// {
//     if (!seq || n == 0) return 0;

//     const int bin_count = 32;
//     unsigned int bins[bin_count];
//     memset(bins, 0, sizeof(bins));

//     unsigned long long min_v = seq[0], max_v = seq[0];
//     for (unsigned int i = 1; i < n; i++) {
//         if (seq[i] < min_v) min_v = seq[i];
//         if (seq[i] > max_v) max_v = seq[i];
//     }

//     if (min_v == max_v) return 0;

//     double span = (double)(max_v - min_v);
//     for (unsigned int i = 0; i < n; i++) {
//         int idx = (int)(((double)(seq[i] - min_v) / span) * (bin_count - 1));
//         if (idx < 0) idx = 0;
//         if (idx >= bin_count) idx = bin_count - 1;
//         bins[idx]++;
//     }

//     return calc_shannon_entropy(bins, bin_count);
// }

// double 版本的 IAT 熵（避免 ULL/double 类型别名问题）
static double calc_iat_entropy_double(double* seq, unsigned int n)
{
    if (!seq || n == 0) return 0;

    const int bin_count = 32;
    unsigned int bins[bin_count];
    memset(bins, 0, sizeof(bins));

    double min_v = seq[0], max_v = seq[0];
    for (unsigned int i = 1; i < n; i++) {
        if (seq[i] < min_v) min_v = seq[i];
        if (seq[i] > max_v) max_v = seq[i];
    }

    if (min_v == max_v) return 0;

    double span = max_v - min_v;
    for (unsigned int i = 0; i < n; i++) {
        int idx = (int)(((seq[i] - min_v) / span) * (bin_count - 1));
        if (idx < 0) idx = 0;
        if (idx >= bin_count) idx = bin_count - 1;
        bins[idx]++;
    }

    return calc_shannon_entropy(bins, bin_count);
}

// double 版本的 IAT 自相关
static double calc_iat_autocorr_double(double* seq, unsigned int n, int lag)
{
    if (!seq || n <= (unsigned int)lag || lag <= 0) return 0;

    double mean = 0;
    for (unsigned int i = 0; i < n; i++) {
        mean += seq[i];
    }
    mean /= n;

    double num = 0, den = 0;
    for (unsigned int i = 0; i < n - (unsigned int)lag; i++) {
        double d1 = seq[i] - mean;
        double d2 = seq[i + lag] - mean;
        num += d1 * d2;
        den += d1 * d1;
    }
    return (den > 0) ? (num / den) : 0;
}

// 更新 IAT 统计
void calc_iat_update(flow_feature_state_t* state,
                     unsigned long long ts_us,
                     int direction)
{
    if (state->last_arrival_us == 0) {
        // 第一个包
        state->last_arrival_us = ts_us;
        return;
    }

    ti_feature_config_t* cfg = feat_config_get();
    unsigned long long iat = ts_us - state->last_arrival_us;

    // 全局 IAT 统计（被 iat_stats 使用）
    if (cfg->enable_iat_stats) {
        running_stats_update(&state->iat_stats, (double)iat);
    }

    // 前/后向 IAT 统计 + 数组记录（被 iat_fwd_bwd 使用）
    if (cfg->need_fwd_bwd_iats) {
        if (direction == DIR_FWD) {
            if (state->last_fwd_us > 0) {
                unsigned long long fwd_iat = ts_us - state->last_fwd_us;
                running_stats_update(&state->fwd_iat.fwd, (double)fwd_iat);
                if (state->fwd_iat_count < state->fwd_iat_capacity) {
                    state->fwd_iats[state->fwd_iat_count++] = fwd_iat;
                }
            }
        } else if (direction == DIR_BWD) {
            if (state->last_bwd_us > 0) {
                unsigned long long bwd_iat = ts_us - state->last_bwd_us;
                running_stats_update(&state->bwd_iat.bwd, (double)bwd_iat);
                if (state->bwd_iat_count < state->bwd_iat_capacity) {
                    state->bwd_iats[state->bwd_iat_count++] = bwd_iat;
                }
            }
        }
    }

    // IAT 序列 circular buffer（被 iat_stats + sequence 模块使用）
    if (cfg->need_iat_seq) {
        circular_buffer_push(&state->iat_seq, &iat);
    }

    // 活跃/空闲时间（被 iat_active 使用）
    if (cfg->enable_iat_active) {
        calc_active_idle_time(state, iat);
    }

    // 始终更新时间戳（burst_iat 在调用前已读取 last_arrival_us，必须保持更新）
    if (direction == DIR_FWD) {
        state->last_fwd_us = ts_us;
    } else if (direction == DIR_BWD) {
        state->last_bwd_us = ts_us;
    }
    state->last_arrival_us = ts_us;
}

// 更新活跃/空闲时间
void calc_active_idle_time(flow_feature_state_t* state,
                            unsigned long long iat_us)
{
    ti_feature_config_t* cfg = feat_config_get();

    if (iat_us <= cfg->active_iat_threshold_us) {
        state->active_time_us += iat_us;
    } else {
        state->idle_time_us += iat_us;
    }
}

// 更新响应延迟（请求 - 响应配对）
void calc_response_delay(flow_feature_state_t* state,
                         unsigned long long ts_us,
                         int direction)
{
    if (direction == DIR_FWD) {
        // 记录请求时间
        state->last_req_ts = ts_us;
    } else if (direction == DIR_BWD && state->last_req_ts > 0) {
        // 计算响应延迟
        unsigned long long delay_us = ts_us - state->last_req_ts;

        if (state->resp_delay_count < state->resp_delay_capacity) {
            state->resp_delays[state->resp_delay_count++] = (double)delay_us / 1000.0;  // 转为 ms
        }
        state->last_req_ts = 0;  // 重置
    }
}

// 计算 IAT 派生特征 - 优化版本：使用临时 buffer 复用
void calc_derived_iat(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    // 【优化】使用预分配的临时 buffer
    double* temp_buf = state->temp_double_buffer;
    double* temp_buf2 = state->temp_double_buffer_2;
    unsigned int max_len = state->temp_uint_buffer_size;

    // === enable_iat_stats: IAT 基本统计 + 熵/自相关 + 序列统计 + log IAT (35 维) ===
    if (cfg->enable_iat_stats) {
        // 从 circular buffer 直接读取，避免 malloc
        unsigned int iat_count = circular_buffer_count(&state->iat_seq);

        // 只有需要时才获取数据
        if (iat_count > 0 && iat_count <= max_len) {
            // 复制到临时 buffer 并转换为 double
            for (unsigned int i = 0; i < iat_count; i++) {
                unsigned long long* val = (unsigned long long*)circular_buffer_get(&state->iat_seq, i);
                if (val) {
                    temp_buf[i] = (double)*val;
                    temp_buf2[i] = log((double)*val + 1.0);  // log IAT 预备
                }
            }
            // 准备排序用的副本
            memcpy(temp_buf + max_len, temp_buf, iat_count * sizeof(double));  // 后半部用于排序
            qsort(temp_buf + max_len, iat_count, sizeof(double), compare_double);
        }

        // IAT 基本统计
        cJSON_AddNumberToObject(output, "iat_mean", state->iat_stats.mean);
        cJSON_AddNumberToObject(output, "iat_std", running_stats_std(&state->iat_stats));
        cJSON_AddNumberToObject(output, "iat_min", state->iat_stats.min_val);
        cJSON_AddNumberToObject(output, "iat_max", state->iat_stats.max_val);
        cJSON_AddNumberToObject(output, "iat_skew", running_stats_skew(&state->iat_stats));
        cJSON_AddNumberToObject(output, "iat_kurt", running_stats_kurt(&state->iat_stats));

        // IAT 变异系数
        double iat_cv = running_stats_cv(&state->iat_stats);
        cJSON_AddNumberToObject(output, "interval_coefficient_of_variation", iat_cv);

        // 熵和自相关（使用临时 buffer）
        if (iat_count > 0 && iat_count <= max_len) {
            // 重新填充为 double 数组（保持 double 类型，避免 ULL/double 类型别名）
            for (unsigned int i = 0; i < iat_count; i++) {
                unsigned long long* val = (unsigned long long*)circular_buffer_get(&state->iat_seq, i);
                if (val) temp_buf[i] = (double)*val;
            }

            cJSON_AddNumberToObject(output, "iat_entropy", calc_iat_entropy_double(temp_buf, iat_count));
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_1", calc_iat_autocorr_double(temp_buf, iat_count, 1));
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_2", calc_iat_autocorr_double(temp_buf, iat_count, 2));
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_3", calc_iat_autocorr_double(temp_buf, iat_count, 3));
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_4", calc_iat_autocorr_double(temp_buf, iat_count, 4));
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_5", calc_iat_autocorr_double(temp_buf, iat_count, 5));

            // 分位数（使用已排序的 temp_buf + max_len）
            double* sorted = temp_buf + max_len;
            cJSON_AddNumberToObject(output, "iat_median", calc_median_double(sorted, iat_count));
            cJSON_AddNumberToObject(output, "iat_q1", calc_q1_double(sorted, iat_count));
            cJSON_AddNumberToObject(output, "iat_q3", calc_q3_double(sorted, iat_count));

            // 间隔比率
            if (iat_count > 1) {
                double ratio_sum = 0;
                unsigned int ratio_n = 0;
                for (unsigned int i = 1; i < iat_count; i++) {
                    if (temp_buf[i - 1] > 0) {
                        ratio_sum += temp_buf[i] / temp_buf[i - 1];
                        ratio_n++;
                    }
                }
                cJSON_AddNumberToObject(output, "interval_ratio_mean", (ratio_n > 0) ? (ratio_sum / ratio_n) : 0);
            } else {
                cJSON_AddNumberToObject(output, "interval_ratio_mean", 0);
            }

            // 计算 IAT 序列统计
            double sum = 0, sum_sq = 0;
            for (unsigned int i = 0; i < iat_count; i++) {
                sum += temp_buf[i];
                sum_sq += temp_buf[i] * temp_buf[i];
            }
            double mean = sum / iat_count;
            double std = (iat_count > 1) ? sqrt((sum_sq - iat_count * mean * mean) / (iat_count - 1)) : 0;

            cJSON_AddNumberToObject(output, "iat_sequence_mean", mean);
            cJSON_AddNumberToObject(output, "iat_sequence_std", std);
            cJSON_AddNumberToObject(output, "iat_sequence_min", sorted[0]);
            cJSON_AddNumberToObject(output, "iat_sequence_max", sorted[iat_count - 1]);
            cJSON_AddNumberToObject(output, "iat_sequence_median", calc_median_double(sorted, iat_count));
            cJSON_AddNumberToObject(output, "iat_sequence_q1", calc_q1_double(sorted, iat_count));
            cJSON_AddNumberToObject(output, "iat_sequence_q3", calc_q3_double(sorted, iat_count));
            cJSON_AddNumberToObject(output, "iat_sequence_skew", calc_skewness(temp_buf, iat_count, mean, std));
            cJSON_AddNumberToObject(output, "iat_sequence_kurt", calc_kurtosis(temp_buf, iat_count, mean, std));

            // log IAT 统计
            double lsum = 0, lsum_sq = 0;
            for (unsigned int i = 0; i < iat_count; i++) {
                lsum += temp_buf2[i];
                lsum_sq += temp_buf2[i] * temp_buf2[i];
            }
            double lmean = lsum / iat_count;
            double lstd = (iat_count > 1) ? sqrt((lsum_sq - iat_count * lmean * lmean) / (iat_count - 1)) : 0;

            // 排序 log IAT
            memcpy(temp_buf + max_len, temp_buf2, iat_count * sizeof(double));
            qsort(temp_buf + max_len, iat_count, sizeof(double), compare_double);
            double* log_sorted = temp_buf + max_len;

            cJSON_AddNumberToObject(output, "log_iat_sequence_mean", lmean);
            cJSON_AddNumberToObject(output, "log_iat_sequence_std", lstd);
            cJSON_AddNumberToObject(output, "log_iat_sequence_min", log_sorted[0]);
            cJSON_AddNumberToObject(output, "log_iat_sequence_max", log_sorted[iat_count - 1]);
            cJSON_AddNumberToObject(output, "log_iat_sequence_median", calc_median_double(log_sorted, iat_count));
            cJSON_AddNumberToObject(output, "log_iat_sequence_q1", calc_q1_double(log_sorted, iat_count));
            cJSON_AddNumberToObject(output, "log_iat_sequence_q3", calc_q3_double(log_sorted, iat_count));
            cJSON_AddNumberToObject(output, "log_iat_sequence_skew", calc_skewness(temp_buf2, iat_count, lmean, lstd));
            cJSON_AddNumberToObject(output, "log_iat_sequence_kurt", calc_kurtosis(temp_buf2, iat_count, lmean, lstd));
        } else {
            // 数据量超过 buffer 容量或为空，输出 0
            cJSON_AddNumberToObject(output, "iat_entropy", 0);
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_1", 0);
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_2", 0);
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_3", 0);
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_4", 0);
            cJSON_AddNumberToObject(output, "iat_autocorr_lag_5", 0);
            cJSON_AddNumberToObject(output, "iat_median", 0);
            cJSON_AddNumberToObject(output, "iat_q1", 0);
            cJSON_AddNumberToObject(output, "iat_q3", 0);
            cJSON_AddNumberToObject(output, "interval_ratio_mean", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_mean", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_std", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_min", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_max", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_median", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_q1", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_q3", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_skew", 0);
            cJSON_AddNumberToObject(output, "iat_sequence_kurt", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_mean", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_std", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_min", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_max", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_median", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_q1", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_q3", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_skew", 0);
            cJSON_AddNumberToObject(output, "log_iat_sequence_kurt", 0);
        }
    }

    // === enable_iat_fwd_bwd: 前向/后向 IAT 统计 (20 维) ===
    if (cfg->enable_iat_fwd_bwd) {
        // === 前向 IAT 详细统计 - 使用临时 buffer ===
        cJSON_AddNumberToObject(output, "fwd_iat_mean", state->fwd_iat.fwd.mean);
        cJSON_AddNumberToObject(output, "fwd_iat_std", running_stats_std(&state->fwd_iat.fwd));
        cJSON_AddNumberToObject(output, "fwd_iat_cv", running_stats_cv(&state->fwd_iat.fwd));
        cJSON_AddNumberToObject(output, "fwd_iat_min", state->fwd_iat.fwd.min_val);
        cJSON_AddNumberToObject(output, "fwd_iat_max", state->fwd_iat.fwd.max_val);
        cJSON_AddNumberToObject(output, "fwd_iat_skew", running_stats_skew(&state->fwd_iat.fwd));
        cJSON_AddNumberToObject(output, "fwd_iat_kurt", running_stats_kurt(&state->fwd_iat.fwd));

        if (state->fwd_iat_count > 0 && state->fwd_iat_count <= max_len) {
            // 复制到临时 buffer
            double* fwd_double = temp_buf;
            for (unsigned int i = 0; i < state->fwd_iat_count; i++) {
                fwd_double[i] = (double)state->fwd_iats[i];
            }

            // 排序用于分位数计算
            double* fwd_sorted = copy_and_sort_double(fwd_double, state->fwd_iat_count);
            if (fwd_sorted) {
                cJSON_AddNumberToObject(output, "fwd_iat_median",
                    calc_median_double(fwd_sorted, state->fwd_iat_count));
                cJSON_AddNumberToObject(output, "fwd_iat_q1",
                    calc_q1_double(fwd_sorted, state->fwd_iat_count));
                cJSON_AddNumberToObject(output, "fwd_iat_q3",
                    calc_q3_double(fwd_sorted, state->fwd_iat_count));
                free(fwd_sorted);
            }
        }

        // === 后向 IAT 详细统计 - 使用临时 buffer ===
        cJSON_AddNumberToObject(output, "bwd_iat_mean", state->bwd_iat.bwd.mean);
        cJSON_AddNumberToObject(output, "bwd_iat_std", running_stats_std(&state->bwd_iat.bwd));
        cJSON_AddNumberToObject(output, "bwd_iat_cv", running_stats_cv(&state->bwd_iat.bwd));
        cJSON_AddNumberToObject(output, "bwd_iat_min", state->bwd_iat.bwd.min_val);
        cJSON_AddNumberToObject(output, "bwd_iat_max", state->bwd_iat.bwd.max_val);
        cJSON_AddNumberToObject(output, "bwd_iat_skew", running_stats_skew(&state->bwd_iat.bwd));
        cJSON_AddNumberToObject(output, "bwd_iat_kurt", running_stats_kurt(&state->bwd_iat.bwd));

        if (state->bwd_iat_count > 0 && state->bwd_iat_count <= max_len) {
            // 复制到临时 buffer
            double* bwd_double = temp_buf;
            for (unsigned int i = 0; i < state->bwd_iat_count; i++) {
                bwd_double[i] = (double)state->bwd_iats[i];
            }

            // 排序用于分位数计算
            double* bwd_sorted = copy_and_sort_double(bwd_double, state->bwd_iat_count);
            if (bwd_sorted) {
                cJSON_AddNumberToObject(output, "bwd_iat_median",
                    calc_median_double(bwd_sorted, state->bwd_iat_count));
                cJSON_AddNumberToObject(output, "bwd_iat_q1",
                    calc_q1_double(bwd_sorted, state->bwd_iat_count));
                cJSON_AddNumberToObject(output, "bwd_iat_q3",
                    calc_q3_double(bwd_sorted, state->bwd_iat_count));
                free(bwd_sorted);
            }
        }
    }

    // === enable_iat_active: 活跃/空闲时间 (3 维) ===
    if (cfg->enable_iat_active) {
        unsigned long long total_time = state->active_time_us + state->idle_time_us;
        cJSON_AddNumberToObject(output, "active_time", (double)state->active_time_us / 1000000.0);
        cJSON_AddNumberToObject(output, "idle_time", (double)state->idle_time_us / 1000000.0);
        if (total_time > 0) {
            cJSON_AddNumberToObject(output, "active_time_ratio",
                (double)state->active_time_us / total_time);
        }
    }

    // === enable_iat_response: 响应延迟统计 (11 维) ===
    if (cfg->enable_iat_response) {
        if (state->resp_delay_count > 0 && state->resp_delay_count <= max_len) {
            // 统一使用 state 中维护的响应延迟数组指针
            double* resp_delays = state->resp_delays;

            if (resp_delays) {
                double* resp_buf = temp_buf;
                memcpy(resp_buf, resp_delays, state->resp_delay_count * sizeof(double));

                double sum = 0, sum_sq = 0, min_val = resp_buf[0], max_val = 0;
                for (unsigned int i = 0; i < state->resp_delay_count; i++) {
                    sum += resp_buf[i];
                    sum_sq += resp_buf[i] * resp_buf[i];
                    if (resp_buf[i] < min_val) min_val = resp_buf[i];
                    if (resp_buf[i] > max_val) max_val = resp_buf[i];
                }
                double mean = sum / state->resp_delay_count;
                cJSON_AddNumberToObject(output, "response_delay_mean", mean);
                cJSON_AddNumberToObject(output, "response_delay_min", min_val);
                cJSON_AddNumberToObject(output, "response_delay_max", max_val);

                if (state->resp_delay_count > 1) {
                    double std = sqrt((sum_sq - state->resp_delay_count * mean * mean) / (state->resp_delay_count - 1));
                    cJSON_AddNumberToObject(output, "response_delay_std", std);

                    // 排序
                    double* resp_sorted = copy_and_sort_double(resp_buf, state->resp_delay_count);
                    if (resp_sorted) {
                        cJSON_AddNumberToObject(output, "response_delay_median",
                            calc_median_double(resp_sorted, state->resp_delay_count));
                        cJSON_AddNumberToObject(output, "response_delay_q1",
                            calc_q1_double(resp_sorted, state->resp_delay_count));
                        cJSON_AddNumberToObject(output, "response_delay_q3",
                            calc_q3_double(resp_sorted, state->resp_delay_count));
                        free(resp_sorted);
                    }

                    cJSON_AddNumberToObject(output, "response_delay_skew",
                        calc_skewness(resp_buf, state->resp_delay_count, mean, std));
                    cJSON_AddNumberToObject(output, "response_delay_kurt",
                        calc_kurtosis(resp_buf, state->resp_delay_count, mean, std));
                } else {
                    cJSON_AddNumberToObject(output, "response_delay_std", 0);
                    cJSON_AddNumberToObject(output, "response_delay_median", mean);
                    cJSON_AddNumberToObject(output, "response_delay_q1", mean);
                    cJSON_AddNumberToObject(output, "response_delay_q3", mean);
                    cJSON_AddNumberToObject(output, "response_delay_skew", 0);
                    cJSON_AddNumberToObject(output, "response_delay_kurt", 0);
                }

                // 判断是否为交互式会话
                cJSON_AddNumberToObject(output, "is_interactive_session",
                    is_interactive_session(mean) ? 1 : 0);
            }
        }
    }
}
