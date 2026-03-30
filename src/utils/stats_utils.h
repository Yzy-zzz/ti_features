#ifndef STATS_UTILS_H_
#define STATS_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "circular_buffer.h"

// ============================================================================
// 统计工具函数
// 提供分位数、熵、直方图等通用统计计算
// ============================================================================

// 比较函数（用于 qsort）
static inline int compare_double(const void* a, const void* b)
{
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static inline int compare_uint(const void* a, const void* b)
{
    unsigned int ua = *(const unsigned int*)a;
    unsigned int ub = *(const unsigned int*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

// 计算中位数（需要排序后的数组）
static inline double calc_median_double(double* arr, int n)
{
    if (n <= 0) return 0;
    if (n == 1) return arr[0];

    if (n % 2 == 0) {
        return (arr[n/2 - 1] + arr[n/2]) / 2.0;
    } else {
        return arr[n/2];
    }
}

static inline unsigned int calc_median_uint(unsigned int* arr, int n)
{
    if (n <= 0) return 0;
    if (n == 1) return arr[0];

    if (n % 2 == 0) {
        return (arr[n/2 - 1] + arr[n/2]) / 2;
    } else {
        return arr[n/2];
    }
}

// 计算百分位数（需要排序后的数组）
// p: 0-100 之间的值
static inline double calc_percentile_double(double* arr, int n, double p)
{
    if (n <= 0) return 0;
    if (n == 1) return arr[0];
    if (p <= 0) return arr[0];
    if (p >= 100) return arr[n-1];

    // 线性插值
    double idx = (p / 100.0) * (n - 1);
    int lower = (int)idx;
    int upper = lower + 1;
    double frac = idx - lower;

    if (upper >= n) return arr[n-1];

    return arr[lower] * (1.0 - frac) + arr[upper] * frac;
}

static inline unsigned int calc_percentile_uint(unsigned int* arr, int n, double p)
{
    if (n <= 0) return 0;
    if (n == 1) return arr[0];
    if (p <= 0) return arr[0];
    if (p >= 100) return arr[n-1];

    double idx = (p / 100.0) * (n - 1);
    int lower = (int)idx;
    int upper = lower + 1;
    double frac = idx - lower;

    if (upper >= n) return arr[n-1];

    return (unsigned int)(arr[lower] * (1.0 - frac) + arr[upper] * frac);
}

// 计算 Q1, Q3
#define calc_q1_double(arr, n) calc_percentile_double(arr, n, 25.0)
#define calc_q3_double(arr, n) calc_percentile_double(arr, n, 75.0)
#define calc_q1_uint(arr, n) calc_percentile_uint(arr, n, 25.0)
#define calc_q3_uint(arr, n) calc_percentile_uint(arr, n, 75.0)

// 计算偏度（需要均值和标准差）
static inline double calc_skewness(double* arr, int n, double mean, double std)
{
    if (n < 3 || std == 0) return 0;

    double sum = 0;
    for (int i = 0; i < n; i++) {
        double z = (arr[i] - mean) / std;
        sum += z * z * z;
    }
    return sum / n;
}

// 计算峰度（需要均值和标准差）
static inline double calc_kurtosis(double* arr, int n, double mean, double std)
{
    if (n < 4 || std == 0) return 0;

    double sum = 0;
    for (int i = 0; i < n; i++) {
        double z = (arr[i] - mean) / std;
        sum += z * z * z * z;
    }
    return (sum / n) - 3.0;  // 超额峰度
}

// 计算变异系数
static inline double calc_cv(double mean, double std)
{
    if (mean == 0) return 0;
    return std / mean;
}

// 计算香农熵
static inline double calc_shannon_entropy(unsigned int* hist, int n)
{
    unsigned long total = 0;
    for (int i = 0; i < n; i++) {
        total += hist[i];
    }

    if (total == 0) return 0;

    double entropy = 0;
    for (int i = 0; i < n; i++) {
        if (hist[i] > 0) {
            double p = (double)hist[i] / total;
            entropy -= p * log2(p);
        }
    }

    return entropy;
}

// 计算卡方统计量（与均匀分布比较）
static inline double calc_chi_square_uniform(unsigned int* hist, int n)
{
    unsigned long total = 0;
    for (int i = 0; i < n; i++) {
        total += hist[i];
    }

    if (total == 0) return 0;

    double expected = (double)total / n;
    double chi_sq = 0;

    for (int i = 0; i < n; i++) {
        double diff = hist[i] - expected;
        chi_sq += (diff * diff) / expected;
    }

    return chi_sq;
}

// 复制并排序数组（不修改原数组）
static inline double* copy_and_sort_double(double* arr, int n)
{
    if (n <= 0 || !arr) return NULL;

    double* sorted = (double*)malloc(n * sizeof(double));
    if (!sorted) return NULL;

    memcpy(sorted, arr, n * sizeof(double));
    qsort(sorted, n, sizeof(double), compare_double);

    return sorted;
}

static inline unsigned int* copy_and_sort_uint(unsigned int* arr, int n)
{
    if (n <= 0 || !arr) return NULL;

    unsigned int* sorted = (unsigned int*)malloc(n * sizeof(unsigned int));
    if (!sorted) return NULL;

    memcpy(sorted, arr, n * sizeof(unsigned int));
    qsort(sorted, n, sizeof(unsigned int), compare_uint);

    return sorted;
}

// 从 circular_buffer 获取并排序数组
static inline double* get_sorted_from_buffer(circular_buffer_t* cb, int* out_count)
{
    if (!cb || !out_count) return NULL;

    unsigned int count = 0;
    double* arr = (double*)circular_buffer_get_array(cb, &count);
    if (!arr) {
        *out_count = 0;
        return NULL;
    }

    // 转换为 double 并排序
    double* sorted = (double*)malloc(count * sizeof(double));
    if (!sorted) {
        free(arr);
        *out_count = 0;
        return NULL;
    }

    for (unsigned int i = 0; i < count; i++) {
        sorted[i] = (double)((unsigned int*)arr)[i];
    }
    free(arr);

    qsort(sorted, count, sizeof(double), compare_double);
    *out_count = count;

    return sorted;
}

#endif /* STATS_UTILS_H_ */
