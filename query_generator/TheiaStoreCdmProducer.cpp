#include "TheiaStoreCdmProducer.h"

#include <string>


TheiaStoreCdmProducer::TheiaStoreCdmProducer(std::string topic_name, std::string connection_string) : tc_kafka::KafkaProducer<tc_schema::TCCDMDatum>(topic_name, connection_string) {
    std::cout << "Creating TheiaProducer for topic " << topic_name << std::endl;
    count = 0;
}


TheiaStoreCdmProducer::TheiaStoreCdmProducer(std::string topic_name, std::string connection_string, avro::ValidSchema writer_schema, std::string producer_id) : tc_kafka::KafkaProducer<tc_schema::TCCDMDatum>(topic_name, connection_string, writer_schema, producer_id) {
  std::cout << "Creating TheiaProducer for topic " << topic_name << " and producer id " << producer_id << std::endl;
  count = 0;
}

TheiaStoreCdmProducer::~TheiaStoreCdmProducer(){

}

void TheiaStoreCdmProducer::publish_record(std::string key, tc_schema::TCCDMDatum record) {
  std::cout << "Publishing record." << std::endl;
  publishRecord(key, record);
  this->count++;
}

unsigned long TheiaStoreCdmProducer::get_count() {
	return count;
}
