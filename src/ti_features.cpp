#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <cmath>
#include <math.h>
#include <dlfcn.h>
#include <MESA/stream.h>
#include <MESA/MESA_prof_load.h>
#include <MESA/MESA_handle_logger.h>
#include <MESA/field_stat2.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <search.h>

#include "ti_features.h"
#include "feature_config.h"
#include "feat_state.h"
#include "calc_basic.h"
#include "calc_iat.h"
#include "calc_burst.h"
#include "calc_freq_domain.h"
#include "calc_sequence.h"
#include "calc_payload.h"
#include "calc_protocol.h"
#include "calc_behavior.h"
#include "calc_window.h"

// 模块标识符用于日志
#define TI_FEATURES "ti_features"

#define GIT_VERSION_CATTER(v) __attribute__((__used__)) const char * GIT_VERSION_##v = NULL
#define GIT_VERSION_EXPAND(v) GIT_VERSION_CATTER(v)

/* VERSION TAG */
#ifdef GIT_VERSION
GIT_VERSION_EXPAND(GIT_VERSION);
#else
static __attribute__((__used__)) const char * GIT_VERSION_UNKNOWN = NULL;
#endif
#undef GIT_VERSION_CATTER
#undef GIT_VERSION_EXPAND

// ============================================================================
// 流状态管理函数实现 - 内存池优化版本（分级分配）
// ============================================================================

typedef struct {
    void* data;
    unsigned int count;
    size_t elem_size;
} ring_buffer_snapshot_t;

static inline void ring_buffer_snapshot_capture(circular_buffer_t* cb,
                                                ring_buffer_snapshot_t* snap)
{
    if (!snap) return;

    snap->data = NULL;
    snap->count = 0;
    snap->elem_size = cb ? cb->elem_size : 0;

    if (!cb || cb->count == 0) {
        return;
    }

    snap->data = circular_buffer_get_array(cb, &snap->count);
    if (!snap->data) {
        snap->count = 0;
    }
}

static inline void ring_buffer_snapshot_release(ring_buffer_snapshot_t* snap)
{
    if (!snap) return;
    if (snap->data) {
        free(snap->data);
        snap->data = NULL;
    }
    snap->count = 0;
    snap->elem_size = 0;
}

static inline int ring_buffer_rebind_from_snapshot(circular_buffer_t* cb,
                                                   flow_mem_pool_t* pool,
                                                   size_t* ring_offset,
                                                   unsigned int capacity,
                                                   size_t elem_size,
                                                   const ring_buffer_snapshot_t* snap)
{
    if (!cb || !pool || !ring_offset) return -1;

    void* ring_data = flow_mem_pool_get_ring_buffer(pool, *ring_offset);
    if (!ring_data) return -1;

    cb->data = ring_data;
    cb->elem_size = elem_size;
    cb->capacity = capacity;
    cb->count = 0;
    cb->head = 0;
    cb->tail = 0;
    cb->is_full = 0;

    *ring_offset += (size_t)capacity * elem_size;

    if (snap && snap->data && snap->count > 0 && snap->elem_size == elem_size) {
        unsigned int copy_count = (snap->count < capacity) ? snap->count : capacity;
        memcpy(cb->data, snap->data, (size_t)copy_count * elem_size);
        cb->count = copy_count;
        cb->tail = 0;
        cb->head = copy_count % capacity;
        cb->is_full = (copy_count == capacity) ? 1 : 0;
    }

    return 0;
}

int flow_state_init(flow_feature_state_t* state)
{
    if (!state) return -1;

    memset(state, 0, sizeof(flow_feature_state_t));

    unsigned int seq_len = 0;
    size_t ring_offset = 0;
    size_t elem_size = 0;

    // 初始化运行统计
    running_stats_init(&state->pkt_len_stats);
    running_stats_init(&state->iat_stats);
    running_stats_init(&state->payload_size_stats);
    running_stats_init(&state->fwd_payload_stats);
    running_stats_init(&state->bwd_payload_stats);
    running_stats_init(&state->payload_byte_stats);
    running_stats_init(&state->ip_ttl_stats);
    running_stats_init(&state->ip_tos_stats);
    running_stats_init(&state->tcp_window_stats);
    running_stats_init(&state->udp_len_stats);
    running_stats_init(&state->payload_entropy_stats);
    running_stats_init(&state->payload_markov_entropy_stats);
    running_stats_init(&state->payload_compression_ratio_stats);
    running_stats_init(&state->payload_autocorr_lag1_stats);
    running_stats_init(&state->payload_size_diff_stats);

    // 初始化 payload mod 统计
    for (int i = 0; i < 8; i++) {
        running_stats_init(&state->payload_mod[i]);
    }

    // 初始化 payload 直方图
    memset(state->payload_byte_hist, 0, sizeof(state->payload_byte_hist));
    memset(state->payload_byte_hist_full, 0, sizeof(state->payload_byte_hist_full));

    // 初始化 Bigram 统计
    bigram_init(&state->bigram_stats);
    state->last_pkt_size_class = PKT_SIZE_SMALL;

    // 获取配置
    ti_feature_config_t* cfg = feat_config_get();
    const unsigned int pool_num_windows = 100;
    const unsigned int pool_resp_delay_capacity = 1000;

    // 【优化 1】分级内存池：新流从最小级别开始（TINY=64 包）
    // 内存占用：~12KB/流（vs 之前 ~890KB/流）
    state->mem_pool = (flow_mem_pool_t*)malloc(sizeof(flow_mem_pool_t));
    if (!state->mem_pool) goto cleanup;

    // 从 TINY 级别开始，按需扩容
    if (flow_mem_pool_init_ex(state->mem_pool, FLOW_LEVEL_TINY,
                              pool_num_windows, pool_resp_delay_capacity) != 0) {
        free(state->mem_pool);
        state->mem_pool = NULL;
        goto cleanup;
    }

    // 设置临时 buffer 指针（用于特征计算时复用）
    seq_len = state->mem_pool->actual_seq_len;
    state->temp_double_buffer = (double*)flow_mem_pool_alloc_temp(state->mem_pool,
        seq_len * sizeof(double) * 2, 8);
    state->temp_uint_buffer_size = seq_len;
    state->temp_double_buffer_2 = state->temp_double_buffer + seq_len;

    // 从内存池获取各数组指针
    state->fwd_pkt_lens = state->mem_pool->fwd_pkt_lens;
    state->bwd_pkt_lens = state->mem_pool->bwd_pkt_lens;
    state->fwd_pkt_len_capacity = seq_len;
    state->bwd_pkt_len_capacity = seq_len;
    state->fwd_pkt_len_count = 0;
    state->bwd_pkt_len_count = 0;

    // 方向过滤的 IAT 数组
    state->fwd_iats = state->mem_pool->fwd_iats;
    state->bwd_iats = state->mem_pool->bwd_iats;
    state->fwd_iat_capacity = seq_len;
    state->bwd_iat_capacity = seq_len;
    state->fwd_iat_count = 0;
    state->bwd_iat_count = 0;

    // 方向 payload 数组
    state->fwd_payload_sizes = state->mem_pool->fwd_payload_sizes;
    state->bwd_payload_sizes = state->mem_pool->bwd_payload_sizes;
    state->fwd_payload_capacity = seq_len;
    state->bwd_payload_capacity = seq_len;
    state->fwd_payload_count = 0;
    state->bwd_payload_count = 0;

    // 窗口数组（使用偏移量访问）
    state->window_pkt_counts = state->mem_pool->window_pkt_counts;
    state->window_byte_counts = state->mem_pool->window_byte_counts;
    state->window_bitrate = state->mem_pool->window_bitrate;
    state->num_windows = pool_num_windows;
    state->window_pkt_counts_offset = 0;
    state->window_byte_counts_offset = pool_num_windows * sizeof(unsigned int);
    state->window_bitrate_offset = 2 * pool_num_windows * sizeof(unsigned int);

    // Burst 数组
    state->burst_sizes = state->mem_pool->burst_sizes;
    state->burst_durations = state->mem_pool->burst_durations;
    state->burst_intervals = state->mem_pool->burst_intervals;
    state->burst_packet_counts = state->mem_pool->burst_packet_counts;
    state->burst_sizes_offset = 0;
    state->burst_durations_offset = seq_len * sizeof(unsigned int);
    state->burst_intervals_offset = seq_len * sizeof(unsigned long long);
    state->burst_packet_counts_offset = seq_len * sizeof(unsigned long long) * 2;

    // 响应延迟数组
    state->resp_delays = state->mem_pool->resp_delays;
    state->resp_delays_offset = 0;
    state->resp_delay_capacity = pool_resp_delay_capacity;
    state->resp_delay_count = 0;

    // 初始化方向统计
    running_stats_init(&state->fwd_pkt_len.fwd);
    running_stats_init(&state->fwd_pkt_len.bwd);
    running_stats_init(&state->bwd_pkt_len.fwd);
    running_stats_init(&state->bwd_pkt_len.bwd);
    running_stats_init(&state->fwd_iat.fwd);
    running_stats_init(&state->fwd_iat.bwd);
    running_stats_init(&state->bwd_iat.fwd);
    running_stats_init(&state->bwd_iat.bwd);

    // 【优化 2】环形 buffer 使用内存池中的数据区，避免单独分配
    // 使用实际的 seq_len（根据流级别动态决定）
    #define INIT_RING_BUFFER(cb, capacity_value, size) \
        do { \
            elem_size = size; \
            void* ring_data = flow_mem_pool_get_ring_buffer(state->mem_pool, ring_offset); \
            if (!ring_data) goto cleanup; \
            memset(&cb, 0, sizeof(circular_buffer_t)); \
            cb.data = ring_data; \
            cb.elem_size = elem_size; \
            cb.capacity = capacity_value; \
            cb.count = 0; \
            cb.head = 0; \
            cb.tail = 0; \
            cb.is_full = 0; \
            ring_offset += (capacity_value) * elem_size; \
        } while(0)

    // 初始化 14 个序列 buffer（数据区在内存池中，非单独分配）
    INIT_RING_BUFFER(state->pkt_len_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->iat_seq, seq_len, sizeof(unsigned long long));
    INIT_RING_BUFFER(state->dir_seq, seq_len, sizeof(signed char));
    INIT_RING_BUFFER(state->ts_seq, seq_len, sizeof(unsigned long long));
    INIT_RING_BUFFER(state->tcp_window_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->udp_len_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->ip_ttl_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->ip_tos_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->payload_size_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->payload_size_diff_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->l3_len_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->l4_len_seq, seq_len, sizeof(unsigned int));
    INIT_RING_BUFFER(state->payload_len_seq, seq_len, sizeof(unsigned int));

    #undef INIT_RING_BUFFER

    // 初始化窗口大小
    state->window_size_ms = cfg->window_size_ms;
    state->window_start_us = 0;
    state->current_window = 0;

    // 初始化 Burst 状态
    state->burst_start_us = 0;
    state->burst_byte_count = 0;
    state->burst_pkt_count = 0;
    state->burst_count = 0;

    return 0;

cleanup:
    flow_state_destroy(state);
    return -1;
}

void flow_state_destroy(flow_feature_state_t* state)
{
    if (!state) return;

    // 【优化 3】使用内存池统一管理，只需一次 free 释放所有动态数组
    if (state->mem_pool) {
        flow_mem_pool_destroy(state->mem_pool);
        free(state->mem_pool);
        state->mem_pool = NULL;
    }

    // 释放 JSON
    if (state->output_json) {
        cJSON_Delete(state->output_json);
        state->output_json = NULL;
    }
}

// ============================================================================
// 流量处理主函数
// ============================================================================

UCHAR traffic_process(struct streaminfo *a_stream, void **pme, int thread_seq,
                      void *a_packet, stream_type a_stream_type)
{
    if (!a_stream || !pme) {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
            "traffic_process invalid args: a_stream=%p, pme=%p, thread_seq=%d",
            a_stream, pme, thread_seq);
        return APP_STATE_DROPME;
    }

    flow_feature_state_t* state = (flow_feature_state_t*)*pme;

    // 获取时间戳
    struct timeval cur_time = {0};
    unsigned long long ts_us = 0;

    if (g_feat_config.run_mode == 0) {
        get_rawpkt_opt_from_streaminfo(a_stream, RAW_PKT_GET_TIMESTAMP, &cur_time);
        ts_us = cur_time.tv_sec * 1000000ULL + cur_time.tv_usec;
    } else {
        int time_val_len = sizeof(ts_us);
        sapp_get_platform_opt(SPO_CURTIME_TIMET_MS, &ts_us, &time_val_len);
        ts_us *= 1000;  // ms 转 us
    }

    // 获取包信息
    struct iphdr *iph = (struct iphdr *)a_packet;
    struct tcphdr *tcph = NULL;
    unsigned int pkt_len = 0;
    unsigned int payload_len = 0;
    unsigned int l2_len = 0;
    unsigned int l3_len = 0;
    unsigned int l4_len = 0;
    int direction = DIR_UNKNOWN;
    int raw_tot_len = 0;
    int raw_pkt_type = __ADDR_TYPE_INIT;
    unsigned char* packet_bytes = (unsigned char*)a_packet;

    if (a_stream_type == TCP) {
        if (a_stream->ptcpdetail) {
            payload_len = a_stream->ptcpdetail->datalen;
        }
        if (packet_bytes && iph) {
            l3_len = (unsigned int)(iph->ihl * 4);
            tcph = (struct tcphdr *)(packet_bytes + l3_len);
            if (tcph) {
                l4_len = (unsigned int)(tcph->doff * 4);
            }
            pkt_len = payload_len + l3_len + l4_len;
        } else {
            pkt_len = payload_len;
        }
        direction = a_stream->curdir;
    } 
    else if (a_stream_type == UDP) {
        if (a_stream->pudpdetail) {
            payload_len = a_stream->pudpdetail->datalen;
        }
        if (iph) {
            l3_len = (unsigned int)(iph->ihl * 4);
        }
        l4_len = 8;
        pkt_len = payload_len + l4_len + l3_len;  // UDP header 8 bytes
        direction = a_stream->curdir;
    }

    // 若原始包从 L2 开始，使用总长度反推出 L2 头长度并补齐到 pkt_len。
    if (get_rawpkt_opt_from_streaminfo(a_stream, RAW_PKT_GET_TOT_LEN, &raw_tot_len) == 0 &&
        get_rawpkt_opt_from_streaminfo(a_stream, RAW_PKT_GET_RAW_PKT_TYPE, &raw_pkt_type) == 0) {
        if (raw_pkt_type == ADDR_TYPE_MAC ||
            raw_pkt_type == ADDR_TYPE_VLAN ||
            raw_pkt_type == ADDR_TYPE_MAC_IN_MAC) {
            unsigned int l3_l4_payload_len = l3_len + l4_len + payload_len;
            if (raw_tot_len >= (int)l3_l4_payload_len) {
                l2_len = (unsigned int)(raw_tot_len - (int)l3_l4_payload_len);
                pkt_len += l2_len;
            }
        }
    }

    // 状态机处理
    UCHAR state_flag = 0;
    switch (a_stream_type) {
        case TCP:
            state_flag = a_stream->pktstate;
            break;
        case UDP:
            state_flag = a_stream->opstate;
            break;
        default:
            break;
    }

    switch (state_flag) {
        case OP_STATE_PENDING: {
            // 初始化流状态
            if (!state) {
                state = (flow_feature_state_t*)dictator_malloc(thread_seq, sizeof(flow_feature_state_t));
                if (!state) {
                    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "%s dictator_malloc failed, size=%zu", printaddr(&a_stream->addr, thread_seq), sizeof(flow_feature_state_t));
                    return APP_STATE_DROPME;
                }
                if (flow_state_init(state) != 0) {
                    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "%s flow_state_init failed", printaddr(&a_stream->addr, thread_seq));
                    dictator_free(thread_seq, state);
                    return APP_STATE_DROPME;
                }

                state->flow_start_us = ts_us;
                *pme = state;
                MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
                    "%s New flow created (TCP/UDP)", printaddr(&a_stream->addr, thread_seq));
            }

            state->last_pkt_us = ts_us;

            // 更新协议头信息
            // if (iph) {
            //     calc_protocol_ip_update(state, iph);
            // }
            // if (a_stream->addr.addrtype == ADDR_TYPE_IPV4) {
            //     struct stream_tuple4_v4 *paddr = (struct stream_tuple4_v4 *)a_stream->addr.paddr;
            //     if (paddr) {
            //         calc_protocol_port_update(state, paddr->source, paddr->dest);
            //     }
            // }

            /* fall through: 首包继续执行 DATA 分支统计逻辑 */
        }

        case OP_STATE_DATA: {
            if (!state) break;

            state->last_pkt_us = ts_us;

            // 【优化 2】检查是否需要升级流级别（按需扩容）
            if (state->mem_pool && flow_mem_pool_check_upgrade(state->mem_pool)) {
                ring_buffer_snapshot_t pkt_len_snap = {0};
                ring_buffer_snapshot_t iat_snap = {0};
                ring_buffer_snapshot_t dir_snap = {0};
                ring_buffer_snapshot_t ts_snap = {0};
                ring_buffer_snapshot_t tcp_window_snap = {0};
                ring_buffer_snapshot_t udp_len_snap = {0};
                ring_buffer_snapshot_t ip_ttl_snap = {0};
                ring_buffer_snapshot_t ip_tos_snap = {0};
                ring_buffer_snapshot_t payload_size_snap = {0};
                ring_buffer_snapshot_t payload_size_diff_snap = {0};
                ring_buffer_snapshot_t l3_len_snap = {0};
                ring_buffer_snapshot_t l4_len_snap = {0};
                ring_buffer_snapshot_t payload_len_snap = {0};

                ring_buffer_snapshot_capture(&state->pkt_len_seq, &pkt_len_snap);
                ring_buffer_snapshot_capture(&state->iat_seq, &iat_snap);
                ring_buffer_snapshot_capture(&state->dir_seq, &dir_snap);
                ring_buffer_snapshot_capture(&state->ts_seq, &ts_snap);
                ring_buffer_snapshot_capture(&state->tcp_window_seq, &tcp_window_snap);
                ring_buffer_snapshot_capture(&state->udp_len_seq, &udp_len_snap);
                ring_buffer_snapshot_capture(&state->ip_ttl_seq, &ip_ttl_snap);
                ring_buffer_snapshot_capture(&state->ip_tos_seq, &ip_tos_snap);
                ring_buffer_snapshot_capture(&state->payload_size_seq, &payload_size_snap);
                ring_buffer_snapshot_capture(&state->payload_size_diff_seq, &payload_size_diff_snap);
                ring_buffer_snapshot_capture(&state->l3_len_seq, &l3_len_snap);
                ring_buffer_snapshot_capture(&state->l4_len_seq, &l4_len_snap);
                ring_buffer_snapshot_capture(&state->payload_len_seq, &payload_len_snap);

                // 需要升级到更大的级别
                flow_level_t new_level = (flow_level_t)((int)state->mem_pool->level + 1);
                if (new_level <= FLOW_LEVEL_LARGE) {
                    if (flow_mem_pool_upgrade(state->mem_pool, new_level) == 0) {
                        // 扩容成功，更新相关指针
                        state->fwd_pkt_lens = state->mem_pool->fwd_pkt_lens;
                        state->bwd_pkt_lens = state->mem_pool->bwd_pkt_lens;
                        state->fwd_iats = state->mem_pool->fwd_iats;
                        state->bwd_iats = state->mem_pool->bwd_iats;
                        state->fwd_payload_sizes = state->mem_pool->fwd_payload_sizes;
                        state->bwd_payload_sizes = state->mem_pool->bwd_payload_sizes;
                        state->window_pkt_counts = state->mem_pool->window_pkt_counts;
                        state->window_byte_counts = state->mem_pool->window_byte_counts;
                        state->window_bitrate = state->mem_pool->window_bitrate;
                        state->burst_sizes = state->mem_pool->burst_sizes;
                        state->burst_durations = state->mem_pool->burst_durations;
                        state->burst_intervals = state->mem_pool->burst_intervals;
                        state->burst_packet_counts = state->mem_pool->burst_packet_counts;
                        state->resp_delays = state->mem_pool->resp_delays;
                        state->fwd_pkt_len_capacity = state->mem_pool->actual_seq_len;
                        state->bwd_pkt_len_capacity = state->mem_pool->actual_seq_len;
                        state->fwd_iat_capacity = state->mem_pool->actual_seq_len;
                        state->bwd_iat_capacity = state->mem_pool->actual_seq_len;
                        state->fwd_payload_capacity = state->mem_pool->actual_seq_len;
                        state->bwd_payload_capacity = state->mem_pool->actual_seq_len;

                        // 重新设置临时 buffer
                        unsigned int seq_len = state->mem_pool->actual_seq_len;
                        state->temp_double_buffer = (double*)flow_mem_pool_alloc_temp(
                            state->mem_pool, seq_len * sizeof(double) * 2, 8);
                        state->temp_uint_buffer_size = seq_len;
                        state->temp_double_buffer_2 = state->temp_double_buffer + seq_len;

                        // 扩容后重绑定 ring buffer，并恢复历史序列
                        size_t ring_offset = 0;
                        ring_buffer_rebind_from_snapshot(&state->pkt_len_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &pkt_len_snap);
                        ring_buffer_rebind_from_snapshot(&state->iat_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned long long), &iat_snap);
                        ring_buffer_rebind_from_snapshot(&state->dir_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(signed char), &dir_snap);
                        ring_buffer_rebind_from_snapshot(&state->ts_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned long long), &ts_snap);
                        ring_buffer_rebind_from_snapshot(&state->tcp_window_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &tcp_window_snap);
                        ring_buffer_rebind_from_snapshot(&state->udp_len_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &udp_len_snap);
                        ring_buffer_rebind_from_snapshot(&state->ip_ttl_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &ip_ttl_snap);
                        ring_buffer_rebind_from_snapshot(&state->ip_tos_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &ip_tos_snap);
                        ring_buffer_rebind_from_snapshot(&state->payload_size_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &payload_size_snap);
                        ring_buffer_rebind_from_snapshot(&state->payload_size_diff_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &payload_size_diff_snap);
                        ring_buffer_rebind_from_snapshot(&state->l3_len_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &l3_len_snap);
                        ring_buffer_rebind_from_snapshot(&state->l4_len_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &l4_len_snap);
                        ring_buffer_rebind_from_snapshot(&state->payload_len_seq, state->mem_pool, &ring_offset,
                            seq_len, sizeof(unsigned int), &payload_len_snap);

                        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
                            "%s Flow upgraded to level %d, seq_len=%u",
                            printaddr(&a_stream->addr, thread_seq), new_level, seq_len);
                    }
                }

                ring_buffer_snapshot_release(&pkt_len_snap);
                ring_buffer_snapshot_release(&iat_snap);
                ring_buffer_snapshot_release(&dir_snap);
                ring_buffer_snapshot_release(&ts_snap);
                ring_buffer_snapshot_release(&tcp_window_snap);
                ring_buffer_snapshot_release(&udp_len_snap);
                ring_buffer_snapshot_release(&ip_ttl_snap);
                ring_buffer_snapshot_release(&ip_tos_snap);
                ring_buffer_snapshot_release(&payload_size_snap);
                ring_buffer_snapshot_release(&payload_size_diff_snap);
                ring_buffer_snapshot_release(&l3_len_snap);
                ring_buffer_snapshot_release(&l4_len_snap);
                ring_buffer_snapshot_release(&payload_len_snap);
            }

            // 每包更新协议头统计
            if (iph) {
                calc_protocol_ip_update(state, iph);
            }
            if (a_stream->addr.addrtype == ADDR_TYPE_IPV4) {
                struct stream_tuple4_v4 *paddr = (struct stream_tuple4_v4 *)a_stream->addr.paddr;
                if (paddr) {
                    calc_protocol_port_update(state, paddr->source, paddr->dest);
                }
            }

            if (a_stream_type == UDP) {
                state->udp_packets++;
            }

            // 更新基础计数器
            calc_basic_counters(state, direction, pkt_len, payload_len);

            // 更新包长统计
            calc_packet_length_stats(state, direction, pkt_len);

            // 更新方向与时间戳序列
            signed char dir_val = (direction == DIR_FWD) ? 1 : ((direction == DIR_BWD) ? -1 : 0);
            circular_buffer_push(&state->dir_seq, &dir_val);
            circular_buffer_push(&state->ts_seq, &ts_us);
            circular_buffer_push(&state->l3_len_seq, &l3_len);
            circular_buffer_push(&state->l4_len_seq, &l4_len);
            circular_buffer_push(&state->payload_len_seq, &payload_len);

            // 更新 IAT 统计
            calc_iat_update(state, ts_us, direction);

            // 更新 TCP 标志统计
            if (a_stream_type == TCP && tcph) {
                calc_protocol_tcp_update(state, tcph, payload_len, ntohs(tcph->window),
                                         direction, ntohl(tcph->seq), ts_us);
            } else if (a_stream_type == UDP) {
                unsigned int udp_len = payload_len + 8;
                running_stats_update(&state->udp_len_stats, (double)udp_len);
                circular_buffer_push(&state->udp_len_seq, &udp_len);
            }

            // 更新 Payload 统计
            if (payload_len > 0 && iph && packet_bytes) {
                unsigned char* payload = packet_bytes + iph->ihl * 4;
                if (a_stream_type == TCP && tcph) {
                    payload += tcph->doff * 4;
                } else if (a_stream_type == UDP) {
                    payload += 8;  // UDP header
                }
                calc_payload_stats(state, payload, payload_len);
                calc_payload_magic(state, payload, payload_len);
            }

            // 更新 Burst 状态
            unsigned long long iat = ts_us - state->last_arrival_us;
            calc_burst_update(state, pkt_len, iat, ts_us);

            // 更新 Bigram 统计
            pkt_size_class curr_class = get_pkt_size_class(pkt_len);
            bigram_update(&state->bigram_stats, state->last_pkt_size_class, curr_class);
            state->last_pkt_size_class = curr_class;

            // 更新响应延迟
            calc_response_delay(state, ts_us, direction);

            // 更新窗口统计
            calc_window_update(state, pkt_len, ts_us);

            break;
        }

        case OP_STATE_CLOSE: {
            if (!state) break;

            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
                "%s Flow closing, total_packets=%u, duration_us=%llu",
                printaddr(&a_stream->addr, thread_seq), state->total_packets,
                state->last_pkt_us - state->flow_start_us);

            // 计算所有特征并输出
            cJSON* output = flow_state_compute_features(state, a_stream, thread_seq, a_stream_type);

            if (output && (g_feat_config.send_kafka_flag || g_feat_config.output_to_log || g_feat_config.feature_bridge_flag)) {
                char* out = cJSON_PrintUnformatted(output);
                if (out) {
                    if (g_feat_config.send_kafka_flag) {
                        string topic = (string)g_feat_config.topic_name;
                        int send_ret = g_feat_config.kafka_producer->SendData(topic, (void*)out, strlen(out));
                        if (send_ret != 0) {
                            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                                "%s Kafka SendData failed, ret=%d, outq_len=%d",
                                printaddr(&a_stream->addr, thread_seq),
                                send_ret,
                                g_feat_config.kafka_producer->MessageInQueue());
                        }
                    }

                    if (g_feat_config.output_to_log) {
                        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
                            "%s send_kafka JSON is %s.", printaddr(&a_stream->addr, thread_seq), out);
                    }

                    // Bridge 输出：复制 JSON 字符串并通过 bridge 传递给其他插件
                    if (g_feat_config.feature_bridge_flag && g_feat_config.feature_bridge_id >= 0) {
                        char* bridge_data = strdup(out);  // 复制一份 JSON 字符串
                        if (bridge_data) {
                            stream_bridge_async_data_put(a_stream, g_feat_config.feature_bridge_id, bridge_data);
                            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_DEBUG, TI_FEATURES,
                                "%s Feature JSON sent to bridge, id=%d, len=%zu",
                                printaddr(&a_stream->addr, thread_seq), g_feat_config.feature_bridge_id, strlen(bridge_data));
                        } else {
                            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                                "%s Failed to duplicate JSON for bridge", printaddr(&a_stream->addr, thread_seq));
                        }
                    }

                    cJSON_free(out);
                }
            }

            if (output) {
                cJSON_Delete(output);
            }

            // 释放流状态
            flow_state_destroy(state);
            dictator_free(thread_seq, state);
            *pme = NULL;

            return APP_STATE_DROPME;
        }

        default:
            break;
    }

    return APP_STATE_GIVEME;
}

// ============================================================================
// 特征计算主函数
// ============================================================================

static void add_flow_tuple_identifier(cJSON* output,
                                      struct streaminfo* a_stream,
                                      stream_type a_stream_type)
{
    if (!output || !a_stream) return;

    char s_ip_str[64] = {0};
    char d_ip_str[64] = {0};

    // 优先写入创建时间，保持基础标识字段在 JSON 开头
    if (a_stream_type == TCP && a_stream->ptcpdetail) {
        cJSON_AddNumberToObject(output, "createtime", a_stream->ptcpdetail->createtime);
    } else {
        cJSON_AddNumberToObject(output, "createtime", 0);
    }

    if (a_stream->addr.addrtype == ADDR_TYPE_IPV4) {
        struct stream_tuple4_v4 *paddr = (struct stream_tuple4_v4 *)a_stream->addr.paddr;
        if (paddr) {
            inet_ntop(AF_INET, &paddr->saddr, s_ip_str, sizeof(s_ip_str));
            inet_ntop(AF_INET, &paddr->daddr, d_ip_str, sizeof(d_ip_str));
            cJSON_AddStringToObject(output, "src_ip", s_ip_str);
            cJSON_AddStringToObject(output, "dst_ip", d_ip_str);
            cJSON_AddNumberToObject(output, "src_port", ntohs(paddr->source));
            cJSON_AddNumberToObject(output, "dst_port", ntohs(paddr->dest));
        }
    } else if (a_stream->addr.addrtype == ADDR_TYPE_IPV6) {
        struct stream_tuple4_v6 *paddr6 = (struct stream_tuple4_v6 *)a_stream->addr.paddr;
        if (paddr6) {
            inet_ntop(AF_INET6, &paddr6->saddr, s_ip_str, sizeof(s_ip_str));
            inet_ntop(AF_INET6, &paddr6->daddr, d_ip_str, sizeof(d_ip_str));
            cJSON_AddStringToObject(output, "src_ip", s_ip_str);
            cJSON_AddStringToObject(output, "dst_ip", d_ip_str);
            cJSON_AddNumberToObject(output, "src_port", ntohs(paddr6->source));
            cJSON_AddNumberToObject(output, "dst_port", ntohs(paddr6->dest));
        }
    }

    cJSON_AddNumberToObject(output, "trans_proto", a_stream_type);
    cJSON_AddNumberToObject(output, "stream_dir", a_stream->curdir);
}

cJSON* flow_state_compute_features(flow_feature_state_t* state,
                                    struct streaminfo* a_stream,
                                    int thread_seq,
                                    stream_type a_stream_type)
{
    if (!state) return NULL;

    cJSON* output = cJSON_CreateObject();
    if (!output) return NULL;

    add_flow_tuple_identifier(output, a_stream, a_stream_type);

    // 从 bridge 获取 SNI（如果配置了 sni_bridge_flag）
    if (g_feat_config.sni_bridge_flag && g_feat_config.sni_bridge_id >= 0) {
        char* sni = (char*)stream_bridge_async_data_get(a_stream, g_feat_config.sni_bridge_id);
        if (sni != NULL) {
            cJSON_AddStringToObject(output, "SNI", sni);
        }
    }

    // 基础特征
    calc_derived_basic(state, output);

    // IAT 特征
    calc_derived_iat(state, output);

    // Burst 特征
    calc_burst_statistics(state, output);

    // 协议特征
    calc_derived_protocol(state, output);

    // Payload 特征
    calc_derived_payload(state, output);

    // 序列特征
    calc_derived_sequence(state, output);

    // FFT 频域特征
    calc_derived_fft(state, output);

    // 窗口特征
    calc_derived_window(state, output);

    // 行为特征
    calc_derived_behavior(state, output);

    // 添加流基本信息
    cJSON_AddNumberToObject(output, "total_packets", state->total_packets);
    cJSON_AddNumberToObject(output, "fwd_packets", state->fwd_packets);
    cJSON_AddNumberToObject(output, "bwd_packets", state->bwd_packets);
    cJSON_AddNumberToObject(output, "total_bytes", state->total_bytes);
    cJSON_AddNumberToObject(output, "fwd_bytes", state->fwd_bytes);
    cJSON_AddNumberToObject(output, "bwd_bytes", state->bwd_bytes);

    // TCP 特征
    cJSON_AddNumberToObject(output, "tcp_syn_count", state->tcp_syn_count);
    cJSON_AddNumberToObject(output, "tcp_fin_count", state->tcp_fin_count);
    cJSON_AddNumberToObject(output, "tcp_rst_count", state->tcp_rst_count);
    cJSON_AddNumberToObject(output, "tcp_ack_count", state->tcp_ack_count);

    // // 添加时间戳
    // struct timeval tv;
    // gettimeofday(&tv, NULL);
    // cJSON_AddNumberToObject(output, "timestamp", tv.tv_sec);

    return output;
}

// ============================================================================
// 插件入口函数
// ============================================================================

int TI_FEATURES_INIT(void)
{
    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "TI_FEATURES_INIT started");

    if (feat_config_read((char*)"./ticonf/ti_features.conf") != 0) {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
            "TI_FEATURES_INIT failed: config read error");
        return -1;
    }

    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "TI_FEATURES_INIT completed successfully");
    return 0;
}

void TI_FEATURES_DESTROY(void)
{
    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "TI_FEATURES_DESTROY called");
    feat_config_destroy();
}

UCHAR TI_FEATURES_TCP_ENTRY(struct streaminfo *a_stream, void **pme, int thread_seq, void *a_packet)
{
    return traffic_process(a_stream, pme, thread_seq, a_packet, TCP);
}

UCHAR TI_FEATURES_UDP_ENTRY(struct streaminfo *a_stream, void **pme, int thread_seq, void *a_packet)
{
    return traffic_process(a_stream, pme, thread_seq, a_packet, UDP);
}

UCHAR TI_FEATURES_SSL_ENTRY(stSessionInfo *session_info, void **pme, int thread_seq,
                            struct streaminfo *a_stream, void *a_packet)
{
    ssl_stream* a_ssl_stream = (ssl_stream*)session_info->app_info;
    char* sni = NULL;
    int len = 0;

    switch(session_info->prot_flag)
    {
    case SSL_CLIENT_HELLO:
        // 提取 SNI 并放入 bridge
        if(a_ssl_stream->stClientHello && strlen((const char*)(a_ssl_stream->stClientHello->server_name)) > 0)
        {
            sni = (char*)malloc(sizeof(char)*MAX_DOMAIN_LEN);
            memset(sni, 0, sizeof(char)*MAX_DOMAIN_LEN);
            len = (int)MIN(strlen((const char*)(a_ssl_stream->stClientHello->server_name)), sizeof(char)*MAX_DOMAIN_LEN);
            memcpy(sni, a_ssl_stream->stClientHello->server_name, len);
            stream_bridge_async_data_put(a_stream, g_feat_config.sni_bridge_id, sni);
        }
        break;
    default:
        break;
    }

    return PROT_STATE_GIVEME;
}

