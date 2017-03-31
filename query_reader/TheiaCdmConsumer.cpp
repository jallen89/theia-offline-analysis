#include "TheiaCdmConsumer.h"

#include <string>

TheiaCdmConsumer::TheiaCdmConsumer(std::string topic_name, std::string connection_string) : tc_kafka::KafkaConsumer<tc_schema::TCCDMDatum>(topic_name, connection_string) {

}

TheiaCdmConsumer::TheiaCdmConsumer(std::string topic_name, std::string connection_string, avro::ValidSchema writer_schema, avro::ValidSchema reader_schema, std::string consumer_group_id) : tc_kafka::KafkaConsumer<tc_schema::TCCDMDatum>(topic_name, connection_string, writer_schema, reader_schema, consumer_group_id) {

}

TheiaCdmConsumer::~TheiaCdmConsumer(){

}

void TheiaCdmConsumer::nextMessage(std::string key, std::unique_ptr<tc_schema::TCCDMDatum> record) {
	printf("Doing something\n");
    tc_schema::TCCDMDatum cdmRecord = *record;
    //if record is of type TheiaQuery has idx equal to 11 according to cdm.h
    if (cdmRecord.datum.idx() == 11) {
    	printf("processing TheiaQuery");
    	tc_schema::TheiaQuery theia_query =  cdmRecord.datum.get_TheiaQuery();
    	std::stringstream s;
    	for (int i=0; i<16; ++i) {
    		s << std::to_string(theia_query.queryId[i]);
    	}
    	printf("uuid:%s",s.str().c_str());
    }
  }
