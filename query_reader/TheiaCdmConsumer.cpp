#include "TheiaCdmConsumer.h"

#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

TheiaCdmConsumer::TheiaCdmConsumer(std::string topic_name, std::string connection_string) : tc_kafka::KafkaConsumer<tc_schema::TCCDMDatum>(topic_name, connection_string) {

}

TheiaCdmConsumer::TheiaCdmConsumer(std::string topic_name, std::string connection_string, avro::ValidSchema writer_schema, avro::ValidSchema reader_schema, std::string consumer_group_id) : tc_kafka::KafkaConsumer<tc_schema::TCCDMDatum>(topic_name, connection_string, writer_schema, reader_schema, consumer_group_id) {

}

TheiaCdmConsumer::~TheiaCdmConsumer(){

}

uint64_t convert_uuid(boost::array<uint8_t, 16> uuid){
	uint64_t result=0;
	uint8_t mask = 0b00000001;
	for (int i=0; i<16; ++i) {
		for(int j=0; j<8; j++){
			uint8_t shifted_masked = (uuid[15-i] >> j) & mask;
			int power = (8*i)+j;
			result = result + (((uint64_t) shifted_masked) * ((uint64_t) pow(2, power)));
	    }
	}
	return result;
}

void TheiaCdmConsumer::nextMessage(std::string key, std::unique_ptr<tc_schema::TCCDMDatum> record) {
    tc_schema::TCCDMDatum cdmRecord = *record;
    //if record is of type TheiaQuery has idx equal to 11 according to cdm.h
    if (cdmRecord.datum.idx() == 11) {
    	tc_schema::TheiaQuery theia_query =  cdmRecord.datum.get_TheiaQuery();

    	//creating new process to handle the query
    	pid_t pid = fork();
    	if(pid<0){
    		std::stringstream ss_query_id;
    		ss_query_id << convert_uuid(theia_query.queryId);
        	printf("could not handle query with uuid:%s\n", ss_query_id.str().c_str());
    	}
    	else if(pid==0){
    		sleep(15);
    		std::stringstream ss_query_id;
    		ss_query_id << convert_uuid(theia_query.queryId);
        	std::string query_id = ss_query_id.str();
        	std::string query_type = "";
        	std::string source_id = "-1";
        	std::string end_timestamp = "-1";
        	std::string sink_id = "-1";
        	std::string start_timestamp = "-1";
        	if(theia_query.type==tc_schema::TheiaQueryType::BACKWARD){
        		std::stringstream ss_sink_id;
        		ss_sink_id << convert_uuid(theia_query.sinkId.get_UUID());
        		sink_id = ss_sink_id.str();
        		if(!theia_query.startTimestamp.is_null()){
        			std::stringstream ss_start_timestamp;
        			ss_start_timestamp << theia_query.startTimestamp.get_long();
        			start_timestamp = ss_start_timestamp.str();
        		}
        		query_type = "backward";
        	}
        	else if(theia_query.type==tc_schema::TheiaQueryType::FORWARD){
        		std::stringstream ss_source_id;
        		ss_source_id << convert_uuid(theia_query.sourceId.get_UUID());
        		source_id = ss_source_id.str();
        		if(!theia_query.endTimestamp.is_null()){
        			std::stringstream ss_end_timestamp;
        			ss_end_timestamp << theia_query.endTimestamp.get_long();
        			end_timestamp = ss_end_timestamp.str();
        		}
        		query_type = "forward";
        	}
        	else if(theia_query.type==tc_schema::TheiaQueryType::POINT_TO_POINT){
        		std::stringstream ss_sink_id;
        		ss_sink_id << convert_uuid(theia_query.sinkId.get_UUID());
        		sink_id = ss_sink_id.str();
        		if(!theia_query.startTimestamp.is_null()){
        			std::stringstream ss_start_timestamp;
        			ss_start_timestamp << theia_query.startTimestamp.get_long();
        			start_timestamp = ss_start_timestamp.str();
        		}
        		std::stringstream ss_source_id;
        		ss_source_id << convert_uuid(theia_query.sourceId.get_UUID());
        		source_id = ss_source_id.str();
        		if(!theia_query.endTimestamp.is_null()){
        			std::stringstream ss_end_timestamp;
        			ss_end_timestamp << theia_query.endTimestamp.get_long();
        			end_timestamp = ss_end_timestamp.str();
        		}
        		query_type = "point-to-point";
        	}
            printf("processing %s query with uuid:%s and source_id:%s and end_timestamp:%s and sink_id:%s and start_timestamp:%s\n",
            		query_type.c_str(), query_id.c_str(), source_id.c_str(), end_timestamp.c_str(), sink_id.c_str(), start_timestamp.c_str());

        	//TODO start reachability analysis here

        	//TODO start taint here
    	}
    }
  }
