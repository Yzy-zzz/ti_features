#include "feature_config.h"
#include "MESA/MESA_prof_load.h"
#include "MESA/MESA_handle_logger.h"

// 模块标识符用于日志
#define TI_FEATURES "ti_features"

// 全局配置实例
ti_feature_config_t g_feat_config;

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

    // FFT 配置
    g_feat_config.fft_top_k = DEFAULT_FFT_TOP_K;

    // 行为特征阈值
    g_feat_config.interactive_threshold_ms = DEFAULT_INTERACTIVE_THRESHOLD_MS;
    g_feat_config.bulk_transfer_kbps = DEFAULT_BULK_TRANSFER_KBPS;
    g_feat_config.short_conn_threshold_s = DEFAULT_SHORT_CONN_THRESHOLD_S;

    // Kafka 配置
    g_feat_config.output_to_log = 0;
    g_feat_config.send_kafka_flag = 0;

    // SNI 配置
    g_feat_config.filter_sni_flag = 1;
    snprintf(g_feat_config.filter_sni, MAX_DOMAIN_LEN, "%s", "googlevideo.com");
    snprintf(g_feat_config.sni_bridge_name, MAX_DOMAIN_LEN, "%s", "TLS_QUIC_SNI");

    // 输出原始序列
    g_feat_config.output_raw_seq = 1;

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
    MESA_load_profile_uint_def(filename, "FEATURE", "run_mode", &g_feat_config.run_mode, 0);
    MESA_load_profile_uint_def(filename, "FEATURE", "small_pkt_threshold", &g_feat_config.small_pkt_threshold, DEFAULT_SMALL_PKT_THRESHOLD);
    MESA_load_profile_uint_def(filename, "FEATURE", "large_pkt_threshold", &g_feat_config.large_pkt_threshold, DEFAULT_LARGE_PKT_THRESHOLD);

    // Burst 配置
    MESA_load_profile_uint_def(filename, "BURST", "burst_iat_threshold_us", &g_feat_config.burst_iat_threshold_us, DEFAULT_BURST_IAT_THRESHOLD_US);

    // 活跃/空闲配置
    MESA_load_profile_uint_def(filename, "ACTIVE", "active_iat_threshold_us", &g_feat_config.active_iat_threshold_us, DEFAULT_ACTIVE_IAT_THRESHOLD_US);

    // 窗口配置
    MESA_load_profile_uint_def(filename, "WINDOW", "window_size_ms", &g_feat_config.window_size_ms, DEFAULT_WINDOW_SIZE_MS);
    MESA_load_profile_uint_def(filename, "WINDOW", "instant_bitrate_window_ms", &g_feat_config.instant_bitrate_window_ms, DEFAULT_INSTANT_BITRATE_WINDOW_MS);

    // FFT 配置
    MESA_load_profile_uint_def(filename, "FFT", "fft_top_k", &g_feat_config.fft_top_k, DEFAULT_FFT_TOP_K);

    // 行为配置
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "interactive_threshold_ms", &g_feat_config.interactive_threshold_ms, DEFAULT_INTERACTIVE_THRESHOLD_MS);
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "bulk_transfer_kbps", &g_feat_config.bulk_transfer_kbps, DEFAULT_BULK_TRANSFER_KBPS);
    MESA_load_profile_uint_def(filename, "BEHAVIOR", "short_conn_threshold_s", &g_feat_config.short_conn_threshold_s, DEFAULT_SHORT_CONN_THRESHOLD_S);

    // SNI 配置
    MESA_load_profile_short_def(filename, "SNI", "filter_sni_flag", &g_feat_config.filter_sni_flag, 1);
    MESA_load_profile_string_def(filename, "SNI", "filter_sni", g_feat_config.filter_sni, sizeof(g_feat_config.filter_sni), "googlevideo.com");
    MESA_load_profile_string_def(filename, "SNI", "sni_bridge_name", g_feat_config.sni_bridge_name, sizeof(g_feat_config.sni_bridge_name), "TLS_QUIC_SNI");

    // SNI 桥接初始化
    g_feat_config.sni_bridge_id = stream_bridge_build(g_feat_config.sni_bridge_name, "w");
    if (g_feat_config.sni_bridge_id < 0) {
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "SNI_BRIDGE stream_bridge_build failed, name=%s!!!", g_feat_config.sni_bridge_name);
        MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
        g_feat_config.log_handle = NULL;
        return -1;
    }
    MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "SNI bridge initialized, name=%s, id=%d", g_feat_config.sni_bridge_name, g_feat_config.sni_bridge_id);

    // Kafka 配置
    MESA_load_profile_uint_def(filename, "KAFKA", "send_kafka_flag", &g_feat_config.send_kafka_flag, 0);
    MESA_load_profile_uint_def(filename, "KAFKA", "output_to_log", &g_feat_config.output_to_log, 1);

    if (g_feat_config.send_kafka_flag) {
        if (MESA_load_profile_string_nodef(filename, "KAFKA", "kafka_brokers", g_feat_config.kafka_brokers, sizeof(g_feat_config.kafka_brokers)) < 0) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,"Get [KAFKA] kafka_brokers error");
            // stream_bridge_close(g_feat_config.sni_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }

        g_feat_config.kafka_producer = new KafkaProducer(g_feat_config.kafka_brokers);
        if (NULL == g_feat_config.kafka_producer) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "KafkaProducer creation failed, brokers=%s", g_feat_config.kafka_brokers);
            // stream_bridge_close(g_feat_config.sni_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }

        if (0 != g_feat_config.kafka_producer->KafkaConnection()) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES, "KafkaConnection failed, brokers=%s", g_feat_config.kafka_brokers);
            delete g_feat_config.kafka_producer;
            g_feat_config.kafka_producer = NULL;
            // stream_bridge_close(g_feat_config.sni_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        } else {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES, "KafkaConnection succ, brokers=%s", g_feat_config.kafka_brokers);
        }

        MESA_load_profile_string_def(filename, "KAFKA", "kafka_topic", g_feat_config.topic_name, sizeof(g_feat_config.topic_name), "flow_features");

        if ((g_feat_config.kafka_producer->CreateTopicHandle(g_feat_config.topic_name)) == NULL) {
            MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_FATAL, TI_FEATURES,
                "Kafka CreateTopicHandle failed, topic=%s", g_feat_config.topic_name);
            delete g_feat_config.kafka_producer;
            g_feat_config.kafka_producer = NULL;
            // stream_bridge_close(g_feat_config.sni_bridge_id);
            MESA_destroy_runtime_log_handle(g_feat_config.log_handle);
            g_feat_config.log_handle = NULL;
            return -1;
        }
        MESA_handle_runtime_log(g_feat_config.log_handle, RLOG_LV_INFO, TI_FEATURES,
            "Kafka CreateTopicHandle succ, topic=%s", g_feat_config.topic_name);
    }

    // 输出配置
    MESA_load_profile_uint_def(filename, "OUTPUT", "output_raw_seq", &g_feat_config.output_raw_seq, 1);

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

    // if (g_feat_config.sni_bridge_id >= 0) {
    //     stream_bridge_close(g_feat_config.sni_bridge_id);
    // }
}
