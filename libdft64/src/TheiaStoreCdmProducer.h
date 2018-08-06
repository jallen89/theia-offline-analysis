#include "tc_schema/cdm.h"
#include "avro/ValidSchema.hh"
#include "services/kafka_producer_impl.h"
#include "services/kafka_client.h"
#include "services/kafka_callbacks.h"
#include "serialization/utils.h"
#include "serialization/avro_generic_file_serializer.h"
#include "serialization/avro_generic_serializer.h"

#ifndef THEIA_PRODUCER_H
#define THEIA_PRODUCER_H

class TheiaStoreCdmProducer : public tc_kafka::KafkaProducer<tc_schema::TCCDMDatum> {
  unsigned long count;

public:
  TheiaStoreCdmProducer(std::string topic_name, std::string connection_string);
  TheiaStoreCdmProducer(std::string topic_name, std::string connection_string, avro::ValidSchema writer_schema, std::string producer_id);
  ~TheiaStoreCdmProducer();

  unsigned long get_count();

  void publish_record(std::string key, tc_schema::TCCDMDatum record);

};

#endif
