/**
 * @file Transformer_master.h
 * @author zhanghognfei (zhanghongfei@iie.ac.cn)
 * @brief
 * @version 0.1
 * @date 2023-05-04
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __TRANSFORMER_MASTER_PLUG_H__
#define __TRANSFORMER_MASTER_PLUG_H__

#include <MESA/Maat_rule.h>
#include <MESA/cJSON.h>
#include <MESA/stream.h>
#include "yyjson.h"
#include <mi_xlgl.h>

#define FORMAT_RECOGNITION_BY_URL 1
#define FORMAT_RECOGNITION_BY_CONTENT_TYPE 2
#define FORMAT_RECOGNITION_BY_CONTENT 3

#define _TF_SELECT_MASK 1
#define _TF_SCAN_MASK 2

#define TF_NO_SELECT_SCAN 0
#define TF_SELECT_ONLY (_TF_SELECT_MASK)
#define TF_SCAN_ONLY (_TF_SCAN_MASK)
#define TF_SELECT_SCAN (_TF_SELECT_MASK | _TF_SCAN_MASK)

#define TF_MAAT_LOG_ABORT 0
#define TF_MAAT_LOG_FILE 1     //modify by lishu
#define TF_MAAT_LOG_MESSAGE 2  //modify by lishu 
#define TF_MAAT_LOG_PCAP 4     //modify by lishu
#define TF_MAAT_LOG_ALL 8
#define TF_MAAT_LOG_NOFILE 16

//Add by qiguangbo
#define MAX_FILE_ID_LEN 256
#define MAX_FILE_NAME_LEN 256

#ifdef __cplusplus
extern "C"
{
#endif
    struct TF_file_handle;

    typedef void TF_record_update_callback_t(void *record_map, void *u_para);
    typedef void TF_fc_update_callback_t(int is_valid, void *u_para);

    typedef struct _transmission_static_t
    {
        unsigned long long c2s_pkts;
        unsigned long long s2c_pkts;
        unsigned long long c2s_bytes;
        unsigned long long s2c_bytes;
        unsigned long long s2c_lost_len;
        unsigned long long c2s_lost_len;
        unsigned long long c2s_pkts_data;
        unsigned long long s2c_pkts_data;
        unsigned long long c2s_bytes_data;
        unsigned long long s2c_bytes_data;
        unsigned long long s2c_lost_len_data;
        unsigned long long c2s_lost_len_data;

    } transmission_static_t;

    struct TF_msg_handle
    {
        yyjson_mut_doc *doc;
        yyjson_mut_val *log;
        char log_id[32];
        u_int64_t stream_id;
        const struct streaminfo *stream;
        int32_t protocol_in;
        int32_t sub_protocol_in;
        int32_t do_log;
        int32_t topic_index;
        transmission_static_t pending_tcp_static;
        transmission_static_t close_tcp_static;
    };

    typedef enum _MSG_PROTO_TYPE
    {
        PROTO_IPv4,
        PROTO_IPv6,
        PROTO_TCP,
        PROTO_UDP,
        PROTO_HTTP,
        PROTO_MAIL,
        PROTO_DNS,
        PROTO_FTP,
        PROTO_IPSEC,
        PROTO_VPN, // G1历史遗留，逻辑上等价OPENVPN
        PROTO_SSL,
        PROTO_SSH,
        PROTO_PPTP,
        PROTO_L2TP,
        PROTO_OPENVPN,
        // PROTO_IP_CNN,
        PROTO_TELNET,
        PROTO_QUIC,
        // PROTO_ECH,
        // PROTO_ESNI, //合并，如果需要区分单独通过SSL_ENCRY_TYPE
        PROTO_GRE,
        PROTO_SOCKS,
        PROTO_SIP,
        PROTO_RTP,
        PROTO_BGP,
        PROTO_RADIUS,
        // PROTO_STREAMING_MEDIA, //未使用
        PROTO_STRATUM,
        PROTO_IM,
        PROTO_ESNI,
        PROTO_ECH,
        PROTO_ICMP,
        PROTO_IGMP,
        PROTO_IP_IN_IP,
        PROTO_DCCP,
        PROTO_RSVP,
        PROTO_DSR,
        PROTO_ESP,
        PROTO_AH,
	    PROTO_ICMPV6,
        PROTO_EIGRP,
        PROTO_OSPF,
        PROTO_ETHERIP,
        PROTO_PIM,
        PROTO_COMP,
        PROTO_IPX_IN_IP,
        PROTO_VRRP,
        PROTO_PGM,
        PROTO_PTP,
        PROTO_ISIS_OVER_IPV4,
        PROTO_SCTP,
        PROTO_FIBRE_CHANNEL,
        PROTO_MOBILITY_HEADER,
        PROTO_UDPLITE,
        PROTO_MPLS_IN_IP,
        PROTO_MANET_PROTOCOLS,
        PROTO_HIP,
        PROTO_SHIM6_PROTOCOL,
        PROTO_WESP,
        PROTO_ROHC,
        PROTO_NSH,
        PROTO_HOMA_TRANSPORT_PROTOCOL,

        PROTO_MAX
    } TF_prot_t;

    typedef enum _transforme_opt
    {
        // Shared MSG options
        MSG_OPT_SCENE_FILE = 1, // IP pcap/Mail content
        MSG_OPT_PROT_SUBTYPE,
        MSG_OPT_FILE_PATH,
        MSG_OPT_INJECTED_PKT_FILE,
        MSG_OPT_PAYLOAD,
        MSG_OPT_FIRST_PKT,
        MSG_OPT_FINAL_PKT,
        MSG_OPT_CURRENT_PKT,
        MSG_OPT_C2S_N_PKT,
        MSG_OPT_S2C_N_PKT, // 10
        MSG_OPT_C2S_ALL_PKT,
        MSG_OPT_S2C_ALL_PKT,
        MSG_OPT_FILE_TYPE,
        MSG_OPT_FILE_SIZE,
        MSG_OPT_FILE_FILE_INTEGRITY,
        MSG_OPT_LOG_POSTBACK_FLAG,
        MSG_OPT_RANDOM_STREAM_FLAG,
        MSG_OPT_APP_NODE_INFORMATION,
        MSG_OPT_STREAM_ACTIVE_FLAG,

        // TCP/UDP
        MSG_OPT_TCPUDP_DETECTION_ALGORITHM = 100,
        MSG_OPT_TCPUDP_C2S_PAYLOAD_LENGTH_SEQ,
        MSG_OPT_TCPUDP_S2C_PAYLOAD_LENGTH_SEQ,
        MSG_OPT_TCPUDP_CURRENT_PAYLOAD,
        MSG_OPT_TCP_FIRST_PAYLOAD,
        MSG_OPT_UDP_FIRST_PAYLOAD,
        MSG_OPT_TCP_FIRST_PAYLOAD_LEN,
        MSG_OPT_UDP_FIRST_PAYLOAD_LEN,
        MSG_OPT_TCPUDP_PAYLOAD_SEQ,
        MSG_OPT_IP_CURRENT_PAYLOAD,
        MSG_OPT_FLOW_FILE_ID,
        MSG_OPT_TRANSPORT_FILE_ID,

        // HTTP
        MSG_OPT_HTTP_REQ_LINE = 200, // 20
        MSG_OPT_HTTP_REQ_HDR,
        MSG_OPT_HTTP_REQ_BODY,
        MSG_OPT_HTTP_RES_LINE,
        MSG_OPT_HTTP_RES_HDR,
        MSG_OPT_HTTP_RES_BODY,
        MSG_OPT_HTTP_URL,
        MSG_OPT_HTTP_C2S_ISN,    // size=4
        MSG_OPT_HTTP_PROXY_FLAG, // size=4 ,0 or 1
        MSG_OPT_HTTP_SEQUENCE,   // size=4
        MSG_OPT_HTTP_REFERER,    // 30
        MSG_OPT_HTTP_HOST,
        MSG_OPT_HTTP_METHOD,
        MSG_OPT_RETURN_CODE,
        MSG_OPT_USER_AGENT,
        MSG_OPT_HTTP_COOKIE,
        MSG_OPT_HTTP_VERSION,
        MSG_OPT_HTTP_X_FORWARD_FOR,
        MSG_OPT_HTTP_CONTENT_IDENTIFY_TYPE,
        MSG_OPT_HTTP_SET_COOKIE,
        MSG_OPT_HTTP_REQUEST_CONTENT_TYPE, // 40
        MSG_OPT_HTTP_RESPONSE_CONTENT_TYPE,
        MSG_OPT_HTTP_REQUEST_CONTENT_LEN,
        MSG_OPT_HTTP_RESPONSE_CONTENT_LEN,
        MSG_OPT_HTTP_CONTENT_FILE_NAME,
        MSG_OPT_HTTP_CURRENT_PAYLOAD,
        MSG_OPT_HTTP_N_PAYLOAD,
        MSG_OPT_HTTP_FILE_PATH,
        MSG_OPT_HTTP_SERVER,
        MSG_OPT_HTTP_URI,
        MSG_OPT_HTTP_VIA,
        MSG_OPT_HTTP_LOCATION,
        MSG_OPT_HTTP_RA_SID,
        MSG_OPT_HTTP_ALTERNATE_PROTOCOL,
        MSG_OPT_HTTP_OTHERS,
        MSG_OPT_HTTP_FILE_ID,
        MSG_OPT_HTTP_RES_VIA,
        MSG_OPT_HTTP_REQ_ACCOUNT,
        MSG_OPT_HTTP_REQ_PASSWORD,
        MSG_OPT_HTTP_ALL_PAYLOAD,
        

        // MAIL
        MSG_OPT_MAIL_PROTO = 300, // string:"pop3","smtp" or "imap4"
        MSG_OPT_MAIL_FROM,
        MSG_OPT_MAIL_USERNAME, // 50
        MSG_OPT_MAIL_PASSWORD,
        MSG_OPT_MAIL_FROM_CMD,
        MSG_OPT_MAIL_TO_CMD,
        MSG_OPT_MAIL_SUBJECT,
        MSG_OPT_MAIL_EML_FILE_ID,
        MSG_OPT_MAIL_TO,
        MSG_OPT_MAIL_CC,
        MSG_OPT_MAIL_BCC,
        MSG_OPT_MAIL_AUTH_INFO,
        MSG_OPT_MAIL_DATE, // 60
        MSG_OPT_MAIL_SUBJECT_CHARSET,
        MSG_OPT_MAIL_RECEIVED,
        MSG_OPT_MAIL_RETURN_PATH,
        MSG_OPT_MAIL_REPLY_TO,
        MSG_OPT_MAIL_RESENT_FROM,
        MSG_OPT_MAIL_RESENT_TO,
        MSG_OPT_MAIL_RESENT_DATE,
        MSG_OPT_MAIL_MESSAGE_ID,
        MSG_OPT_MAIL_X_ORIGINATING_IP,
        MSG_OPT_MAIL_LOGIN_RESULT, // 70
        MSG_OPT_MAIL_ATTACHMENTS_NAME,
        MSG_OPT_MAIL_ATTACHMENTS_NAME_CHARSET,
        MSG_OPT_MAIL_CONTENT_PATH,
        MSG_OPT_MAIL_ATTACHMENTS_PATH,
        MSG_OPT_MAIL_EML_PATH,
        MSG_OPT_MAIL_SERVER_DOMAIN,
        MSG_OPT_MAIL_EHLO,
        MSG_OPT_MAIL_CONTENT,
        MSG_OPT_MAIL_CONTENT_CHARSET,
        MSG_OPT_MAIL_ATTACH_CONTENT,
        MSG_OPT_MAIL_ATTACH_FILE_ID,

        // DNS
        // Shared with FD JC  FC
        MSG_OPT_DNS_QTYPE = 400,    // Shared with FD JC  FC
        MSG_OPT_DNS_QCLASS,         // Shared with FD JC  FC
        MSG_OPT_DNS_OPCODE,         // Shared with FD JC  FC
        MSG_OPT_DNS_QNAME,          // 80 Shared with FD JC  FC
        MSG_OPT_DNS_CHEAT_TYPE,     // Only in FD
        MSG_OPT_DNS_CHEAT_RCODE,    // Only in FD
        MSG_OPT_DNS_CHEAT_STRATEGY, // Only in FD
        MSG_OPT_DNS_CHEAT_RECORD,   // Only in FD
        MSG_OPT_DNS_CHEAT_TTL,      // Only in FD
        MSG_OPT_DNS_QR,
        MSG_OPT_DNS_RA,
        MSG_OPT_DNS_RR,
        MSG_OPT_DNS_TTL,
        MSG_OPT_DNS_DNS_SUB, // 90 size=sizeof(int) 0-DNS,1-DNSSEC
        MSG_OPT_DNS_MESSAGE_ID,
        MSG_OPT_DNS_QDCOUNT,
        MSG_OPT_DNS_ANCOUNT,
        MSG_OPT_DNS_AUCOUNT,
        MSG_OPT_DNS_ADCOUNT,
        MSG_OPT_DNS_AA,
        MSG_OPT_DNS_TC,
        MSG_OPT_DNS_RD,
        MSG_OPT_DNS_FLAGS,
        MSG_OPT_DNS_CNAME,
        MSG_OPT_DNS_A,
        MSG_OPT_DNS_AAAA,
        MSG_OPT_DNS_MX,
        MSG_OPT_DNS_NS,
        MSG_OPT_DNS_AD,
        MSG_OPT_DNS_CD,
        MSG_OPT_DNS_RCODE,
        MSG_OPT_DNS_RES_RR_NAME,
        MSG_OPT_DNS_RR_TYPE,
        MSG_OPT_DNS_RR_CLASS,
        MSG_OPT_DNS_RR_TTL,
        MSG_OPT_DNS_RR_LENGTH,
        MSG_OPT_DNS_RES_RR_A,
        MSG_OPT_DNS_RES_RR_AAAA,
        MSG_OPT_DNS_RES_RR_MX,
        MSG_OPT_DNS_RES_RR_TXT,
        MSG_OPT_DNS_RES_RR_NS,
        MSG_OPT_DNS_RES_RR_CNAME,
        MSG_OPT_DNS_OPT_UDP_PAYLOAD,
        MSG_OPT_DNS_TXT,

        // FTP
        MSG_OPT_FTP_LINK_TYPE = 500,
        MSG_OPT_FTP_USERNAME, // 100
        MSG_OPT_FTP_PASSWORD,
        MSG_OPT_FTP_MSGIN_RESULT,
        MSG_OPT_FTP_URL,
        MSG_OPT_FTP_DIRECTION,
        MSG_OPT_FTP_FILE_NAME,
        MSG_OPT_FTP_CONTENT,
        MSG_OPT_FTP_FILE_PATH,
        MSG_OPT_FTP_COMMAND,
        MSG_OPT_FTP_COMMAND_RESULT,
        MSG_OPT_FTP_FILE_ID,

        // PPTP
        MSG_OPT_PPTP_TUNNEL_TYPE = 600, //  110 size=sizeof(int),1-control,2-data
        MSG_OPT_PPTP_ENCRYPT_MODE,      // size=sizeof(int),1-MMPE 2-IPSEC 3-PAP 4-CHAP 5-MS-CHAP(v1/v2) 6-EAP-TLS
        MSG_OPT_PPTP_CONTROL_MESSAGE_TYPE,
        MSG_OPT_PPTP_VENDOR_NAME,
        MSG_OPT_PPTP_HOST_NAME,
        MSG_OPT_PPTP_REQUEST_CALL_ID,
        MSG_OPT_PPTP_REPLY_CALL_ID,
        MSG_OPT_PPTP_CONTENT_TYPE,

        // L2TP
        MSG_OPT_L2TP_TUNNEL_TYPE = 700, // size=sizeof(int),1-control,2-data
        MSG_OPT_L2TP_ENCRYPT_MODE,      // size=sizeof(int),0-other,1-IPSEC 2-none
        MSG_OPT_L2TP_CHAP_NAME,         // L2TP Username
        MSG_OPT_L2TP_TUNNEL_ID,
        MSG_OPT_L2TP_SESSION_ID,
        MSG_OPT_L2TP_MESSAGE_TYPE,
        MSG_OPT_L2TP_CLIENT_HOST_NAME,
        MSG_OPT_L2TP_SERVER_HOST_NAME,
        MSG_OPT_L2TP_CLIENT_VENDOR_NAME,
        MSG_OPT_L2TP_SERVER_VENDOR_NAME,
        MSG_OPT_L2TP_CONTENT_TYPE, // L2TP Username

        // IPSEC
        MSG_OPT_IPSEC_EX_PROTOCOL = 800, // size=sizeof(int),1-ISAKMP(V1) 2-IKEv2 3-other
        MSG_OPT_IPSEC_ISAKMP_MODE,

        // OPENVPN
        MSG_OPT_OPENVPN_VERSION = 900, // size=sizeof(int)
        MSG_OPT_OPENVPN_ENCRYPT_MODE,  // string
        MSG_OPT_OPENVPN_HMAC,          // size=sizeof(int),1-has,0-not has
        MSG_OPT_OPENVPN_TUNNEL_TYPE,   // 120
        MSG_OPT_OPENVPN_CLIENT_SESSION_ID,
        MSG_OPT_OPENVPN_SERVER_SESSION_ID,
        MSG_OPT_OPENVPN_MESSAGE_TYPE,

        // SSH
        MSG_OPT_SSH_VERSION = 1000, // string
        MSG_OPT_SSH_HOST_KEY,       // string
        MSG_OPT_SSH_HOST_COOKIE,    // string
        MSG_OPT_SSH_ENCRYPT_MODE,   // size=sizeof(int)
        MSG_OPT_SSH_MAC,            // string
        MSG_OPT_SSH_TUNNEL_TYPE,
        MSG_OPT_SSH_S_VERSION,
        MSG_OPT_SSH_C_VERSION,
        MSG_OPT_SSH_S_KEX_ALGOR,
        MSG_OPT_SSH_C_KEX_ALGOR,
        MSG_OPT_SSH_S_HOST_ALGOR,
        MSG_OPT_SSH_C_HOST_ALGOR,
        MSG_OPT_SSH_S_MAC_ALGOR,
        MSG_OPT_SSH_C_MAC_ALGOR,

        // SSL
        MSG_OPT_SSL_VERSION = 1100, // string
        MSG_OPT_SSL_SNI,            // string
        MSG_OPT_SSL_JA3,
        MSG_OPT_SSL_JA3S, // 130
        MSG_OPT_ENCRYPT_TYPE,
        MSG_OPT_SSL_SERVER_CIPHERSUITS,
        MSG_OPT_SSL_CLIENT_CIPHERSUITS,
        MSG_OPT_SSL_SERVER_CERT_FINGERPRINT,
        MSG_OPT_SSL_SERVER_CERT,
        MSG_OPT_SSL_SERVER_CERT_TYPE,
        MSG_OPT_SSL_SERVER_VERSION,
        MSG_OPT_SSL_SERVER_SESSION_ID,
        MSG_OPT_SSL_SERVER_RANDOM_NUMBER,
        MSG_OPT_SSL_SERVER_EXTENSIONS, // 140
        MSG_OPT_SSL_SERVER_SERIAL_NUMBER,
        MSG_OPT_SSL_SERVER_ALGORITHM_ID,
        MSG_OPT_SSL_SERVER_ISSUER,
        MSG_OPT_SSL_SERVER_ISSUER_COUNTRY,
        MSG_OPT_SSL_SERVER_ISSUER_PROVINCE,
        MSG_OPT_SSL_SERVER_ISSUER_LOCALITY,
        MSG_OPT_SSL_SERVER_ISSUER_ORGANIZE,
        MSG_OPT_SSL_SERVER_ISSUER_CNAME,
        MSG_OPT_SSL_SERVER_SUB,
        MSG_OPT_SSL_SERVER_SUB_COUNTRY, // 150
        MSG_OPT_SSL_SERVER_SUB_ORGANIZE,
        MSG_OPT_SSL_SERVER_SUB_CNAME,
        MSG_OPT_SSL_SERVER_START_TIME,
        MSG_OPT_SSL_SERVER_EXPIRE_TIME,
        MSG_OPT_SSL_SERVER_SAN,
        MSG_OPT_SSL_SERVER_CERT_PUBLIC_KEY,
        MSG_OPT_SSL_SERVER_ISSUER_UNIT_NAME,
        MSG_OPT_SSL_SERVER_ISSUER_TITLE,
        MSG_OPT_SSL_SERVER_ISSUER_DESCRIPTION,
        MSG_OPT_SSL_SERVER_ISSUER_BUSINESS_CATEGORY, // 160
        MSG_OPT_SSL_SERVER_ISSUER_ADDRESS,
        MSG_OPT_SSL_SERVER_SUB_UNIT_NAME,
        MSG_OPT_SSL_SERVER_SUB_TITLE,
        MSG_OPT_SSL_SERVER_SUB_DESCRIPTION,
        MSG_OPT_SSL_SERVER_CERT_PATH,
        MSG_OPT_SSL_CLIENT_CERT,
        MSG_OPT_SSL_CLIENT_CERT_TYPE,
        MSG_OPT_SSL_CLIENT_VERSION,
        MSG_OPT_SSL_CLIENT_SESSION_ID,
        MSG_OPT_SSL_CLIENT_RANDOM_NUMBER,
        MSG_OPT_SSL_CLIENT_EXTENSIONS, // 170
        MSG_OPT_SSL_CLIENT_SERIAL_NUMBER,
        MSG_OPT_SSL_CLIENT_ALGORITHM_ID,
        MSG_OPT_SSL_CLIENT_ISSUER,
        MSG_OPT_SSL_CLIENT_ISSUER_COUNTRY,
        MSG_OPT_SSL_CLIENT_ISSUER_PROVINCE,
        MSG_OPT_SSL_CLIENT_ISSUER_LOCALITY,
        MSG_OPT_SSL_CLIENT_ISSUER_ORGANIZE,
        MSG_OPT_SSL_CLIENT_ISSUER_CNAME,
        MSG_OPT_SSL_CLIENT_SUB,
        MSG_OPT_SSL_CLIENT_SUB_COUNTRY, // 180
        MSG_OPT_SSL_CLIENT_SUB_ORGANIZE,
        MSG_OPT_SSL_CLIENT_SUB_CNAME,
        MSG_OPT_SSL_CLIENT_START_TIME,
        MSG_OPT_SSL_CLIENT_EXPIRE_TIME,
        MSG_OPT_SSL_CLIENT_SAN,
        MSG_OPT_SSL_CLIENT_CERT_PUBLIC_KEY,
        MSG_OPT_SSL_CLIENT_ISSUER_UNIT_NAME,
        MSG_OPT_SSL_CLIENT_ISSUER_TITLE,
        MSG_OPT_SSL_CLIENT_ISSUER_DESCRIPTION,
        MSG_OPT_SSL_CLIENT_ISSUER_BUSINESS_CATEGORY, // 190
        MSG_OPT_SSL_CLIENT_ISSUER_ADDRESS,
        MSG_OPT_SSL_CLIENT_SUB_UNIT_NAME,
        MSG_OPT_SSL_CLIENT_SUB_TITLE,
        MSG_OPT_SSL_CLIENT_SUB_DESCRIPTION,
        MSG_OPT_SSL_CLIENT_CERT_PATH,
        MSG_OPT_SSL_JA3_FULL_STRING,
        MSG_OPT_SSL_JA3S_FULL_STRING, 
        MSG_OPT_SSL_SERVER_SUPPORT_VERSION, 
        MSG_OPT_SSL_SERVER_SUB_PROVINCE,
        MSG_OPT_SSL_SERVER_SUB_LOCALITY,
        MSG_OPT_SSL_CLIENT_SUB_PROVINCE,
        MSG_OPT_SSL_CLIENT_SUB_LOCALITY,
        MSG_OPT_SSL_CLIENT_ALL_VERSION,
        MSG_OPT_SSL_CLIENT_EX_SUPPORT_VERSION,
        MSG_OPT_SSL_CLIENT_EX_ALPN,
        MSG_OPT_SSL_JA4,
        MSG_OPT_SSL_CLIENT_HELLO_PAYLOAD_LEN,
        MSG_OPT_SSL_JA4_FULL_STRING,
        MSG_OPT_SSL_JA4S,
        MSG_OPT_SSL_JA4S_FULL_STRING,
        MSG_OPT_SSL_ESNI,
        MSG_OPT_SSL_ECH,
        MSG_OPT_SSL_SERIAL_NUM,
        MSG_OPT_SSL_CLIENT_CERT_FINGERPRINT,
        MSG_OPT_SSL_CLIENT_CERT_CHAIN,
        MSG_OPT_SSL_SERVER_CERT_CHAIN,
        MSG_OPT_SSL_CLIENT_CERT_FILE_ID,
        MSG_OPT_SSL_SERVER_CERT_FILE_ID,
        // RADIUS-MSG
        MSG_OPT_RADIUS_CODE = 1200,
        MSG_OPT_RADIUS_ACCOUNT,
        MSG_OPT_NAS_IP,
        MSG_OPT_FRAMED_IP,

        // QUIC
        MSG_OPT_QUIC_VERSION = 1300,
        MSG_OPT_QUIC_SNI,
        MSG_OPT_QUIC_UA,

        // TELNET
        MSG_OPT_TELNET_USERNAME = 1400,
        MSG_OPT_TELNET_PASSWORD,
        MSG_OPT_TELNET_CONTENT_PATH,

        // 兼容已有格式
        MSG_OPT_LOG_TYPE = 1500,

        // VOIP新增字段
        MSG_OPT_SIP_CALLING_ACCOUNT = 1600,
        MSG_OPT_SIP_CALLED_ACCOUNT,
        MSG_OPT_SIP_CALL_ID,
        MSG_OPT_SIP_AUDIO_D_IP,
        MSG_OPT_SIP_AUDIO_S_IP,
        MSG_OPT_SIP_AUDIO_D_PORT,
        MSG_OPT_SIP_AUDIO_S_PORT,
        MSG_OPT_SIP_SDP_C2S,
        MSG_OPT_SIP_SDP_S2C,
        MSG_OPT_SIP_CONTACTS,
        MSG_OPT_SIP_VIA,
        MSG_OPT_SIP_ROUTE,
        MSG_OPT_SIP_RECORD_ROUTE,
        MSG_OPT_SIP_USER_AGANT,
        MSG_OPT_SIP_SERVER,
        MSG_OPT_SIP_DURATION_S,
        MSG_OPT_SIP_BYER,
        MSG_OPT_RTP_PT_C2S,
        MSG_OPT_RTP_PT_S2C,
        MSG_OPT_VOIP_PROTOCOL_TYPE,
        MSG_OPT_VOIP_CALLING_PROG_ID,
        MSG_OPT_VOIP_CALLED_PROG_ID,
        MSG_OPT_VOIP_FAIL_REASON,
        MSG_OPT_VOIP_SHIELD_SWITVH,
        MSG_OPT_VOIP_TALK_BLOCK_START_TIME,
        MSG_OPT_VOIP_TEMPLET_CHECK_TIME,
        MSG_OPT_VOIP_SHIELD_TIME,
        MSG_OPT_VOIP_SHIELD_HARMFUL_FLAG,
        MSG_OPT_VOIP_HIT_RULE_LEVEL,
        MSG_OPT_VOIP_ID,
        MSG_OPT_VOIP_INTERCEPT_STATUS,
        MSG_OPT_VOIP_FILE_ATTRIBUTE,
        MSG_OPT_VOIP_ASSOCIATE_TIME,
        MSG_OPT_VOIP_CALLING_FILE_ID,
        MSG_OPT_VOIP_CALLED_FILE_ID,
        MSG_OPT_VOIP_SIP_CALLING_GATEWAY,
        MSG_OPT_VOIP_SIP_CALLED_GATEWAY,
        MSG_OPT_VOIP_SIP_CALLING_GATEWAY_COUNTRY,
        MSG_OPT_VOIP_SIP_CALLED_GATEWAY_COUNTRY,
        MSG_OPT_VOIP_SIP_CALLING_GATEWAY_PROVINCE,
        MSG_OPT_VOIP_SIP_CALLED_GATEWAY_PROVINCE,
        MSG_OPT_VOIP_SIP_CALLING_GATEWAY_CITY,
        MSG_OPT_VOIP_SIP_CALLED_GATEWAY_CITY,
        MSG_OPT_VOIP_SIP_CALLING_NUMBER,
        MSG_OPT_VOIP_SIP_CALLED_NUMBER,
        MSG_OPT_VOIP_SIP_CALLING_OPEN_PORT,
        MSG_OPT_VOIP_SIP_CALLED_OPEN_PORT,
        MSG_OPT_VOIP_SIP_CALLING_CODEC_TYPE,
        MSG_OPT_VOIP_SIP_CALLED_CODEC_TYPE,
        MSG_OPT_VOIP_RTP_CALLING_CODEC_TYPE,
        MSG_OPT_VOIP_RTP_CALLED_CODEC_TYPE,



        //BGP
        MSG_OPT_BGP_PROT_TYPE = 1700,
        MSG_OPT_BGP_AS,
        MSG_OPT_BGP_VERSION,
        MSG_OPT_BGP_HOLD_TIME,
        MSG_OPT_BGP_IDENTIFIER,
        MSG_OPT_BGP_CAPABILITY_TYPE,
        MSG_OPT_BGP_NLRI_ROUTES_IP,
        MSG_OPT_BGP_WITHDRAWN_ROUTES_IP,
        MSG_OPT_BGP_PATH_ATTRIBUTE_TYPE,

        MSG_OPT_GTEST_MASTER = 1800,

        MSG_OPT_MAX = 1900
    } TF_opt_t;

    typedef enum _TF_SSL_ENC
    {
        TF_SSL_DEFAULT,
        TF_SSL_ESNI,
        TF_SSL_ECH,
        TF_SSL_ENC_MAX
    }TF_SSL_ENC_T;

    typedef enum _TF_action
    {
        TF_ACTION_ABORT,   // FX
        TF_ACTION_BLOCK,   // FD
        TF_ACTION_MONITOR, // JC
        TF_ACTION_COLLECT, // FC
        TF_ACTION_WAIT,    // WAIT
        TF_ACTION_UNKNOWN  // DEFAULT
    } TF_action_t;

    typedef enum _TF_table_type_t
    {
        IP_PORT_TABLE,
        UNIVERSAL_IP_TABLE = 10,
        UNIVERSAL_PROT_TABLE,
        HTTP_URL_TABLE = 20,
        HTTP_REQ_HDR_TABLE,
        HTTP_RES_HDR_TABLE,
        HTTP_REQ_BODY_TABLE,
        HTTP_RES_BODY_TABLE,
        HTTP_INTERVAL_TABLE,
        HTTP_REQ_INTERVAL_TABLE,
        HTTP_RES_INTERVAL_TABLE,

        DNS_REQ_REGION_TABLE = 30,
        DNS_RES_REGION_TABLE,
        DNS_RESPONSE_STRATEGY,
        DNS_GROUP_TYPE,
        DNS_FAKE_INFO,
        DNS_FAKE_IP,
        SSL_REGION_TABLE = 40,
        MAIL_HDR_TABLE = 50,
        MAIL_BODY_TABLE,
        FTP_REGION_TABLE = 60,
        IP_REGION_TABLE = 70,
        SIP_REGION_TABLE = 80,
        USER_CODE_REGION_TABLE = 90,
        USER_IP_REGION_TABLE,
        DJ_PAYLOAD_TABLE=100,
        DJ_PAYLOAD_LEN_TABLE,
        QUIC_REGION_TABLE = 110,
        VPN_INTEGER_REGION_TABLE = 120,
        VPN_STRING_REGION_TABLE,

        TABLE_MAX // DEFAULT
    } TF_table_type_t;

    typedef enum _TF_pro_id_type_t
    {
        TF_DEFAULT_ENUM,
        L7_PORT_TYPE,
        TF_PROT_ID_TYPE_MAX
    } TF_pro_id_type_t;

    typedef enum _TF_file_meta_opt_t
    {
        META_OPT_FILE_ID,
        META_OPT_FILE_NAME,
        META_OPT_FILE_TYPE,
        META_OPT_META,
        META_OPT_COMBINE_MODE,
        META_OPT_BUSINESS_TYPE,
        META_OPT_STREAM_INFO
    } TF_file_meta_opt_t;

    typedef struct _TF_opt_unit_t
    {
        TF_opt_t opt_type;
        int opt_len;
        const void *opt_value;
    } TF_opt_unit_t;

    typedef struct _TF_label_t
    {
        int meta_num;
        TF_opt_unit_t *prot_meta;
    } TF_label_t;
    // xlgl msg_info
    typedef struct signal_msg_info_
    {
        char imsi[MAX_XL_VALUE_LEN];
        char msisdn[MAX_XL_VALUE_LEN];
        char imei[MAX_XL_VALUE_LEN];
        char apn[32];
        char usr_pos_info[128];
        char src_ip_host[128];
        char dst_ip_host[128];
        char src_tunnel_ip_host[128];
        char dst_tunnel_ip_host[128];
        uint32_t nat_b_addr; // 公网IP（主机序）
        uint16_t nat_b_port; // 公网Port (主机序)
        char imsi_mcc_str[4];
        char imsi_mnc_str[3];
        char msisdn_home_str[8];
        char imei_brand_str[9];
        uint16_t tunnel_version; // 隧道版本号
        char key_version_str[32]; 

    } signal_msg_info;

    typedef struct _TF_scan_rule_t{
        int result_num;
        int log_type[MAAT_MAX_HIT_RULE_NUM];
        Maat_rule_t result[MAAT_MAX_HIT_RULE_NUM];
    } TF_scan_rule;

    // 定义元组结构体 TDRZ需求专用
    typedef struct _Tuple_t{
        volatile long long timestamp;						//时间戳
        int index;											//序号
        char direction;										//方向
        int length;											//长度
    } Tuple;

    typedef struct _TF_hit_flag_
    {
        struct _TF_hit_flag_ * next;
        int config_id;
        char hit_flag;
    }TF_hit_flag_t;

    typedef enum TF_bridge_type_
    {
        L7_APP_INFO,
        SIGNAL_MSG,
        APP_ID,
        IP_MONITOR,
        TDRZ_PB,
        FLOW_FILE_ID,  //add by lishu
        L7_PROTO_FLAG,
        FEATURE_BRIDGE, //add by qiguangbo
        TF_BRIDGE_MAX
    } TF_bridge_type;

    /*********************************************************************************
     *
             NOTE:
            global variables
     *
    ************************************************************************************/
    typedef enum _TF_region_type
    {
        TF_IP_TYPE,
        TF_IP_PLUS_TYPE,
        TF_EXPR_TYPE,
        TF_EXPR_PLUS_TYPE,
        TF_INTVAL_TYPE,
        TF_INTVAL_PLUS_TYPE,
        TF_REGION_TYPE_MAX // DEFAULT
    } TF_region_type;

    extern char *g_prot_region_name[PROTO_MAX][TF_REGION_TYPE_MAX];
    extern char *g_district_name[MSG_OPT_MAX];
    extern volatile long long g_current_time;                   //系统当前时间（毫秒级）

    /***************************************************************************************
        NOTE:
              thransformer MSG interface

    ****************************************************************************************/

    /**
     * @brief
     *
     * @param prot      transforme defined prot type
     * @param a_stream  mesa_platform defined prot type
     * @return struct TF_msg_handle*   MSG handle
     */
    struct TF_msg_handle *TF_msg_start(TF_prot_t prot,
                                       const struct streaminfo *a_stream);

    /**
     * @brief
     *
     * @param msg_handle  MSG handle
     * @param prot_opt    transforme defined prot opt
     * @return int        <0 failed, 0 succ
     */
    int TF_msg_update_prot(struct TF_msg_handle *msg_handle, TF_prot_t prot_opt);

    /**
     * @brief
     *
     * @param msg_handle  MSG handle
     * @param prot_opt    transforme defined prot opt
     * @param name        prot name
     * @param value       prot value
     * @return int        <0 failed, 0 succ
     */
    int TF_msg_update_int(struct TF_msg_handle *msg_handle,
                          TF_opt_t prot_opt,
                          long value);

    /**
     * @brief
     *
     * @param msg_handle  MSG handle
     * @param prot_opt    transforme defined prot opt
     * @param name        prot name
     * @param value       prot value
     * @return int        <0 failed, 0 succ
     */
    int TF_msg_replace_update_int(struct TF_msg_handle *msg_handle, TF_opt_t prot_opt, long value);

    /**
     * @brief
     *
     * @param msg_handle  MSG handle
     * @param prot_opt    transforme defined prot opt
     * @param value       prot value
     * @return int        <0 failed, 0 succ
     */
    int TF_msg_update_string(struct TF_msg_handle *msg_handle,
                             TF_opt_t prot_opt,
                             const char *value);

    /**
     * @brief
     *
     * @param msg_handle MSG handle
     * @param prot_opt   transforme defined prot opt
     * @param value      prot value  ,type is binary
     * @param length     <0 failed, 0 succ
     * @return int
     */
    int TF_msg_update_binary(struct TF_msg_handle *msg_handle,
                             TF_opt_t prot_opt,
                             const char *value,
                             const int length);

    /**
     * @brief
     *
     * @param msg_handle  MSG handle
     * @param prot_opt    transforme defined prot opt
     * @param json        prot value, type is cjson
     * @return int        <0 failed, 0 succ
     */
    int TF_msg_update_json(struct TF_msg_handle *msg_handle, TF_opt_t prot_opt, yyjson_mut_val *value);

    /**
     * @brief
     *
     * @param msg_handle MSG handle
     * @param hit_result Maat hit result，you can set null
     * @param cnt        Maat hit cnt
     * @return int       <0 failed, 0 succ
     */
    int TF_msg_end(struct TF_msg_handle **msg_handle,
                   const struct streaminfo *a_stream,
                   Maat_rule_t *hit_result,
                   int result_num);

    /**
     * @brief
     *
     * @param msg_handle MSG handle
     * @return int       <0 failed, 0 succ
     */
    int TF_msg_cancel(struct TF_msg_handle **msg_handle);

    /**
     * @brief
     *
     * @param prot_opt
     * @return char* \0 end
     */
    char *TF_get_opt_name(TF_opt_t prot_opt);

    /***************************************************************************************
         NOTE:
               thransformer JG common interface

     ****************************************************************************************/
    /**
     * @brief this funtion can only use once time ,otherwise Morpher_plug will face fatal bug.
     *
     * @param a_stream     mesa platform defined stream struct
     * @param hit_result   Maat hit result
     * @param cnt          Maat hit cnt
     * @param p_block      block maat rule
     * @return TF_action_t  transforme defined action
     */
    TF_action_t TF_decide_action(const struct streaminfo *a_stream,
                                 Maat_rule_t *hit_result,
                                 const Maat_rule_t **pp_block,
                                 int cnt);

    /**
     * @brief Check whether it is whitelist
     *
     * @param a_stream   mesa platform defined stream struct
     * @return int       if stream in whitelist return 1, otherwise return 0;
     */
    int is_TF_whitelist(struct streaminfo *a_stream);

    /**
     * @brief
     *
     * @param a_stream  mesa platform defined stream struct
     * @param result    Maat hit result
     */
    void TF_make_blacklist(struct streaminfo *a_stream, Maat_rule_t *result);

    /**
     * @brief
     *
     * @param a_stream
     */
    void TF_make_whitelist(struct streaminfo *a_stream);

    /**
     * @brief
     *
     * @param a_stream       mesa platform defined stream struct
     * @param hit_result     Maat hit result
     * @param cnt            Maat hit cnt
     * @return Maat_rule_t*  fetch block rule
     */
    Maat_rule_t *TF_fetch_block_rule(const struct streaminfo *a_stream,
                                     Maat_rule_t *hit_result,
                                     int cnt);

    /**
     * @brief Get the transformer maat feather object
     *
     * @return Maat_feather_t
     */
    Maat_feather_t TF_get_maat_feather();

    /**
     * @brief
     *
     * @param maat_feather
     * @param a_stream
     * @param proto
     * @param mid
     * @param result
     * @param result_num
     * @return int      <0 failed, 0 succ
     */
    int TF_scan_nesting_proto_addr(Maat_feather_t maat_feather,
                                   const struct streaminfo *a_stream,
                                   TF_prot_t proto,
                                   scan_status_t *mid,
                                   Maat_rule_t *result,
                                   int result_num);
    /**
     * @brief
     *
     * @param maat_feather
     * @param proto_id
     * @param mid
     * @param result
     * @param result_num
     * @return int
     */
    int TF_scan_universe_proto(Maat_feather_t maat_feather,
                               const struct streaminfo *a_stream,
                               int proto_id,
                               scan_status_t *mid,
                               Maat_rule_t *result,
                               int result_num);

    /**
     * @brief
     *
     * @param prot_opt
     * @return char*
     */
    char *TF_get_opt_select_result(TF_opt_t prot_opt, TF_action_t action);
    /**
     * @brief
     *
     * @param stream
     * @param result
     * @param result_num
     * @return int : The number of app_id
     */
    int TF_put_appid_to_bridge(const struct streaminfo *stream, Maat_rule_t *result, int result_num);
    /**
     * @brief
     *
     * @param result
     * @param result_num
     * @return int <0 failed ,>0 value is the prot_id
     */
    int TF_fetch_app_id(Maat_rule_t *result, int result_num);
    
    /**
     * @brief
     *
     * @param prot
     * @param update
     * @param u_para
     * @return int
     */
    // int TF_fc_strategy_callback_register(TF_prot_t prot,
    //	                                 TF_fc_update_callback_t *update,
    //		                             void* u_para);
    /**
     * @brief
     *
     * @param prot
     * @return int
     */
    int is_TF_fc_strategy_valid(TF_prot_t prot);

    /**
     * @brief
     *
     * @param prot_opt
     * @param action
     * @param opt_parm : inout parm ,if OPT have the parm
     * @return int :TF_NO_SELECT_SCAN/TF_SELECT_ONLY/TF_SCAN_ONLY/TF_SELECT_SCAN
     * /-1(error)
     */

    int TF_get_opt_select_parm(TF_opt_t prot_opt,
                               TF_action_t action,
                               char *opt_parm,
                               int *parm_length);

    /**
     * @brief
     *
     * @param max_members
     * @param one_member_num
     * @param seq
     * @param lenth
     * @param length_seq_list : inout parm
     * @return int  0 succ, 1 members is up to max
     */
    int TF_tcp_length_seq_encap(int max_members,
                                int one_member_num,
                                int seq,
                                int lenth,
                                cJSON *length_seq_list);

    /**
     * @brief
     *
     * @param table_type
     * @return char*
     */
    char *TF_get_maat_table_name(TF_table_type_t table_type);
    /**
     * @brief : get dns_strategy_id by the user_region
     *
     * @param user_region
     * @return int
     */
    int TF_get_dns_response_strategy_id(const char *user_region);

    /**
     * @brief 
     *
     * @param user_region
     * @return void
     */
    void TF_get_yyjson_region_item(const char *user_region, const char *item_name, void *item_value);

    /***************************************************************************************
        NOTE:
                thransformer common tools interface

    ****************************************************************************************/

    /**
     * @brief
     *
     * @param src     src string
     * @param len     src string length
     * @return char*  b64 encoding string
     */
    char *TF_b64_encode(const unsigned char *src, int len);


    /**
     * @brief 
     * 
     * @param bin_string 
     * @param bin_strlen 
     * @return char* 
     */
    char *TF_convert_bin2string(const unsigned char *bin_string, unsigned int bin_strlen);

    /**
     * @brief
     *
     * @param buf
     * @param len
     * @param identify_way
     * @return file_type
     */
    char *TF_file_type_analysis(const char *buf, int len, int identify_way);

    /**
     * @brief get the l7_prot by the interface
     *
     * @param a_stream
     * @param l7_protol_name
     * @param length
     */
    void TF_get_L7_lable(struct streaminfo *a_stream,
                         char *l7_protol_name,
                         uint length);

    /**
     * @brief
     *
     * @param l7_protol_name
     * @param length
     * @return true
     * @return false
     */
    bool TF_is_L7_vpn_prot(char *l7_protol_name, uint length);

    /**
     * @brief
     *
     * @param msg_handle
     * @param sub_prot_id
     */
    // Because of the app_id get function changed,now it would be deleted.BY YSC
    // void TF_set_prot_sub_id(TF_msg_handle *msg_handle, int sub_prot_id);
    
    /**
    // get the encrypt_id:0 unknown,1 no_encrypt,2 encrypt
     */
    int  TF_get_encrypt_id(const struct streaminfo *a_stream);
    
    /**
     * @brief
     *
     * @param a_stream
     * @return int
     */
    int TF_get_prot_id(struct TF_msg_handle *msg_handle, const struct streaminfo *a_stream);
    /**
     * @brief
     *
     * @param a_stream
     * @return int
     */
    char* TF_get_prot_sub_id(const struct streaminfo *a_stream);
    /**
     * @brief
     *
     * @param a_stream
     * @return int
     */
    int TF_get_L7_sub_id(struct streaminfo *a_stream);
        
    /**
     * @brief 
     * 
     * @param a_stream 
     * @return char* 
     */
    char * TF_get_prot_name(struct TF_msg_handle *msg_handle, const struct streaminfo *a_stream);
    /**
     * @brief
     *
     */
    int TF_get_L7_info_brgid();

    /**
     * @brief
     *
     */
    int TF_get_signal_info_brgid();

    /**
     * @brief
     *
     * @param bridge_type
     * @return int
     */
    int TF_get_brgid(TF_bridge_type bridge_type);

    /**
     * @brief ysc 该接口所返回的指针地址必须要调用者调用TF_free(&result)释放，不释放会造成内存泄漏，请一定注意。
     * 空间大小至少为packet_sequence_size * 64 * sizeof(char)
     *
     * @param num_tuples 当前有多少个缓存包系列
     * @param packet_sequence_size 总共包序列的大小
     * @param packet_sequence 包序列元组
     * @return  char* result 
     *          失败会返回NULL
     */
    char* TF_encap_packet_seq(int num_tuples, int packet_sequence_size, Tuple *packet_sequence);

    /**
     * @brief ysc 完美空间释放函数
     *
     * @param &result
     */
    void TF_free(void** result);

    /**
     * @brief 
     *      
     * @param a_stream 
     * @param hit_result 
     * @param rule_num 
     */
    void TF_send_instruction(const struct streaminfo *a_stream, Maat_rule_t *hit_result, int rule_num);

    /**
     * @brief
     *
     * @param current
     * @return int
     */
    int is_lowest_ip(const struct streaminfo *current);
    /**
     * @brief
     *
     * @param current
     * @return const struct streaminfo*
     */
    const struct streaminfo *find_lowest_ip_layer(const struct streaminfo *current);
    /**
     * @brief ysc 回流管扫描控
     *
     * @param a_stream table_id result size
     */
    TF_action_t HLGK_scan_addr(const struct streaminfo *a_stream, int table_id,Maat_rule_t *result, int *size);
    /**
     * @brief
     *
     * @param a_stream
     * @param result
     * @param size
     * @param proto
     * @return TF_action_t
     */
    TF_action_t check_stream_tuple4_rule(const struct streaminfo *a_stream, int table_id,Maat_rule_t *result, int *size, int proto);

    /***************************************************************************************
        NOTE:
              thransformer file interface

    ****************************************************************************************/
    union TF_file_id
    {
        UINT8  id_char[8];
        UINT64 id_long;
    };
    /**
    * name : TF_file_id_create
    * functionality : 创建文件唯一标识ID
    * param:
    *       stream : 流结构信息
    *       prot_type : 协议类型
    *       file_type : 文件类型描述
    *       file_type_len : 文件类型描述有效长度
    *       buf  : 业务插件自定义域，用于保证生成Id的唯一性，例如：http协议可以将etag、last_modify字段写入buf传入，若不需要则为NULL
    *       buf_len :  业务插件自定义域长度
    *       fileId : 文件唯一标识ID
    *       fileId_len : 文件唯一标识ID长度
    *
    *  returns:
    *        ==0 : sucess
    *        !=0 : failed
    * */
    int TF_file_id_create(const struct streaminfo *stream, 
                          TF_prot_t prot_type,  
                          char *file_type, 
                          int file_type_len,
                          void *buf, 
                          int buf_len, 
                          char *fileId, 
                          int *fileId_len);

    /**
    * name :TF_file_handle_create
    * functionality : 文件句柄创建函数
    * param: 无
    *
    *  returns:
    *        struct TF_file_handle*
    * */
    struct TF_file_handle* TF_file_handle_create();
    /**
    * name :TF_file_handle_destory
    * functionality : 文件句柄销毁函数
    * param: 
    *       file_handle :  文件句柄
    *
    *  returns:
    *        struct TF_file_handle*
    * */
    int TF_file_handle_destory(struct TF_file_handle *file_handle);


    /**
    * name :TF_file_send_meta
    * functionality : 文件meta发送函数
    * param:
    *       file_handle :  文件句柄
    *       option : meta字段类型
    *       data : 数据
    *       data_len : 数据有效长度，字符串类型不包含'\0'
    *
    *  returns:
    *        struct TF_file_handle*
    * */
    int TF_file_send_meta(struct TF_file_handle *file_handle, TF_file_meta_opt_t option, void *data, size_t data_len);
    /**
    * name :TF_file_send_data
    * functionality : 文件中间包发送函数
    * param:
    *       file_handle: 文件句柄
    *       offset : 偏移量，第一分片包偏移量为0
    *       chunk : 传输数据内容
    *       chunk_len : 当前传输数据长度
    *
    *  returns:
    *        ==0 : sucess
    *        !=0 : failed
    * */
    int TF_file_send_data(struct TF_file_handle *file_handle, u_int64_t offset, const char *chunk, int chunk_len);

    /**
     * @brief
     *
     * @param head
     * @param ele
     * @return void
     **/
    void hit_flag_list_insert(TF_hit_flag_t *head, TF_hit_flag_t **ele);


#ifdef __cplusplus
}
#endif

#endif
