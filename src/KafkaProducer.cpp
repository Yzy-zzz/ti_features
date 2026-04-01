/*
 * KafkaProducer.cpp
 *
 *  Created on: 
 *      Author: 
 */
#include "KafkaProducer.h"
#include <stdio.h>

static void kafka_delivery_report_cb(rd_kafka_t* /*rk*/, const rd_kafka_message_t* rkmessage, void* /*opaque*/)
{
	if (!rkmessage) {
		return;
	}

	if (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
		const char* topic_name = rkmessage->rkt ? rd_kafka_topic_name(rkmessage->rkt) : "unknown";
		fprintf(stderr, "[ti_features] Kafka delivery failed, topic=%s, err=%s\n",
			topic_name, rd_kafka_err2str(rkmessage->err));
	}
}

KafkaProducer::KafkaProducer(const string& b):brokers(b)
{
	partition = RD_KAFKA_PARTITION_UA;
	//partition = 0;
};

int KafkaProducer::KafkaConnection()
{
	config = rd_kafka_conf_new();
	rd_kafka_conf_set_dr_msg_cb(config, kafka_delivery_report_cb);
	rd_kafka_conf_set(config, "queue.buffering.max.messages", "1000000", NULL, 0);
	rd_kafka_conf_set(config, "topic.metadata.refresh.interval.ms", "600000", NULL, 0);
	rd_kafka_conf_set(config, "acks", "all", NULL, 0);
	rd_kafka_conf_set(config, "message.timeout.ms", "10000", NULL, 0);

	if (!(kafka = rd_kafka_new(RD_KAFKA_PRODUCER, config, errString, sizeof(errString))))
	{
		return -1;
	}

	if (rd_kafka_brokers_add(kafka, brokers.c_str()) == 0)
	{
		return -2;
	}

	const rd_kafka_metadata_t* metadata = NULL;
	rd_kafka_resp_err_t md_err = rd_kafka_metadata(kafka, 1, NULL, &metadata, 3000);
	if (metadata) {
		rd_kafka_metadata_destroy(metadata);
	}

	if (md_err != RD_KAFKA_RESP_ERR_NO_ERROR) {
		fprintf(stderr, "[ti_features] Kafka metadata probe failed, brokers=%s, err=%s\n",
			brokers.c_str(), rd_kafka_err2str(md_err));
		rd_kafka_destroy(kafka);
		kafka = NULL;
		return -3;
	}

	return 0;
}

KafkaProducer::~KafkaProducer()
{
	if (kafka) {
		rd_kafka_flush(kafka, 5000);
	}

	for(iter = topicHandleMap.begin(); iter!=topicHandleMap.end(); ++iter)
	{
		if (iter->second) {
			rd_kafka_topic_destroy(iter->second);
		}
	}
	topicHandleMap.clear();

	if (kafka) {
		rd_kafka_destroy(kafka);
		kafka = NULL;
		rd_kafka_wait_destroyed(5000);
	}
}

rd_kafka_topic_t* KafkaProducer::CreateTopicHandle(const string& topicName)
{
	if (!kafka) {
		return NULL;
	}

	if(!topicHandleMap.count(topicName))
	{
		rd_kafka_topic_conf_t* config = rd_kafka_topic_conf_new();
		rd_kafka_topic_t* rkt = rd_kafka_topic_new(kafka, topicName.c_str(), config);
		if (!rkt) {
			fprintf(stderr, "[ti_features] rd_kafka_topic_new failed, topic=%s\n", topicName.c_str());
			return NULL;
		}

		const rd_kafka_metadata_t* metadata = NULL;
		rd_kafka_resp_err_t md_err = rd_kafka_metadata(kafka, 0, rkt, &metadata, 3000);
		if (md_err == RD_KAFKA_RESP_ERR_NO_ERROR && metadata && metadata->topic_cnt > 0) {
			if (metadata->topics[0].err != RD_KAFKA_RESP_ERR_NO_ERROR) {
				fprintf(stderr,
					"[ti_features] topic metadata warning, topic=%s, err=%s (broker may disable auto topic creation)\n",
					topicName.c_str(), rd_kafka_err2str(metadata->topics[0].err));
			}
		} else if (md_err != RD_KAFKA_RESP_ERR_NO_ERROR) {
			fprintf(stderr, "[ti_features] rd_kafka_metadata failed, topic=%s, err=%s\n",
				topicName.c_str(), rd_kafka_err2str(md_err));
		}

		if (metadata) {
			rd_kafka_metadata_destroy(metadata);
		}

		topicHandleMap[topicName] = rkt;
	}
	return topicHandleMap[topicName];
}

int KafkaProducer::SendData(string& topicName, void *payload, size_t paylen)
{
	if (!kafka || !payload || paylen == 0) {
		return -1;
	}

	rd_kafka_topic_t* currentTopicHandle = NULL;
	if (topicHandleMap.count(topicName)) {
		currentTopicHandle = topicHandleMap[topicName];
	} else {
		currentTopicHandle = CreateTopicHandle(topicName);
	}

	if (!currentTopicHandle) {
		return -1;
	}
		
	int status = rd_kafka_produce(currentTopicHandle, partition, RD_KAFKA_MSG_F_COPY, payload,
			paylen, NULL, 0, NULL);
	if (status != 0) {
		rd_kafka_resp_err_t err = rd_kafka_last_error();
		fprintf(stderr, "[ti_features] Kafka produce enqueue failed, topic=%s, err=%s\n",
			topicName.c_str(), rd_kafka_err2str(err));
	}

	rd_kafka_poll(kafka, 0);
	return status;

}

int KafkaProducer::MessageInQueue()
{
	if (!kafka) {
		return 0;
	}

	return rd_kafka_outq_len(kafka);
}

int KafkaProducer::Flush(int timeout_ms)
{
	if (!kafka) {
		return -1;
	}

	return rd_kafka_flush(kafka, timeout_ms);
}

void KafkaProducer::KafkaPoll(int interval)
{
	if (!kafka) {
		return;
	}

	rd_kafka_poll(kafka, interval);
}




