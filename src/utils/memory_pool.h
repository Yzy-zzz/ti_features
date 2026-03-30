#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ============================================================================
// 内存池结构 - 优化版本：分级分配 + 按需扩容
// 优化目标：减少小流的内存占用，同时支持大流
// ============================================================================

// 流大小分级（根据包数量）
#define FLOW_TINY_THRESHOLD     100     // <= 100 包 (DNS, 握手等)
#define FLOW_SMALL_THRESHOLD    500     // <= 500 包 (HTTP 短连接)
#define FLOW_MEDIUM_THRESHOLD   2000    // <= 2000 包 (普通流)
// > 2000 包为大流（视频、下载等）

// 分级内存配置
#define FLOW_TINY_SEQ_LEN       64      // 微小流序列长度
#define FLOW_SMALL_SEQ_LEN      256     // 小流序列长度
#define FLOW_MEDIUM_SEQ_LEN     1000    // 中等流序列长度
#define FLOW_LARGE_SEQ_LEN      5000    // 大流序列长度

// 环形 buffer 元素类型大小
#define RING_ELEM_SIZE_U32      sizeof(unsigned int)
#define RING_ELEM_SIZE_U64      sizeof(unsigned long long)
#define RING_ELEM_SIZE_I8       sizeof(signed char)

// 内存池区域定义
#define FLOW_MEM_REGION_DYNAMIC     0   // 动态数组区域
#define FLOW_MEM_REGION_TEMP        1   // 临时计算 buffer 区域
#define FLOW_MEM_REGION_COUNT       2

// 流大小级别
typedef enum
{
    FLOW_LEVEL_TINY = 0,
    FLOW_LEVEL_SMALL = 1,
    FLOW_LEVEL_MEDIUM = 2,
    FLOW_LEVEL_LARGE = 3
} flow_level_t;

// 流状态内存池结构
typedef struct
{
    // 动态数组区域（用于 fwd_pkt_lens, bwd_pkt_lens 等）
    unsigned char* dynamic_mem;
    size_t dynamic_mem_size;
    size_t dynamic_mem_offset;

    // 临时计算 buffer（用于特征计算时的临时分配）
    unsigned char* temp_mem;
    size_t temp_mem_size;
    size_t temp_mem_offset;

    // 内嵌的动态数组指针（指向 dynamic_mem 中的位置）
    unsigned int* fwd_pkt_lens;
    unsigned int* bwd_pkt_lens;
    unsigned long long* fwd_iats;
    unsigned long long* bwd_iats;
    unsigned int* fwd_payload_sizes;
    unsigned int* bwd_payload_sizes;

    // 窗口和 burst 数组
    unsigned int* window_pkt_counts;
    unsigned int* window_byte_counts;
    unsigned int* window_bitrate;
    unsigned int* burst_sizes;
    unsigned long long* burst_durations;
    unsigned long long* burst_intervals;
    unsigned int* burst_packet_counts;
    double* resp_delays;

    // 环形 buffer 数据区（内嵌，避免单独分配）
    unsigned char* ring_buffer_data;
    size_t ring_buffer_size;

    // 流级别和实际包数
    flow_level_t level;
    unsigned int actual_seq_len;      // 实际序列长度
    unsigned int current_pkt_count;   // 当前包数

    // 扩容标志
    int needs_upgrade;                // 是否需要升级到更大级别

    // 运行时配置（升级时复用）
    unsigned int num_windows;
    unsigned int resp_delay_capacity;

} flow_mem_pool_t;

// 内存池配置
typedef struct
{
    unsigned int max_seq_len;        // 最大序列长度
    unsigned int num_windows;        // 窗口数量
    unsigned int resp_delay_capacity; // 响应延迟容量
    int enable_tiered_alloc;         // 启用分级分配
} flow_mem_config_t;

// 根据包数量推断流级别
static inline flow_level_t flow_mem_guess_level(unsigned int pkt_count)
{
    if (pkt_count <= FLOW_TINY_THRESHOLD) return FLOW_LEVEL_TINY;
    if (pkt_count <= FLOW_SMALL_THRESHOLD) return FLOW_LEVEL_SMALL;
    if (pkt_count <= FLOW_MEDIUM_THRESHOLD) return FLOW_LEVEL_MEDIUM;
    return FLOW_LEVEL_LARGE;
}

// 获取级别的序列长度
static inline unsigned int flow_mem_level_to_seq_len(flow_level_t level)
{
    switch (level) {
        case FLOW_LEVEL_TINY:   return FLOW_TINY_SEQ_LEN;
        case FLOW_LEVEL_SMALL:  return FLOW_SMALL_SEQ_LEN;
        case FLOW_LEVEL_MEDIUM: return FLOW_MEDIUM_SEQ_LEN;
        case FLOW_LEVEL_LARGE:  return FLOW_LARGE_SEQ_LEN;
        default:                return FLOW_SMALL_SEQ_LEN;
    }
}

// 计算所需内存池大小
static inline size_t flow_mem_pool_calc_size_ex(flow_level_t level,
                                                 unsigned int num_windows,
                                                 unsigned int resp_delay_capacity)
{
    unsigned int seq_len = flow_mem_level_to_seq_len(level);
    size_t size = 0;

    // 方向数组（按级别分配）
    size += seq_len * sizeof(unsigned int);         // fwd_pkt_lens
    size += seq_len * sizeof(unsigned int);         // bwd_pkt_lens
    size += seq_len * sizeof(unsigned long long);   // fwd_iats
    size += seq_len * sizeof(unsigned long long);   // bwd_iats
    size += seq_len * sizeof(unsigned int);         // fwd_payload_sizes
    size += seq_len * sizeof(unsigned int);         // bwd_payload_sizes

    // 窗口数组
    size += num_windows * sizeof(unsigned int);     // window_pkt_counts
    size += num_windows * sizeof(unsigned int);     // window_byte_counts
    size += num_windows * sizeof(unsigned int);     // window_bitrate

    // Burst 数组（按级别分配）
    size += seq_len * sizeof(unsigned int);         // burst_sizes
    size += seq_len * sizeof(unsigned long long);   // burst_durations
    size += seq_len * sizeof(unsigned long long);   // burst_intervals
    size += seq_len * sizeof(unsigned int);         // burst_packet_counts

    // 响应延迟数组
    size += resp_delay_capacity * sizeof(double);

    // 环形 buffer 数据区（14 个序列，按级别和类型计算）
    // 优化：根据实际需要的元素类型大小分配
    size += 5 * seq_len * sizeof(unsigned int);     // pkt_len, udp_len, ip_ttl, ip_tos, payload_size
    size += 4 * seq_len * sizeof(unsigned long long); // iat, ts, payload_size_diff
    size += 1 * seq_len * sizeof(signed char);      // dir_seq
    size += 4 * seq_len * sizeof(unsigned int);     // l3_len, l4_len, payload_len, tcp_window

    // 临时 buffer（固定 8KB）
    size += 8192;

    // 对齐到 64 字节边界
    return (size + 63) & ~63ULL;
}

// 简化版本（保持向后兼容）
static inline size_t flow_mem_pool_calc_size(const flow_mem_config_t* cfg)
{
    // 根据配置推断级别
    flow_level_t level;
    if (cfg->max_seq_len <= FLOW_TINY_SEQ_LEN) level = FLOW_LEVEL_TINY;
    else if (cfg->max_seq_len <= FLOW_SMALL_SEQ_LEN) level = FLOW_LEVEL_SMALL;
    else if (cfg->max_seq_len <= FLOW_MEDIUM_SEQ_LEN) level = FLOW_LEVEL_MEDIUM;
    else level = FLOW_LEVEL_LARGE;

    return flow_mem_pool_calc_size_ex(level, cfg->num_windows, cfg->resp_delay_capacity);
}

// 初始化内存池（分级分配版本）
static inline int flow_mem_pool_init_ex(flow_mem_pool_t* pool,
                                         flow_level_t level,
                                         unsigned int num_windows,
                                         unsigned int resp_delay_capacity)
{
    if (!pool) return -1;

    memset(pool, 0, sizeof(flow_mem_pool_t));

    // 根据级别计算实际序列长度
    unsigned int seq_len = flow_mem_level_to_seq_len(level);
    pool->level = level;
    pool->actual_seq_len = seq_len;
    pool->current_pkt_count = 0;
    pool->needs_upgrade = 0;
    pool->num_windows = num_windows;
    pool->resp_delay_capacity = resp_delay_capacity;

    // 计算该级别所需的内存大小
    size_t total_size = flow_mem_pool_calc_size_ex(level, num_windows, resp_delay_capacity);

    pool->dynamic_mem = (unsigned char*)malloc(total_size);
    if (!pool->dynamic_mem) return -1;

    memset(pool->dynamic_mem, 0, total_size);
    pool->dynamic_mem_size = total_size;
    pool->dynamic_mem_offset = 0;

    // 设置临时计算 buffer（从 dynamic_mem 尾部预留 8KB）
    pool->temp_mem_size = 8192;
    pool->temp_mem = pool->dynamic_mem + total_size - pool->temp_mem_size;
    pool->temp_mem_offset = 0;

    // 在 dynamic_mem 中按顺序分配各数组（使用实际 seq_len）
    #define ALLOC_ARRAY(type, name, count) \
        do { \
            pool->name = (type*)(pool->dynamic_mem + pool->dynamic_mem_offset); \
            pool->dynamic_mem_offset += (count) * sizeof(type); \
        } while(0)

    // 方向数组（按级别分配）
    ALLOC_ARRAY(unsigned int, fwd_pkt_lens, seq_len);
    ALLOC_ARRAY(unsigned int, bwd_pkt_lens, seq_len);
    ALLOC_ARRAY(unsigned long long, fwd_iats, seq_len);
    ALLOC_ARRAY(unsigned long long, bwd_iats, seq_len);
    ALLOC_ARRAY(unsigned int, fwd_payload_sizes, seq_len);
    ALLOC_ARRAY(unsigned int, bwd_payload_sizes, seq_len);

    // 窗口数组
    ALLOC_ARRAY(unsigned int, window_pkt_counts, num_windows);
    ALLOC_ARRAY(unsigned int, window_byte_counts, num_windows);
    ALLOC_ARRAY(unsigned int, window_bitrate, num_windows);

    // Burst 数组（按级别分配）
    ALLOC_ARRAY(unsigned int, burst_sizes, seq_len);
    ALLOC_ARRAY(unsigned long long, burst_durations, seq_len);
    ALLOC_ARRAY(unsigned long long, burst_intervals, seq_len);
    ALLOC_ARRAY(unsigned int, burst_packet_counts, seq_len);

    // 响应延迟数组
    ALLOC_ARRAY(double, resp_delays, resp_delay_capacity);

    #undef ALLOC_ARRAY

    // 环形 buffer 数据区从当前 offset 开始
    pool->ring_buffer_data = pool->dynamic_mem + pool->dynamic_mem_offset;

    // 计算环形 buffer 实际可用大小
    pool->ring_buffer_size = total_size - pool->dynamic_mem_offset - pool->temp_mem_size;

    return 0;
}

// 简化版本（保持向后兼容）
static inline int flow_mem_pool_init(flow_mem_pool_t* pool, const flow_mem_config_t* cfg)
{
    // 根据配置推断级别
    flow_level_t level;
    if (cfg->max_seq_len <= FLOW_TINY_SEQ_LEN) level = FLOW_LEVEL_TINY;
    else if (cfg->max_seq_len <= FLOW_SMALL_SEQ_LEN) level = FLOW_LEVEL_SMALL;
    else if (cfg->max_seq_len <= FLOW_MEDIUM_SEQ_LEN) level = FLOW_LEVEL_MEDIUM;
    else level = FLOW_LEVEL_LARGE;

    return flow_mem_pool_init_ex(pool, level, cfg->num_windows, cfg->resp_delay_capacity);
}

// 检查是否需要升级流级别
static inline int flow_mem_pool_check_upgrade(flow_mem_pool_t* pool)
{
    if (!pool) return 0;

    // LARGE 级别不再升级，避免无意义的计数增长。
    if (pool->level >= FLOW_LEVEL_LARGE) {
        pool->needs_upgrade = 0;
        return 0;
    }

    // 饱和递增，避免长连接场景下无符号回绕。
    if (pool->current_pkt_count < (unsigned int)-1) {
        pool->current_pkt_count++;
    }

    // 检查当前包数是否接近容量限制（80% 阈值触发升级）
    unsigned int threshold = (pool->actual_seq_len * 8) / 10;
    if (threshold == 0) threshold = 1;

    if (pool->current_pkt_count >= threshold) {
        // 需要升级到更大的级别
        flow_level_t next_level = (flow_level_t)((int)pool->level + 1);
        if (next_level <= FLOW_LEVEL_LARGE) {
            pool->needs_upgrade = 1;
            return 1;  // 需要扩容
        }
    }
    return 0;
}

// 销毁内存池
static inline void flow_mem_pool_destroy(flow_mem_pool_t* pool)
{
    if (pool && pool->dynamic_mem) {
        free(pool->dynamic_mem);
        pool->dynamic_mem = NULL;
    }
    pool->temp_mem = NULL;
    pool->dynamic_mem_size = 0;
    pool->dynamic_mem_offset = 0;
    pool->temp_mem_offset = 0;
    pool->ring_buffer_data = NULL;
    pool->ring_buffer_size = 0;
}

// 从内存池分配临时 buffer（用于特征计算）
static inline void* flow_mem_pool_alloc_temp(flow_mem_pool_t* pool, size_t size, size_t alignment)
{
    if (!pool || size == 0) return NULL;

    // 对齐 offset
    size_t offset = pool->temp_mem_offset;
    if (alignment > 1) {
        offset = (offset + alignment - 1) & ~(alignment - 1);
    }

    if (offset + size > pool->temp_mem_size) {
        // 临时 buffer 不足，重置后从头开始
        pool->temp_mem_offset = 0;
        offset = 0;
    }

    void* ptr = pool->temp_mem + offset;
    pool->temp_mem_offset = offset + size;
    return ptr;
}

// 重置临时分配 offset（每轮特征计算后调用）
static inline void flow_mem_pool_reset_temp(flow_mem_pool_t* pool)
{
    if (pool) {
        pool->temp_mem_offset = 0;
    }
}

// 获取环形 buffer 数据区的偏移位置
static inline void* flow_mem_pool_get_ring_buffer(flow_mem_pool_t* pool, size_t offset)
{
    if (!pool || offset >= pool->ring_buffer_size) return NULL;
    return pool->ring_buffer_data + offset;
}

// ============================================================================
// 流扩容函数 - 升级到更大的内存级别
// ============================================================================

// 扩容内存池到更大的级别
static inline int flow_mem_pool_upgrade(flow_mem_pool_t* pool, flow_level_t new_level)
{
    if (!pool || new_level <= pool->level) return -1;

    unsigned int old_seq_len = pool->actual_seq_len;
    unsigned int new_seq_len = flow_mem_level_to_seq_len(new_level);
    unsigned int copy_seq_len = (old_seq_len < new_seq_len) ? old_seq_len : new_seq_len;

    // 计算新大小
    size_t new_size = flow_mem_pool_calc_size_ex(new_level, pool->num_windows,
                                                 pool->resp_delay_capacity);
    unsigned char* new_mem = (unsigned char*)malloc(new_size);
    if (!new_mem) return -1;

    memset(new_mem, 0, new_size);

    // 保存旧内存与旧数组指针
    unsigned char* old_mem = pool->dynamic_mem;
    unsigned int* old_fwd_pkt_lens = pool->fwd_pkt_lens;
    unsigned int* old_bwd_pkt_lens = pool->bwd_pkt_lens;
    unsigned long long* old_fwd_iats = pool->fwd_iats;
    unsigned long long* old_bwd_iats = pool->bwd_iats;
    unsigned int* old_fwd_payload_sizes = pool->fwd_payload_sizes;
    unsigned int* old_bwd_payload_sizes = pool->bwd_payload_sizes;
    unsigned int* old_window_pkt_counts = pool->window_pkt_counts;
    unsigned int* old_window_byte_counts = pool->window_byte_counts;
    unsigned int* old_window_bitrate = pool->window_bitrate;
    unsigned int* old_burst_sizes = pool->burst_sizes;
    unsigned long long* old_burst_durations = pool->burst_durations;
    unsigned long long* old_burst_intervals = pool->burst_intervals;
    unsigned int* old_burst_packet_counts = pool->burst_packet_counts;
    double* old_resp_delays = pool->resp_delays;
    unsigned char* old_ring_data = pool->ring_buffer_data;
    size_t old_ring_size = pool->ring_buffer_size;

    // 在新内存中按 init_ex 相同布局重建指针
    size_t offset = 0;

    // 辅助宏：分配并按最小长度复制序列数组
    #define UPGRADE_SEQ_ARRAY(type, field) \
        do { \
            pool->field = (type*)(new_mem + offset); \
            if (old_##field && copy_seq_len > 0) { \
                memcpy(pool->field, old_##field, copy_seq_len * sizeof(type)); \
            } \
            offset += new_seq_len * sizeof(type); \
        } while(0)

    // 辅助宏：分配并按固定长度复制数组
    #define UPGRADE_FIXED_ARRAY(type, field, count_value) \
        do { \
            pool->field = (type*)(new_mem + offset); \
            if (old_##field && (count_value) > 0) { \
                memcpy(pool->field, old_##field, (count_value) * sizeof(type)); \
            } \
            offset += (count_value) * sizeof(type); \
        } while(0)

    UPGRADE_SEQ_ARRAY(unsigned int, fwd_pkt_lens);
    UPGRADE_SEQ_ARRAY(unsigned int, bwd_pkt_lens);
    UPGRADE_SEQ_ARRAY(unsigned long long, fwd_iats);
    UPGRADE_SEQ_ARRAY(unsigned long long, bwd_iats);
    UPGRADE_SEQ_ARRAY(unsigned int, fwd_payload_sizes);
    UPGRADE_SEQ_ARRAY(unsigned int, bwd_payload_sizes);

    UPGRADE_FIXED_ARRAY(unsigned int, window_pkt_counts, pool->num_windows);
    UPGRADE_FIXED_ARRAY(unsigned int, window_byte_counts, pool->num_windows);
    UPGRADE_FIXED_ARRAY(unsigned int, window_bitrate, pool->num_windows);

    UPGRADE_SEQ_ARRAY(unsigned int, burst_sizes);
    UPGRADE_SEQ_ARRAY(unsigned long long, burst_durations);
    UPGRADE_SEQ_ARRAY(unsigned long long, burst_intervals);
    UPGRADE_SEQ_ARRAY(unsigned int, burst_packet_counts);

    UPGRADE_FIXED_ARRAY(double, resp_delays, pool->resp_delay_capacity);

    #undef UPGRADE_SEQ_ARRAY
    #undef UPGRADE_FIXED_ARRAY

    // 重建 ring buffer 区域
    pool->dynamic_mem_offset = offset;
    pool->ring_buffer_data = new_mem + offset;
    pool->temp_mem_size = 8192;
    pool->ring_buffer_size = new_size - offset - pool->temp_mem_size;

    if (old_ring_data && old_ring_size > 0 && pool->ring_buffer_size > 0) {
        size_t ring_copy_size = (old_ring_size < pool->ring_buffer_size)
                                ? old_ring_size : pool->ring_buffer_size;
        memcpy(pool->ring_buffer_data, old_ring_data, ring_copy_size);
    }

    // 更新池指针
    pool->dynamic_mem = new_mem;
    pool->dynamic_mem_size = new_size;
    pool->level = new_level;
    pool->actual_seq_len = new_seq_len;
    pool->needs_upgrade = 0;

    // 重新计算临时 buffer 位置
    pool->temp_mem = new_mem + new_size - pool->temp_mem_size;
    pool->temp_mem_offset = 0;

    // 释放旧内存
    free(old_mem);

    return 0;
}

// ============================================================================
// 数组访问辅助宏 - 通过偏移量访问内存池中的数组
// ============================================================================

// 从内存池获取 uint 数组指针
static inline unsigned int* flow_mem_get_uint_array(flow_mem_pool_t* pool, size_t offset, unsigned int count)
{
    if (!pool || offset >= pool->dynamic_mem_size) return NULL;
    return (unsigned int*)(pool->dynamic_mem + offset);
}

// 从内存池获取 ulonglong 数组指针
static inline unsigned long long* flow_mem_get_ull_array(flow_mem_pool_t* pool, size_t offset, size_t max_offset)
{
    if (!pool || offset >= max_offset) return NULL;
    return (unsigned long long*)(pool->dynamic_mem + offset);
}

// 从内存池获取 double 数组指针
static inline double* flow_mem_get_double_array(flow_mem_pool_t* pool, size_t offset, size_t max_offset)
{
    if (!pool || offset >= max_offset) return NULL;
    return (double*)(pool->dynamic_mem + offset);
}

#endif /* MEMORY_POOL_H_ */
