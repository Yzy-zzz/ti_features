# ti_features - 网络流特征统计插件

## 项目概述

`ti_features` 是一个高性能网络流特征统计插件，用于从网络流量中提取 512+ 维特征，支持流检测、流量分析、异常检测等应用场景。插件采用在线增量计算 + 流结束二次计算的设计模式，配合分级内存池优化，在高并发场景下显著降低内存占用。

---

## 特征总体分类

| 大类 | 数量 | 计算方式 |
|------|------|----------|
| 统计特征 | 172 | 在线累加 + 离线统计 |
| 序列特征 | 165 | 需要存储序列 |
| Payload 特征 | 86 | 需要 payload 数据 |
| 协议头部特征 | 62 | 在线累加 |
| 行为特征 | 27 | 二次计算 |
| **总计** | **512** | |

---

## 项目结构

```
ti_features/
├── CMakeLists.txt              # 顶层 CMake 配置
├── bin/
│   ├── ti_features.conf        # 配置文件模板
│   └── ti_features.inf         # 插件信息文件
├── src/
│   ├── CMakeLists.txt          # 源码 CMake 配置
│   │
│   ├── ti_features.h           # 主头文件（入口函数声明、阈值定义）
│   ├── ti_features.cpp         # 主入口实现
│   │
│   ├── feature_config.h        # 配置参数结构
│   ├── feature_config.cpp      # 配置读取实现
│   │
│   ├── feature_stats.h         # 运行统计结构（Welford 算法）
│   ├── feat_state.h            # 每流状态结构
│   │
│   ├── calc_basic.h/cpp        # 基础特征计算
│   ├── calc_iat.h/cpp          # IAT 特征计算
│   ├── calc_burst.h/cpp        # Burst 特征计算
│   ├── calc_freq_domain.h/cpp  # 频域特征计算 (FFT)
│   ├── calc_sequence.h/cpp     # 序列特征计算
│   ├── calc_payload.h/cpp      # Payload 特征计算
│   ├── calc_protocol.h/cpp     # 协议头特征计算
│   ├── calc_behavior.h/cpp     # 行为特征计算
│   ├── calc_window.h/cpp       # 窗口特征计算
│   │
│   ├── utils/
│   │   ├── circular_buffer.h/cpp  # 环形 Buffer 实现
│   │   ├── fft.h/cpp              # FFT 计算实现
│   │   ├── memory_pool.h          # 分级内存池（关键优化）
│   │   └── stats_utils.h          # 统计辅助函数
│   │
│   └── KafkaProducer.h/cpp     # Kafka 生产者
│
├── inc_for_refer/              # 参考头文件（MESA 接口）
│   ├── stream.h
│   ├── ssl.h
│   └── stream_inc/
│
├── cmake/                      # CMake 模块
│   ├── Version.cmake
│   ├── Package.cmake
│   └── ...
│
└── ci/                         # CI 脚本
```

---

## 模块划分

| 模块 | 文件 | 职责 |
|------|------|------|
| **配置模块** | feature_config.h/cpp | 读取配置文件，管理全局参数 |
| **状态模块** | feat_state.h | 定义每流状态结构体 |
| **统计工具** | feature_stats.h | Welford 在线统计算法、Bigram 统计 |
| **基础特征** | calc_basic.h/cpp | 包数、字节数、包长统计 |
| **IAT 特征** | calc_iat.h/cpp | 时间间隔统计、响应延迟、活跃/空闲时间 |
| **Burst 特征** | calc_burst.h/cpp | 突发检测与统计、Burstiness 指数 |
| **频域特征** | calc_freq_domain.h/cpp | FFT 分析、功率谱密度、Top K 频率 |
| **序列特征** | calc_sequence.h/cpp | 自相关、复杂度、游程、Hurst 指数 |
| **Payload 特征** | calc_payload.h/cpp | 载荷熵、字节分布、协议检测 |
| **协议特征** | calc_protocol.h/cpp | TCP 标志、IP 头部、端口统计 |
| **行为特征** | calc_behavior.h/cpp | 流模式、周期性判断、会话类型 |
| **窗口特征** | calc_window.h/cpp | 时间窗口内的包数、字节数、比特率 |
| **内存管理** | utils/memory_pool.h | 分级内存池，按需扩容 |

---

## 按计算方式分类

### A. 在线累加特征（259 个）

这些特征可以在**每个包到达时增量更新**，无需存储所有历史数据。

#### A1. 基础计数器（8 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| total_packets | int | 包计数器++ |
| fwd_packets | int | direction==FWD ? counter++ |
| bwd_packets | int | direction==BWD ? counter++ |
| total_bytes | int | bytes += pkt_len |
| fwd_bytes | int | direction==FWD ? bytes += pkt_len |
| bwd_bytes | int | direction==BWD ? bytes += pkt_len |
| flow_duration | float | last_pkt_ts - first_pkt_ts |
| timestamp | int | 输出时刻 Unix 时间戳（秒） |

#### A2. Payload 相关（90 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| fwd_total_payload | int | FWD 方向 payload 字节累加 |
| bwd_total_payload | int | BWD 方向 payload 字节累加 |
| payload_entropy | float | 香农熵 |
| payload_byte_mean/std/min/max/... | float | payload 字节值分布统计（9 个） |
| payload_hist_bin_0~15 | float | 16-bin 字节值直方图 |
| payload_first_64/128/256_entropy | float | 前 N 字节的熵（3 个） |
| payload_printable_ratio | float | 可打印 ASCII 比例 |
| payload_alnum_ratio | float | 字母数字比例 |
| payload_magic_* | int | 协议魔数检测（HTTP/TLS/SSH/JPEG/PNG/GIF，11 个） |
| payload_looks_like_* | int | 协议启发式检测（5 个） |
| payload_chi_square | float | 卡方检验 |
| payload_markov_entropy | float | 马尔可夫熵 |
| payload_compression_ratio | float | zlib 压缩率 |
| payload_size_* | float | payload 大小统计（9 个） |
| payload_length_mod_N_mean/std | float | payload 长度 mod 统计（N=2,4,8,16,32,64,128,256，16 个） |

#### A3. TCP 标志统计（24 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| tcp_syn/fin/rst/psh/ack/urg_count | int | 各标志包计数（6 个） |
| tcp_syn_only_count | int | 仅 SYN 标志包 |
| tcp_syn_ack_packets | int | SYN+ACK 包 |
| tcp_ack_only_count | int | 仅 ACK（无 payload） |
| tcp_payload_packets | int | 有 payload 的 TCP 包 |
| tcp_window_* | float | TCP 窗口大小统计（9 个） |
| tcp_connection_success_rate | float | (SYN-ACK) / (SYN) |
| tcp_window_update_frequency | int | Window Update 次数 |

#### A4. IP 头部特征（25 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| ip_ttl_* | float | TTL 统计（9 个） |
| ip_ttl_eq_32/64/128/255_count | int | TTL 特定值计数（4 个） |
| ip_tos_* | float | ToS 统计（9 个） |
| src/dst_ip_is_private | int | 私有地址判断（2 个） |
| ip_fragmentation_flag | int | IP 分片标志 |

#### A5. 端口相关（10 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| unique_src/dst_ports | int | 唯一端口数（2 个） |
| well_known_src/dst_port_ratio | float | 知名端口比例（2 个） |
| src/dst_port_is_ephemeral | int | 临时端口判断（2 个） |
| src/dst_port_protocol | string | 端口协议推断（2 个） |
| is_common_service | int | 常见服务端口 |

#### A6. UDP 特征（10 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| udp_length_* | float | UDP 长度统计（9 个） |
| quic_usage_flag | int | 是否为 QUIC (UDP/443) |

#### A7. 窗口化统计（39 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| window_packet_count_* | float | 1 秒窗口包数统计（9 个） |
| window_byte_count_* | float | 1 秒窗口字节数统计（9 个） |
| window_bitrate_mean/std/min/max | float | 窗口比特率（4 个） |
| instant_bitrate_* | float | 100ms 窗口瞬时比特率（9 个） |
| peak_bitrate | float | 最大比特率 |
| bitrate_percentile_25/50/75/90 | float | 比特率分位数（4 个） |
| traffic_entropy_ts_mean/peak | float | 窗口字节熵时间序列（2 个） |

#### A8. Bigram 频率（45 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| top_1~5_bigram_SS/SM/SL/MS/MM/ML/LS/LM/LL_freq | float | 各双元组 Top5 频率（9×5=45 个） |

> 小包：≤64B, 中包：64-1200B, 大包：>1200B

#### A9. Burst 检测（8 个）

| 特征名 | 类型 | 计算逻辑 |
|--------|------|---------|
| burst_count | int | burst 数量 |
| avg_burst_duration | float | burst 平均持续时间 |
| avg_burst_size | float | burst 平均字节数 |
| burst_interval_mean | float | burst 间平均间隔 |
| burst_size_std | float | burst 大小标准差 |
| burst_packet_count_mean | float | burst 内平均包数 |
| burst_packet_rate_peak | float | burst 内峰值包速率 |
| burstiness_index | float | Burstiness 指数 |

---

### B. 二次计算特征 - 需要序列 buffer（223 个）

这些特征需要**存储原始序列**，在流结束时或定期计算。

#### B1. 基于 packet_length_seq（95 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| first_10_packets_length_* | 前 10 包长度统计（9 个） |
| packet_length_median/q1/q3 | 包长分位数（3 个） |
| packet_length_entropy | 包长分布熵 |
| packet_length_percentile_1/5/10/90/95/99/100 | 百分位数（7 个） |
| length_run_* | 游程统计（9 个） |
| length_autocorr_lag_1~5 | 包长自相关（5 个） |
| all_fft_* | 全量 FFT 特征（19 个） |
| fwd_fft_* | 前向 FFT 特征（19 个） |
| bwd_fft_* | 后向 FFT 特征（19 个） |
| psd_peak | 功率谱密度峰值 |
| lz78_complexity_index | LZ78 复杂度 |

#### B2. 基于 packet_iat_seq（13 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| iat_* | IAT 统计（9 个） |
| active_time | IAT≤1ms 时间累加 |
| idle_time | IAT>1ms 时间累加 |
| active_time_ratio | active / (active + idle) |
| iat_entropy | IAT 分布熵 |

#### B3. 基于 packet_direction_seq（3 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| direction_sequence_entropy | 方向序列熵 |
| direction_transition_unique_count | 方向转换模式数 |
| direction_change_frequency | 方向变化频率 |

#### B4. 基于 IAT + direction（25 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| fwd_iat_* | 前向 IAT 统计（9 个） |
| bwd_iat_* | 后向 IAT 统计（9 个） |
| fwd/bwd_iat_cv | 变异系数（2 个） |
| iat_autocorr_lag_1~5 | IAT 自相关（5 个） |

#### B5. 基于 packet_length_seq + direction（46 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| fwd_packet_length_* | 前向包长统计（9 个） |
| bwd_packet_length_* | 后向包长统计（9 个） |
| fwd_payload_* | 前向 payload 统计（9 个） |
| bwd_payload_* | 后向 payload 统计（9 个） |
| length_direction_correlation | 包长-方向相关系数 |

#### B6. 高级序列特征（22 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| iat_sequence_* | IAT 序列统计（9 个） |
| log_iat_sequence_* | log(IAT) 统计（9 个） |
| hurst_exponent_estimate | R/S 分析 Hurst 指数 |
| has_regular_intervals | 是否有规律间隔 |
| periodic_dominant_freq_ratio | FFT 主频能量占比 |
| has_strong_periodicity | 是否有强周期性 |

#### B7. 响应延迟特征（10 个）

| 特征名 | 计算逻辑 |
|--------|---------|
| response_delay_* | 请求-响应延迟统计（9 个） |
| is_interactive_session | 平均响应延迟<100ms |

#### B8. 原始序列输出（8 个）

| 特征名 | 格式 |
|--------|------|
| packet_length_seq | "123,456,789,..."
| packet_iat_seq | "1000,2000,..."
| packet_direction_seq | "1,-1,1,-1,..."
| L2L3L4Pl_Iat | 复合序列 |
| dl_chunk_seq | 下载 chunk 序列 |
| dl_chunk_count | chunk 计数 |
| uplink_rate_seq | 上行速率序列 |
| downlink_rate_seq | 下行速率序列 |

---

### C. 二次计算特征（基于其他特征）（31 个）

| 特征名 | 依赖 | 计算逻辑 |
|--------|------|---------|
| fwd_bwd_packet_ratio | fwd_packets, bwd_packets | fwd/bwd |
| fwd_bwd_byte_ratio | fwd_bytes, bwd_bytes | fwd/bwd |
| avg_packet_length | total_bytes, total_packets | bytes/packets |
| avg_bitrate | total_bytes, flow_duration | bytes*8/duration |
| avg_packet_rate | total_packets, flow_duration | packets/duration |
| avg_throughput | total_bytes, flow_duration | bytes/duration |
| flow_asymmetry_ratio | fwd_bytes, bwd_bytes | fwd/bwd |
| flow_pattern | flow_asymmetry_ratio | download/upload/balanced |
| is_bulk_transfer | avg_throughput | >10KB/s |
| is_interactive_session | response_delay_mean | <100ms |
| has_regular_intervals | CV | CV<阈值 |
| has_strong_periodicity | dominant_freq_ratio | ratio>阈值 |
| tcp_syn/rst/ack_only_ratio | 各计数 | count/total |
| tcp_connection_success_rate | syn_ack, syn_packets | syn_ack/syn |
| protocol_tcp/udp/icmp/other_ratio | 各计数 | count/total |

---

## 内存优化设计

### 分级内存池架构

为解决高并发场景下的内存占用问题，采用**分级分配 + 按需扩容**策略。

| 级别 | 包数量范围 | 序列长度 | 内存占用 | 典型场景 |
|------|-----------|---------|---------|---------|
| **TINY** | ≤100 | 64 | ~12 KB | DNS 查询、TCP 握手 |
| **SMALL** | ≤500 | 256 | ~45 KB | HTTP 短连接、API 调用 |
| **MEDIUM** | ≤2000 | 1000 | ~170 KB | 普通文件传输 |
| **LARGE** | >2000 | 5000 | ~890 KB | 视频流、大文件下载 |

### 自动扩容机制

```
新流开始 → TINY 级别 (64 包)
    │ 包数达到 80% 阈值 (51 包)
    ▼
SMALL 级别 (256 包)
    │ 包数达到 80% 阈值 (205 包)
    ▼
MEDIUM 级别 (1000 包)
    │ 包数达到 80% 阈值 (800 包)
    ▼
LARGE 级别 (5000 包)
```

### 内存占用对比

**10,000 并发流场景**：

| 流类型分布 | 优化前 | 优化后 | 节省 |
|-----------|--------|--------|------|
| 80% 小流 + 20% 大流 | 8.9 GB | ~2.7 GB | **70%** |
| 50% 小流 + 50% 中等流 | 8.9 GB | ~1.1 GB | **88%** |
| 90% 微小流 + 10% 中等流 | 8.9 GB | ~200 MB | **98%** |

---

## 数据结构设计

### 运行统计结构（Welford 算法）

```c
typedef struct {
    double mean;
    double M2;      // 平方差累加（方差）
    double M3;      // 用于偏度
    double M4;      // 用于峰度
    double min_val;
    double max_val;
    unsigned long count;
} running_stats_t;
```

支持在线计算：均值、方差、标准差、偏度、峰度、变异系数。

### 方向分组统计

```c
typedef struct {
    running_stats_t fwd;
    running_stats_t bwd;
} directional_running_stats_t;
```

### 环形 Buffer

```c
typedef struct {
    void* data;
    size_t elem_size;
    unsigned int capacity;
    unsigned int count;
    unsigned int head;
    unsigned int tail;
    int is_full;
} circular_buffer_t;
```

### FFT 结果结构

```c
typedef struct {
    double* magnitudes;
    double* frequencies;
    int n;
    int top_k_indices[5];
    double top_k_mags[5];
    double mag_mean/std/min/max/median/q1/q3/skew/kurt;
} fft_result_t;
```

---

## 处理流程

```
包到达 → TI_FEATURES_xxx_ENTRY
    │
    ├─ PENDING: 分配 flow_feature_state_t
    │           初始化 TINY 级别内存池
    │           初始化所有计数器为 0
    │           记录 flow_start_us
    │
    ├─ DATA:    更新所有在线统计
    │           │
    │           ├── 更新计数器 (packets, bytes)
    │           ├── 更新运行统计 (Welford 算法)
    │           ├── 更新序列 buffer
    │           ├── 更新窗口统计
    │           ├── 更新 Burst 状态机
    │           ├── 更新 Bigram 统计
    │           ├── 更新 Payload 直方图
    │           ├── 检查内存池扩容
    │           └── 更新响应延迟状态
    │
    └─ CLOSE:   计算所有二次特征
                │
                ├── 计算分位数
                ├── 计算 FFT
                ├── 计算自相关
                ├── 计算复杂度
                ├── 计算 Hurst 指数
                ├── 计算所有派生特征
                └── 生成输出 JSON / 发送 Kafka
```

---

## 配置说明

配置文件路径：`bin/ti_features.conf`

```ini
# =============================================================================
# LOG 配置
# =============================================================================
[LOG]
log_level = 10
log_path = ./featlog/ti_features_log

# =============================================================================
# FEATURE 基础配置
# =============================================================================
[FEATURE]
max_seq_len = 5000           # 序列 buffer 最大长度
small_pkt_threshold = 64     # 小包阈值
large_pkt_threshold = 1200   # 大包阈值
run_mode = 0                 # 0=离线 pcap，1=在线模式

# =============================================================================
# BURST 配置
# =============================================================================
[BURST]
burst_iat_threshold_us = 1000  # Burst IAT 阈值（微秒）

# =============================================================================
# ACTIVE 配置（活跃/空闲时间）
# =============================================================================
[ACTIVE]
active_iat_threshold_us = 1000  # 活跃时间阈值（微秒）

# =============================================================================
# WINDOW 配置
# =============================================================================
[WINDOW]
window_size_ms = 1000           # 窗口大小（毫秒）
instant_bitrate_window_ms = 100 # 瞬时比特率窗口

# =============================================================================
# FFT 配置
# =============================================================================
[FFT]
fft_top_k = 5                   # Top K 频率数量

# =============================================================================
# BEHAVIOR 配置
# =============================================================================
[BEHAVIOR]
interactive_threshold_ms = 100   # 交互式会话阈值
bulk_transfer_kbps = 10          # 大流量传输阈值
short_conn_threshold_s = 2       # 短连接阈值

# =============================================================================
# SNI 配置
# =============================================================================
[SNI]
filter_sni_flag = 0              # 0=不过滤，1=过滤，2=仅 443
filter_sni = googlevideo.com
sni_bridge_name = TLS_QUIC_SNI

# =============================================================================
# KAFKA 配置
# =============================================================================
[KAFKA]
send_kafka_flag = 0              # 是否发送 Kafka
output_to_log = 1                # 同时输出到日志
kafka_brokers = 10.26.22.71:9092
kafka_topic = flow_features

# =============================================================================
# OUTPUT 配置
# =============================================================================
[OUTPUT]
output_raw_seq = 1               # 输出原始序列
```

---

## 编译方法

```bash
cd ti_features
mkdir build && cd build
cmake ..
make
```

编译选项：
- `ASAN_OPTION=ADDRESS` - 启用 AddressSanitizer
- `ASAN_OPTION=THREAD` - 启用 ThreadSanitizer

---

## 输出格式

```json
{
  "timestamp": 1711944000,
  "flow_duration": 12.345,
  "total_packets": 1234,
  "fwd_packets": 567,
  "bwd_packets": 667,
  "total_bytes": 1234567,
  "avg_packet_length": 1000.5,
  "fwd_bwd_packet_ratio": 0.85,
  "iat_mean": 10000.5,
  "iat_std": 5000.2,
  "burst_count": 15,
  "tcp_syn_count": 1,
  "tcp_ack_count": 1200,
  "payload_entropy": 7.8,
  "flow_pattern": "download_heavy",
  "window_stats_unavailable": null,
  "packet_length_seq": "123,456,789,...",
  "packet_iat_seq": "1000,2000,1500,...",
  ...
}
```

---

## 待完善功能

### 1. FFT 算法优化
当前 `utils/fft.cpp` 使用 DFT 实现（O(n²)），仅用于框架演示。

**建议**: 使用 kissfft 或 FFmpeg 的 FFT 模块

```cpp
#include "kiss_fft.h"
kiss_fft_cfg cfg = kissfft_alloc(n, 0, NULL, NULL);
kiss_fft(cfg, (kiss_fft_cpx*)in, (kiss_fft_cpx*)out);
kiss_fft_free(cfg);
```

### 2. 复杂度算法实现
- `calc_lz78_complexity()` - 需要完整 LZ78 算法实现
- `calc_hurst_exponent()` - 需要完整 R/S 分析实现

### 3. 分位数计算优化
当前使用完整排序（O(n log n）。

**建议**: 使用 P² 算法或 t-digest 进行近似分位数计算。

---

## 依赖

- MESA 框架（stream.h, cJSON.h 等）
- librdkafka（Kafka 输出）
- pthread

---

## 版本与更新

- 2026-03-26: 特征字段枚举与代码对齐基线
- 2026-03-27: 内存池分级架构实现
- 2026-03-30: 文档合并整理