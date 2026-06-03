#include "calc_sequence.h"
#include "feature_config.h"
#include "utils/stats_utils.h"
#include <math.h>
#include <sstream>
#include <set>
#include <string>

static std::string serialize_uint_array(const unsigned int* arr, unsigned int n)
{
    std::ostringstream oss;
    for (unsigned int i = 0; i < n; i++) {
        if (i > 0) oss << ",";
        oss << arr[i];
    }
    return oss.str();
}

static std::string serialize_ull_array(const unsigned long long* arr, unsigned int n)
{
    std::ostringstream oss;
    for (unsigned int i = 0; i < n; i++) {
        if (i > 0) oss << ",";
        oss << arr[i];
    }
    return oss.str();
}

static std::string serialize_dir_array(const signed char* arr, unsigned int n)
{
    std::ostringstream oss;
    for (unsigned int i = 0; i < n; i++) {
        if (i > 0) oss << ",";
        oss << (int)arr[i];
    }
    return oss.str();
}

static std::string serialize_double_array(const double* arr, unsigned int n)
{
    std::ostringstream oss;
    for (unsigned int i = 0; i < n; i++) {
        if (i > 0) oss << ",";
        oss << arr[i];
    }
    return oss.str();
}

// 计算自相关系数
static double calc_autocorr(unsigned int* seq, int n, int lag)
{
    if (n <= lag) return 0;

    double mean = 0;
    for (int i = 0; i < n; i++) {
        mean += seq[i];
    }
    mean /= n;

    double num = 0, den = 0;
    for (int i = 0; i < n - lag; i++) {
        double d1 = seq[i] - mean;
        double d2 = seq[i + lag] - mean;
        num += d1 * d2;
        den += d1 * d1;
    }
    return (den > 0) ? (num / den) : 0;
}

// 计算包长序列自相关
void calc_autocorr_packet_length(flow_feature_state_t* state,
                                  cJSON* output,
                                  int max_lag)
{
    unsigned int count;
    unsigned int* seq = (unsigned int*)circular_buffer_get_array(
        &state->pkt_len_seq, &count);

    if (!seq || count <= (unsigned int)max_lag) {
        if (seq) free(seq);
        cJSON_AddNumberToObject(output, "length_autocorr_lag_1", 0);
        cJSON_AddNumberToObject(output, "length_autocorr_lag_2", 0);
        cJSON_AddNumberToObject(output, "length_autocorr_lag_3", 0);
        cJSON_AddNumberToObject(output, "length_autocorr_lag_4", 0);
        cJSON_AddNumberToObject(output, "length_autocorr_lag_5", 0);
        return;
    }

    for (int lag = 1; lag <= max_lag && lag <= 5; lag++) {
        char key[32];
        snprintf(key, sizeof(key), "length_autocorr_lag_%d", lag);
        cJSON_AddNumberToObject(output, key, calc_autocorr(seq, count, lag));
    }

    free(seq);
}

// 计算 IAT 序列自相关
void calc_autocorr_iat(flow_feature_state_t* state,
                       cJSON* output,
                       int max_lag)
{
    unsigned int count;
    unsigned long long* seq = (unsigned long long*)circular_buffer_get_array(
        &state->iat_seq, &count);

    if (!seq || count <= (unsigned int)max_lag) {
        if (seq) free(seq);
        return;
    }

    // 转为 double 数组
    double* dbl_seq = (double*)malloc(count * sizeof(double));
    for (unsigned int i = 0; i < count; i++) {
        dbl_seq[i] = (double)seq[i];
    }
    free(seq);

    // 计算自相关（简化实现）
    // 实际应使用与 calc_autocorr 类似的函数

    free(dbl_seq);
}

// 计算游程统计
void calc_run_length(flow_feature_state_t* state,
                     cJSON* output)
{
    unsigned int count;
    unsigned int* seq = (unsigned int*)circular_buffer_get_array(
        &state->pkt_len_seq, &count);

    if (!seq || count == 0) {
        if (seq) free(seq);
        return;
    }

    if (count == 1) {
        cJSON_AddNumberToObject(output, "length_run_mean", 1);
        cJSON_AddNumberToObject(output, "length_run_std", 0);
        cJSON_AddNumberToObject(output, "length_run_min", 1);
        cJSON_AddNumberToObject(output, "length_run_max", 1);
        cJSON_AddNumberToObject(output, "length_run_median", 1);
        cJSON_AddNumberToObject(output, "length_run_q1", 1);
        cJSON_AddNumberToObject(output, "length_run_q3", 1);
        cJSON_AddNumberToObject(output, "length_run_skew", 0);
        cJSON_AddNumberToObject(output, "length_run_kurt", 0);
        free(seq);
        return;
    }

    unsigned int* runs = (unsigned int*)malloc(count * sizeof(unsigned int));
    unsigned int run_n = 0;
    unsigned int cur = 1;
    for (unsigned int i = 1; i < count; i++) {
        if (seq[i] == seq[i - 1]) cur++;
        else {
            runs[run_n++] = cur;
            cur = 1;
        }
    }
    runs[run_n++] = cur;

    double sum = 0, sum_sq = 0;
    unsigned int min_v = runs[0], max_v = runs[0];
    double* runs_d = (double*)malloc(run_n * sizeof(double));
    for (unsigned int i = 0; i < run_n; i++) {
        sum += runs[i];
        sum_sq += (double)runs[i] * runs[i];
        if (runs[i] < min_v) min_v = runs[i];
        if (runs[i] > max_v) max_v = runs[i];
        runs_d[i] = (double)runs[i];
    }
    double mean = sum / run_n;
    double std = (run_n > 1) ? sqrt((sum_sq - run_n * mean * mean) / (run_n - 1)) : 0;

    unsigned int* sorted = copy_and_sort_uint(runs, run_n);
    cJSON_AddNumberToObject(output, "length_run_mean", mean);
    cJSON_AddNumberToObject(output, "length_run_std", std);
    cJSON_AddNumberToObject(output, "length_run_min", min_v);
    cJSON_AddNumberToObject(output, "length_run_max", max_v);
    if (sorted) {
        cJSON_AddNumberToObject(output, "length_run_median", calc_median_uint(sorted, run_n));
        cJSON_AddNumberToObject(output, "length_run_q1", calc_q1_uint(sorted, run_n));
        cJSON_AddNumberToObject(output, "length_run_q3", calc_q3_uint(sorted, run_n));
        free(sorted);
    } else {
        cJSON_AddNumberToObject(output, "length_run_median", mean);
        cJSON_AddNumberToObject(output, "length_run_q1", mean);
        cJSON_AddNumberToObject(output, "length_run_q3", mean);
    }
    cJSON_AddNumberToObject(output, "length_run_skew", calc_skewness(runs_d, run_n, mean, std));
    cJSON_AddNumberToObject(output, "length_run_kurt", calc_kurtosis(runs_d, run_n, mean, std));

    free(runs_d);
    free(runs);

    free(seq);
}

// 计算 LZ78 复杂度
double calc_lz78_complexity(unsigned int* seq, int n)
{
    if (!seq || n <= 1) return 0;

    std::set<std::string> dict;
    int i = 0;
    int phrases = 0;
    while (i < n) {
        std::string cur;
        int j = i;
        while (j < n) {
            cur += std::to_string(seq[j]) + "|";
            if (dict.find(cur) == dict.end()) {
                dict.insert(cur);
                phrases++;
                break;
            }
            j++;
        }
        i = j + 1;
    }

    return (double)phrases;
}

// 计算 Hurst 指数（R/S 分析）
double calc_hurst_exponent(unsigned long long* iat_seq, int n)
{
    if (n < 10) return 0.5;

    int max_block = n / 2;
    if (max_block < 8) return 0.5;

    double x1 = 0, y1 = 0, x2 = 0, xy = 0;
    int points = 0;
    for (int block = 8; block <= max_block; block *= 2) {
        int m = n / block;
        if (m < 1) continue;
        double rs_avg = 0;
        for (int k = 0; k < m; k++) {
            int start = k * block;
            double mean = 0;
            for (int i = 0; i < block; i++) mean += iat_seq[start + i];
            mean /= block;

            double cum = 0, min_c = 0, max_c = 0, var = 0;
            for (int i = 0; i < block; i++) {
                double d = iat_seq[start + i] - mean;
                cum += d;
                if (cum < min_c) min_c = cum;
                if (cum > max_c) max_c = cum;
                var += d * d;
            }
            double r = max_c - min_c;
            double s = (block > 1) ? sqrt(var / (block - 1)) : 0;
            if (s > 0) rs_avg += r / s;
        }
        rs_avg /= m;
        if (rs_avg > 0) {
            double lx = log((double)block);
            double ly = log(rs_avg);
            x1 += lx; y1 += ly; x2 += lx * lx; xy += lx * ly;
            points++;
        }
    }

    if (points < 2) return 0.5;
    double den = points * x2 - x1 * x1;
    if (den == 0) return 0.5;
    double h = (points * xy - x1 * y1) / den;
    if (h < 0) h = 0;
    if (h > 1.5) h = 1.5;
    return h;
}

// 计算方向序列熵
double calc_direction_entropy(flow_feature_state_t* state)
{
    unsigned int count = 0;
    signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &count);
    if (!dirs || count == 0) {
        if (dirs) free(dirs);
        return 0;
    }

    unsigned int fwd = 0, bwd = 0;
    for (unsigned int i = 0; i < count; i++) {
        if (dirs[i] > 0) fwd++;
        else if (dirs[i] < 0) bwd++;
    }
    free(dirs);

    if (fwd == 0 || bwd == 0) return 0;
    double p_fwd = (double)fwd / (fwd + bwd);
    double p_bwd = (double)bwd / (fwd + bwd);
    return -(p_fwd * log2(p_fwd) + p_bwd * log2(p_bwd));
}

// 计算长度 - 方向相关系数
double calc_length_direction_correlation(flow_feature_state_t* state)
{
    unsigned int pkt_count = 0;
    unsigned int* pkt_lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &pkt_count);
    unsigned int dir_count = 0;
    signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &dir_count);

    if (!pkt_lens || !dirs || pkt_count == 0 || pkt_count != dir_count) {
        if (pkt_lens) free(pkt_lens);
        if (dirs) free(dirs);
        return 0;
    }

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
    int n = (int)pkt_count;
    for (int i = 0; i < n; i++) {
        double x = (double)pkt_lens[i];
        double y = (double)dirs[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }

    double num = n * sum_xy - sum_x * sum_y;
    double den = sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));

    free(pkt_lens);
    free(dirs);

    return (den > 0) ? (num / den) : 0;
}

// 计算长度-IAT 相关系数
double calc_length_iat_correlation(flow_feature_state_t* state)
{
    unsigned int pkt_count;
    unsigned int* pkt_lens = (unsigned int*)circular_buffer_get_array(
        &state->pkt_len_seq, &pkt_count);

    unsigned int iat_count;
    unsigned long long* iat_seq = (unsigned long long*)circular_buffer_get_array(
        &state->iat_seq, &iat_count);

    if (!pkt_lens || !iat_seq || pkt_count != iat_count + 1) {
        if (pkt_lens) free(pkt_lens);
        if (iat_seq) free(iat_seq);
        return 0;
    }

    // Pearson 相关系数
    int n = iat_count;
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;

    for (int i = 0; i < n; i++) {
        double x = pkt_lens[i];
        double y = (double)iat_seq[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }

    double num = n * sum_xy - sum_x * sum_y;
    double den = sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));

    if (pkt_lens) free(pkt_lens);
    if (iat_seq) free(iat_seq);

    return (den > 0) ? (num / den) : 0;
}

// 计算序列派生特征
void calc_derived_sequence(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    // === enable_sequence_stats: 序列统计特征 (45 维) ===
    if (cfg->enable_sequence_stats) {
        // 包长序列统计
        cJSON_AddNumberToObject(output, "packet_length_seq_len",
            circular_buffer_count(&state->pkt_len_seq));

        // IAT 序列统计
        cJSON_AddNumberToObject(output, "packet_iat_seq_len",
            circular_buffer_count(&state->iat_seq));
        cJSON_AddNumberToObject(output, "packet_direction_seq_len",
            circular_buffer_count(&state->dir_seq));

        // 自相关特征
        calc_autocorr_packet_length(state, output, 5);

        // 游程统计
        calc_run_length(state, output);

        // 长度-IAT 相关
        cJSON_AddNumberToObject(output, "length_iat_correlation",
            calc_length_iat_correlation(state));

        // 方向序列熵
        cJSON_AddNumberToObject(output, "direction_sequence_entropy",
            calc_direction_entropy(state));

        // 方向变化与转换特征
        {
            unsigned int dir_count = 0;
            signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &dir_count);
            if (dirs && dir_count > 1) {
                unsigned int change_count = 0;
                int transition_seen[4] = {0};
                for (unsigned int i = 1; i < dir_count; i++) {
                    if (dirs[i] != dirs[i - 1]) {
                        change_count++;
                    }

                    int prev_idx = (dirs[i - 1] > 0) ? 0 : 1;
                    int curr_idx = (dirs[i] > 0) ? 0 : 1;
                    transition_seen[prev_idx * 2 + curr_idx] = 1;
                }

                int unique_transitions = 0;
                for (int i = 0; i < 4; i++) {
                    unique_transitions += transition_seen[i];
                }

                cJSON_AddNumberToObject(output, "direction_change_frequency",
                    (double)change_count / (dir_count - 1));
                cJSON_AddNumberToObject(output, "direction_transition_unique_count",
                    unique_transitions);
                free(dirs);
            } else {
                cJSON_AddNumberToObject(output, "direction_change_frequency", 0);
                cJSON_AddNumberToObject(output, "direction_transition_unique_count", 0);
                if (dirs) free(dirs);
            }
        }

        // 长度-方向相关
        cJSON_AddNumberToObject(output, "length_direction_correlation",
            calc_length_direction_correlation(state));

        // 包长唯一值比例
        {
            unsigned int count = 0;
            unsigned int* lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &count);
            if (lens && count > 0) {
                unsigned int* sorted = (unsigned int*)malloc(count * sizeof(unsigned int));
                if (sorted) {
                    memcpy(sorted, lens, count * sizeof(unsigned int));
                    qsort(sorted, count, sizeof(unsigned int), compare_uint);
                    unsigned int unique_cnt = 1;
                    for (unsigned int i = 1; i < count; i++) {
                        if (sorted[i] != sorted[i - 1]) unique_cnt++;
                    }
                    cJSON_AddNumberToObject(output, "length_sequence_unique_ratio",
                        (double)unique_cnt / count);
                    free(sorted);
                }
                free(lens);
            }
        }

        // 包长分布特征
        {
            unsigned int count = 0;
            unsigned int* lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &count);
            if (lens && count > 0) {
                unsigned int max_v = lens[0];
                for (unsigned int i = 1; i < count; i++) {
                    if (lens[i] > max_v) max_v = lens[i];
                }
                unsigned int bins = (max_v > 0 && max_v < 4096) ? (max_v + 1) : 4096;
                unsigned int* hist = (unsigned int*)calloc(bins, sizeof(unsigned int));
                if (hist) {
                    for (unsigned int i = 0; i < count; i++) {
                        unsigned int v = (lens[i] < bins) ? lens[i] : (bins - 1);
                        hist[v]++;
                    }
                    cJSON_AddNumberToObject(output, "packet_length_entropy", calc_shannon_entropy(hist, bins));
                    free(hist);
                }

                unsigned int* sorted = copy_and_sort_uint(lens, count);
                if (sorted) {
                    cJSON_AddNumberToObject(output, "packet_length_percentile_1", calc_percentile_uint(sorted, count, 1.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_5", calc_percentile_uint(sorted, count, 5.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_10", calc_percentile_uint(sorted, count, 10.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_90", calc_percentile_uint(sorted, count, 90.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_95", calc_percentile_uint(sorted, count, 95.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_99", calc_percentile_uint(sorted, count, 99.0));
                    cJSON_AddNumberToObject(output, "packet_length_percentile_100", calc_percentile_uint(sorted, count, 100.0));
                    free(sorted);
                }

                double lz = calc_lz78_complexity(lens, (int)count);
                cJSON_AddNumberToObject(output, "lz78_complexity_index", lz);
                cJSON_AddNumberToObject(output, "length_sequence_complexity", (count > 0) ? lz / count : 0);

                free(lens);
            }
        }

        // Hurst 指数
        {
            unsigned int iat_count = 0;
            unsigned long long* iats = (unsigned long long*)circular_buffer_get_array(&state->iat_seq, &iat_count);
            double periodic_ratio = 0;
            if (iats && iat_count > 6) {
                double ac1 = 0, ac_max = 0;
                double mean = 0;
                for (unsigned int i = 0; i < iat_count; i++) mean += iats[i];
                mean /= iat_count;
                double den = 0;
                for (unsigned int i = 0; i < iat_count; i++) {
                    double d = iats[i] - mean;
                    den += d * d;
                }
                if (den > 0) {
                    for (int lag = 1; lag <= 5; lag++) {
                        if (iat_count <= (unsigned int)lag) break;
                        double num = 0;
                        for (unsigned int i = 0; i < iat_count - (unsigned int)lag; i++) {
                            num += (iats[i] - mean) * (iats[i + lag] - mean);
                        }
                        double ac = fabs(num / den);
                        if (lag == 1) ac1 = ac;
                        if (ac > ac_max) ac_max = ac;
                    }
                    periodic_ratio = (ac1 > 1e-9) ? (ac_max / ac1) : ac_max;
                }
            }

            cJSON_AddNumberToObject(output, "hurst_exponent_estimate",
                (iats && iat_count > 0) ? calc_hurst_exponent(iats, (int)iat_count) : 0.5);
            cJSON_AddNumberToObject(output, "periodic_dominant_freq_ratio", periodic_ratio);
            cJSON_AddNumberToObject(output, "has_strong_periodicity", (periodic_ratio > 1.5) ? 1 : 0);
            if (iats) free(iats);
        }

        cJSON_AddStringToObject(output, "custom_protocol_length_pattern", "unknown");
        cJSON_AddNumberToObject(output, "ar_prediction_mse", 0);
        cJSON_AddNumberToObject(output, "ar_prediction_mae", 0);

        // Bigram 频率
        bigram_stats_t* b = &state->bigram_stats;
        if (b->total > 0) {
            const char* names[9] = {"ss","sm","sl","ms","mm","ml","ls","lm","ll"};
            struct rank_item { int type; unsigned int count; } ranks[9];
            for (int i = 0; i < 9; i++) {
                ranks[i].type = i;
                ranks[i].count = b->counts[i];
            }
            for (int i = 0; i < 9; i++) {
                for (int j = i + 1; j < 9; j++) {
                    if (ranks[i].count < ranks[j].count) {
                        rank_item t = ranks[i];
                        ranks[i] = ranks[j];
                        ranks[j] = t;
                    }
                }
            }

            for (int k = 1; k <= 5; k++) {
                for (int t = 0; t < 9; t++) {
                    char key[64];
                    snprintf(key, sizeof(key), "top_%d_bigram_%s_freq", k, names[t]);
                    cJSON_AddNumberToObject(output, key, 0);
                }
                int type = ranks[k - 1].type;
                char win_key[64];
                snprintf(win_key, sizeof(win_key), "top_%d_bigram_%s_freq", k, names[type]);
                cJSON_ReplaceItemInObject(output, win_key,
                    cJSON_CreateNumber((double)ranks[k - 1].count / b->total));
            }
        }
    }

    // === enable_raw_sequences: 原始序列输出 (4 字段) ===
    if (cfg->enable_raw_sequences) {
        unsigned int len_count = 0, iat_count = 0, dir_count = 0;
        unsigned int l3_count = 0, l4_count = 0, payload_len_count = 0;
        //获取一些序列(长度、IAT、方向、L3/L4/Payload长度等)，并序列化为字符串输出
        unsigned int* lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &len_count);
        unsigned long long* iats = (unsigned long long*)circular_buffer_get_array(&state->iat_seq, &iat_count);
        signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &dir_count);
        unsigned int* l3_lens = (unsigned int*)circular_buffer_get_array(&state->l3_len_seq, &l3_count);
        unsigned int* l4_lens = (unsigned int*)circular_buffer_get_array(&state->l4_len_seq, &l4_count);
        unsigned int* payload_lens = (unsigned int*)circular_buffer_get_array(&state->payload_len_seq, &payload_len_count);

        if (lens) {
            std::string seq = serialize_uint_array(lens, len_count);
            cJSON_AddStringToObject(output, "packet_length_seq", seq.c_str());
        }
        if (iats) {
            std::string seq = serialize_ull_array(iats, iat_count);
            cJSON_AddStringToObject(output, "packet_iat_seq", seq.c_str());
        }
        if (dirs) {
            std::string seq = serialize_dir_array(dirs, dir_count);
            cJSON_AddStringToObject(output, "packet_direction_seq", seq.c_str());
            free(dirs);
        }

        // L2/L3/L4/Payload/IAT 组合序列
        if (l3_lens && l4_lens && payload_lens) {
            unsigned int n = l3_count;
            if (l4_count < n) n = l4_count;
            if (payload_len_count < n) n = payload_len_count;
            if (lens && len_count < n) n = len_count;

            std::ostringstream l2l3l4pl_iat;
            for (unsigned int i = 0; i < n; i++) {
                if (i > 0) l2l3l4pl_iat << ",";
                unsigned long long iat_v = (i == 0) ? 0ULL : ((i - 1 < iat_count) ? iats[i - 1] : 0ULL);
                unsigned int l2_v = 0;
                if (lens) {
                    unsigned int l3l4pl = l3_lens[i] + l4_lens[i] + payload_lens[i];
                    if (lens[i] >= l3l4pl) {
                        l2_v = lens[i] - l3l4pl;
                    }
                }
                l2l3l4pl_iat << l2_v << "/" << l3_lens[i] << "/" << l4_lens[i] << "/" << payload_lens[i] << "/" << iat_v;
            }
            cJSON_AddStringToObject(output, "l2l3l4pl_iat", l2l3l4pl_iat.str().c_str());
        }

        if (lens) free(lens);
        if (l3_lens) free(l3_lens);
        if (l4_lens) free(l4_lens);
        if (payload_lens) free(payload_lens);
        if (iats) free(iats);
    }

    // === enable_chunk_sequences: chunk 序列 (2 字段) ===
    if (cfg->enable_chunk_sequences) {
        unsigned int ts_count = 0, len_count = 0, dir_count = 0;
        unsigned long long* ts = (unsigned long long*)circular_buffer_get_array(&state->ts_seq, &ts_count);
        unsigned int* lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &len_count);
        signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &dir_count);
        if (ts && lens && dirs && ts_count == len_count && ts_count == dir_count && ts_count > 0) {
            // dl_chunk_seq：按方向切换或大IAT切分，记录每chunk字节和
            std::ostringstream chunk_oss;
            unsigned long long last_ts = ts[0];
            signed char last_dir = dirs[0];
            unsigned long long chunk_bytes = lens[0];
            unsigned int chunk_count = 0;
            for (unsigned int i = 1; i < ts_count; i++) {
                unsigned long long iat = ts[i] - last_ts;
                if (dirs[i] != last_dir || iat > 100000) {
                    if (chunk_count > 0) chunk_oss << ",";
                    chunk_oss << chunk_bytes;
                    chunk_count++;
                    chunk_bytes = 0;
                }
                chunk_bytes += lens[i];
                last_ts = ts[i];
                last_dir = dirs[i];
            }
            if (chunk_bytes > 0) {
                if (chunk_count > 0) chunk_oss << ",";
                chunk_oss << chunk_bytes;
                chunk_count++;
            }
            cJSON_AddStringToObject(output, "dl_chunk_seq", chunk_oss.str().c_str());
            state->last_dl_chunk_count = chunk_count;
        }
        if (ts) free(ts);
        if (lens) free(lens);
        if (dirs) free(dirs);

        cJSON_AddNumberToObject(output, "dl_chunk_seq_len", state->last_dl_chunk_count);
    }

    // === enable_rate_sequences: 速率序列 (4 字段) ===
    if (cfg->enable_rate_sequences) {
        unsigned int ts_count = 0, len_count = 0, dir_count = 0;
        unsigned long long* ts = (unsigned long long*)circular_buffer_get_array(&state->ts_seq, &ts_count);
        unsigned int* lens = (unsigned int*)circular_buffer_get_array(&state->pkt_len_seq, &len_count);
        signed char* dirs = (signed char*)circular_buffer_get_array(&state->dir_seq, &dir_count);
        if (ts && lens && dirs && ts_count == len_count && ts_count == dir_count && ts_count > 0) {
            const unsigned long long win_us = 50000ULL;
            unsigned long long start = ts[0];
            unsigned long long end = ts[ts_count - 1];
            unsigned int win_n = (unsigned int)((end - start) / win_us) + 1;
            double* up = (double*)calloc(win_n, sizeof(double));
            double* down = (double*)calloc(win_n, sizeof(double));
            if (up && down) {
                for (unsigned int i = 0; i < ts_count; i++) {
                    unsigned int idx = (unsigned int)((ts[i] - start) / win_us);
                    if (idx >= win_n) idx = win_n - 1;
                    double rate = (double)lens[i] * 8.0 / 0.05; // bps
                    if (dirs[i] > 0) up[idx] += rate;
                    else if (dirs[i] < 0) down[idx] += rate;
                }
                std::string up_seq = serialize_double_array(up, win_n);
                std::string down_seq = serialize_double_array(down, win_n);
                cJSON_AddStringToObject(output, "uplink_rate_seq", up_seq.c_str());
                cJSON_AddStringToObject(output, "downlink_rate_seq", down_seq.c_str());
                state->last_uplink_rate_seq_len = win_n;
                state->last_downlink_rate_seq_len = win_n;
            }
            if (up) free(up);
            if (down) free(down);
        }
        if (ts) free(ts);
        if (lens) free(lens);
        if (dirs) free(dirs);

        cJSON_AddNumberToObject(output, "uplink_rate_seq_len", state->last_uplink_rate_seq_len);
        cJSON_AddNumberToObject(output, "downlink_rate_seq_len", state->last_downlink_rate_seq_len);
    }
}
