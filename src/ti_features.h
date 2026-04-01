#ifndef TI_FEATURES_H_
#define TI_FEATURES_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <map>
#include <vector>
#include <stdbool.h>
#include <math.h>
#include <MESA/stream.h>
#include <MESA/MESA_prof_load.h>
#include <MESA/MESA_handle_logger.h>
#include <MESA/field_stat2.h>


#include "KafkaProducer.h"
#include "MESA/cJSON.h"
#include "ssl.h"
#include "quic.h"

#define TI_FEATURES_SO "ti_features.so"
#define MAX_PATH_LEN        256
#define MAX_DOMAIN_LEN      128

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

// 序列最大长度（可配置）
#define DEFAULT_MAX_SEQ_LEN     5000

// 阈值默认值
#define DEFAULT_SMALL_PKT_THRESHOLD     64
#define DEFAULT_LARGE_PKT_THRESHOLD     1200
#define DEFAULT_BURST_IAT_THRESHOLD_US  1000
#define DEFAULT_ACTIVE_IAT_THRESHOLD_US 1000
#define DEFAULT_WINDOW_SIZE_MS          1000
#define DEFAULT_INSTANT_BITRATE_WINDOW_MS 100
#define DEFAULT_INTERACTIVE_THRESHOLD_MS  100
#define DEFAULT_BULK_TRANSFER_KBPS      10
#define DEFAULT_SHORT_CONN_THRESHOLD_S  2
#define DEFAULT_FFT_TOP_K               5

// 流方向
#define DIR_UNKNOWN     0
#define DIR_FWD         1   // 源->目标
#define DIR_BWD         2   // 目标->源

typedef enum
{
    TCP,
    UDP,
    SSL,
    QUIC,
    TYPE_NUM
}stream_type;

// 包大小分类（用于 Bigram）
typedef enum
{
    PKT_SIZE_SMALL = 0,   // <= 64B
    PKT_SIZE_MEDIUM = 1,  // 64-1200B
    PKT_SIZE_LARGE = 2    // > 1200B
}pkt_size_class;

#ifdef __cplusplus
extern "C" {
#endif

// 插件入口函数
UCHAR TI_FEATURES_UDP_ENTRY(struct streaminfo *a_stream, void **pme, int thread_seq, void *a_packet);
UCHAR TI_FEATURES_TCP_ENTRY(struct streaminfo *a_stream, void **pme, int thread_seq, void *a_packet);
UCHAR TI_FEATURES_SSL_ENTRY(stSessionInfo *session_info, void **pme, int thread_seq, struct streaminfo *a_stream, void *a_packet);
// UCHAR TI_FEATURES_QUIC_ENTRY(stSessionInfo *session_info, void **pme, int thread_seq, struct streaminfo *a_stream, void *a_packet);
int TI_FEATURES_INIT(void);
void TI_FEATURES_DESTROY(void);

#ifdef __cplusplus
}
#endif

#endif /* TI_FEATURES_H_ */
