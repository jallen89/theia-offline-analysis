#include "tc_schema/cdm.h"
#include "avro/ValidSchema.hh"
#include "services/kafka_consumer_impl.h"
#include "services/kafka_client.h"
#include "services/kafka_callbacks.h"
#include "serialization/utils.h"
#include "records/cdm_record_parser.h"

#ifndef THEIA_CONSUMER_H
#define THEIA_CONSUMER_H

class TheiaCdmConsumer : public tc_kafka::KafkaConsumer<tc_schema::TCCDMDatum> {

	public:
		TheiaCdmConsumer(std::string topic_name, std::string connection_string);
		TheiaCdmConsumer(std::string topic_name, std::string connection_string, avro::ValidSchema writer_schema, avro::ValidSchema reader_schema, std::string consumer_group_id);
		~TheiaCdmConsumer();
		void nextMessage(std::string key, std::unique_ptr<tc_schema::TCCDMDatum> record);
};

#endif
