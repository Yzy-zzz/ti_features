#include "calc_payload.h"
#include "feature_config.h"
#include "utils/stats_utils.h"
#include <math.h>
#include <zlib.h>

// 计算字节熵（香农熵）
double calc_byte_entropy(unsigned char* data, int len)
{
    if (len == 0) return 0;

    unsigned int hist[256] = {0};
    for (int i = 0; i < len; i++) {
        hist[data[i]]++;
    }

    double entropy = 0;
    for (int i = 0; i < 256; i++) {
        if (hist[i] > 0) {
            double p = (double)hist[i] / len;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

// 计算马尔可夫熵
double calc_markov_entropy(unsigned char* data, int len)
{
    if (len < 2) return 0;

    // 简化的 1 阶马尔可夫熵
    unsigned int trans[256][256] = {0};
    unsigned int from_count[256] = {0};

    for (int i = 0; i < len - 1; i++) {
        trans[data[i]][data[i+1]]++;
        from_count[data[i]]++;
    }

    double entropy = 0;
    for (int i = 0; i < 256; i++) {
        if (from_count[i] > 0) {
            double h = 0;
            for (int j = 0; j < 256; j++) {
                if (trans[i][j] > 0) {
                    double p = (double)trans[i][j] / from_count[i];
                    h -= p * log2(p);
                }
            }
            entropy += (double)from_count[i] / (len - 1) * h;
        }
    }
    return entropy;
}

// 计算卡方统计量
double calc_chi_square(unsigned char* data, int len)
{
    if (len == 0) return 0;

    unsigned int hist[256] = {0};
    for (int i = 0; i < len; i++) {
        hist[data[i]]++;
    }

    double expected = (double)len / 256;
    double chi_sq = 0;
    for (int i = 0; i < 256; i++) {
        double diff = hist[i] - expected;
        chi_sq += diff * diff / expected;
    }
    return chi_sq;
}

// 检测是否像 HTTP
int payload_looks_like_http(unsigned char* data, int len)
{
    if (len < 4) return 0;
    // 检查常见 HTTP 方法
    if (memcmp(data, "GET ", 4) == 0) return 1;
    if (len >= 5 && memcmp(data, "POST ", 5) == 0) return 1;
    if (memcmp(data, "PUT ", 4) == 0) return 1;
    if (len >= 5 && memcmp(data, "HEAD ", 5) == 0) return 1;
    if (memcmp(data, "HTTP", 4) == 0) return 1;
    return 0;
}

// 检测是否像 JSON
int payload_looks_like_json(unsigned char* data, int len)
{
    if (len < 2) return 0;
    // JSON 通常以 { 或 [ 开头
    if (data[0] == '{' || data[0] == '[') {
        // 检查结尾是否有 } 或 ]
        if (data[len-1] == '}' || data[len-1] == ']') {
            return 1;
        }
    }
    return 0;
}

// 检测是否像 XML
int payload_looks_like_xml(unsigned char* data, int len)
{
    if (len < 5) return 0;
    // XML 声明或以 < 开头
    if (memcmp(data, "<?xml", 5) == 0 || data[0] == '<') {
        return 1;
    }
    return 0;
}

// 检测是否像 TLS
int payload_looks_like_tls(unsigned char* data, int len)
{
    if (len < 3) return 0;
    // TLS 记录层 0x16 (握手) 0x17 (应用数据) 0x15 (告警)
    if (data[0] == 0x16 || data[0] == 0x17 || data[0] == 0x15) {
        if (data[1] == 0x03) {  // TLS 版本
            return 1;
        }
    }
    return 0;
}

// 检测是否像 SSH
int payload_looks_like_ssh(unsigned char* data, int len)
{
    if (len < 4) return 0;
    // SSH 协议版本字符串
    if (memcmp(data, "SSH-", 4) == 0) {
        return 1;
    }
    // SSH 二进制包（长度字段在前）
    if (len >= 4 && data[0] < 200 && data[1] < 10) {
        return 1;
    }
    return 0;
}

// 计算压缩率
double calc_compression_ratio(unsigned char* data, int len)
{
    if (len == 0) return 1.0;

    // 使用 zlib 压缩
    uLongf compressed_len = compressBound(len);
    Bytef* compressed = (Bytef*)malloc(compressed_len);

    if (compress(compressed, &compressed_len, data, len) != Z_OK) {
        free(compressed);
        return 1.0;
    }

    double ratio = (double)compressed_len / len;
    free(compressed);
    return ratio;
}

// 计算 Payload 派生特征
void calc_derived_payload(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();

    unsigned int payload_size_count = 0;
    unsigned int* payload_sizes = NULL;

    // === enable_payload_size: 载荷大小统计 (9 维) ===
    if (cfg->enable_payload_size) {
        payload_sizes = (unsigned int*)circular_buffer_get_array(&state->payload_size_seq, &payload_size_count);

        cJSON_AddNumberToObject(output, "payload_size_mean", state->payload_size_stats.mean);
        cJSON_AddNumberToObject(output, "payload_size_std", running_stats_std(&state->payload_size_stats));
        cJSON_AddNumberToObject(output, "payload_size_min", state->payload_size_stats.min_val);
        cJSON_AddNumberToObject(output, "payload_size_max", state->payload_size_stats.max_val);
        cJSON_AddNumberToObject(output, "payload_size_skew", running_stats_skew(&state->payload_size_stats));
        cJSON_AddNumberToObject(output, "payload_size_kurt", running_stats_kurt(&state->payload_size_stats));

        if (payload_sizes && payload_size_count > 0) {
            unsigned int* sorted = copy_and_sort_uint(payload_sizes, payload_size_count);
            if (sorted) {
                cJSON_AddNumberToObject(output, "payload_size_median",
                    calc_median_uint(sorted, payload_size_count));
                cJSON_AddNumberToObject(output, "payload_size_q1",
                    calc_q1_uint(sorted, payload_size_count));
                cJSON_AddNumberToObject(output, "payload_size_q3",
                    calc_q3_uint(sorted, payload_size_count));
                free(sorted);
            }
        }

        if (state->payload_size_diff_stats.count > 0) {
            cJSON_AddNumberToObject(output, "payload_size_diff_mean", state->payload_size_diff_stats.mean);
            cJSON_AddNumberToObject(output, "payload_size_diff_std", running_stats_std(&state->payload_size_diff_stats));
            cJSON_AddNumberToObject(output, "payload_size_diff_min", state->payload_size_diff_stats.min_val);
            cJSON_AddNumberToObject(output, "payload_size_diff_max", state->payload_size_diff_stats.max_val);
            cJSON_AddNumberToObject(output, "payload_size_diff_skew", running_stats_skew(&state->payload_size_diff_stats));
            cJSON_AddNumberToObject(output, "payload_size_diff_kurt", running_stats_kurt(&state->payload_size_diff_stats));

            unsigned int diff_count = 0;
            unsigned int* diffs = (unsigned int*)circular_buffer_get_array(&state->payload_size_diff_seq, &diff_count);
            if (diffs && diff_count > 0) {
                unsigned int* sorted = copy_and_sort_uint(diffs, diff_count);
                if (sorted) {
                    cJSON_AddNumberToObject(output, "payload_size_diff_median", calc_median_uint(sorted, diff_count));
                    cJSON_AddNumberToObject(output, "payload_size_diff_q1", calc_q1_uint(sorted, diff_count));
                    cJSON_AddNumberToObject(output, "payload_size_diff_q3", calc_q3_uint(sorted, diff_count));
                    free(sorted);
                }
                free(diffs);
            }
        }
    }

    // === enable_payload_content: 内容特征 (3 维) ===
    if (cfg->enable_payload_content) {
        if (state->payload_total_bytes > 0) {
            cJSON_AddNumberToObject(output, "payload_printable_ratio",
                (double)state->payload_printable_count / state->payload_total_bytes);
            cJSON_AddNumberToObject(output, "payload_alnum_ratio",
                (double)state->payload_alnum_count / state->payload_total_bytes);
        }
        if (state->total_packets > 0) {
            cJSON_AddNumberToObject(output, "payload_non_empty_ratio",
                (double)state->payload_non_empty_count / state->total_packets);
        }
    }

    // === enable_payload_hist: 字节直方图 + 字节统计 (25 维) ===
    if (cfg->enable_payload_hist) {
        // 字节直方图（16 个 bin，每 bin 16 个值）
        unsigned int total_bytes = 0;
        for (int i = 0; i < 16; i++) {
            total_bytes += state->payload_byte_hist[i];
        }
        for (int i = 0; i < 16; i++) {
            char key[32];
            snprintf(key, sizeof(key), "payload_hist_bin_%d", i);
            cJSON_AddNumberToObject(output, key,
                (total_bytes > 0) ? (double)state->payload_byte_hist[i] / total_bytes : 0);
        }

        // payload 字节值统计（0-255）
        cJSON_AddNumberToObject(output, "payload_byte_mean", state->payload_byte_stats.mean);
        cJSON_AddNumberToObject(output, "payload_byte_std", running_stats_std(&state->payload_byte_stats));
        cJSON_AddNumberToObject(output, "payload_byte_min", state->payload_byte_stats.min_val);
        cJSON_AddNumberToObject(output, "payload_byte_max", state->payload_byte_stats.max_val);
        cJSON_AddNumberToObject(output, "payload_byte_skew", running_stats_skew(&state->payload_byte_stats));
        cJSON_AddNumberToObject(output, "payload_byte_kurt", running_stats_kurt(&state->payload_byte_stats));

        unsigned int byte_total = 0;
        for (int i = 0; i < 256; i++) {
            byte_total += state->payload_byte_hist_full[i];
        }
        if (byte_total > 0) {
            unsigned int cum = 0;
            int p25 = 0, p50 = 0, p75 = 0;
            int has25 = 0, has50 = 0, has75 = 0;
            unsigned int th25 = (unsigned int)(0.25 * (byte_total - 1));
            unsigned int th50 = (unsigned int)(0.50 * (byte_total - 1));
            unsigned int th75 = (unsigned int)(0.75 * (byte_total - 1));
            for (int v = 0; v < 256; v++) {
                cum += state->payload_byte_hist_full[v];
                if (cum > th25 && !has25) { p25 = v; has25 = 1; }
                if (cum > th50 && !has50) { p50 = v; has50 = 1; }
                if (cum > th75 && !has75) {
                    p75 = v;
                    has75 = 1;
                    break;
                }
            }
            cJSON_AddNumberToObject(output, "payload_byte_q1", p25);
            cJSON_AddNumberToObject(output, "payload_byte_median", p50);
            cJSON_AddNumberToObject(output, "payload_byte_q3", p75);
        } else {
            cJSON_AddNumberToObject(output, "payload_byte_q1", 0);
            cJSON_AddNumberToObject(output, "payload_byte_median", 0);
            cJSON_AddNumberToObject(output, "payload_byte_q3", 0);
        }
    }

    // === enable_payload_magic: 魔数/协议检测 (18 维) ===
    if (cfg->enable_payload_magic) {
        cJSON_AddNumberToObject(output, "payload_magic_http_get", state->payload_magic_HTTP_GET);
        cJSON_AddNumberToObject(output, "payload_magic_http_post", state->payload_magic_HTTP_POST);
        cJSON_AddNumberToObject(output, "payload_magic_tls_1_0", state->payload_magic_TLS_1_0);
        cJSON_AddNumberToObject(output, "payload_magic_tls_1_1", state->payload_magic_TLS_1_1);
        cJSON_AddNumberToObject(output, "payload_magic_tls_1_2", state->payload_magic_TLS_1_2);
        cJSON_AddNumberToObject(output, "payload_magic_tls_1_3", state->payload_magic_TLS_1_3);
        cJSON_AddNumberToObject(output, "payload_magic_ssh", state->payload_magic_SSH);
        cJSON_AddNumberToObject(output, "payload_magic_jpeg", state->payload_magic_JPEG);
        cJSON_AddNumberToObject(output, "payload_magic_png", state->payload_magic_PNG);
        cJSON_AddNumberToObject(output, "payload_magic_gif", state->payload_magic_GIF);
        int known_magic = state->payload_magic_HTTP_GET || state->payload_magic_HTTP_POST ||
            state->payload_magic_TLS_1_0 || state->payload_magic_TLS_1_1 || state->payload_magic_TLS_1_2 ||
            state->payload_magic_TLS_1_3 || state->payload_magic_SSH || state->payload_magic_JPEG ||
            state->payload_magic_PNG || state->payload_magic_GIF;
        cJSON_AddNumberToObject(output, "payload_magic_unknown",
            (state->payload_non_empty_count > 0 && !known_magic) ? 1 : 0);

        // 协议启发式识别
        cJSON_AddNumberToObject(output, "payload_looks_like_http", state->payload_looks_like_http);
        cJSON_AddNumberToObject(output, "payload_looks_like_json", state->payload_looks_like_json);
        cJSON_AddNumberToObject(output, "payload_looks_like_xml", state->payload_looks_like_xml);
        cJSON_AddNumberToObject(output, "payload_looks_like_tls", state->payload_looks_like_tls);
        cJSON_AddNumberToObject(output, "payload_looks_like_ssh", state->payload_looks_like_ssh);

        // 卡方值
        double hist_chi_square = calc_chi_square_uniform(state->payload_byte_hist, 16);
        cJSON_AddNumberToObject(output, "payload_chi_square", hist_chi_square);

        // 前 64 字节熵
        unsigned int prefix_n = state->payload_first_bytes_count;
        if (prefix_n > 0) {
            cJSON_AddNumberToObject(output, "payload_first_64_entropy",
                calc_byte_entropy(state->payload_first_bytes, (prefix_n > 64) ? 64 : prefix_n));
        } else {
            cJSON_AddNumberToObject(output, "payload_first_64_entropy", 0);
        }
    }

    // === enable_payload_stats: 每包高级统计 (22 维) ===
    if (cfg->enable_payload_stats) {
        // Payload mod 统计
        const char* mod_keys[] = {
            "payload_length_mod_2", "payload_length_mod_4",
            "payload_length_mod_8", "payload_length_mod_16",
            "payload_length_mod_32", "payload_length_mod_64",
            "payload_length_mod_128", "payload_length_mod_256"
        };
        for (int i = 0; i < 8; i++) {
            char mean_key[64], std_key[64];
            snprintf(mean_key, sizeof(mean_key), "%s_mean", mod_keys[i]);
            snprintf(std_key, sizeof(std_key), "%s_std", mod_keys[i]);
            cJSON_AddNumberToObject(output, mean_key, state->payload_mod[i].mean);
            cJSON_AddNumberToObject(output, std_key, running_stats_std(&state->payload_mod[i]));
        }

        // Payload 高级统计（按包聚合）
        cJSON_AddNumberToObject(output, "payload_entropy", state->payload_entropy_stats.mean);
        cJSON_AddNumberToObject(output, "payload_markov_entropy", state->payload_markov_entropy_stats.mean);
        cJSON_AddNumberToObject(output, "payload_compression_ratio", state->payload_compression_ratio_stats.mean);
        cJSON_AddNumberToObject(output, "payload_autocorrelation_lag1", state->payload_autocorr_lag1_stats.mean);

        // 前缀熵
        unsigned int prefix_n = state->payload_first_bytes_count;
        if (prefix_n > 0) {
            cJSON_AddNumberToObject(output, "payload_first_128_entropy",
                calc_byte_entropy(state->payload_first_bytes, (prefix_n > 128) ? 128 : prefix_n));
            cJSON_AddNumberToObject(output, "payload_first_256_entropy",
                calc_byte_entropy(state->payload_first_bytes, (prefix_n > 256) ? 256 : prefix_n));
        } else {
            cJSON_AddNumberToObject(output, "payload_first_128_entropy", 0);
            cJSON_AddNumberToObject(output, "payload_first_256_entropy", 0);
        }
    }

    if (payload_sizes) {
        free(payload_sizes);
    }
}
