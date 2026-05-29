# ti_features 特征模块清单与可配置方案

## 一、设计思路

采用 **模块级开关 + 每包阶段可选关闭** 的两层配置：

1. **流结束阶段**：9 个 `calc_derived_*` 函数各自有开关，关了就跳过整个函数调用
2. **每包阶段**：单独对最昂贵的 `calc_payload_stats`（熵+压缩率）加开关
3. **联动逻辑**：关了 payload 的每包统计后，derived 阶段对应输出为 0

## 二、配置结构

在 `ti_features.conf` 中新增 `[FEATURES]` 配置段：

```ini
[FEATURES]
enable_basic = 1
enable_iat = 1
enable_burst = 1
enable_protocol = 1
enable_payload = 1
enable_sequence = 1
enable_fft = 1
enable_window = 1
enable_behavior = 1
enable_payload_stats = 1
```

## 三、性能优化建议（配置组合）

| 场景 | 建议配置 | 预期收益 |
|------|----------|----------|
| 最大性能 | 只开 basic + protocol，其余全关 | ~60-70% 性能恢复 |
| 保留核心 | 关 fft + sequence + payload_stats | ~40-50% 性能恢复 |
| 轻量优化 | 只关 fft | ~15-20% 性能恢复 |

## 四、改动文件清单

| 文件 | 改动内容 |
|------|----------|
| `src/feature_config.h` | 结构体加 10 个 `unsigned int` 字段 |
| `src/feature_config.cpp` | `feat_config_init` 加默认值 + `feat_config_read` 加 INI 读取 |
| `src/ti_features.cpp` | `flow_state_compute_features` 加 9 个 if + `traffic_process` DATA 阶段加 1 个 if |
| `bin/ti_features.conf` | 加 `[FEATURES]` 配置段 |

---

## 五、完整特征清单（按模块分组）

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

### Module A: `enable_basic` -- 基础计数 + 包长 (calc_basic, ~56 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | fwd_bwd_packet_ratio | number | 前/后向包数比 |
| 2 | fwd_bwd_byte_ratio | number | 前/后向字节比 |
| 3 | avg_packet_length | number | 平均包长 |
| 4 | fwd_avg_packet_length | number | 前向平均包长 |
| 5 | bwd_avg_packet_length | number | 后向平均包长 |
| 6 | fwd_total_payload | number | 前向总载荷 |
| 7 | fwd_payload_mean | number | 前向载荷均值 |
| 8 | fwd_payload_std | number | 前向载荷标准差 |
| 9 | fwd_payload_min | number | 前向载荷最小值 |
| 10 | fwd_payload_max | number | 前向载荷最大值 |
| 11 | fwd_payload_skew | number | 前向载荷偏度 |
| 12 | fwd_payload_kurt | number | 前向载荷峰度 |
| 13 | fwd_payload_median | number | 前向载荷中位数 |
| 14 | fwd_payload_q1 | number | 前向载荷 Q1 |
| 15 | fwd_payload_q3 | number | 前向载荷 Q3 |
| 16 | bwd_total_payload | number | 后向总载荷 |
| 17 | bwd_payload_mean | number | 后向载荷均值 |
| 18 | bwd_payload_std | number | 后向载荷标准差 |
| 19 | bwd_payload_min | number | 后向载荷最小值 |
| 20 | bwd_payload_max | number | 后向载荷最大值 |
| 21 | bwd_payload_skew | number | 后向载荷偏度 |
| 22 | bwd_payload_kurt | number | 后向载荷峰度 |
| 23 | bwd_payload_median | number | 后向载荷中位数 |
| 24 | bwd_payload_q1 | number | 后向载荷 Q1 |
| 25 | bwd_payload_q3 | number | 后向载荷 Q3 |
| 26 | first_n_packets_length_mean | number | 前N包长度均值 |
| 27 | first_n_packets_length_min | number | 前N包长度最小 |
| 28 | first_n_packets_length_max | number | 前N包长度最大 |
| 29 | first_n_packets_length_std | number | 前N包长度标准差 |
| 30 | first_n_packets_length_median | number | 前N包长度中位数 |
| 31 | first_n_packets_length_q1 | number | 前N包长度 Q1 |
| 32 | first_n_packets_length_q3 | number | 前N包长度 Q3 |
| 33 | first_n_packets_length_skew | number | 前N包长度偏度 |
| 34 | first_n_packets_length_kurt | number | 前N包长度峰度 |
| 35 | small_packet_ratio | number | 小包比例 |
| 36 | large_packet_ratio | number | 大包比例 |
| 37 | fwd_packet_length_mean | number | 前向包长均值 |
| 38 | fwd_packet_length_std | number | 前向包长标准差 |
| 39 | fwd_packet_length_min | number | 前向包长最小 |
| 40 | fwd_packet_length_max | number | 前向包长最大 |
| 41 | fwd_packet_length_median | number | 前向包长中位数 |
| 42 | fwd_packet_length_q1 | number | 前向包长 Q1 |
| 43 | fwd_packet_length_q3 | number | 前向包长 Q3 |
| 44 | fwd_packet_length_cv | number | 前向包长变异系数 |
| 45 | fwd_packet_length_skew | number | 前向包长偏度 |
| 46 | fwd_packet_length_kurt | number | 前向包长峰度 |
| 47 | bwd_packet_length_mean | number | 后向包长均值 |
| 48 | bwd_packet_length_std | number | 后向包长标准差 |
| 49 | bwd_packet_length_min | number | 后向包长最小 |
| 50 | bwd_packet_length_max | number | 后向包长最大 |
| 51 | bwd_packet_length_median | number | 后向包长中位数 |
| 52 | bwd_packet_length_q1 | number | 后向包长 Q1 |
| 53 | bwd_packet_length_q3 | number | 后向包长 Q3 |
| 54 | bwd_packet_length_cv | number | 后向包长变异系数 |
| 55 | bwd_packet_length_skew | number | 后向包长偏度 |
| 56 | bwd_packet_length_kurt | number | 后向包长峰度 |

---

### Module B: `enable_iat` -- 到达间隔 (calc_iat, ~69 维)

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
| 36 | fwd_iat_mean | number | 前向 IAT 均值 |
| 37 | fwd_iat_std | number | 前向 IAT 标准差 |
| 38 | fwd_iat_cv | number | 前向 IAT 变异系数 |
| 39 | fwd_iat_min | number | 前向 IAT 最小 |
| 40 | fwd_iat_max | number | 前向 IAT 最大 |
| 41 | fwd_iat_skew | number | 前向 IAT 偏度 |
| 42 | fwd_iat_kurt | number | 前向 IAT 峰度 |
| 43 | fwd_iat_median | number | 前向 IAT 中位数 |
| 44 | fwd_iat_q1 | number | 前向 IAT Q1 |
| 45 | fwd_iat_q3 | number | 前向 IAT Q3 |
| 46 | bwd_iat_mean | number | 后向 IAT 均值 |
| 47 | bwd_iat_std | number | 后向 IAT 标准差 |
| 48 | bwd_iat_cv | number | 后向 IAT 变异系数 |
| 49 | bwd_iat_min | number | 后向 IAT 最小 |
| 50 | bwd_iat_max | number | 后向 IAT 最大 |
| 51 | bwd_iat_skew | number | 后向 IAT 偏度 |
| 52 | bwd_iat_kurt | number | 后向 IAT 峰度 |
| 53 | bwd_iat_median | number | 后向 IAT 中位数 |
| 54 | bwd_iat_q1 | number | 后向 IAT Q1 |
| 55 | bwd_iat_q3 | number | 后向 IAT Q3 |
| 56 | active_time | number | 活跃时间(秒) |
| 57 | idle_time | number | 空闲时间(秒) |
| 58 | active_time_ratio | number | 活跃时间比 |
| 59 | response_delay_mean | number | 响应延迟均值 |
| 60 | response_delay_min | number | 响应延迟最小 |
| 61 | response_delay_max | number | 响应延迟最大 |
| 62 | response_delay_std | number | 响应延迟标准差 |
| 63 | response_delay_median | number | 响应延迟中位数 |
| 64 | response_delay_q1 | number | 响应延迟 Q1 |
| 65 | response_delay_q3 | number | 响应延迟 Q3 |
| 66 | response_delay_skew | number | 响应延迟偏度 |
| 67 | response_delay_kurt | number | 响应延迟峰度 |
| 68 | is_interactive_session | number | 是否交互式会话 |
| 69 | has_regular_intervals | number | 是否有规律间隔 |

---

### Module C: `enable_burst` -- 突发检测 (calc_burst, 8 维)

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

### Module D: `enable_protocol` -- 协议头 (calc_protocol, ~85 维)

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
| 21 | avg_retransmission_rate | number | 平均重传率 |
| 22 | rst_trigger_interval | number | RST 触发间隔 |
| 23 | tcp_window_mean | number | TCP 窗口均值 |
| 24 | tcp_window_std | number | TCP 窗口标准差 |
| 25 | tcp_window_min | number | TCP 窗口最小 |
| 26 | tcp_window_max | number | TCP 窗口最大 |
| 27 | tcp_window_skew | number | TCP 窗口偏度 |
| 28 | tcp_window_kurt | number | TCP 窗口峰度 |
| 29 | tcp_window_median | number | TCP 窗口中位数 |
| 30 | tcp_window_q1 | number | TCP 窗口 Q1 |
| 31 | tcp_window_q3 | number | TCP 窗口 Q3 |
| 32 | ip_ttl_mean | number | TTL 均值 |
| 33 | ip_ttl_std | number | TTL 标准差 |
| 34 | ip_ttl_min | number | TTL 最小 |
| 35 | ip_ttl_max | number | TTL 最大 |
| 36 | ip_ttl_skew | number | TTL 偏度 |
| 37 | ip_ttl_kurt | number | TTL 峰度 |
| 38 | ip_ttl_median | number | TTL 中位数 |
| 39 | ip_ttl_q1 | number | TTL Q1 |
| 40 | ip_ttl_q3 | number | TTL Q3 |
| 41 | ip_ttl_eq_32_count | number | TTL=32 计数 |
| 42 | ip_ttl_eq_64_count | number | TTL=64 计数 |
| 43 | ip_ttl_eq_128_count | number | TTL=128 计数 |
| 44 | ip_ttl_eq_255_count | number | TTL=255 计数 |
| 45 | ip_tos_mean | number | ToS 均值 |
| 46 | ip_tos_std | number | ToS 标准差 |
| 47 | ip_tos_min | number | ToS 最小 |
| 48 | ip_tos_max | number | ToS 最大 |
| 49 | ip_tos_skew | number | ToS 偏度 |
| 50 | ip_tos_kurt | number | ToS 峰度 |
| 51 | ip_tos_median | number | ToS 中位数 |
| 52 | ip_tos_q1 | number | ToS Q1 |
| 53 | ip_tos_q3 | number | ToS Q3 |
| 54 | udp_length_mean | number | UDP 长度均值 |
| 55 | udp_length_std | number | UDP 长度标准差 |
| 56 | udp_length_min | number | UDP 长度最小 |
| 57 | udp_length_max | number | UDP 长度最大 |
| 58 | udp_length_skew | number | UDP 长度偏度 |
| 59 | udp_length_kurt | number | UDP 长度峰度 |
| 60 | udp_length_median | number | UDP 长度中位数 |
| 61 | udp_length_q1 | number | UDP 长度 Q1 |
| 62 | udp_length_q3 | number | UDP 长度 Q3 |
| 63 | src_ip_is_private | number | 源 IP 是否私有 |
| 64 | dst_ip_is_private | number | 目的 IP 是否私有 |
| 65 | ip_fragmentation_flag | number | IP 分片标志 |
| 66 | src_port_is_ephemeral | number | 源端口是否临时 |
| 67 | dst_port_is_ephemeral | number | 目的端口是否临时 |
| 68 | well_known_dst_port_ratio | number | 知名目的端口比 |
| 69 | well_known_src_port_ratio | number | 知名源端口比 |
| 70 | unique_src_ports | number | 唯一源端口数 |
| 71 | unique_dst_ports | number | 唯一目的端口数 |
| 72 | is_common_service | number | 是否常见服务 |
| 73 | quic_usage_flag | number | QUIC 使用标志 |
| 74 | src_port_protocol | string | 源端口协议名 |
| 75 | src_port_range | string | 源端口范围 |
| 76 | dst_port_protocol | string | 目的端口协议名 |
| 77 | tcpackfaultcnt | number | (遗留兼容) |
| 78 | tcpbflgtmx | number | (遗留兼容) |
| 79 | tcpflwlssackrcvdbytes | number | (遗留兼容) |
| 80 | tcpiseqn | number | (遗留兼容) |
| 81 | tcpinitwinsz | number | (遗留兼容) |
| 82 | tcppackcnt | number | (遗留兼容) |
| 83 | tcppseqcnt | number | (遗留兼容) |
| 84 | tcpseqfaultcnt | number | (遗留兼容) |
| 85 | tcpseqsntbytes | number | (遗留兼容) |

---

### Module E: `enable_payload` -- 载荷分析 (calc_payload, ~86 维)

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
| 10 | payload_printable_ratio | number | 可打印字符比 |
| 11 | payload_alnum_ratio | number | 字母数字比 |
| 12 | payload_non_empty_ratio | number | 非空载荷比 |
| 13 | payload_hist_bin_0 | number | 字节直方图 bin0 |
| 14 | payload_hist_bin_1 | number | 字节直方图 bin1 |
| 15 | payload_hist_bin_2 | number | 字节直方图 bin2 |
| 16 | payload_hist_bin_3 | number | 字节直方图 bin3 |
| 17 | payload_hist_bin_4 | number | 字节直方图 bin4 |
| 18 | payload_hist_bin_5 | number | 字节直方图 bin5 |
| 19 | payload_hist_bin_6 | number | 字节直方图 bin6 |
| 20 | payload_hist_bin_7 | number | 字节直方图 bin7 |
| 21 | payload_hist_bin_8 | number | 字节直方图 bin8 |
| 22 | payload_hist_bin_9 | number | 字节直方图 bin9 |
| 23 | payload_hist_bin_10 | number | 字节直方图 bin10 |
| 24 | payload_hist_bin_11 | number | 字节直方图 bin11 |
| 25 | payload_hist_bin_12 | number | 字节直方图 bin12 |
| 26 | payload_hist_bin_13 | number | 字节直方图 bin13 |
| 27 | payload_hist_bin_14 | number | 字节直方图 bin14 |
| 28 | payload_hist_bin_15 | number | 字节直方图 bin15 |
| 29 | payload_magic_http_get | number | HTTP GET 魔数检测 |
| 30 | payload_magic_http_post | number | HTTP POST 魔数检测 |
| 31 | payload_magic_tls_1_0 | number | TLS 1.0 魔数检测 |
| 32 | payload_magic_tls_1_1 | number | TLS 1.1 魔数检测 |
| 33 | payload_magic_tls_1_2 | number | TLS 1.2 魔数检测 |
| 34 | payload_magic_tls_1_3 | number | TLS 1.3 魔数检测 |
| 35 | payload_magic_ssh | number | SSH 魔数检测 |
| 36 | payload_magic_jpeg | number | JPEG 魔数检测 |
| 37 | payload_magic_png | number | PNG 魔数检测 |
| 38 | payload_magic_gif | number | GIF 魔数检测 |
| 39 | payload_magic_unknown | number | 未知魔数检测 |
| 40 | payload_looks_like_http | number | 启发式 HTTP 检测 |
| 41 | payload_looks_like_json | number | 启发式 JSON 检测 |
| 42 | payload_looks_like_xml | number | 启发式 XML 检测 |
| 43 | payload_looks_like_tls | number | 启发式 TLS 检测 |
| 44 | payload_looks_like_ssh | number | 启发式 SSH 检测 |
| 45 | payload_length_mod_2_mean | number | 载荷长度 mod2 均值 |
| 46 | payload_length_mod_2_std | number | 载荷长度 mod2 标准差 |
| 47 | payload_length_mod_4_mean | number | 载荷长度 mod4 均值 |
| 48 | payload_length_mod_4_std | number | 载荷长度 mod4 标准差 |
| 49 | payload_length_mod_8_mean | number | 载荷长度 mod8 均值 |
| 50 | payload_length_mod_8_std | number | 载荷长度 mod8 标准差 |
| 51 | payload_length_mod_16_mean | number | 载荷长度 mod16 均值 |
| 52 | payload_length_mod_16_std | number | 载荷长度 mod16 标准差 |
| 53 | payload_length_mod_32_mean | number | 载荷长度 mod32 均值 |
| 54 | payload_length_mod_32_std | number | 载荷长度 mod32 标准差 |
| 55 | payload_length_mod_64_mean | number | 载荷长度 mod64 均值 |
| 56 | payload_length_mod_64_std | number | 载荷长度 mod64 标准差 |
| 57 | payload_length_mod_128_mean | number | 载荷长度 mod128 均值 |
| 58 | payload_length_mod_128_std | number | 载荷长度 mod128 标准差 |
| 59 | payload_length_mod_256_mean | number | 载荷长度 mod256 均值 |
| 60 | payload_length_mod_256_std | number | 载荷长度 mod256 标准差 |
| 61 | payload_chi_square | number | 载荷卡方值 |
| 62 | payload_entropy | number | 载荷香农熵 |
| 63 | payload_markov_entropy | number | 载荷马尔可夫熵 |
| 64 | payload_compression_ratio | number | 载荷压缩率 |
| 65 | payload_autocorrelation_lag1 | number | 载荷自相关 lag1 |
| 66 | payload_first_64_entropy | number | 前64字节熵 |
| 67 | payload_first_128_entropy | number | 前128字节熵 |
| 68 | payload_first_256_entropy | number | 前256字节熵 |
| 69 | payload_byte_mean | number | 载荷字节均值 |
| 70 | payload_byte_std | number | 载荷字节标准差 |
| 71 | payload_byte_min | number | 载荷字节最小 |
| 72 | payload_byte_max | number | 载荷字节最大 |
| 73 | payload_byte_skew | number | 载荷字节偏度 |
| 74 | payload_byte_kurt | number | 载荷字节峰度 |
| 75 | payload_byte_q1 | number | 载荷字节 Q1 |
| 76 | payload_byte_median | number | 载荷字节中位数 |
| 77 | payload_byte_q3 | number | 载荷字节 Q3 |
| 78 | payload_size_diff_mean | number | 载荷大小差分均值 |
| 79 | payload_size_diff_std | number | 载荷大小差分标准差 |
| 80 | payload_size_diff_min | number | 载荷大小差分最小 |
| 81 | payload_size_diff_max | number | 载荷大小差分最大 |
| 82 | payload_size_diff_skew | number | 载荷大小差分偏度 |
| 83 | payload_size_diff_kurt | number | 载荷大小差分峰度 |
| 84 | payload_size_diff_median | number | 载荷大小差分中位数 |
| 85 | payload_size_diff_q1 | number | 载荷大小差分 Q1 |
| 86 | payload_size_diff_q3 | number | 载荷大小差分 Q3 |

---

### Module F: `enable_sequence` -- 序列特征 (calc_sequence, ~99 维)

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
| 40 | top_1_bigram_ss_freq | number | Bigram rank1 ss 频率 |
| 41 | top_1_bigram_sm_freq | number | Bigram rank1 sm 频率 |
| 42 | top_1_bigram_sl_freq | number | Bigram rank1 sl 频率 |
| 43 | top_1_bigram_ms_freq | number | Bigram rank1 ms 频率 |
| 44 | top_1_bigram_mm_freq | number | Bigram rank1 mm 频率 |
| 45 | top_1_bigram_ml_freq | number | Bigram rank1 ml 频率 |
| 46 | top_1_bigram_ls_freq | number | Bigram rank1 ls 频率 |
| 47 | top_1_bigram_lm_freq | number | Bigram rank1 lm 频率 |
| 48 | top_1_bigram_ll_freq | number | Bigram rank1 ll 频率 |
| 49 | top_2_bigram_ss_freq | number | Bigram rank2 ss 频率 |
| 50 | top_2_bigram_sm_freq | number | Bigram rank2 sm 频率 |
| 51 | top_2_bigram_sl_freq | number | Bigram rank2 sl 频率 |
| 52 | top_2_bigram_ms_freq | number | Bigram rank2 ms 频率 |
| 53 | top_2_bigram_mm_freq | number | Bigram rank2 mm 频率 |
| 54 | top_2_bigram_ml_freq | number | Bigram rank2 ml 频率 |
| 55 | top_2_bigram_ls_freq | number | Bigram rank2 ls 频率 |
| 56 | top_2_bigram_lm_freq | number | Bigram rank2 lm 频率 |
| 57 | top_2_bigram_ll_freq | number | Bigram rank2 ll 频率 |
| 58 | top_3_bigram_ss_freq | number | Bigram rank3 ss 频率 |
| 59 | top_3_bigram_sm_freq | number | Bigram rank3 sm 频率 |
| 60 | top_3_bigram_sl_freq | number | Bigram rank3 sl 频率 |
| 61 | top_3_bigram_ms_freq | number | Bigram rank3 ms 频率 |
| 62 | top_3_bigram_mm_freq | number | Bigram rank3 mm 频率 |
| 63 | top_3_bigram_ml_freq | number | Bigram rank3 ml 频率 |
| 64 | top_3_bigram_ls_freq | number | Bigram rank3 ls 频率 |
| 65 | top_3_bigram_lm_freq | number | Bigram rank3 lm 频率 |
| 66 | top_3_bigram_ll_freq | number | Bigram rank3 ll 频率 |
| 67 | top_4_bigram_ss_freq | number | Bigram rank4 ss 频率 |
| 68 | top_4_bigram_sm_freq | number | Bigram rank4 sm 频率 |
| 69 | top_4_bigram_sl_freq | number | Bigram rank4 sl 频率 |
| 70 | top_4_bigram_ms_freq | number | Bigram rank4 ms 频率 |
| 71 | top_4_bigram_mm_freq | number | Bigram rank4 mm 频率 |
| 72 | top_4_bigram_ml_freq | number | Bigram rank4 ml 频率 |
| 73 | top_4_bigram_ls_freq | number | Bigram rank4 ls 频率 |
| 74 | top_4_bigram_lm_freq | number | Bigram rank4 lm 频率 |
| 75 | top_4_bigram_ll_freq | number | Bigram rank4 ll 频率 |
| 76 | top_5_bigram_ss_freq | number | Bigram rank5 ss 频率 |
| 77 | top_5_bigram_sm_freq | number | Bigram rank5 sm 频率 |
| 78 | top_5_bigram_sl_freq | number | Bigram rank5 sl 频率 |
| 79 | top_5_bigram_ms_freq | number | Bigram rank5 ms 频率 |
| 80 | top_5_bigram_mm_freq | number | Bigram rank5 mm 频率 |
| 81 | top_5_bigram_ml_freq | number | Bigram rank5 ml 频率 |
| 82 | top_5_bigram_ls_freq | number | Bigram rank5 ls 频率 |
| 83 | top_5_bigram_lm_freq | number | Bigram rank5 lm 频率 |
| 84 | top_5_bigram_ll_freq | number | Bigram rank5 ll 频率 |
| 85 | packet_length_seq | string | 原始包长序列(output_raw_seq=1) |
| 86 | packet_iat_seq | string | 原始 IAT 序列 |
| 87 | packet_direction_seq | string | 原始方向序列 |
| 88 | l2l3l4pl_iat | string | 复合序列 |
| 89 | dl_chunk_seq | string | 下行 chunk 序列 |
| 90 | dl_chunk_seq_len | number | 下行 chunk 序列长度 |
| 91 | uplink_rate_seq | string | 上行速率序列 |
| 92 | downlink_rate_seq | string | 下行速率序列 |
| 93 | uplink_rate_seq_len | number | 上行速率序列长度 |
| 94 | downlink_rate_seq_len | number | 下行速率序列长度 |

---

### Module G: `enable_fft` -- 频域特征 (calc_freq_domain, ~58 维)

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
| 21 | fwd_fft_freq_0_idx | number | 前向 FFT 主频索引 0 |
| 22 | fwd_fft_mag_0 | number | 前向 FFT 主频幅度 0 |
| 23 | fwd_fft_freq_1_idx | number | 前向 FFT 主频索引 1 |
| 24 | fwd_fft_mag_1 | number | 前向 FFT 主频幅度 1 |
| 25 | fwd_fft_freq_2_idx | number | 前向 FFT 主频索引 2 |
| 26 | fwd_fft_mag_2 | number | 前向 FFT 主频幅度 2 |
| 27 | fwd_fft_freq_3_idx | number | 前向 FFT 主频索引 3 |
| 28 | fwd_fft_mag_3 | number | 前向 FFT 主频幅度 3 |
| 29 | fwd_fft_freq_4_idx | number | 前向 FFT 主频索引 4 |
| 30 | fwd_fft_mag_4 | number | 前向 FFT 主频幅度 4 |
| 31 | fwd_fft_magnitude_mean | number | 前向 FFT 幅度均值 |
| 32 | fwd_fft_magnitude_std | number | 前向 FFT 幅度标准差 |
| 33 | fwd_fft_magnitude_min | number | 前向 FFT 幅度最小 |
| 34 | fwd_fft_magnitude_max | number | 前向 FFT 幅度最大 |
| 35 | fwd_fft_magnitude_median | number | 前向 FFT 幅度中位数 |
| 36 | fwd_fft_magnitude_q1 | number | 前向 FFT 幅度 Q1 |
| 37 | fwd_fft_magnitude_q3 | number | 前向 FFT 幅度 Q3 |
| 38 | fwd_fft_magnitude_skew | number | 前向 FFT 幅度偏度 |
| 39 | fwd_fft_magnitude_kurt | number | 前向 FFT 幅度峰度 |
| 40 | bwd_fft_freq_0_idx | number | 后向 FFT 主频索引 0 |
| 41 | bwd_fft_mag_0 | number | 后向 FFT 主频幅度 0 |
| 42 | bwd_fft_freq_1_idx | number | 后向 FFT 主频索引 1 |
| 43 | bwd_fft_mag_1 | number | 后向 FFT 主频幅度 1 |
| 44 | bwd_fft_freq_2_idx | number | 后向 FFT 主频索引 2 |
| 45 | bwd_fft_mag_2 | number | 后向 FFT 主频幅度 2 |
| 46 | bwd_fft_freq_3_idx | number | 后向 FFT 主频索引 3 |
| 47 | bwd_fft_mag_3 | number | 后向 FFT 主频幅度 3 |
| 48 | bwd_fft_freq_4_idx | number | 后向 FFT 主频索引 4 |
| 49 | bwd_fft_mag_4 | number | 后向 FFT 主频幅度 4 |
| 50 | bwd_fft_magnitude_mean | number | 后向 FFT 幅度均值 |
| 51 | bwd_fft_magnitude_std | number | 后向 FFT 幅度标准差 |
| 52 | bwd_fft_magnitude_min | number | 后向 FFT 幅度最小 |
| 53 | bwd_fft_magnitude_max | number | 后向 FFT 幅度最大 |
| 54 | bwd_fft_magnitude_median | number | 后向 FFT 幅度中位数 |
| 55 | bwd_fft_magnitude_q1 | number | 后向 FFT 幅度 Q1 |
| 56 | bwd_fft_magnitude_q3 | number | 后向 FFT 幅度 Q3 |
| 57 | bwd_fft_magnitude_skew | number | 后向 FFT 幅度偏度 |
| 58 | bwd_fft_magnitude_kurt | number | 后向 FFT 幅度峰度 |

---

### Module H: `enable_window` -- 时间窗特征 (calc_window, 18 维)

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
| 10 | window_byte_count_mean | number | 窗口字节数均值 |
| 11 | window_byte_count_std | number | 窗口字节数标准差 |
| 12 | window_byte_count_min | number | 窗口字节数最小 |
| 13 | window_byte_count_max | number | 窗口字节数最大 |
| 14 | window_byte_count_median | number | 窗口字节数中位数 |
| 15 | window_byte_count_q1 | number | 窗口字节数 Q1 |
| 16 | window_byte_count_q3 | number | 窗口字节数 Q3 |
| 17 | window_byte_count_skew | number | 窗口字节数偏度 |
| 18 | window_byte_count_kurt | number | 窗口字节数峰度 |

---

### Module I: `enable_behavior` -- 行为特征 (calc_behavior, ~29 维)

| # | 特征名 | 类型 | 说明 |
|---|--------|------|------|
| 1 | flow_duration | number | 流持续时间(秒) |
| 2 | avg_bitrate | number | 平均比特率 |
| 3 | avg_packet_rate | number | 平均包速率 |
| 4 | avg_throughput | number | 平均吞吐量 |
| 5 | is_bulk_transfer | number | 是否大流量传输 |
| 6 | short_connection_ratio | number | 短连接比例 |
| 7 | flow_asymmetry_ratio | number | 流不对称比 |
| 8 | flow_pattern | string | 流模式 |
| 9 | has_regular_intervals | number | 是否规律间隔 |
| 10 | protocol_tcp_ratio | number | TCP 协议比 |
| 11 | protocol_udp_ratio | number | UDP 协议比 |
| 12 | protocol_icmp_ratio | number | ICMP 协议比 |
| 13 | protocol_other_ratio | number | 其他协议比 |
| 14 | instant_bitrate_mean | number | 瞬时比特率均值 |
| 15 | instant_bitrate_std | number | 瞬时比特率标准差 |
| 16 | instant_bitrate_min | number | 瞬时比特率最小 |
| 17 | instant_bitrate_max | number | 瞬时比特率最大 |
| 18 | instant_bitrate_median | number | 瞬时比特率中位数 |
| 19 | instant_bitrate_q1 | number | 瞬时比特率 Q1 |
| 20 | instant_bitrate_q3 | number | 瞬时比特率 Q3 |
| 21 | instant_bitrate_skew | number | 瞬时比特率偏度 |
| 22 | instant_bitrate_kurt | number | 瞬时比特率峰度 |
| 23 | peak_bitrate | number | 峰值比特率 |
| 24 | bitrate_percentile_25 | number | 比特率 P25 |
| 25 | bitrate_percentile_50 | number | 比特率 P50 |
| 26 | bitrate_percentile_75 | number | 比特率 P75 |
| 27 | bitrate_percentile_90 | number | 比特率 P90 |
| 28 | traffic_entropy_ts_mean | number | 流量时序熵均值 |
| 29 | traffic_entropy_ts_peak | number | 流量时序熵峰值 |

---

## 六、总计

| 模块 | 开关 | 特征数 |
|------|------|--------|
| 流标识 | (常开) | ~18 |
| Basic | enable_basic | ~56 |
| IAT | enable_iat | ~69 |
| Burst | enable_burst | 8 |
| Protocol | enable_protocol | ~85 |
| Payload | enable_payload | ~86 |
| Sequence | enable_sequence | ~94 |
| FFT | enable_fft | ~58 |
| Window | enable_window | 18 |
| Behavior | enable_behavior | ~29 |
| **合计** | | **~521** |
