# ti_features 特征模块清单与可配置方案

## 一、设计思路

采用 **子功能级开关** 的细粒度配置，每个模块拆分为多个独立子开关：

1. **流结束阶段**：各 `calc_derived_*` 函数内部按子开关分段控制
2. **每包阶段**：`enable_payload_stats` 控制最昂贵的熵/压缩率计算
3. **原始序列**：替代原 `output_raw_seq`，细分为 3 个独立开关
4. **总计 28 个开关**：提供更精细的性能与功能权衡

## 二、配置结构

在 `ti_features.conf` 的 `[FEATURES]` 配置段：

```ini
[FEATURES]
# === 主开关（默认全部开启，关闭主开关将跳过整个模块的计算和输出）===
enable_basic = 1
enable_iat = 1
enable_protocol = 1
enable_payload = 1
enable_sequence = 1
enable_fft = 1
enable_window = 1
enable_behavior = 1

# === 基础模块子开关 ===
enable_basic_ratios = 1
enable_basic_payload_dir = 1
enable_basic_first_n = 1
enable_basic_pkt_length = 1

# === IAT 模块子开关 ===
enable_iat_stats = 1
enable_iat_fwd_bwd = 1
enable_iat_active = 1
enable_iat_response = 1

# === Burst 模块 ===
enable_burst = 1

# === 协议模块子开关 ===
enable_protocol_tcp_flags = 1
enable_protocol_tcp_window = 1
enable_protocol_ip = 1
enable_protocol_udp = 1
enable_protocol_port = 1

# === Payload 模块子开关 ===
enable_payload_size = 1
enable_payload_content = 1
enable_payload_hist = 1
enable_payload_magic = 1
enable_payload_stats = 1

# === 序列模块子开关 ===
enable_sequence_stats = 1
enable_raw_sequences = 1
enable_chunk_sequences = 1
enable_rate_sequences = 1

# === FFT 模块 ===
enable_fft_global = 1
enable_fft_fwd = 1
enable_fft_bwd = 1

# === Window 模块 ===
enable_window_pkt = 1
enable_window_byte = 1

# === Behavior 模块 ===
enable_behavior_basic = 1
enable_behavior_pattern = 1
enable_behavior_bitrate = 1
```

## 三、性能优化建议（配置组合）

| 场景 | 建议配置 | 预期收益 |
|------|----------|----------|
| 极致性能 | 只开 basic_ratios + protocol_tcp_flags + behavior_basic | ~80% 性能恢复 |
| 轻量分析 | 关 fft + raw_sequences + payload_stats + payload_hist | ~60% 性能恢复 |
| 序列分析 | 开 sequence_stats + raw_sequences + rate_sequences，关其他 | 专注序列特征 |
| 保留核心 | 关 fft + raw_sequences + chunk_sequences + payload_stats | ~50% 性能恢复 |

---

## 三点五、开关依赖关系与 per-packet 优化

### 设计原理

特征计算分为两个阶段：
1. **每包阶段 (per-packet)**：每个数据包到达时执行，CPU 开销与总包数成正比
2. **流关闭阶段 (flow-close)**：流结束时执行一次，CPU 开销与流长度成正比但频次低

28 个子开关在流关闭阶段控制 JSON 输出。为了在 per-packet 阶段也跳过不必要的计算，系统引入了 **10 个复合依赖标志 (`need_*`)**，由多个子开关自动聚合而成。这些标志在配置加载时自动计算，用户无需手动设置。

### 复合依赖标志（自动计算）

| 复合标志 | 聚合规则 | 控制的 per-packet 操作 |
|----------|----------|----------------------|
| `need_pkt_len_seq` | `fft_global \| sequence_stats \| raw_seq \| chunk_seq \| rate_seq` | pkt_len_seq circular buffer push |
| `need_dir_seq` | `sequence_stats \| raw_seq \| chunk_seq \| rate_seq` | dir_seq circular buffer push |
| `need_ts_seq` | `chunk_seq \| rate_seq` | ts_seq circular buffer push |
| `need_l3_l4_payload_seq` | `raw_sequences` | l3_len_seq, l4_len_seq, payload_len_seq push |
| `need_fwd_bwd_pkt_lens` | `basic_pkt_length \| fft_fwd \| fft_bwd` | 方向包长数组写入 + directional_stats 更新 |
| `need_first_n` | `enable_basic && basic_first_n` | 前 N 包长度记录 |
| `need_payload_dir` | `enable_basic && basic_payload_dir` | fwd/bwd payload 统计 + 数组写入 |
| `need_fwd_bwd_iats` | `enable_iat && iat_fwd_bwd` | fwd/bwd IAT 数组写入 + directional_stats |
| `need_window_update` | `window_pkt \| window_byte \| behavior_pattern \| behavior_bitrate` | 窗口统计更新 |
| `need_iat_seq` | `iat_stats \| sequence_stats \| raw_sequences` | iat_seq circular buffer push |

### 子开关与主开关的依赖

子开关受主开关约束。当主开关关闭时，对应的子开关即使配置为 1 也不会生效：

| 主开关 | 受约束的子开关 |
|--------|--------------|
| `enable_basic` | `enable_basic_ratios`, `enable_basic_payload_dir`, `enable_basic_first_n`, `enable_basic_pkt_length` |
| `enable_iat` | `enable_iat_stats`, `enable_iat_fwd_bwd`, `enable_iat_active`, `enable_iat_response` |
| `enable_protocol` | `enable_protocol_tcp_flags`, `enable_protocol_tcp_window`, `enable_protocol_ip`, `enable_protocol_udp`, `enable_protocol_port` |
| `enable_payload` | `enable_payload_size`, `enable_payload_content`, `enable_payload_hist`, `enable_payload_magic`, `enable_payload_stats` |
| `enable_sequence` | `enable_sequence_stats`, `enable_raw_sequences`, `enable_chunk_sequences`, `enable_rate_sequences` |
| `enable_fft` | `enable_fft_global`, `enable_fft_fwd`, `enable_fft_bwd` |
| `enable_window` | `enable_window_pkt`, `enable_window_byte` |
| `enable_behavior` | `enable_behavior_basic`, `enable_behavior_pattern`, `enable_behavior_bitrate` |

配置加载时会检测并打印 WARNING 日志。

### 子开关之间的隐式依赖

某些子开关之间存在数据依赖关系：

| 开关 A | 依赖的开关 B | 原因 |
|--------|-------------|------|
| `enable_basic_pkt_length` | (无) | 独立 |
| `enable_basic_first_n` | (无) | 独立 |
| `enable_basic_payload_dir` | (无) | 独立 |
| `enable_iat_stats` | (无) | 独立 |
| `enable_iat_fwd_bwd` | (无) | 独立 |
| `enable_iat_active` | (无) | 独立，但使用 IAT 时间差 |
| `enable_iat_response` | (无) | 独立 |
| `enable_fft_global` | `need_pkt_len_seq` | FFT 需要包长序列数据 |
| `enable_fft_fwd` | `need_fwd_bwd_pkt_lens` | 前向 FFT 需要前向包长数组 |
| `enable_fft_bwd` | `need_fwd_bwd_pkt_lens` | 后向 FFT 需要后向包长数组 |
| `enable_sequence_stats` | `need_pkt_len_seq`, `need_dir_seq` | 序列统计需要包长和方向序列 |
| `enable_raw_sequences` | `need_pkt_len_seq`, `need_dir_seq`, `need_ts_seq`, `need_l3_l4_payload_seq` | 原始序列输出需要所有序列 buffer |
| `enable_chunk_sequences` | `need_pkt_len_seq`, `need_dir_seq`, `need_ts_seq` | chunk 序列需要包长、方向、时间戳 |
| `enable_rate_sequences` | `need_pkt_len_seq`, `need_dir_seq`, `need_ts_seq` | 速率序列需要包长、方向、时间戳 |
| `enable_behavior_pattern` | `need_window_update` | 流模式需要窗口统计数据 |
| `enable_behavior_bitrate` | `need_window_update` | 比特率统计需要窗口数据 |
| `enable_protocol_port` | `enable_protocol_tcp_flags` (部分) | 端口特征读取 TCP 重传状态 |

**注意**：`last_arrival_us` 时间戳始终更新，不受任何开关控制（burst 模块依赖它计算 IAT）。

### per-packet 计算开销对照表

| per-packet 操作 | 受控开关 | 每包开销 | 关闭后节省 |
|----------------|---------|---------|-----------|
| `calc_payload_stats()` (熵+压缩) | `enable_payload_stats` | **极高** (zlib compress + 256x256 矩阵) | **最大** |
| `calc_payload_magic()` | `enable_payload_magic` | 中 | 中 |
| `calc_packet_length_stats()` 全量 | 多个 `need_*` | 中-高 | 中-高 |
| `calc_basic_counters()` payload 部分 | `need_payload_dir` | 中 | 中 |
| `calc_iat_update()` 全量 | 多个 `need_*` | 中 | 中 |
| `calc_burst_update()` | `enable_burst` | 低-中 | 低-中 |
| `calc_window_update()` | `need_window_update` | 低 | 低 |
| circular buffer push × 5 | 多个 `need_*` | 低 | 低 |
| `calc_protocol_tcp_update()` | `enable_protocol_tcp_flags/window` | 低 | 低 |
| `calc_response_delay()` | `enable_iat_response` | 低 | 低 |

### 推荐高性能配置

```ini
[FEATURES]
# === 极致性能：只保留流标识 + 基础比率 ===
enable_basic = 1
enable_basic_ratios = 1
enable_basic_payload_dir = 0    # 跳过 payload 统计采集
enable_basic_first_n = 0        # 跳过前 N 包记录
enable_basic_pkt_length = 0     # 跳过包长统计采集

enable_iat = 0                  # 关闭整个 IAT 模块

enable_burst = 0

enable_protocol = 1
enable_protocol_tcp_flags = 1   # 保留 TCP 标志
enable_protocol_tcp_window = 0
enable_protocol_ip = 0
enable_protocol_udp = 0
enable_protocol_port = 0

enable_payload = 0              # 关闭整个 Payload 模块（跳过熵/压缩）

enable_sequence = 0             # 关闭整个序列模块

enable_fft = 0                  # 关闭 FFT

enable_window = 0               # 关闭窗口

enable_behavior = 1
enable_behavior_basic = 1       # 保留基础行为
enable_behavior_pattern = 0
enable_behavior_bitrate = 0
```

## 四、改动文件清单

| 文件 | 改动内容 |
|------|----------|
| `src/feature_config.h` | 结构体加 28 个子开关字段 + 10 个复合依赖标志 (`need_*`) |
| `src/feature_config.cpp` | `feat_config_init` 加默认值 + `feat_config_read` 加 INI 读取 + 复合标志自动计算 + 依赖校验日志 |
| `src/calc_basic.cpp` | `calc_derived_basic` 按子开关分段输出；`calc_basic_counters` 和 `calc_packet_length_stats` 按开关跳过 per-packet 采集 |
| `src/calc_iat.cpp` | `calc_derived_iat` 按子开关分段输出；`calc_iat_update` 按开关跳过 per-packet 采集（时间戳始终更新） |
| `src/calc_protocol.cpp` | `calc_derived_protocol` 按子开关分段输出 |
| `src/calc_sequence.cpp` | 按子开关分段输出，替代 output_raw_seq |
| `src/ti_features.cpp` | `traffic_process` 中 circular buffer push 和 window_update 按 `need_*` 标志跳过 |
| `bin/ti_features.conf` | 更新 `[FEATURES]` 配置段 |

---

## 五、完整特征清单（按子开关分组）

### 流标识特征 (始终输出，不受开关控制，~18 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | createtime | number | 流创建时间 |
| 2 | src_ip | string | 源 IP |
| 3 | dst_ip | string | 目的 IP |
| 4 | src_port | number | 源端口 |
| 5 | dst_port | number | 目的端口 |
| 6 | trans_proto | number | 传输协议 |
| 7 | stream_dir | number | 流方向 |
| 8 | sni | string | SNI (SSL/QUIC) |
| 9 | total_packets | number | 总包数 |
| 10 | fwd_packets | number | 前向包数 |
| 11 | bwd_packets | number | 后向包数 |
| 12 | total_bytes | number | 总字节数 |
| 13 | fwd_bytes | number | 前向字节数 |
| 14 | bwd_bytes | number | 后向字节数 |
| 15 | tcp_syn_count | number | SYN 计数 |
| 16 | tcp_fin_count | number | FIN 计数 |
| 17 | tcp_rst_count | number | RST 计数 |
| 18 | tcp_ack_count | number | ACK 计数 |

---

### 基础模块 (enable_basic_*)

#### `enable_basic_ratios` -- 基础比率 (5 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | fwd_bwd_packet_ratio | number | 前/后向包数比 |
| 2 | fwd_bwd_byte_ratio | number | 前/后向字节比 |
| 3 | avg_packet_length | number | 平均包长 |
| 4 | fwd_avg_packet_length | number | 前向平均包长 |
| 5 | bwd_avg_packet_length | number | 后向平均包长 |

#### `enable_basic_payload_dir` -- 前/后向 Payload 统计 (20 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | fwd_total_payload | number | 前向总载荷 |
| 2 | fwd_payload_mean | number | 前向载荷均值 |
| 3 | fwd_payload_std | number | 前向载荷标准差 |
| 4 | fwd_payload_min | number | 前向载荷最小值 |
| 5 | fwd_payload_max | number | 前向载荷最大值 |
| 6 | fwd_payload_skew | number | 前向载荷偏度 |
| 7 | fwd_payload_kurt | number | 前向载荷峰度 |
| 8 | fwd_payload_median | number | 前向载荷中位数 |
| 9 | fwd_payload_q1 | number | 前向载荷 Q1 |
| 10 | fwd_payload_q3 | number | 前向载荷 Q3 |
| 11 | bwd_total_payload | number | 后向总载荷 |
| 12 | bwd_payload_mean | number | 后向载荷均值 |
| 13 | bwd_payload_std | number | 后向载荷标准差 |
| 14 | bwd_payload_min | number | 后向载荷最小值 |
| 15 | bwd_payload_max | number | 后向载荷最大值 |
| 16 | bwd_payload_skew | number | 后向载荷偏度 |
| 17 | bwd_payload_kurt | number | 后向载荷峰度 |
| 18 | bwd_payload_median | number | 后向载荷中位数 |
| 19 | bwd_payload_q1 | number | 后向载荷 Q1 |
| 20 | bwd_payload_q3 | number | 后向载荷 Q3 |

#### `enable_basic_first_n` -- 前 N 包统计 (9 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | first_n_packets_length_mean | number | 前N包长度均值 |
| 2 | first_n_packets_length_min | number | 前N包长度最小 |
| 3 | first_n_packets_length_max | number | 前N包长度最大 |
| 4 | first_n_packets_length_std | number | 前N包长度标准差 |
| 5 | first_n_packets_length_median | number | 前N包长度中位数 |
| 6 | first_n_packets_length_q1 | number | 前N包长度 Q1 |
| 7 | first_n_packets_length_q3 | number | 前N包长度 Q3 |
| 8 | first_n_packets_length_skew | number | 前N包长度偏度 |
| 9 | first_n_packets_length_kurt | number | 前N包长度峰度 |

#### `enable_basic_pkt_length` -- 包长统计 (22 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | small_packet_ratio | number | 小包比例 |
| 2 | large_packet_ratio | number | 大包比例 |
| 3 | fwd_packet_length_mean | number | 前向包长均值 |
| 4 | fwd_packet_length_std | number | 前向包长标准差 |
| 5 | fwd_packet_length_min | number | 前向包长最小 |
| 6 | fwd_packet_length_max | number | 前向包长最大 |
| 7 | fwd_packet_length_median | number | 前向包长中位数 |
| 8 | fwd_packet_length_q1 | number | 前向包长 Q1 |
| 9 | fwd_packet_length_q3 | number | 前向包长 Q3 |
| 10 | fwd_packet_length_cv | number | 前向包长变异系数 |
| 11 | fwd_packet_length_skew | number | 前向包长偏度 |
| 12 | fwd_packet_length_kurt | number | 前向包长峰度 |
| 13 | bwd_packet_length_mean | number | 后向包长均值 |
| 14 | bwd_packet_length_std | number | 后向包长标准差 |
| 15 | bwd_packet_length_min | number | 后向包长最小 |
| 16 | bwd_packet_length_max | number | 后向包长最大 |
| 17 | bwd_packet_length_median | number | 后向包长中位数 |
| 18 | bwd_packet_length_q1 | number | 后向包长 Q1 |
| 19 | bwd_packet_length_q3 | number | 后向包长 Q3 |
| 20 | bwd_packet_length_cv | number | 后向包长变异系数 |
| 21 | bwd_packet_length_skew | number | 后向包长偏度 |
| 22 | bwd_packet_length_kurt | number | 后向包长峰度 |

---

### IAT 模块 (enable_iat_*)

#### `enable_iat_stats` -- IAT 统计 (35 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | iat_mean | number | IAT 均值 |
| 2 | iat_std | number | IAT 标准差 |
| 3 | iat_min | number | IAT 最小 |
| 4 | iat_max | number | IAT 最大 |
| 5 | iat_skew | number | IAT 偏度 |
| 6 | iat_kurt | number | IAT 峰度 |
| 7 | iat_entropy | number | IAT 熵 |
| 8 | iat_autocorr_lag_1 | number | IAT 自相关 lag1 |
| 9 | iat_autocorr_lag_2 | number | IAT 自相关 lag2 |
| 10 | iat_autocorr_lag_3 | number | IAT 自相关 lag3 |
| 11 | iat_autocorr_lag_4 | number | IAT 自相关 lag4 |
| 12 | iat_autocorr_lag_5 | number | IAT 自相关 lag5 |
| 13 | iat_median | number | IAT 中位数 |
| 14 | iat_q1 | number | IAT Q1 |
| 15 | iat_q3 | number | IAT Q3 |
| 16 | interval_ratio_mean | number | 间隔比均值 |
| 17 | interval_coefficient_of_variation | number | IAT 变异系数 |
| 18 | iat_sequence_mean | number | IAT 序列均值 |
| 19 | iat_sequence_std | number | IAT 序列标准差 |
| 20 | iat_sequence_min | number | IAT 序列最小 |
| 21 | iat_sequence_max | number | IAT 序列最大 |
| 22 | iat_sequence_median | number | IAT 序列中位数 |
| 23 | iat_sequence_q1 | number | IAT 序列 Q1 |
| 24 | iat_sequence_q3 | number | IAT 序列 Q3 |
| 25 | iat_sequence_skew | number | IAT 序列偏度 |
| 26 | iat_sequence_kurt | number | IAT 序列峰度 |
| 27 | log_iat_sequence_mean | number | log-IAT 序列均值 |
| 28 | log_iat_sequence_std | number | log-IAT 序列标准差 |
| 29 | log_iat_sequence_min | number | log-IAT 序列最小 |
| 30 | log_iat_sequence_max | number | log-IAT 序列最大 |
| 31 | log_iat_sequence_median | number | log-IAT 序列中位数 |
| 32 | log_iat_sequence_q1 | number | log-IAT 序列 Q1 |
| 33 | log_iat_sequence_q3 | number | log-IAT 序列 Q3 |
| 34 | log_iat_sequence_skew | number | log-IAT 序列偏度 |
| 35 | log_iat_sequence_kurt | number | log-IAT 序列峰度 |

#### `enable_iat_fwd_bwd` -- 前/后向 IAT (20 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | fwd_iat_mean | number | 前向 IAT 均值 |
| 2 | fwd_iat_std | number | 前向 IAT 标准差 |
| 3 | fwd_iat_cv | number | 前向 IAT 变异系数 |
| 4 | fwd_iat_min | number | 前向 IAT 最小 |
| 5 | fwd_iat_max | number | 前向 IAT 最大 |
| 6 | fwd_iat_skew | number | 前向 IAT 偏度 |
| 7 | fwd_iat_kurt | number | 前向 IAT 峰度 |
| 8 | fwd_iat_median | number | 前向 IAT 中位数 |
| 9 | fwd_iat_q1 | number | 前向 IAT Q1 |
| 10 | fwd_iat_q3 | number | 前向 IAT Q3 |
| 11 | bwd_iat_mean | number | 后向 IAT 均值 |
| 12 | bwd_iat_std | number | 后向 IAT 标准差 |
| 13 | bwd_iat_cv | number | 后向 IAT 变异系数 |
| 14 | bwd_iat_min | number | 后向 IAT 最小 |
| 15 | bwd_iat_max | number | 后向 IAT 最大 |
| 16 | bwd_iat_skew | number | 后向 IAT 偏度 |
| 17 | bwd_iat_kurt | number | 后向 IAT 峰度 |
| 18 | bwd_iat_median | number | 后向 IAT 中位数 |
| 19 | bwd_iat_q1 | number | 后向 IAT Q1 |
| 20 | bwd_iat_q3 | number | 后向 IAT Q3 |

#### `enable_iat_active` -- 活跃/空闲时间 (3 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | active_time | number | 活跃时间(秒) |
| 2 | idle_time | number | 空闲时间(秒) |
| 3 | active_time_ratio | number | 活跃时间比 |

#### `enable_iat_response` -- 响应延迟 (11 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | response_delay_mean | number | 响应延迟均值 |
| 2 | response_delay_min | number | 响应延迟最小 |
| 3 | response_delay_max | number | 响应延迟最大 |
| 4 | response_delay_std | number | 响应延迟标准差 |
| 5 | response_delay_median | number | 响应延迟中位数 |
| 6 | response_delay_q1 | number | 响应延迟 Q1 |
| 7 | response_delay_q3 | number | 响应延迟 Q3 |
| 8 | response_delay_skew | number | 响应延迟偏度 |
| 9 | response_delay_kurt | number | 响应延迟峰度 |
| 10 | is_interactive_session | number | 是否交互式会话 |
| 11 | has_regular_intervals | number | 是否有规律间隔 |

---

### Burst 模块 (enable_burst, 8 维，保持不变)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | burst_count | number | 突发次数 |
| 2 | avg_burst_duration | number | 平均突发持续时间 |
| 3 | avg_burst_size | number | 平均突发大小 |
| 4 | burst_size_std | number | 突发大小标准差 |
| 5 | burst_interval_mean | number | 突发间隔均值 |
| 6 | burst_packet_count_mean | number | 突发包数均值 |
| 7 | burst_packet_rate_peak | number | 突发峰值包速率 |
| 8 | burstiness_index | number | 突发性指数 |

---

### 协议模块 (enable_protocol_*)

#### `enable_protocol_tcp_flags` -- TCP 标志统计 (20 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | tcp_psh_count | number | TCP PSH 计数 |
| 2 | tcp_urg_count | number | TCP URG 计数 |
| 3 | window_update_frequency | number | 窗口更新频率 |
| 4 | tcp_syn_packets | number | SYN 包数 |
| 5 | tcp_syn_ack_packets | number | SYN-ACK 包数 |
| 6 | tcp_ack_only_count | number | 纯 ACK 包数 |
| 7 | tcp_payload_packets | number | 有载荷的 TCP 包数 |
| 8 | tcp_zero_window_count | number | 零窗口计数 |
| 9 | icmp_packet_count | number | ICMP 包数 |
| 10 | l2_packets_count | number | L2 包数 |
| 11 | header_anomaly_count | number | 头部异常计数 |
| 12 | tcp_header_anomaly_count | number | TCP 头部异常计数 |
| 13 | header_anomaly_ratio | number | 头部异常比例 |
| 14 | tcp_connection_success_rate | number | TCP 连接成功率 |
| 15 | tcp_syn_ratio | number | SYN 比例 |
| 16 | tcp_rst_ratio | number | RST 比例 |
| 17 | tcp_ack_only_ratio | number | 纯 ACK 比例 |
| 18 | tcp_payload_packet_ratio | number | 有载荷包比例 |
| 19 | tcp_syn_only_ratio | number | 纯 SYN 比例 |
| 20 | tcp_half_open_ratio | number | 半开连接比例 |

#### `enable_protocol_tcp_window` -- TCP 窗口统计 (9 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | tcp_window_mean | number | TCP 窗口均值 |
| 2 | tcp_window_std | number | TCP 窗口标准差 |
| 3 | tcp_window_min | number | TCP 窗口最小 |
| 4 | tcp_window_max | number | TCP 窗口最大 |
| 5 | tcp_window_skew | number | TCP 窗口偏度 |
| 6 | tcp_window_kurt | number | TCP 窗口峰度 |
| 7 | tcp_window_median | number | TCP 窗口中位数 |
| 8 | tcp_window_q1 | number | TCP 窗口 Q1 |
| 9 | tcp_window_q3 | number | TCP 窗口 Q3 |

#### `enable_protocol_ip` -- IP TTL/ToS 统计 (22 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | ip_ttl_mean | number | TTL 均值 |
| 2 | ip_ttl_std | number | TTL 标准差 |
| 3 | ip_ttl_min | number | TTL 最小 |
| 4 | ip_ttl_max | number | TTL 最大 |
| 5 | ip_ttl_skew | number | TTL 偏度 |
| 6 | ip_ttl_kurt | number | TTL 峰度 |
| 7 | ip_ttl_median | number | TTL 中位数 |
| 8 | ip_ttl_q1 | number | TTL Q1 |
| 9 | ip_ttl_q3 | number | TTL Q3 |
| 10 | ip_ttl_eq_32_count | number | TTL=32 计数 |
| 11 | ip_ttl_eq_64_count | number | TTL=64 计数 |
| 12 | ip_ttl_eq_128_count | number | TTL=128 计数 |
| 13 | ip_ttl_eq_255_count | number | TTL=255 计数 |
| 14 | ip_tos_mean | number | ToS 均值 |
| 15 | ip_tos_std | number | ToS 标准差 |
| 16 | ip_tos_min | number | ToS 最小 |
| 17 | ip_tos_max | number | ToS 最大 |
| 18 | ip_tos_skew | number | ToS 偏度 |
| 19 | ip_tos_kurt | number | ToS 峰度 |
| 20 | ip_tos_median | number | ToS 中位数 |
| 21 | ip_tos_q1 | number | ToS Q1 |
| 22 | ip_tos_q3 | number | ToS Q3 |

#### `enable_protocol_udp` -- UDP 长度统计 (9 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | udp_length_mean | number | UDP 长度均值 |
| 2 | udp_length_std | number | UDP 长度标准差 |
| 3 | udp_length_min | number | UDP 长度最小 |
| 4 | udp_length_max | number | UDP 长度最大 |
| 5 | udp_length_skew | number | UDP 长度偏度 |
| 6 | udp_length_kurt | number | UDP 长度峰度 |
| 7 | udp_length_median | number | UDP 长度中位数 |
| 8 | udp_length_q1 | number | UDP 长度 Q1 |
| 9 | udp_length_q3 | number | UDP 长度 Q3 |

#### `enable_protocol_port` -- 端口/IP 特征 + 历史兼容 (25 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | src_ip_is_private | number | 源 IP 是否私有 |
| 2 | dst_ip_is_private | number | 目的 IP 是否私有 |
| 3 | ip_fragmentation_flag | number | IP 分片标志 |
| 4 | src_port_is_ephemeral | number | 源端口是否临时 |
| 5 | dst_port_is_ephemeral | number | 目的端口是否临时 |
| 6 | well_known_dst_port_ratio | number | 知名目的端口比 |
| 7 | well_known_src_port_ratio | number | 知名源端口比 |
| 8 | unique_src_ports | number | 唯一源端口数 |
| 9 | unique_dst_ports | number | 唯一目的端口数 |
| 10 | is_common_service | number | 是否常见服务 |
| 11 | quic_usage_flag | number | QUIC 使用标志 |
| 12 | src_port_protocol | string | 源端口协议名 |
| 13 | src_port_range | string | 源端口范围 |
| 14 | dst_port_protocol | string | 目的端口协议名 |
| 15 | avg_retransmission_rate | number | 平均重传率 |
| 16 | rst_trigger_interval | number | RST 触发间隔 |
| 17 | tcpackfaultcnt | number | (遗留兼容) |
| 18 | tcpbflgtmx | number | (遗留兼容) |
| 19 | tcpflwlssackrcvdbytes | number | (遗留兼容) |
| 20 | tcpiseqn | number | (遗留兼容) |
| 21 | tcpinitwinsz | number | (遗留兼容) |
| 22 | tcppackcnt | number | (遗留兼容) |
| 23 | tcppseqcnt | number | (遗留兼容) |
| 24 | tcpseqfaultcnt | number | (遗留兼容) |
| 25 | tcpseqsntbytes | number | (遗留兼容) |

---

### Payload 模块 (enable_payload_*)

#### `enable_payload_size` -- 载荷大小统计 (18 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | payload_size_mean | number | 载荷大小均值 |
| 2 | payload_size_std | number | 载荷大小标准差 |
| 3 | payload_size_min | number | 载荷大小最小 |
| 4 | payload_size_max | number | 载荷大小最大 |
| 5 | payload_size_skew | number | 载荷大小偏度 |
| 6 | payload_size_kurt | number | 载荷大小峰度 |
| 7 | payload_size_median | number | 载荷大小中位数 |
| 8 | payload_size_q1 | number | 载荷大小 Q1 |
| 9 | payload_size_q3 | number | 载荷大小 Q3 |
| 10 | payload_size_diff_mean | number | 载荷大小差分均值 |
| 11 | payload_size_diff_std | number | 载荷大小差分标准差 |
| 12 | payload_size_diff_min | number | 载荷大小差分最小 |
| 13 | payload_size_diff_max | number | 载荷大小差分最大 |
| 14 | payload_size_diff_skew | number | 载荷大小差分偏度 |
| 15 | payload_size_diff_kurt | number | 载荷大小差分峰度 |
| 16 | payload_size_diff_median | number | 载荷大小差分中位数 |
| 17 | payload_size_diff_q1 | number | 载荷大小差分 Q1 |
| 18 | payload_size_diff_q3 | number | 载荷大小差分 Q3 |

#### `enable_payload_content` -- 内容特征 (3 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | payload_printable_ratio | number | 可打印字符比 |
| 2 | payload_alnum_ratio | number | 字母数字比 |
| 3 | payload_non_empty_ratio | number | 非空载荷比 |

#### `enable_payload_hist` -- 字节直方图 + 字节统计 (25 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | payload_hist_bin_0 | number | 字节直方图 bin0 |
| 2 | payload_hist_bin_1 | number | 字节直方图 bin1 |
| 3 | payload_hist_bin_2 | number | 字节直方图 bin2 |
| 4 | payload_hist_bin_3 | number | 字节直方图 bin3 |
| 5 | payload_hist_bin_4 | number | 字节直方图 bin4 |
| 6 | payload_hist_bin_5 | number | 字节直方图 bin5 |
| 7 | payload_hist_bin_6 | number | 字节直方图 bin6 |
| 8 | payload_hist_bin_7 | number | 字节直方图 bin7 |
| 9 | payload_hist_bin_8 | number | 字节直方图 bin8 |
| 10 | payload_hist_bin_9 | number | 字节直方图 bin9 |
| 11 | payload_hist_bin_10 | number | 字节直方图 bin10 |
| 12 | payload_hist_bin_11 | number | 字节直方图 bin11 |
| 13 | payload_hist_bin_12 | number | 字节直方图 bin12 |
| 14 | payload_hist_bin_13 | number | 字节直方图 bin13 |
| 15 | payload_hist_bin_14 | number | 字节直方图 bin14 |
| 16 | payload_hist_bin_15 | number | 字节直方图 bin15 |
| 17 | payload_byte_mean | number | 载荷字节均值 |
| 18 | payload_byte_std | number | 载荷字节标准差 |
| 19 | payload_byte_min | number | 载荷字节最小 |
| 20 | payload_byte_max | number | 载荷字节最大 |
| 21 | payload_byte_skew | number | 载荷字节偏度 |
| 22 | payload_byte_kurt | number | 载荷字节峰度 |
| 23 | payload_byte_q1 | number | 载荷字节 Q1 |
| 24 | payload_byte_median | number | 载荷字节中位数 |
| 25 | payload_byte_q3 | number | 载荷字节 Q3 |

#### `enable_payload_magic` -- 魔数/协议检测 (18 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | payload_magic_http_get | number | HTTP GET 魔数检测 |
| 2 | payload_magic_http_post | number | HTTP POST 魔数检测 |
| 3 | payload_magic_tls_1_0 | number | TLS 1.0 魔数检测 |
| 4 | payload_magic_tls_1_1 | number | TLS 1.1 魔数检测 |
| 5 | payload_magic_tls_1_2 | number | TLS 1.2 魔数检测 |
| 6 | payload_magic_tls_1_3 | number | TLS 1.3 魔数检测 |
| 7 | payload_magic_ssh | number | SSH 魔数检测 |
| 8 | payload_magic_jpeg | number | JPEG 魔数检测 |
| 9 | payload_magic_png | number | PNG 魔数检测 |
| 10 | payload_magic_gif | number | GIF 魔数检测 |
| 11 | payload_magic_unknown | number | 未知魔数检测 |
| 12 | payload_looks_like_http | number | 启发式 HTTP 检测 |
| 13 | payload_looks_like_json | number | 启发式 JSON 检测 |
| 14 | payload_looks_like_xml | number | 启发式 XML 检测 |
| 15 | payload_looks_like_tls | number | 启发式 TLS 检测 |
| 16 | payload_looks_like_ssh | number | 启发式 SSH 检测 |
| 17 | payload_chi_square | number | 载荷卡方值 |
| 18 | payload_first_64_entropy | number | 前64字节熵 |

#### `enable_payload_stats` -- 每包高级统计 (22 维，可关闭提升性能)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | payload_entropy | number | 载荷香农熵 |
| 2 | payload_markov_entropy | number | 载荷马尔可夫熵 |
| 3 | payload_compression_ratio | number | 载荷压缩率 |
| 4 | payload_autocorrelation_lag1 | number | 载荷自相关 lag1 |
| 5 | payload_first_128_entropy | number | 前128字节熵 |
| 6 | payload_first_256_entropy | number | 前256字节熵 |
| 7 | payload_length_mod_2_mean | number | 载荷长度 mod2 均值 |
| 8 | payload_length_mod_2_std | number | 载荷长度 mod2 标准差 |
| 9 | payload_length_mod_4_mean | number | 载荷长度 mod4 均值 |
| 10 | payload_length_mod_4_std | number | 载荷长度 mod4 标准差 |
| 11 | payload_length_mod_8_mean | number | 载荷长度 mod8 均值 |
| 12 | payload_length_mod_8_std | number | 载荷长度 mod8 标准差 |
| 13 | payload_length_mod_16_mean | number | 载荷长度 mod16 均值 |
| 14 | payload_length_mod_16_std | number | 载荷长度 mod16 标准差 |
| 15 | payload_length_mod_32_mean | number | 载荷长度 mod32 均值 |
| 16 | payload_length_mod_32_std | number | 载荷长度 mod32 标准差 |
| 17 | payload_length_mod_64_mean | number | 载荷长度 mod64 均值 |
| 18 | payload_length_mod_64_std | number | 载荷长度 mod64 标准差 |
| 19 | payload_length_mod_128_mean | number | 载荷长度 mod128 均值 |
| 20 | payload_length_mod_128_std | number | 载荷长度 mod128 标准差 |
| 21 | payload_length_mod_256_mean | number | 载荷长度 mod256 均值 |
| 22 | payload_length_mod_256_std | number | 载荷长度 mod256 标准差 |

---

### 序列模块 (enable_sequence_*)

#### `enable_sequence_stats` -- 序列统计特征 (45 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | packet_length_seq_len | number | 包长序列长度 |
| 2 | packet_iat_seq_len | number | IAT 序列长度 |
| 3 | packet_direction_seq_len | number | 方向序列长度 |
| 4 | length_autocorr_lag_1 | number | 包长自相关 lag1 |
| 5 | length_autocorr_lag_2 | number | 包长自相关 lag2 |
| 6 | length_autocorr_lag_3 | number | 包长自相关 lag3 |
| 7 | length_autocorr_lag_4 | number | 包长自相关 lag4 |
| 8 | length_autocorr_lag_5 | number | 包长自相关 lag5 |
| 9 | length_run_mean | number | 游程长度均值 |
| 10 | length_run_std | number | 游程长度标准差 |
| 11 | length_run_min | number | 游程长度最小 |
| 12 | length_run_max | number | 游程长度最大 |
| 13 | length_run_median | number | 游程长度中位数 |
| 14 | length_run_q1 | number | 游程长度 Q1 |
| 15 | length_run_q3 | number | 游程长度 Q3 |
| 16 | length_run_skew | number | 游程长度偏度 |
| 17 | length_run_kurt | number | 游程长度峰度 |
| 18 | length_iat_correlation | number | 包长-IAT 相关性 |
| 19 | direction_sequence_entropy | number | 方向序列熵 |
| 20 | direction_change_frequency | number | 方向变化频率 |
| 21 | direction_transition_unique_count | number | 方向转换唯一数 |
| 22 | length_direction_correlation | number | 包长-方向相关性 |
| 23 | length_sequence_unique_ratio | number | 包长唯一值比 |
| 24 | packet_length_entropy | number | 包长序列熵 |
| 25 | packet_length_percentile_1 | number | 包长百分位 P1 |
| 26 | packet_length_percentile_5 | number | 包长百分位 P5 |
| 27 | packet_length_percentile_10 | number | 包长百分位 P10 |
| 28 | packet_length_percentile_90 | number | 包长百分位 P90 |
| 29 | packet_length_percentile_95 | number | 包长百分位 P95 |
| 30 | packet_length_percentile_99 | number | 包长百分位 P99 |
| 31 | packet_length_percentile_100 | number | 包长百分位 P100 |
| 32 | lz78_complexity_index | number | LZ78 复杂度 |
| 33 | length_sequence_complexity | number | 序列复杂度 |
| 34 | hurst_exponent_estimate | number | Hurst 指数估计 |
| 35 | periodic_dominant_freq_ratio | number | 周期性主频比 |
| 36 | has_strong_periodicity | number | 是否强周期性 |
| 37 | custom_protocol_length_pattern | string | 协议长度模式(占位) |
| 38 | ar_prediction_mse | number | AR 预测 MSE(占位) |
| 39 | ar_prediction_mae | number | AR 预测 MAE(占位) |
| 40-44 | top_1_bigram_*_freq ~ top_5_bigram_*_freq | number | Bigram 频率 (45 维) |

#### `enable_raw_sequences` -- 原始序列输出 (4 字段)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | packet_length_seq | string | 原始包长序列 |
| 2 | packet_iat_seq | string | 原始 IAT 序列 |
| 3 | packet_direction_seq | string | 原始方向序列 |
| 4 | l2l3l4pl_iat | string | 复合序列 |

#### `enable_chunk_sequences` -- Chunk 序列 (2 字段)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | dl_chunk_seq | string | 下行 chunk 序列 |
| 2 | dl_chunk_seq_len | number | 下行 chunk 序列长度 |

#### `enable_rate_sequences` -- 速率序列 (4 字段)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | uplink_rate_seq | string | 上行速率序列 |
| 2 | downlink_rate_seq | string | 下行速率序列 |
| 3 | uplink_rate_seq_len | number | 上行速率序列长度 |
| 4 | downlink_rate_seq_len | number | 下行速率序列长度 |

---

### FFT 模块 (enable_fft_*)

#### `enable_fft_global` -- 全局 FFT (20 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | all_fft_freq_0_idx | number | 全局 FFT 主频索引 0 |
| 2 | all_fft_mag_0 | number | 全局 FFT 主频幅度 0 |
| 3 | all_fft_freq_1_idx | number | 全局 FFT 主频索引 1 |
| 4 | all_fft_mag_1 | number | 全局 FFT 主频幅度 1 |
| 5 | all_fft_freq_2_idx | number | 全局 FFT 主频索引 2 |
| 6 | all_fft_mag_2 | number | 全局 FFT 主频幅度 2 |
| 7 | all_fft_freq_3_idx | number | 全局 FFT 主频索引 3 |
| 8 | all_fft_mag_3 | number | 全局 FFT 主频幅度 3 |
| 9 | all_fft_freq_4_idx | number | 全局 FFT 主频索引 4 |
| 10 | all_fft_mag_4 | number | 全局 FFT 主频幅度 4 |
| 11 | all_fft_magnitude_mean | number | 全局 FFT 幅度均值 |
| 12 | all_fft_magnitude_std | number | 全局 FFT 幅度标准差 |
| 13 | all_fft_magnitude_min | number | 全局 FFT 幅度最小 |
| 14 | all_fft_magnitude_max | number | 全局 FFT 幅度最大 |
| 15 | all_fft_magnitude_median | number | 全局 FFT 幅度中位数 |
| 16 | all_fft_magnitude_q1 | number | 全局 FFT 幅度 Q1 |
| 17 | all_fft_magnitude_q3 | number | 全局 FFT 幅度 Q3 |
| 18 | all_fft_magnitude_skew | number | 全局 FFT 幅度偏度 |
| 19 | all_fft_magnitude_kurt | number | 全局 FFT 幅度峰度 |
| 20 | psd_peak | number | PSD 峰值 |

#### `enable_fft_fwd` -- 前向 FFT (19 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | fwd_fft_freq_0_idx | number | 前向 FFT 主频索引 0 |
| 2 | fwd_fft_mag_0 | number | 前向 FFT 主频幅度 0 |
| 3 | fwd_fft_freq_1_idx | number | 前向 FFT 主频索引 1 |
| 4 | fwd_fft_mag_1 | number | 前向 FFT 主频幅度 1 |
| 5 | fwd_fft_freq_2_idx | number | 前向 FFT 主频索引 2 |
| 6 | fwd_fft_mag_2 | number | 前向 FFT 主频幅度 2 |
| 7 | fwd_fft_freq_3_idx | number | 前向 FFT 主频索引 3 |
| 8 | fwd_fft_mag_3 | number | 前向 FFT 主频幅度 3 |
| 9 | fwd_fft_freq_4_idx | number | 前向 FFT 主频索引 4 |
| 10 | fwd_fft_mag_4 | number | 前向 FFT 主频幅度 4 |
| 11 | fwd_fft_magnitude_mean | number | 前向 FFT 幅度均值 |
| 12 | fwd_fft_magnitude_std | number | 前向 FFT 幅度标准差 |
| 13 | fwd_fft_magnitude_min | number | 前向 FFT 幅度最小 |
| 14 | fwd_fft_magnitude_max | number | 前向 FFT 幅度最大 |
| 15 | fwd_fft_magnitude_median | number | 前向 FFT 幅度中位数 |
| 16 | fwd_fft_magnitude_q1 | number | 前向 FFT 幅度 Q1 |
| 17 | fwd_fft_magnitude_q3 | number | 前向 FFT 幅度 Q3 |
| 18 | fwd_fft_magnitude_skew | number | 前向 FFT 幅度偏度 |
| 19 | fwd_fft_magnitude_kurt | number | 前向 FFT 幅度峰度 |

#### `enable_fft_bwd` -- 后向 FFT (19 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | bwd_fft_freq_0_idx | number | 后向 FFT 主频索引 0 |
| 2 | bwd_fft_mag_0 | number | 后向 FFT 主频幅度 0 |
| 3 | bwd_fft_freq_1_idx | number | 后向 FFT 主频索引 1 |
| 4 | bwd_fft_mag_1 | number | 后向 FFT 主频幅度 1 |
| 5 | bwd_fft_freq_2_idx | number | 后向 FFT 主频索引 2 |
| 6 | bwd_fft_mag_2 | number | 后向 FFT 主频幅度 2 |
| 7 | bwd_fft_freq_3_idx | number | 后向 FFT 主频索引 3 |
| 8 | bwd_fft_mag_3 | number | 后向 FFT 主频幅度 3 |
| 9 | bwd_fft_freq_4_idx | number | 后向 FFT 主频索引 4 |
| 10 | bwd_fft_mag_4 | number | 后向 FFT 主频幅度 4 |
| 11 | bwd_fft_magnitude_mean | number | 后向 FFT 幅度均值 |
| 12 | bwd_fft_magnitude_std | number | 后向 FFT 幅度标准差 |
| 13 | bwd_fft_magnitude_min | number | 后向 FFT 幅度最小 |
| 14 | bwd_fft_magnitude_max | number | 后向 FFT 幅度最大 |
| 15 | bwd_fft_magnitude_median | number | 后向 FFT 幅度中位数 |
| 16 | bwd_fft_magnitude_q1 | number | 后向 FFT 幅度 Q1 |
| 17 | bwd_fft_magnitude_q3 | number | 后向 FFT 幅度 Q3 |
| 18 | bwd_fft_magnitude_skew | number | 后向 FFT 幅度偏度 |
| 19 | bwd_fft_magnitude_kurt | number | 后向 FFT 幅度峰度 |

---

### Window 模块 (enable_window_*)

#### `enable_window_pkt` -- 窗口包数统计 (9 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | window_packet_count_mean | number | 窗口包数均值 |
| 2 | window_packet_count_std | number | 窗口包数标准差 |
| 3 | window_packet_count_min | number | 窗口包数最小 |
| 4 | window_packet_count_max | number | 窗口包数最大 |
| 5 | window_packet_count_median | number | 窗口包数中位数 |
| 6 | window_packet_count_q1 | number | 窗口包数 Q1 |
| 7 | window_packet_count_q3 | number | 窗口包数 Q3 |
| 8 | window_packet_count_skew | number | 窗口包数偏度 |
| 9 | window_packet_count_kurt | number | 窗口包数峰度 |

#### `enable_window_byte` -- 窗口字节数统计 (9 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | window_byte_count_mean | number | 窗口字节数均值 |
| 2 | window_byte_count_std | number | 窗口字节数标准差 |
| 3 | window_byte_count_min | number | 窗口字节数最小 |
| 4 | window_byte_count_max | number | 窗口字节数最大 |
| 5 | window_byte_count_median | number | 窗口字节数中位数 |
| 6 | window_byte_count_q1 | number | 窗口字节数 Q1 |
| 7 | window_byte_count_q3 | number | 窗口字节数 Q3 |
| 8 | window_byte_count_skew | number | 窗口字节数偏度 |
| 9 | window_byte_count_kurt | number | 窗口字节数峰度 |

---

### Behavior 模块 (enable_behavior_*)

#### `enable_behavior_basic` -- 基础行为 (6 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | flow_duration | number | 流持续时间(秒) |
| 2 | avg_bitrate | number | 平均比特率 |
| 3 | avg_packet_rate | number | 平均包速率 |
| 4 | avg_throughput | number | 平均吞吐量 |
| 5 | is_bulk_transfer | number | 是否大流量传输 |
| 6 | short_connection_ratio | number | 短连接比例 |

#### `enable_behavior_pattern` -- 流模式 (8 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | flow_asymmetry_ratio | number | 流不对称比 |
| 2 | flow_pattern | string | 流模式 |
| 3 | has_regular_intervals | number | 是否规律间隔 |
| 4 | protocol_tcp_ratio | number | TCP 协议比 |
| 5 | protocol_udp_ratio | number | UDP 协议比 |
| 6 | protocol_icmp_ratio | number | ICMP 协议比 |
| 7 | protocol_other_ratio | number | 其他协议比 |
| 8 | peak_bitrate | number | 峰值比特率 |

#### `enable_behavior_bitrate` -- 瞬时比特率统计 (15 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | instant_bitrate_mean | number | 瞬时比特率均值 |
| 2 | instant_bitrate_std | number | 瞬时比特率标准差 |
| 3 | instant_bitrate_min | number | 瞬时比特率最小 |
| 4 | instant_bitrate_max | number | 瞬时比特率最大 |
| 5 | instant_bitrate_median | number | 瞬时比特率中位数 |
| 6 | instant_bitrate_q1 | number | 瞬时比特率 Q1 |
| 7 | instant_bitrate_q3 | number | 瞬时比特率 Q3 |
| 8 | instant_bitrate_skew | number | 瞬时比特率偏度 |
| 9 | instant_bitrate_kurt | number | 瞬时比特率峰度 |
| 10 | bitrate_percentile_25 | number | 比特率 P25 |
| 11 | bitrate_percentile_50 | number | 比特率 P50 |
| 12 | bitrate_percentile_75 | number | 比特率 P75 |
| 13 | bitrate_percentile_90 | number | 比特率 P90 |
| 14 | traffic_entropy_ts_mean | number | 流量时序熵均值 |
| 15 | traffic_entropy_ts_peak | number | 流量时序熵峰值 |

---

## 六、总计

| 模块 | 子开关 | 特征数 |
|------|--------|--------|
| 流标识 | (常开) | ~18 |
| Basic | enable_basic_ratios | 5 |
| | enable_basic_payload_dir | 20 |
| | enable_basic_first_n | 9 |
| | enable_basic_pkt_length | 22 |
| IAT | enable_iat_stats | 35 |
| | enable_iat_fwd_bwd | 20 |
| | enable_iat_active | 3 |
| | enable_iat_response | 11 |
| Burst | enable_burst | 8 |
| Protocol | enable_protocol_tcp_flags | 20 |
| | enable_protocol_tcp_window | 9 |
| | enable_protocol_ip | 22 |
| | enable_protocol_udp | 9 |
| | enable_protocol_port | 25 |
| Payload | enable_payload_size | 18 |
| | enable_payload_content | 3 |
| | enable_payload_hist | 25 |
| | enable_payload_magic | 18 |
| | enable_payload_stats | 22 |
| Sequence | enable_sequence_stats | ~45 |
| | enable_raw_sequences | 4 字段 |
| | enable_chunk_sequences | 2 字段 |
| | enable_rate_sequences | 4 字段 |
| FFT | enable_fft_global | 20 |
| | enable_fft_fwd | 19 |
| | enable_fft_bwd | 19 |
| Window | enable_window_pkt | 9 |
| | enable_window_byte | 9 |
| Behavior | enable_behavior_basic | 6 |
| | enable_behavior_pattern | 8 |
| | enable_behavior_bitrate | 15 |
| **合计** | **28 个开关** | **~530** |
