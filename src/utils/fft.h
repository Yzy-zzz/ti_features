#ifndef FFT_H_
#define FFT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// FFT 结果结构
// ============================================================================

#define FFT_MAX_TOP_K 5

typedef struct
{
    double* magnitudes;          // 幅值数组
    double* frequencies;         // 频率数组
    int n;                       // FFT 点数
    int top_k_indices[FFT_MAX_TOP_K];  // Top K 频率索引
    double top_k_mags[FFT_MAX_TOP_K];  // Top K 幅值
    double mag_mean;
    double mag_std;
    double mag_min;
    double mag_max;
    double mag_median;
    double mag_q1;
    double mag_q3;
    double mag_skew;
    double mag_kurt;
} fft_result_t;

// ============================================================================
// 函数声明
// ============================================================================

// 执行 FFT（使用 kissfft 或类似库）
int fft_compute(unsigned int* input, int n, fft_result_t* result);

// 计算 FFT 统计
int fft_compute_statistics(fft_result_t* result);

// 获取 Top K 频率
int fft_get_top_k(fft_result_t* result, int k);

// 释放 FFT 结果
void fft_result_destroy(fft_result_t* result);

// 功率谱密度
double fft_compute_psd_peak(fft_result_t* result);

#endif /* FFT_H_ */
