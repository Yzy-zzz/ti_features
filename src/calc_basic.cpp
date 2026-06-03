#include "calc_basic.h"
#include "calc_payload.h"
#include "feature_config.h"
#include "utils/stats_utils.h"
#include "utils/memory_pool.h"
#include <ctype.h>

// ============================================================================
// 辅助函数 - 使用栈上或小数组优化，避免 malloc
// ============================================================================

// 在栈上排序小数组（最多 256 元素）
static inline void sort_small_uint_array(unsigned int* arr, int n)
{
    if (n <= 0) return;
    // 小数组使用简单的插入排序（对小数组更快）
    for (int i = 1; i < n; i++) {
        unsigned int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// 在临时 buffer 中准备 double 数组
static inline double* prepare_double_buffer(double* temp_buf, const unsigned int* src, int n)
{
    for (int i = 0; i < n; i++) {
        temp_buf[i] = (double)src[i];
    }
    return temp_buf;
}

// 更新基础计数器
void calc_basic_counters(flow_feature_state_t* state,
                         int direction,
                         unsigned int pkt_len,
                         unsigned int payload_len)
{
    state->total_packets++;
    state->l2_packets++;
    state->total_bytes += pkt_len;

    ti_feature_config_t* cfg = feat_config_get();

    if (direction == DIR_FWD) {
        state->fwd_packets++;
        state->fwd_bytes += pkt_len;
        state->fwd_payload_bytes += payload_len;
        if (cfg->need_payload_dir && payload_len > 0) {
            running_stats_update(&state->fwd_payload_stats, (double)payload_len);
            if (state->fwd_payload_count < state->fwd_payload_capacity) {
                state->fwd_payload_sizes[state->fwd_payload_count++] = payload_len;
            }
        }
    } else if (direction == DIR_BWD) {
        state->bwd_packets++;
        state->bwd_bytes += pkt_len;
        state->bwd_payload_bytes += payload_len;
        if (cfg->need_payload_dir && payload_len > 0) {
            running_stats_update(&state->bwd_payload_stats, (double)payload_len);
            if (state->bwd_payload_count < state->bwd_payload_capacity) {
                state->bwd_payload_sizes[state->bwd_payload_count++] = payload_len;
            }
        }
    }
}

// 更新包长统计
void calc_packet_length_stats(flow_feature_state_t* state,
                               int direction,
                               unsigned int pkt_len)
{
    ti_feature_config_t* cfg = feat_config_get();

    // Welford 统计 + 小/大包计数（被 basic_pkt_length 使用）
    if (cfg->enable_basic_pkt_length) {
        running_stats_update(&state->pkt_len_stats, (double)pkt_len);
        if (pkt_len <= cfg->small_pkt_threshold) state->small_pkt_count++;
        if (pkt_len >= cfg->large_pkt_threshold) state->large_pkt_count++;
    }

    // 方向包长统计（被 basic_pkt_length + fft_fwd/bwd 使用）
    if (cfg->need_fwd_bwd_pkt_lens) {
        directional_stats_update(&state->fwd_pkt_len, direction, (double)pkt_len);
        directional_stats_update(&state->bwd_pkt_len, direction, (double)pkt_len);
    }

    // pkt_len_seq circular buffer（被 fft_global + sequence 模块使用）
    if (cfg->need_pkt_len_seq) {
        circular_buffer_push(&state->pkt_len_seq, &pkt_len);
    }

    // 记录前 N 包（被 basic_first_n 使用）
    if (cfg->need_first_n && state->first_n_lens && state->first_n_count < state->first_n_capacity) {
        state->first_n_lens[state->first_n_count++] = pkt_len;
    }

    // 记录方向包长数组（被 basic_pkt_length 分位数 + fft_fwd/bwd 使用）
    if (cfg->need_fwd_bwd_pkt_lens) {
        if (direction == DIR_FWD && state->fwd_pkt_len_count < state->fwd_pkt_len_capacity) {
            state->fwd_pkt_lens[state->fwd_pkt_len_count++] = pkt_len;
        } else if (direction == DIR_BWD && state->bwd_pkt_len_count < state->bwd_pkt_len_capacity) {
            state->bwd_pkt_lens[state->bwd_pkt_len_count++] = pkt_len;
        }
    }
}

// 更新 Payload 统计
void calc_payload_stats(flow_feature_state_t* state,
                        unsigned char* payload,
                        unsigned int payload_len)
{
    if (payload_len == 0) {
        return;
    }

    // 更新 payload 大小统计
    running_stats_update(&state->payload_size_stats, (double)payload_len);
    circular_buffer_push(&state->payload_size_seq, &payload_len);

    if (state->has_last_payload_len) {
        double diff = fabs((double)payload_len - state->last_payload_len);
        running_stats_update(&state->payload_size_diff_stats, diff);
        unsigned int diff_u = (unsigned int)diff;
        circular_buffer_push(&state->payload_size_diff_seq, &diff_u);
    }
    state->last_payload_len = payload_len;
    state->has_last_payload_len = 1;

    state->payload_total_bytes += payload_len;
    state->payload_non_empty_count++;

    // Payload 高级统计（按包采样）
    running_stats_update(&state->payload_entropy_stats, calc_byte_entropy(payload, payload_len));
    running_stats_update(&state->payload_markov_entropy_stats, calc_markov_entropy(payload, payload_len));
    running_stats_update(&state->payload_compression_ratio_stats, calc_compression_ratio(payload, payload_len));
    if (payload_len > 1) {
        double mean = 0;
        for (unsigned int i = 0; i < payload_len; i++) {
            mean += payload[i];
        }
        mean /= payload_len;
        double num = 0, den = 0;
        for (unsigned int i = 0; i + 1 < payload_len; i++) {
            double d1 = payload[i] - mean;
            double d2 = payload[i + 1] - mean;
            num += d1 * d2;
            den += d1 * d1;
        }
        running_stats_update(&state->payload_autocorr_lag1_stats, (den > 0) ? (num / den) : 0);
    }

    // 协议形态识别标志
    if (payload_looks_like_http(payload, payload_len)) state->payload_looks_like_http = 1;
    if (payload_looks_like_json(payload, payload_len)) state->payload_looks_like_json = 1;
    if (payload_looks_like_xml(payload, payload_len)) state->payload_looks_like_xml = 1;
    if (payload_looks_like_tls(payload, payload_len)) state->payload_looks_like_tls = 1;
    if (payload_looks_like_ssh(payload, payload_len)) state->payload_looks_like_ssh = 1;

    // 更新字节值直方图（16 个 bin，每 bin 16 个值）
    for (unsigned int i = 0; i < payload_len; i++) {
        int bin = payload[i] / 16;
        if (bin < 16) {
            state->payload_byte_hist[bin]++;
        }
        state->payload_byte_hist_full[payload[i]]++;
        running_stats_update(&state->payload_byte_stats, (double)payload[i]);

        // 统计可打印字符和字母数字
        if (payload[i] >= 32 && payload[i] <= 126) {
            state->payload_printable_count++;
        }
        if (isalnum(payload[i])) {
            state->payload_alnum_count++;
        }

        if (state->payload_first_bytes_count < sizeof(state->payload_first_bytes)) {
            state->payload_first_bytes[state->payload_first_bytes_count++] = payload[i];
        }
    }

    // 计算 mod 统计
    unsigned int mods[] = {2, 4, 8, 16, 32, 64, 128, 256};
    for (int i = 0; i < 8; i++) {
        running_stats_update(&state->payload_mod[i], (double)(payload_len % mods[i]));
    }
}

// 检测 Payload 魔数/协议特征
void calc_payload_magic(flow_feature_state_t* state,
                        unsigned char* payload,
                        unsigned int payload_len)
{
    if (payload_len < 4) {
        return;
    }

    // HTTP GET
    if (payload_len >= 4 && memcmp(payload, "GET ", 4) == 0) {
        state->payload_magic_HTTP_GET = 1;
    }

    // HTTP POST
    if (payload_len >= 5 && memcmp(payload, "POST ", 5) == 0) {
        state->payload_magic_HTTP_POST = 1;
    }

    // TLS 握手 (0x16 0x03 xx)
    if (payload_len >= 3 && payload[0] == 0x16 && payload[1] == 0x03) {
        if (payload[2] == 0x00 || payload[2] == 0x01) {
            state->payload_magic_TLS_1_0 = 1;
        } else if (payload[2] == 0x02) {
            state->payload_magic_TLS_1_1 = 1;
        } else if (payload[2] == 0x03) {
            state->payload_magic_TLS_1_2 = 1;
        } else if (payload[2] == 0x04) {
            state->payload_magic_TLS_1_3 = 1;
        }
    }

    // SSH (以 "SSH-" 开头)
    if (payload_len >= 4 && memcmp(payload, "SSH-", 4) == 0) {
        state->payload_magic_SSH = 1;
    }

    // JPEG (0xFFD8)
    if (payload_len >= 2 && payload[0] == 0xFF && payload[1] == 0xD8) {
        state->payload_magic_JPEG = 1;
    }

    // PNG (0x89PNG)
    if (payload_len >= 4 && payload[0] == 0x89 &&
        memcmp(payload + 1, "PNG", 3) == 0) {
        state->payload_magic_PNG = 1;
    }

    // GIF (GIF87a or GIF89a)
    if (payload_len >= 6 && memcmp(payload, "GIF8", 4) == 0 &&
        (payload[4] == '7' || payload[4] == '9') && payload[5] == 'a') {
        state->payload_magic_GIF = 1;
    }
}

// 计算派生特征 - 优化版本：使用临时 buffer 复用，避免 malloc
void calc_derived_basic(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    // === enable_basic_ratios: 基础比率 (5 维) ===
    if (cfg->enable_basic_ratios) {
        // 前后向包数比
        double fwd_bwd_pkt_ratio = (state->bwd_packets > 0) ?
            (double)state->fwd_packets / state->bwd_packets : INFINITY;
        cJSON_AddNumberToObject(output, "fwd_bwd_packet_ratio", fwd_bwd_pkt_ratio);

        // 前后向字节比
        double fwd_bwd_byte_ratio = (state->bwd_bytes > 0) ?
            (double)state->fwd_bytes / state->bwd_bytes : INFINITY;
        cJSON_AddNumberToObject(output, "fwd_bwd_byte_ratio", fwd_bwd_byte_ratio);

        // 平均包长
        double avg_pkt_len = (state->total_packets > 0) ?
            (double)state->total_bytes / state->total_packets : 0;
        cJSON_AddNumberToObject(output, "avg_packet_length", avg_pkt_len);

        // 前向平均包长
        double fwd_avg_pkt_len = (state->fwd_packets > 0) ?
            (double)state->fwd_bytes / state->fwd_packets : 0;
        cJSON_AddNumberToObject(output, "fwd_avg_packet_length", fwd_avg_pkt_len);

        // 后向平均包长
        double bwd_avg_pkt_len = (state->bwd_packets > 0) ?
            (double)state->bwd_bytes / state->bwd_packets : 0;
        cJSON_AddNumberToObject(output, "bwd_avg_packet_length", bwd_avg_pkt_len);
    }

    // === enable_basic_payload_dir: 前/后向 Payload 统计 (20 维) ===
    if (cfg->enable_basic_payload_dir) {
        // 前向 payload 统计
        cJSON_AddNumberToObject(output, "fwd_total_payload", state->fwd_payload_bytes);
        if (state->fwd_packets > 0) {
            cJSON_AddNumberToObject(output, "fwd_payload_mean",
                (double)state->fwd_payload_bytes / state->fwd_packets);
        }

        cJSON_AddNumberToObject(output, "fwd_payload_std", running_stats_std(&state->fwd_payload_stats));
        cJSON_AddNumberToObject(output, "fwd_payload_min",
            (state->fwd_payload_stats.count > 0) ? state->fwd_payload_stats.min_val : 0);
        cJSON_AddNumberToObject(output, "fwd_payload_max",
            (state->fwd_payload_stats.count > 0) ? state->fwd_payload_stats.max_val : 0);

        // 后向 payload 统计
        cJSON_AddNumberToObject(output, "bwd_total_payload", state->bwd_payload_bytes);
        if (state->bwd_packets > 0) {
            cJSON_AddNumberToObject(output, "bwd_payload_mean",
                (double)state->bwd_payload_bytes / state->bwd_packets);
        }

        cJSON_AddNumberToObject(output, "bwd_payload_std", running_stats_std(&state->bwd_payload_stats));
        cJSON_AddNumberToObject(output, "bwd_payload_min",
            (state->bwd_payload_stats.count > 0) ? state->bwd_payload_stats.min_val : 0);
        cJSON_AddNumberToObject(output, "bwd_payload_max",
            (state->bwd_payload_stats.count > 0) ? state->bwd_payload_stats.max_val : 0);

        // 【优化】使用预分配的临时 buffer，避免 malloc/free
        // double* temp_buf = state->temp_double_buffer;
        double* temp_buf2 = state->temp_double_buffer_2;
        unsigned int max_len = state->temp_uint_buffer_size;

        // 前向 payload 统计（使用临时 buffer）
        if (state->fwd_payload_count > 0 && state->fwd_payload_count <= max_len) {
            unsigned int count = state->fwd_payload_count;
            const unsigned int* src = state->fwd_payload_sizes;

            double fwd_mean = state->fwd_payload_stats.mean;
            double fwd_std = running_stats_std(&state->fwd_payload_stats);

            // 准备 double 数组用于偏度/峰度计算
            prepare_double_buffer(temp_buf2, src, count);

            cJSON_AddNumberToObject(output, "fwd_payload_skew",
                calc_skewness(temp_buf2, count, fwd_mean, fwd_std));
            cJSON_AddNumberToObject(output, "fwd_payload_kurt",
                calc_kurtosis(temp_buf2, count, fwd_mean, fwd_std));

            // 使用 copy_and_sort_uint 对原始 uint 数组排序（避免 double/uint 类型混淆）
            unsigned int* fwd_sorted = copy_and_sort_uint((unsigned int*)src, count);
            if (fwd_sorted) {
                cJSON_AddNumberToObject(output, "fwd_payload_median", calc_median_uint(fwd_sorted, count));
                cJSON_AddNumberToObject(output, "fwd_payload_q1", calc_q1_uint(fwd_sorted, count));
                cJSON_AddNumberToObject(output, "fwd_payload_q3", calc_q3_uint(fwd_sorted, count));
                free(fwd_sorted);
            }
        }

        // 后向 payload 统计（使用临时 buffer）
        if (state->bwd_payload_count > 0 && state->bwd_payload_count <= max_len) {
            unsigned int count = state->bwd_payload_count;
            const unsigned int* src = state->bwd_payload_sizes;

            double bwd_mean = state->bwd_payload_stats.mean;
            double bwd_std = running_stats_std(&state->bwd_payload_stats);

            // 准备 double 数组用于偏度/峰度计算
            prepare_double_buffer(temp_buf2, src, count);

            cJSON_AddNumberToObject(output, "bwd_payload_skew",
                calc_skewness(temp_buf2, count, bwd_mean, bwd_std));
            cJSON_AddNumberToObject(output, "bwd_payload_kurt",
                calc_kurtosis(temp_buf2, count, bwd_mean, bwd_std));

            // 使用 copy_and_sort_uint 对原始 uint 数组排序（避免 double/uint 类型混淆）
            unsigned int* bwd_sorted = copy_and_sort_uint((unsigned int*)src, count);
            if (bwd_sorted) {
                cJSON_AddNumberToObject(output, "bwd_payload_median", calc_median_uint(bwd_sorted, count));
                cJSON_AddNumberToObject(output, "bwd_payload_q1", calc_q1_uint(bwd_sorted, count));
                cJSON_AddNumberToObject(output, "bwd_payload_q3", calc_q3_uint(bwd_sorted, count));
                free(bwd_sorted);
            }
        }
    }

    // === enable_basic_first_n: 前 N 包统计 (9 维) ===
    if (cfg->enable_basic_first_n) {
        if (state->first_n_count > 0 && state->first_n_lens) {
            unsigned int count = state->first_n_count;
            double sum = 0, sum_sq = 0;
            double min_val = state->first_n_lens[0], max_val = state->first_n_lens[0];

            for (unsigned int i = 0; i < count; i++) {
                sum += state->first_n_lens[i];
                sum_sq += state->first_n_lens[i] * state->first_n_lens[i];
                if (state->first_n_lens[i] < min_val) min_val = state->first_n_lens[i];
                if (state->first_n_lens[i] > max_val) max_val = state->first_n_lens[i];
            }

            double mean = sum / count;
            double std = (count > 1) ?
                sqrt((sum_sq - count * mean * mean) / (count - 1)) : 0;

            cJSON_AddNumberToObject(output, "first_n_packets_length_mean", mean);
            cJSON_AddNumberToObject(output, "first_n_packets_length_min", min_val);
            cJSON_AddNumberToObject(output, "first_n_packets_length_max", max_val);
            cJSON_AddNumberToObject(output, "first_n_packets_length_std", std);

            double* temp_buf = state->temp_double_buffer;
            double* temp_buf2 = state->temp_double_buffer_2;
            unsigned int max_len = state->temp_uint_buffer_size;

            unsigned int* sorted_buf = NULL;
            double* first_n_double = NULL;
            int use_temp_buf = (temp_buf && temp_buf2 && count <= max_len);

            if (use_temp_buf) {
                sorted_buf = (unsigned int*)temp_buf;
                first_n_double = temp_buf2;
            } else {
                sorted_buf = (unsigned int*)malloc((size_t)count * sizeof(unsigned int));
                first_n_double = (double*)malloc((size_t)count * sizeof(double));
            }

            if (sorted_buf) {
                memcpy(sorted_buf, state->first_n_lens, (size_t)count * sizeof(unsigned int));
                sort_small_uint_array(sorted_buf, (int)count);
                cJSON_AddNumberToObject(output, "first_n_packets_length_median",
                    calc_median_uint(sorted_buf, count));
                cJSON_AddNumberToObject(output, "first_n_packets_length_q1",
                    calc_q1_uint(sorted_buf, count));
                cJSON_AddNumberToObject(output, "first_n_packets_length_q3",
                    calc_q3_uint(sorted_buf, count));
            } else {
                cJSON_AddNumberToObject(output, "first_n_packets_length_median", 0);
                cJSON_AddNumberToObject(output, "first_n_packets_length_q1", 0);
                cJSON_AddNumberToObject(output, "first_n_packets_length_q3", 0);
            }

            if (first_n_double && count > 1) {
                prepare_double_buffer(first_n_double, state->first_n_lens, (int)count);
                cJSON_AddNumberToObject(output, "first_n_packets_length_skew",
                    calc_skewness(first_n_double, (int)count, mean, std));
                cJSON_AddNumberToObject(output, "first_n_packets_length_kurt",
                    calc_kurtosis(first_n_double, (int)count, mean, std));
            } else {
                cJSON_AddNumberToObject(output, "first_n_packets_length_skew", 0);
                cJSON_AddNumberToObject(output, "first_n_packets_length_kurt", 0);
            }

            if (!use_temp_buf) {
                if (sorted_buf) free(sorted_buf);
                if (first_n_double) free(first_n_double);
            }
        }
    }

    // === enable_basic_pkt_length: 包长统计 (22 维) ===
    if (cfg->enable_basic_pkt_length) {
        // 小包/大包比例（使用 DATA 阶段增量维护的计数器）
        unsigned int pkt_count = state->total_packets;
        if (pkt_count > 0) {
            cJSON_AddNumberToObject(output, "small_packet_ratio",
                (double)state->small_pkt_count / pkt_count);
            cJSON_AddNumberToObject(output, "large_packet_ratio",
                (double)state->large_pkt_count / pkt_count);
        } else {
            cJSON_AddNumberToObject(output, "small_packet_ratio", 0);
            cJSON_AddNumberToObject(output, "large_packet_ratio", 0);
        }

        double* temp_buf = state->temp_double_buffer;
        double* temp_buf2 = state->temp_double_buffer_2;
        unsigned int max_len = state->temp_uint_buffer_size;

        // === 方向过滤的包长统计（分位数等） - 使用临时 buffer ===
        // 前向包长统计
        if (state->fwd_pkt_len_count > 0 && state->fwd_pkt_len_count <= max_len) {
            unsigned int count = state->fwd_pkt_len_count;
            unsigned int* fwd_sorted = (unsigned int*)temp_buf;

            // 复制到临时 buffer 并排序
            memcpy(fwd_sorted, state->fwd_pkt_lens, count * sizeof(unsigned int));
            sort_small_uint_array(fwd_sorted, count);

            double fwd_sum = 0, fwd_sum_sq = 0;
            for (unsigned int i = 0; i < count; i++) {
                fwd_sum += state->fwd_pkt_lens[i];
                fwd_sum_sq += state->fwd_pkt_lens[i] * state->fwd_pkt_lens[i];
            }
            double fwd_mean = fwd_sum / count;
            double fwd_std = (count > 1) ?
                sqrt((fwd_sum_sq - count * fwd_mean * fwd_mean) / (count - 1)) : 0;

            cJSON_AddNumberToObject(output, "fwd_packet_length_mean", fwd_mean);
            cJSON_AddNumberToObject(output, "fwd_packet_length_std", fwd_std);
            cJSON_AddNumberToObject(output, "fwd_packet_length_min", state->fwd_pkt_len.fwd.min_val);
            cJSON_AddNumberToObject(output, "fwd_packet_length_max", state->fwd_pkt_len.fwd.max_val);

            cJSON_AddNumberToObject(output, "fwd_packet_length_median",
                calc_median_uint(fwd_sorted, count));
            cJSON_AddNumberToObject(output, "fwd_packet_length_q1",
                calc_q1_uint(fwd_sorted, count));
            cJSON_AddNumberToObject(output, "fwd_packet_length_q3",
                calc_q3_uint(fwd_sorted, count));

            cJSON_AddNumberToObject(output, "fwd_packet_length_cv", calc_cv(fwd_mean, fwd_std));

            // 偏度和峰度（使用 temp_buf2）
            prepare_double_buffer(temp_buf2, state->fwd_pkt_lens, count);
            cJSON_AddNumberToObject(output, "fwd_packet_length_skew",
                calc_skewness(temp_buf2, count, fwd_mean, fwd_std));
            cJSON_AddNumberToObject(output, "fwd_packet_length_kurt",
                calc_kurtosis(temp_buf2, count, fwd_mean, fwd_std));
        }

        // 后向包长统计
        if (state->bwd_pkt_len_count > 0 && state->bwd_pkt_len_count <= max_len) {
            unsigned int count = state->bwd_pkt_len_count;
            unsigned int* bwd_sorted = (unsigned int*)temp_buf;

            // 复制到临时 buffer 并排序
            memcpy(bwd_sorted, state->bwd_pkt_lens, count * sizeof(unsigned int));
            sort_small_uint_array(bwd_sorted, count);

            double bwd_sum = 0, bwd_sum_sq = 0;
            for (unsigned int i = 0; i < count; i++) {
                bwd_sum += state->bwd_pkt_lens[i];
                bwd_sum_sq += state->bwd_pkt_lens[i] * state->bwd_pkt_lens[i];
            }
            double bwd_mean = bwd_sum / count;
            double bwd_std = (count > 1) ?
                sqrt((bwd_sum_sq - count * bwd_mean * bwd_mean) / (count - 1)) : 0;

            cJSON_AddNumberToObject(output, "bwd_packet_length_mean", bwd_mean);
            cJSON_AddNumberToObject(output, "bwd_packet_length_std", bwd_std);
            cJSON_AddNumberToObject(output, "bwd_packet_length_min", state->bwd_pkt_len.bwd.min_val);
            cJSON_AddNumberToObject(output, "bwd_packet_length_max", state->bwd_pkt_len.bwd.max_val);

            cJSON_AddNumberToObject(output, "bwd_packet_length_median",
                calc_median_uint(bwd_sorted, count));
            cJSON_AddNumberToObject(output, "bwd_packet_length_q1",
                calc_q1_uint(bwd_sorted, count));
            cJSON_AddNumberToObject(output, "bwd_packet_length_q3",
                calc_q3_uint(bwd_sorted, count));

            cJSON_AddNumberToObject(output, "bwd_packet_length_cv", calc_cv(bwd_mean, bwd_std));

            // 偏度和峰度（使用 temp_buf2）
            prepare_double_buffer(temp_buf2, state->bwd_pkt_lens, count);
            cJSON_AddNumberToObject(output, "bwd_packet_length_skew",
                calc_skewness(temp_buf2, count, bwd_mean, bwd_std));
            cJSON_AddNumberToObject(output, "bwd_packet_length_kurt",
                calc_kurtosis(temp_buf2, count, bwd_mean, bwd_std));
        }
    }
}
