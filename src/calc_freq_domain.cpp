#include "calc_freq_domain.h"
#include "feature_config.h"
#include "utils/fft.h"
#include "utils/stats_utils.h"

// 计算全量包长序列 FFT
int calc_fft_all(flow_feature_state_t* state, fft_result_t* result)
{
    unsigned int count;
    unsigned int* seq = (unsigned int*)circular_buffer_get_array(
        &state->pkt_len_seq, &count);

    if (!seq || count < 2) {
        if (seq) free(seq);
        return -1;
    }

    int ret = fft_compute(seq, count, result);
    free(seq);
    return ret;
}

// 计算前向包 FFT
int calc_fft_fwd(flow_feature_state_t* state, fft_result_t* result)
{
    if (state->fwd_pkt_len_count < 2) {
        return -1;
    }
    return fft_compute(state->fwd_pkt_lens, state->fwd_pkt_len_count, result);
}

// 计算后向包 FFT
int calc_fft_bwd(flow_feature_state_t* state, fft_result_t* result)
{
    if (state->bwd_pkt_len_count < 2) {
        return -1;
    }
    return fft_compute(state->bwd_pkt_lens, state->bwd_pkt_len_count, result);
}

// 辅助函数：输出 FFT 结果到 JSON
static void output_fft_result(fft_result_t* result, cJSON* output, const char* prefix)
{
    // Top K 频率
    for (int i = 0; i < 5; i++) {
        char freq_key[64], mag_key[64];
        snprintf(freq_key, sizeof(freq_key), "%s_fft_freq_%d_idx", prefix, i);
        snprintf(mag_key, sizeof(mag_key), "%s_fft_mag_%d", prefix, i);
        cJSON_AddNumberToObject(output, freq_key, result->top_k_indices[i]);
        cJSON_AddNumberToObject(output, mag_key, result->top_k_mags[i]);
    }

    // FFT 幅值统计（使用 prefix 参数生成 key）
    char key_buf[64];
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_mean", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_mean);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_std", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_std);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_min", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_min);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_max", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_max);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_median", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_median);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_q1", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_q1);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_q3", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_q3);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_skew", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_skew);
    snprintf(key_buf, sizeof(key_buf), "%s_fft_magnitude_kurt", prefix);
    cJSON_AddNumberToObject(output, key_buf, result->mag_kurt);

    // 功率谱密度峰值
    cJSON_AddNumberToObject(output, "psd_peak", fft_compute_psd_peak(result));
}

// 计算 FFT 派生特征
void calc_derived_fft(flow_feature_state_t* state, cJSON* output)
{
    ti_feature_config_t* cfg = feat_config_get();
    fft_result_t result;

    // === enable_fft_global: 全局 FFT (20 维) ===
    if (cfg->enable_fft_global) {
        memset(&result, 0, sizeof(fft_result_t));
        if (calc_fft_all(state, &result) == 0) {
            fft_get_top_k(&result, 5);
            output_fft_result(&result, output, "all");
        }
        fft_result_destroy(&result);
    }

    // === enable_fft_fwd: 前向 FFT (19 维) ===
    if (cfg->enable_fft_fwd) {
        memset(&result, 0, sizeof(fft_result_t));
        if (calc_fft_fwd(state, &result) == 0) {
            fft_get_top_k(&result, 5);

            for (int i = 0; i < 5; i++) {
                char freq_key[64], mag_key[64];
                snprintf(freq_key, sizeof(freq_key), "fwd_fft_freq_%d_idx", i);
                snprintf(mag_key, sizeof(mag_key), "fwd_fft_mag_%d", i);
                cJSON_AddNumberToObject(output, freq_key, result.top_k_indices[i]);
                cJSON_AddNumberToObject(output, mag_key, result.top_k_mags[i]);
            }

            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_mean", result.mag_mean);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_std", result.mag_std);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_min", result.mag_min);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_max", result.mag_max);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_median", result.mag_median);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_q1", result.mag_q1);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_q3", result.mag_q3);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_skew", result.mag_skew);
            cJSON_AddNumberToObject(output, "fwd_fft_magnitude_kurt", result.mag_kurt);
        }
        fft_result_destroy(&result);
    }

    // === enable_fft_bwd: 后向 FFT (19 维) ===
    if (cfg->enable_fft_bwd) {
        memset(&result, 0, sizeof(fft_result_t));
        if (calc_fft_bwd(state, &result) == 0) {
            fft_get_top_k(&result, 5);

            for (int i = 0; i < 5; i++) {
                char freq_key[64], mag_key[64];
                snprintf(freq_key, sizeof(freq_key), "bwd_fft_freq_%d_idx", i);
                snprintf(mag_key, sizeof(mag_key), "bwd_fft_mag_%d", i);
                cJSON_AddNumberToObject(output, freq_key, result.top_k_indices[i]);
                cJSON_AddNumberToObject(output, mag_key, result.top_k_mags[i]);
            }

            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_mean", result.mag_mean);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_std", result.mag_std);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_min", result.mag_min);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_max", result.mag_max);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_median", result.mag_median);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_q1", result.mag_q1);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_q3", result.mag_q3);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_skew", result.mag_skew);
            cJSON_AddNumberToObject(output, "bwd_fft_magnitude_kurt", result.mag_kurt);
        }
        fft_result_destroy(&result);
    }
}

// 计算功率谱密度峰值
double calc_psd_peak(fft_result_t* result)
{
    return fft_compute_psd_peak(result);
}
