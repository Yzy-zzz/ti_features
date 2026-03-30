#ifndef FEATURE_STATS_H_
#define FEATURE_STATS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ti_features.h"
#include "feature_config.h"

// ============================================================================
// 运行统计结构 - Welford 在线算法
// ============================================================================

typedef struct
{
    double mean;
    double M2;
    double M3;  // 用于偏度
    double M4;  // 用于峰度
    double min_val;
    double max_val;
    unsigned long count;
} running_stats_t;

// 初始化运行统计
static inline void running_stats_init(running_stats_t* s)
{
    s->mean = 0;
    s->M2 = 0;
    s->M3 = 0;
    s->M4 = 0;
    s->min_val = 1e18;
    s->max_val = -1e18;
    s->count = 0;
}

// 更新运行统计
static inline void running_stats_update(running_stats_t* s, double x)
{
    s->count++;
    double delta = x - s->mean;
    s->mean += delta / s->count;
    double delta2 = x - s->mean;
    double delta3 = delta * delta2;

    s->M2 += delta * delta2;
    s->M3 += delta3 * delta2 - 3.0 * delta2 * delta2 * delta / s->count;
    s->M4 += delta3 * delta2 * delta2 - 4.0 * delta3 * delta2 * delta / s->count
             + 6.0 * delta2 * delta2 * delta2 / (s->count * s->count);

    if (x < s->min_val) s->min_val = x;
    if (x > s->max_val) s->max_val = x;
}

// 获取方差
static inline double running_stats_variance(running_stats_t* s)
{
    return (s->count > 1) ? (s->M2 / (s->count - 1)) : 0;
}

// 获取标准差
static inline double running_stats_std(running_stats_t* s)
{
    return sqrt(running_stats_variance(s));
}

// 获取变异系数 CV
static inline double running_stats_cv(running_stats_t* s)
{
    if (s->mean == 0) return 0;
    return running_stats_std(s) / s->mean;
}

// 获取偏度
static inline double running_stats_skew(running_stats_t* s)
{
    if (s->count < 3) return 0;
    double var = running_stats_variance(s);
    if (var == 0) return 0;
    return (s->M3 / s->count) / pow(var, 1.5);
}

// 获取峰度（超额峰度）
static inline double running_stats_kurt(running_stats_t* s)
{
    if (s->count < 4) return 0;
    double var = running_stats_variance(s);
    if (var == 0) return 0;
    return (s->M4 / s->count) / (var * var) - 3.0;
}

// ============================================================================
// 方向分组运行统计
// ============================================================================

typedef struct
{
    running_stats_t fwd;
    running_stats_t bwd;
} directional_running_stats_t;

// 更新方向分组统计
static inline void directional_stats_update(directional_running_stats_t* s,
                                            int direction, double x)
{
    if (direction == DIR_FWD) {
        running_stats_update(&s->fwd, x);
    } else if (direction == DIR_BWD) {
        running_stats_update(&s->bwd, x);
    }
}

// ============================================================================
// 直方图结构（用于分位数近似）
// ============================================================================

#define HIST_BIN_COUNT 1024

typedef struct
{
    unsigned int bins[HIST_BIN_COUNT];
    unsigned int total_count;
    unsigned int min_val;
    unsigned int max_val;
    unsigned int log_scale;  // 1=对数刻度，0=线性刻度
} histogram_t;

// 初始化直方图
static inline void histogram_init(histogram_t* h, unsigned int log_scale)
{
    memset(h->bins, 0, sizeof(h->bins));
    h->total_count = 0;
    h->min_val = 0xFFFFFFFF;
    h->max_val = 0;
    h->log_scale = log_scale;
}

// 添加到直方图
static inline void histogram_add(histogram_t* h, unsigned int x)
{
    unsigned int bin;
    if (h->log_scale) {
        // 对数刻度
        if (x == 0) bin = 0;
        else {
            bin = (unsigned int)(log2(x + 1) * (HIST_BIN_COUNT / 16.0));
            if (bin >= HIST_BIN_COUNT) bin = HIST_BIN_COUNT - 1;
        }
    } else {
        // 线性刻度（需要知道最大值）
        bin = x % HIST_BIN_COUNT;
    }
    h->bins[bin]++;
    h->total_count++;
    if (x < h->min_val) h->min_val = x;
    if (x > h->max_val) h->max_val = x;
}

// ============================================================================
// Bigram 统计结构
// ============================================================================

// Bigram 类型枚举
typedef enum
{
    BIGRAM_SS = 0,  // Small-Small
    BIGRAM_SM = 1,  // Small-Medium
    BIGRAM_SL = 2,  // Small-Large
    BIGRAM_MS = 3,  // Medium-Small
    BIGRAM_MM = 4,  // Medium-Medium
    BIGRAM_ML = 5,  // Medium-Large
    BIGRAM_LS = 6,  // Large-Small
    BIGRAM_LM = 7,  // Large-Medium
    BIGRAM_LL = 8,  // Large-Large
    BIGRAM_COUNT = 9
} bigram_type;

// Bigram 类型名称
// static const char* bigram_names[] = {
//     "SS", "SM", "SL", "MS", "MM", "ML", "LS", "LM", "LL"
// };

typedef struct
{
    unsigned int counts[BIGRAM_COUNT];  // 9 种 bigram 计数
    unsigned int total;
} bigram_stats_t;

// 初始化 Bigram 统计
static inline void bigram_init(bigram_stats_t* b)
{
    memset(b, 0, sizeof(bigram_stats_t));
}

// 获取包大小分类
static inline pkt_size_class get_pkt_size_class(unsigned int len)
{
    ti_feature_config_t* cfg = feat_config_get();
    if (len <= cfg->small_pkt_threshold) return PKT_SIZE_SMALL;
    if (len <= cfg->large_pkt_threshold) return PKT_SIZE_MEDIUM;
    return PKT_SIZE_LARGE;
}

// 更新 Bigram 统计
static inline void bigram_update(bigram_stats_t* b, pkt_size_class prev,
                                 pkt_size_class curr)
{
    int idx = prev * 3 + curr;
    if (idx >= 0 && idx < BIGRAM_COUNT) {
        b->counts[idx]++;
        b->total++;
    }
}

#endif /* FEATURE_STATS_H_ */
