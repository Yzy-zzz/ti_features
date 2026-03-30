#include "utils/fft.h"
#include <limits.h>

// 简化的 FFT 实现（实际项目可使用 kissfft 或 FFmpeg）
// 这里提供框架，实际使用时需要替换为真正的 FFT 库

static int compare_double(const void* a, const void* b)
{
    const double da = *(const double*)a;
    const double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

// 蝴蝶运算
static int fft_butterfly(double* real, double* imag, int n)
{
    if (!real || !imag || n <= 0) {
        return -1;
    }

    // 简化的 DFT 实现（O(n^2)，仅用于框架演示）
    // 实际应使用 Cooley-Tukey FFT 算法（O(n log n)）

    double* temp_real = (double*)malloc(n * sizeof(double));
    double* temp_imag = (double*)malloc(n * sizeof(double));

    if (!temp_real || !temp_imag) {
        free(temp_real);
        free(temp_imag);
        return -1;
    }

    const double PI = 3.14159265358979323846;

    for (int k = 0; k < n; k++) {
        temp_real[k] = 0;
        temp_imag[k] = 0;
        for (int t = 0; t < n; t++) {
            double angle = 2 * PI * k * t / n;
            temp_real[k] += real[t] * cos(angle) + imag[t] * sin(angle);
            temp_imag[k] += imag[t] * cos(angle) - real[t] * sin(angle);
        }
    }

    memcpy(real, temp_real, n * sizeof(double));
    memcpy(imag, temp_imag, n * sizeof(double));

    free(temp_real);
    free(temp_imag);

    return 0;
}

// 计算 FFT
int fft_compute(unsigned int* input, int n, fft_result_t* result)
{
    if (!input || n <= 0 || !result) {
        return -1;
    }

    memset(result, 0, sizeof(*result));

    // 确保 n 是 2 的幂
    int fft_size = 1;
    while (fft_size < n) {
        if (fft_size > INT_MAX / 2) {
            return -1;
        }
        fft_size *= 2;
    }

    // 分配并初始化输入
    double* real = (double*)calloc(fft_size, sizeof(double));
    double* imag = (double*)calloc(fft_size, sizeof(double));

    if (!real || !imag) {
        free(real);
        free(imag);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        real[i] = (double)input[i];
    }

    // 执行 FFT（这里调用简化实现，实际应使用 kissfft）
    // kissfft 调用示例:
    // kissfft_cfg cfg = kissfft_alloc(fft_size, 0, NULL, NULL);
    // kf_fft(cfg, (kiss_fft_cpx*)real, (kiss_fft_cpx*)real);
    // kissfft_free(cfg);

    if (fft_butterfly(real, imag, fft_size) != 0) {
        free(real);
        free(imag);
        return -1;
    }

    // 计算幅值
    result->n = fft_size / 2;  // 只取前半部分（奈奎斯特频率）
    if (result->n <= 0) {
        free(real);
        free(imag);
        return -1;
    }

    result->magnitudes = (double*)malloc(result->n * sizeof(double));
    result->frequencies = (double*)malloc(result->n * sizeof(double));

    if (!result->magnitudes || !result->frequencies) {
        fft_result_destroy(result);
        free(real);
        free(imag);
        return -1;
    }

    for (int i = 0; i < result->n; i++) {
        result->magnitudes[i] = sqrt(real[i] * real[i] + imag[i] * imag[i]);
        result->frequencies[i] = (double)i / fft_size;  // 归一化频率
    }

    // 计算统计
    if (fft_compute_statistics(result) != 0) {
        fft_result_destroy(result);
        free(real);
        free(imag);
        return -1;
    }

    free(real);
    free(imag);

    return 0;
}

// 计算 FFT 统计
int fft_compute_statistics(fft_result_t* result)
{
    if (!result || !result->magnitudes || result->n <= 0) {
        return -1;
    }

    int n = result->n;
    double sum = 0, sum_sq = 0, sum_cube = 0, sum_quad = 0;
    double min_val = result->magnitudes[0];
    double max_val = result->magnitudes[0];

    // 排序用于分位数
    double* sorted = (double*)malloc(n * sizeof(double));
    if (!sorted) {
        return -1;
    }

    memcpy(sorted, result->magnitudes, n * sizeof(double));
    qsort(sorted, n, sizeof(double), compare_double);

    for (int i = 0; i < n; i++) {
        double v = result->magnitudes[i];
        sum += v;
        sum_sq += v * v;
        sum_cube += v * v * v;
        sum_quad += v * v * v * v;
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }

    double mean = sum / n;
    double variance = (n > 1) ? (sum_sq - n * mean * mean) / (n - 1) : 0;
    if (variance < 0) {
        variance = 0;
    }

    double std = sqrt(variance);

    result->mag_mean = mean;
    result->mag_std = std;
    result->mag_min = min_val;
    result->mag_max = max_val;
    result->mag_median = sorted[n / 2];
    result->mag_q1 = sorted[n / 4];
    result->mag_q3 = sorted[3 * n / 4];

    // 偏度
    if (std > 0) {
        double m3 = (sum_cube / n) - 3 * mean * sum_sq / n + 2 * mean * mean * mean;
        result->mag_skew = m3 / (std * std * std);
    } else {
        result->mag_skew = 0;
    }

    // 峰度
    if (variance > 0) {
        double m4 = (sum_quad / n) - 4 * mean * sum_cube / n
                    + 6 * mean * mean * sum_sq / n - 3 * mean * mean * mean * mean;
        result->mag_kurt = m4 / (variance * variance) - 3;
    } else {
        result->mag_kurt = 0;
    }

    free(sorted);

    return 0;
}

// 获取 Top K 频率
int fft_get_top_k(fft_result_t* result, int k)
{
    if (!result || !result->magnitudes || result->n <= 1 || k <= 0 || k > FFT_MAX_TOP_K) {
        return -1;
    }

    // 初始化
    for (int i = 0; i < k; i++) {
        result->top_k_indices[i] = -1;
        result->top_k_mags[i] = -1;
    }

    // 查找 Top K
    for (int i = 1; i < result->n; i++) {  // 跳过 DC 分量
        double mag = result->magnitudes[i];
        for (int j = 0; j < k; j++) {
            if (mag > result->top_k_mags[j]) {
                // 插入
                for (int l = k - 1; l > j; l--) {
                    result->top_k_indices[l] = result->top_k_indices[l-1];
                    result->top_k_mags[l] = result->top_k_mags[l-1];
                }
                result->top_k_indices[j] = i;
                result->top_k_mags[j] = mag;
                break;
            }
        }
    }

    return 0;
}

// 释放 FFT 结果
void fft_result_destroy(fft_result_t* result)
{
    if (result) {
        if (result->magnitudes) {
            free(result->magnitudes);
            result->magnitudes = NULL;
        }
        if (result->frequencies) {
            free(result->frequencies);
            result->frequencies = NULL;
        }
    }
}

// 计算功率谱密度峰值
double fft_compute_psd_peak(fft_result_t* result)
{
    if (!result || !result->magnitudes || result->n <= 1) {
        return 0;
    }

    double peak = 0;
    for (int i = 1; i < result->n; i++) {  // 跳过 DC
        double psd = result->magnitudes[i] * result->magnitudes[i];
        if (psd > peak) {
            peak = psd;
        }
    }
    return peak;
}
