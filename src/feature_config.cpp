#include "feature_config.h"
#include "MESA/MESA_prof_load.h"
#include "MESA/MESA_handle_logger.h"

// 模块标识符用于日志
#define TI_FEATURES "ti_features"

// 全局配置实例
ti_feature_config_t g_feat_config;

// ============================================================================
// Bridge 数据释放回调函数
// ============================================================================

// 用于释放通过 bridge 传递的 JSON 字符串数据
void free_feature_bridge_data(const struct streaminfo *a_stream, int bridge_id, void *data)
{
    (void)a_stream;
    (void)bridge_id;
    char* json_str = (char*)data;
    if (json_str != NULL) {
        free(json_str);
        json_str = NULL;
    }
}

// 用于释放通过 SNI bridge 传递的 SNI 字符串数据
void free_sni_bridge_data(const struct streaminfo *a_stream, int bridge_id, void *data)
{
    (void)a_stream;
    (void)bridge_id;
    char* sni = (char*)data;
    if (sni != NULL) {
        free(sni);
        sni = NULL;
    }
}

// 获取配置实例
ti_feature_config_t* feat_config_get(void)
{
    return &g_feat_config;
}

// 配置初始化（设置默认值）
int feat_config_init(void)
{
    memset(&g_feat_config, 0, sizeof(ti_feature_config_t));

    // 序列长度限制
    g_feat_config.max_seq_len = DEFAULT_MAX_SEQ_LEN;
    g_feat_config.first_n_packets = DEFAULT_FIRST_N_PACKETS;
    g_feat_config.run_mode = 0;

    // 包大小阈值
    g_feat_config.small_pkt_threshold = DEFAULT_SMALL_PKT_THRESHOLD;
    g_feat_config.large_pkt_threshold = DEFAULT_LARGE_PKT_THRESHOLD;

    // Burst 检测阈值
    g_feat_config.burst_iat_threshold_us = DEFAULT_BURST_IAT_THRESHOLD_US;

    // 活跃/空闲时间阈值
    g_feat_config.active_iat_threshold_us = DEFAULT_ACTIVE_IAT_THRESHOLD_US;

    // 时间窗口大小
    g_feat_config.window_size_ms = DEFAULT_WINDOW_SIZE_MS;
    g_feat_config.instant_bitrate_window_ms = DEFAULT_INSTANT_BITRATE_WINDOW_MS;

    // 行为特征阈值
    g_feat_config.interactive_threshold_ms = DEFAULT_INTERACTIVE_THRESHOLD_MS;
    g_feat_config.bulk_transfer_kbps = DEFAULT_BULK_TRANSFER_KBPS;
    g_feat_config.short_conn_threshold_s = DEFAULT_SHORT_CONN_THRESHOLD_S;

    // Kafka 配置
    g_feat_config.output_to_log = 0;
    g_feat_config.send_kafka_flag = 0;

    // bridge 配置
    g_feat_config.feature_bridge_flag = 1;
    snprintf(g_feat_config.feature_bridge_name, MAX_DOMAIN_LEN, "%s", "FEATURE_BRIDGE");

    // 特征模块总开关（默认全部开启）
    g_feat_config.enable_basic = 1;
    g_feat_config.enable_iat = 1;
    // enable_burst 在下方子开关中设置
    g_feat_config.enable_protocol = 1;
    g_feat_config.enable_payload = 1;
    g_feat_config.enable_sequence = 1;
    g_feat_config.enable_fft = 1;
    g_feat_config.enable_window = 1;
    g_feat_config.enable_behavior = 1;

    // 特征模块子开关（默认全部开启）

    // 基础模块
    g_feat_config.enable_basic_ratios = 1;
    g_feat_config.enable_basic_payload_dir = 1;
    g_feat_config.enable_basic_first_n = 1;
    g_feat_config.enable_basic_pkt_length = 1;

    // IAT 模块
    g_feat_config.enable_iat_stats = 1;
    g_feat_config.enable_iat_fwd_bwd = 1;
    g_feat_config.enable_iat_active = 1;
    g_feat_config.enable_iat_response = 1;

    // Burst 模块
    g_feat_config.enable_burst = 1;

    // 协议模块
    g_feat_config.enable_protocol_tcp_flags = 1;
    g_feat_config.enable_protocol_tcp_window = 1;
    g_feat_config.enable_protocol_ip = 1;
    g_feat_config.enable_protocol_udp = 1;
    g_feat_config.enable_protocol_port = 1;

    // Payload 模块
    g_feat_config.enable_payload_size = 1;
    g_feat_config.enable_payload_content = 1;
    g_feat_config.enable_payload_hist = 1;
    g_feat_config.enable_payload_magic = 1;
    g_feat_config.enable_payload_stats = 1;

    // 序列模块
    g_feat_config.enable_sequence_stats = 1;
    g_feat_config.enable_raw_sequences = 1;
    g_feat_config.enable_chunk_sequences = 1;
    g_feat_config.enable_rate_sequences = 1;

    // FFT 模块
    g_feat_config.enable_fft_global = 1;
    g_feat_config.enable_fft_fwd = 1;
    g_feat_config.enable_fft_bwd = 1;

    // Window 模块
    g_feat_config.enable_window_pkt = 1;
    g_feat_config.enable_window_byte = 1;

    // Behavior 模块
    g_feat_config.enable_behavior_basic = 1;
    g_feat_config.enable_behavior_pattern = 1;
    g_feat_config.enable_behavior_bitrate = 1;

    return 0;
}

// 读取配置文件
int feat_config_read(const char* filename)
{
    // 先初始化默认值
    feat_config_init();

    short log_level;
    char log_filename[MAX_PATH_LEN];

    // LOG 配置
    MESA_load_profile_short_def(filename, "LOG", "log_level", &log_level, 10);
    MESA_load_profile_string_def(filename, "LOG", "log_path", log_filename, sizeof(log_filename), "./featlog/ti_features_log");
    g_feat_config.log_handle = MESA_create_runtime_log_handle(log_filename, log_level);
    if (g_feat_config.log_handle == NULL) {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "Failed to create log handle");
        return -1;
    }
    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "Log handle created, path=%s, level=%d", log_filename, log_level);


    // 基础配置
    MESA_load_profile_uint_def(filename, "FEATURE", "max_seq_len", &g_feat_config.max_seq_len, DEFAULT_MAX_SEQ_LEN);
    MESA_load_profile_uint_def(filename, "FEATURE", "first_n_packets", &g_feat_config.first_n_packets, DEFAULT_FIRST_N_PACKETS);
    MESA_load_profile_uint_def(filename, "FEATURE", "run_mode", &g_feat_config.run_mode, 0);
    MESA_load_profile_uint_def(filename, "FEATURE", "small_pkt_threshold", &g_feat_config.small_pkt_threshold, DEFAULT_SMALL_PKT_THRESHOLD);
    MESA_load_profile_uint_def(filename, "FEATURE", "large_pkt_threshold", &g_feat_config.large_pkt_threshold, DEFAULT_LARGE_PKT_THRESHOLD);

    if (g_feat_config.first_n_packets == 0) {
        g_feat_config.first_n_packets = DEFAULT_FIRST_N_PACKETS;
    } else if (g_feat_config.first_n_packets > MAX_FIRST_N_PACKETS) {
        g_feat_config.first_n_packets = MAX_FIRST_N_PACKETS;
    }

    // Burst 配置
    MESA_load_profile_uint_def(filename, "BURST", "burst_iat_threshold_us", &g_feat_config.burst_iat_threshold_us, DEFAULT_BURST_IAT_THRESHOLD_US);

    // 活跃/空闲配置
    MESA_load_profile_uint_def(filename, "ACTIVE", "active_iat_threshold_us", &g_feat_config.active_iat_threshold_us, DEFAULT_ACTIVE_IAT_THRESHOLD_US);

    // 窗口配置
    MESA_load_profile_uint_def(filename, "WINDOW", "window_size_ms", &g_feat_config.window_size_ms, DEFAULT_WINDOW_SIZE_MS);
    MESA_load_profile_uint_def(filename, "WINDOW", "instant_bitrate_window_ms", &g_feat_config.instant_bitrate_window_ms, DEFAULT_INSTANT_BITRATE_WINDOW_MS);

    // 行为配置
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "interactive_threshold_ms", &g_feat_config.interactive_threshold_ms, DEFAULT_INTERACTIVE_THRESHOLD_MS);
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "bulk_transfer_kbps", &g_feat_config.bulk_transfer_kbps, DEFAULT_BULK_TRANSFER_KBPS);
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "short_conn_threshold_s", &g_feat_config.short_conn_threshold_s, DEFAULT_SHORT_CONN_THRESHOLD_S);

    // bridge 配置
    MESA_load_profile_short_def(filename, "BRIDGE", "feature_bridge_flag", &g_feat_config.feature_bridge_flag, 1);
    MESA_load_profile_string_def(filename, "BRIDGE", "feature_bridge_name", g_feat_config.feature_bridge_name, sizeof(g_feat_config.feature_bridge_name), "FEATURE_BRIDGE");

    // SNI bridge 配置
    MESA_load_profile_short_def(filename, "BRIDGE", "sni_bridge_flag", &g_feat_config.sni_bridge_flag, 1);
    MESA_load_profile_string_def(filename, "BRIDGE", "sni_bridge_name", g_feat_config.sni_bridge_name, sizeof(g_feat_config.sni_bridge_name), "SNI_BRIDGE");

    // SNI bridge 初始化 - 仅当 sni_bridge_flag 为 1 时初始化
    if (g_feat_config.sni_bridge_flag) {
        g_feat_config.sni_bridge_id = stream_bridge_build(g_feat_config.sni_bridge_name, "w");
        if (g_feat_config.sni_bridge_id < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "SNI_BRIDGE stream_bridge_build failed, name=%s!!!", g_feat_config.sni_bridge_name);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "SNI bridge initialized, name=%s, id=%d", g_feat_config.sni_bridge_name, g_feat_config.sni_bridge_id);

        // 注册 SNI bridge 数据释放回调函数
        int ret = stream_bridge_register_data_free_cb(g_feat_config.sni_bridge_id, free_sni_bridge_data);
        if (ret < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "SNI_BRIDGE register free callback failed, bridge_id=%d!!!", g_feat_config.sni_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "SNI bridge free callback registered, bridge_id=%d", g_feat_config.sni_bridge_id);
    } else {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "SNI bridge disabled by config");
    }

    // bridge 桥接初始化 - 仅当 feature_bridge_flag 为 1 时初始化
    if (g_feat_config.feature_bridge_flag) {
#if 1
        g_feat_config.feature_bridge_id = TF_get_brgid(FEATURE_BRIDGE);
        if(g_feat_config.feature_bridge_id < 0)
        {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, "INIT_BRIDGE", "stream_bridge_build is error, bridge_name: FEATURE_BRIDGE");
        }
#else
        g_feat_config.feature_bridge_id = stream_bridge_build(g_feat_config.feature_bridge_name, "w");
        if (g_feat_config.feature_bridge_id < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "FEATURE_BRIDGE stream_bridge_build failed, name=%s!!!", g_feat_config.feature_bridge_name);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "FEATURE bridge initialized, name=%s, id=%d", g_feat_config.feature_bridge_name, g_feat_config.feature_bridge_id);

        // 注册 bridge 数据释放回调函数
        int ret = stream_bridge_register_data_free_cb(g_feat_config.feature_bridge_id, free_feature_bridge_data);
        if (ret < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "FEATURE_BRIDGE register free callback failed, bridge_id=%d!!!", g_feat_config.feature_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "FEATURE bridge free callback registered, bridge_id=%d", g_feat_config.feature_bridge_id);
#endif
    } else {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "FEATURE bridge disabled by config");
    }

    // Kafka 配置
    MESA_load_profile_uint_def(filename, "KAFKA", "send_kafka_flag", &g_feat_config.send_kafka_flag, 0);
    MESA_load_profile_uint_def(filename, "KAFKA", "output_to_log", &g_feat_config.output_to_log, 1);

    if (g_feat_config.send_kafka_flag) {
        if (MESA_load_profile_string_nodef(filename, "KAFKA", "kafka_brokers", g_feat_config.kafka_brokers, sizeof(g_feat_config.kafka_brokers)) < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,"Get [KAFKA] kafka_brokers error");
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }

        g_feat_config.kafka_producer = new KafkaProducer(g_feat_config.kafka_brokers);
        if (NULL == g_feat_config.kafka_producer) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "KafkaProducer creation failed, brokers=%s", g_feat_config.kafka_brokers);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }

        int conn_ret = g_feat_config.kafka_producer->KafkaConnection();
        if (conn_ret != 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "KafkaConnection failed, ret=%d, brokers=%s", conn_ret, g_feat_config.kafka_brokers);
            delete g_feat_config.kafka_producer;
            g_feat_config.kafka_producer = NULL;
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        } else {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
                "Kafka producer initialized and metadata probe passed, brokers=%s", g_feat_config.kafka_brokers);
        }

        MESA_load_profile_string_def(filename, "KAFKA", "kafka_topic", g_feat_config.topic_name, sizeof(g_feat_config.topic_name), "flow_features");

        if ((g_feat_config.kafka_producer->CreateTopicHandle(g_feat_config.topic_name)) == NULL) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "Kafka CreateTopicHandle failed, topic=%s", g_feat_config.topic_name);
            delete g_feat_config.kafka_producer;
            g_feat_config.kafka_producer = NULL;
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
            "Kafka topic handle created, topic=%s (this does not guarantee broker topic exists)", g_feat_config.topic_name);
    }

    // 特征模块总开关
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_basic", &g_feat_config.enable_basic, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_iat", &g_feat_config.enable_iat, 1);
    // enable_burst 在下方子开关中加载
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol", &g_feat_config.enable_protocol, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload", &g_feat_config.enable_payload, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_sequence", &g_feat_config.enable_sequence, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_fft", &g_feat_config.enable_fft, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_window", &g_feat_config.enable_window, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_behavior", &g_feat_config.enable_behavior, 1);

    // 特征模块子开关

    // 基础模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_basic_ratios", &g_feat_config.enable_basic_ratios, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_basic_payload_dir", &g_feat_config.enable_basic_payload_dir, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_basic_first_n", &g_feat_config.enable_basic_first_n, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_basic_pkt_length", &g_feat_config.enable_basic_pkt_length, 1);

    // IAT 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_iat_stats", &g_feat_config.enable_iat_stats, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_iat_fwd_bwd", &g_feat_config.enable_iat_fwd_bwd, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_iat_active", &g_feat_config.enable_iat_active, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_iat_response", &g_feat_config.enable_iat_response, 1);

    // Burst 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_burst", &g_feat_config.enable_burst, 1);

    // 协议模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol_tcp_flags", &g_feat_config.enable_protocol_tcp_flags, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol_tcp_window", &g_feat_config.enable_protocol_tcp_window, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol_ip", &g_feat_config.enable_protocol_ip, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol_udp", &g_feat_config.enable_protocol_udp, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_protocol_port", &g_feat_config.enable_protocol_port, 1);

    // Payload 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload_size", &g_feat_config.enable_payload_size, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload_content", &g_feat_config.enable_payload_content, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload_hist", &g_feat_config.enable_payload_hist, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload_magic", &g_feat_config.enable_payload_magic, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_payload_stats", &g_feat_config.enable_payload_stats, 1);

    // 序列模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_sequence_stats", &g_feat_config.enable_sequence_stats, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_raw_sequences", &g_feat_config.enable_raw_sequences, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_chunk_sequences", &g_feat_config.enable_chunk_sequences, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_rate_sequences", &g_feat_config.enable_rate_sequences, 1);

    // FFT 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_fft_global", &g_feat_config.enable_fft_global, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_fft_fwd", &g_feat_config.enable_fft_fwd, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_fft_bwd", &g_feat_config.enable_fft_bwd, 1);

    // Window 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_window_pkt", &g_feat_config.enable_window_pkt, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_window_byte", &g_feat_config.enable_window_byte, 1);

    // Behavior 模块
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_behavior_basic", &g_feat_config.enable_behavior_basic, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_behavior_pattern", &g_feat_config.enable_behavior_pattern, 1);
    MESA_load_profile_uint_def(filename, "FEATURES", "enable_behavior_bitrate", &g_feat_config.enable_behavior_bitrate, 1);

    // ============================================================================
    // 计算复合依赖标志（由多个子开关聚合，用于 per-packet 阶段跳过不需要的数据采集）
    // ============================================================================
    g_feat_config.need_pkt_len_seq = g_feat_config.enable_fft_global ||
        g_feat_config.enable_sequence_stats || g_feat_config.enable_raw_sequences ||
        g_feat_config.enable_chunk_sequences || g_feat_config.enable_rate_sequences;

    g_feat_config.need_dir_seq = g_feat_config.enable_sequence_stats ||
        g_feat_config.enable_raw_sequences || g_feat_config.enable_chunk_sequences ||
        g_feat_config.enable_rate_sequences;

    g_feat_config.need_ts_seq = g_feat_config.enable_chunk_sequences ||
        g_feat_config.enable_rate_sequences;

    g_feat_config.need_l3_l4_payload_seq = g_feat_config.enable_raw_sequences;

    g_feat_config.need_fwd_bwd_pkt_lens = g_feat_config.enable_basic_pkt_length ||
        g_feat_config.enable_fft_fwd || g_feat_config.enable_fft_bwd;

    g_feat_config.need_first_n = g_feat_config.enable_basic &&
        g_feat_config.enable_basic_first_n;

    g_feat_config.need_payload_dir = g_feat_config.enable_basic &&
        g_feat_config.enable_basic_payload_dir;

    g_feat_config.need_fwd_bwd_iats = g_feat_config.enable_iat &&
        g_feat_config.enable_iat_fwd_bwd;

    g_feat_config.need_window_update = g_feat_config.enable_window_pkt ||
        g_feat_config.enable_window_byte || g_feat_config.enable_behavior_pattern ||
        g_feat_config.enable_behavior_bitrate;

    g_feat_config.need_iat_seq = g_feat_config.enable_iat_stats ||
        g_feat_config.enable_sequence_stats || g_feat_config.enable_raw_sequences;

    // 依赖校验：子开关被主开关覆盖时打印 WARNING
    if (!g_feat_config.enable_basic) {
        if (g_feat_config.enable_basic_ratios || g_feat_config.enable_basic_payload_dir ||
            g_feat_config.enable_basic_first_n || g_feat_config.enable_basic_pkt_length) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_basic=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }
    if (!g_feat_config.enable_iat) {
        if (g_feat_config.enable_iat_stats || g_feat_config.enable_iat_fwd_bwd ||
            g_feat_config.enable_iat_active || g_feat_config.enable_iat_response) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_iat=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }
    if (!g_feat_config.enable_protocol) {
        if (g_feat_config.enable_protocol_tcp_flags || g_feat_config.enable_protocol_tcp_window ||
            g_feat_config.enable_protocol_ip || g_feat_config.enable_protocol_udp ||
            g_feat_config.enable_protocol_port) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_protocol=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }
    if (!g_feat_config.enable_payload) {
        if (g_feat_config.enable_payload_size || g_feat_config.enable_payload_content ||
            g_feat_config.enable_payload_hist || g_feat_config.enable_payload_magic ||
            g_feat_config.enable_payload_stats) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_payload=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }
    if (!g_feat_config.enable_sequence) {
        if (g_feat_config.enable_sequence_stats || g_feat_config.enable_raw_sequences ||
            g_feat_config.enable_chunk_sequences || g_feat_config.enable_rate_sequences) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_sequence=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }
    if (!g_feat_config.enable_fft) {
        if (g_feat_config.enable_fft_global || g_feat_config.enable_fft_fwd || g_feat_config.enable_fft_bwd) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "WARNING: enable_fft=0 but sub-switches are ON, sub-switches will be ignored");
        }
    }

    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "Feature master switches: basic=%u iat=%u burst=%u protocol=%u payload=%u sequence=%u fft=%u window=%u behavior=%u",
        g_feat_config.enable_basic, g_feat_config.enable_iat, g_feat_config.enable_burst,
        g_feat_config.enable_protocol, g_feat_config.enable_payload, g_feat_config.enable_sequence,
        g_feat_config.enable_fft, g_feat_config.enable_window, g_feat_config.enable_behavior);

    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "Feature modules: basic(%u/%u/%u/%u) iat(%u/%u/%u/%u) burst=%u protocol(%u/%u/%u/%u/%u) payload(%u/%u/%u/%u/%u) sequence(%u/%u/%u/%u) fft(%u/%u/%u) window(%u/%u) behavior(%u/%u/%u)",
        g_feat_config.enable_basic_ratios, g_feat_config.enable_basic_payload_dir,
        g_feat_config.enable_basic_first_n, g_feat_config.enable_basic_pkt_length,
        g_feat_config.enable_iat_stats, g_feat_config.enable_iat_fwd_bwd,
        g_feat_config.enable_iat_active, g_feat_config.enable_iat_response,
        g_feat_config.enable_burst,
        g_feat_config.enable_protocol_tcp_flags, g_feat_config.enable_protocol_tcp_window,
        g_feat_config.enable_protocol_ip, g_feat_config.enable_protocol_udp,
        g_feat_config.enable_protocol_port,
        g_feat_config.enable_payload_size, g_feat_config.enable_payload_content,
        g_feat_config.enable_payload_hist, g_feat_config.enable_payload_magic,
        g_feat_config.enable_payload_stats,
        g_feat_config.enable_sequence_stats, g_feat_config.enable_raw_sequences,
        g_feat_config.enable_chunk_sequences, g_feat_config.enable_rate_sequences,
        g_feat_config.enable_fft_global, g_feat_config.enable_fft_fwd,
        g_feat_config.enable_fft_bwd,
        g_feat_config.enable_window_pkt, g_feat_config.enable_window_byte,
        g_feat_config.enable_behavior_basic, g_feat_config.enable_behavior_pattern,
        g_feat_config.enable_behavior_bitrate);

    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
        "Compound dependency flags: pkt_len_seq=%u dir_seq=%u ts_seq=%u l3_l4_payload_seq=%u "
        "fwd_bwd_pkt_lens=%u first_n=%u payload_dir=%u fwd_bwd_iats=%u window_update=%u iat_seq=%u",
        g_feat_config.need_pkt_len_seq, g_feat_config.need_dir_seq,
        g_feat_config.need_ts_seq, g_feat_config.need_l3_l4_payload_seq,
        g_feat_config.need_fwd_bwd_pkt_lens, g_feat_config.need_first_n,
        g_feat_config.need_payload_dir, g_feat_config.need_fwd_bwd_iats,
        g_feat_config.need_window_update, g_feat_config.need_iat_seq);

    return 0;
}

// 配置销毁
void feat_config_destroy(void)
{
    if (g_feat_config.log_handle) {
        MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
        g_feat_config.log_handle = NULL;
    }

    if (g_feat_config.send_kafka_flag && g_feat_config.kafka_producer) {
        delete g_feat_config.kafka_producer;
        g_feat_config.kafka_producer = NULL;
    }


}
