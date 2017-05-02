#include "TheiaCdmConsumer.h"
#include "TheiaDB.h"

#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>

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

string kafka_ipport = "10.0.50.19:9092"; 
string kafka_topic = "ta1-theia-qr";
string kafka_binfile = "/home/theia/theia-qr.bin";

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

            cout << "Bang! We do not support backward yet.\n";
//            execl("utils/proc_index.py", "utils/proc_index.py");
//            int pid = 0;
//            string cmdline;
//            get_pid_cmdline(sink_id, &pid, &cmdline);
//            string replay_path = get_replay_path(pid, cmdline);
//            if(replay_path == "ERROR") {
//              cout << "Cannot find pid " << pid << "," << "cmdline " << cmdline << "\n";
//            }
//            else {
//              execl("./start_taint.py", "./start_taint.py", query_type, 
//                  replay_path.c_str(), kafka_ipport, kafka_topic, 
//                  kafka_binfile, "-1", sink_id);
//            }
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

            int pid = 0;
            string cmdline;
            get_pid_cmdline(source_id, &pid, &cmdline);
            string replay_path = get_replay_path(pid, cmdline);
            if(replay_path == "ERROR") {
              cout << "Cannot find pid, load proc" << pid << "," << "cmdline " << cmdline << "\n";
              execl("utils/proc_index.py", "utils/proc_index.py");
              replay_path = get_replay_path(pid, cmdline);
            }
            if(replay_path == "ERROR") {
              cout << "Still cannot find pid, terminate " << pid << "," << "cmdline " << cmdline << "\n";
            }
            else {
              execl("utils/start_taint.py", "utils/start_taint.py", query_type.c_str(), query_id.c_str(), 
                  replay_path.c_str(), kafka_ipport.c_str(), kafka_topic.c_str(), 
                  kafka_binfile.c_str(), source_id.c_str(), "-1");

            }
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

            execl("utils/proc_index.py", "utils/proc_index.py");
            int pid = 0;
            string cmdline;
            get_pid_cmdline(source_id, &pid, &cmdline);
            string replay_path = get_replay_path(pid, cmdline);
            if(replay_path == "ERROR") {
              cout << "Cannot find pid, load proc" << pid << "," << "cmdline " << cmdline << "\n";
              execl("utils/proc_index.py", "utils/proc_index.py");
              replay_path = get_replay_path(pid, cmdline);
            }
            if(replay_path == "ERROR") {
              cout << "Still cannot find pid, terminate " << pid << "," << "cmdline " << cmdline << "\n";
            }
            else {
              execl("utils/start_taint.py", "utils/start_taint.py", query_type.c_str(), query_id.c_str(), 
                  replay_path.c_str(), kafka_ipport.c_str(), kafka_topic.c_str(), 
                  kafka_binfile.c_str(), source_id.c_str(), sink_id.c_str());
            }
        	}
          printf("processing %s query with uuid:%s and source_id:%s and end_timestamp:%s and sink_id:%s and start_timestamp:%s\n",
              query_type.c_str(), query_id.c_str(), source_id.c_str(), end_timestamp.c_str(), sink_id.c_str(), start_timestamp.c_str());

        	//TODO start reachability analysis here

        	//TODO start taint here
		
		//examples
		/*
#forward query within the same process with source uuid specified and no timestamp specified
#-no_neo4j true => no neo4j logging
#-tag_count_file /home/theia/tags.txt => loads current tag count and saves final tag count (does not work for parallel queries)
#-publish_to_kafka true => publish data to kafka flag
#-kafka_server 127.0.0.1:9092 => kafka server location
#-kafka_topic ta1-theia-qr => kafka topic
#-create_avro_file true => create avro file
#-avro_file /home/theia/1_tags.bin => location of avro file
#-query_id 1 => id of the query specified by ta2
#-use_source_id true => using source id flag
#-source_id 700000269 => source id value

#/home/theia/theia-es/pin_tools/dtracker/pin/pin -pid 7226 -t /home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so -no_neo4j true -tag_count_file /home/theia/tags.txt -publish_to_kafka true -kafka_server 127.0.0.1:9092 -kafka_topic ta1-theia-qr -create_avro_file true -avro_file /home/theia/1_tags.bin -query_id 1 -use_source_id true -source_id 700000269


#forward query within the same process with source uuid specified and timestamp specified
#-no_neo4j true => no neo4j logging
#-tag_count_file /home/theia/tags.txt => loads current tag count and saves final tag count (does not work for parallel queries)
#-publish_to_kafka true => publish data to kafka flag
#-kafka_server 127.0.0.1:9092 => kafka server location
#-kafka_topic ta1-theia-qr => kafka topic
#-create_avro_file true => create avro file
#-avro_file /home/theia/1_tags.bin => location of avro file
#-query_id 2 => id of the query specified by ta2
#-use_source_id true => using source id flag
#-source_id 700000269 => source id value
#-use_end_timestamp true => using timestamp flag
#-end_timestamp 1493070403022796 => timestamp value

/home/theia/theia-es/pin_tools/dtracker/pin/pin -pid 9409 -t /home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so -no_neo4j true -tag_count_file /home/theia/tags.txt -publish_to_kafka true -kafka_server 127.0.0.1:9092 -kafka_topic ta1-theia-qr -create_avro_file true -avro_file /home/theia/2_tags.bin -query_id 2 -use_source_id true -source_id 700000269 -use_end_timestamp true -end_timestamp 1493070403022796

#point to point query within the same process with source uuid and sink id specified 
#-no_neo4j true => no neo4j logging
#-tag_count_file /home/theia/tags.txt => loads current tag count and saves final tag count (does not work for parallel queries)
#-publish_to_kafka true => publish data to kafka flag
#-kafka_server 127.0.0.1:9092 => kafka server location
#-kafka_topic ta1-theia-qr => kafka topic
#-create_avro_file true => create avro file
#-avro_file /home/theia/1_tags.bin => location of avro file
#-query_id 2 => id of the query specified by ta2
#-use_source_id true => using source id flag
#-source_id 700000269 => source id value
#-use_sink_id true => using sink id
#-sink_id 700000275 sink id value


/home/theia/theia-es/pin_tools/dtracker/pin/pin -pid 9818 -t /home/theia/theia-es/pin_tools/dtracker/obj-ia32/dtracker.so -no_neo4j true -tag_count_file /home/theia/tags.txt -publish_to_kafka true -kafka_server 127.0.0.1:9092 -kafka_topic ta1-theia-qr -create_avro_file true -avro_file /home/theia/3_tags.bin -query_id 3 -use_source_id true -source_id 700000269 -use_sink_id true -sink_id 700000275

*/
    	}
    }
  }
