#include <stdio.h>

#include "tc_schema/cdm.h"
#include "TheiaStoreCdmProducer.h"

#include <unistd.h>

#define THEIA_DEFAULT_SCHEMA_FILE "/usr/local/include/tc_schema/TCCDMDatum.avsc"
std::string DEFAULT_KEY = "ta1-theia-q";

void generateBackwardQuery(TheiaStoreCdmProducer *producer){
	  tc_schema::TheiaQuery new_query_object;
	  uint64_t query_id = 591;
	  boost::array<uint8_t, 16> query_id_array = {};
	  size_t i,j;
	  for(i=0,j=15; i<sizeof(query_id); ++i,--j) {
		  query_id_array[j] = ((query_id >> (i*8)) & 0xff);
	  }
	  new_query_object.queryId = query_id_array;
	  new_query_object.type = tc_schema::TheiaQueryType::BACKWARD;
	  uint64_t sink_id = 2;
	  boost::array<uint8_t, 16> sink_id_array = {};
	  for(i=0,j=15; i<sizeof(sink_id); ++i,--j) {
		  sink_id_array[j] = ((sink_id >> (i*8)) & 0xff);
	  }
	  new_query_object.sinkId.set_UUID(sink_id_array);
	  new_query_object.startTimestamp.set_long(0);
	  tc_schema::TCCDMDatum new_query_object_record;
	  new_query_object_record.datum.set_TheiaQuery(new_query_object);
	  new_query_object_record.CDMVersion = "17";
	  producer->publish_record(DEFAULT_KEY, new_query_object_record);
	  printf("Published backward query.\n");
}

void generateForwardQuery(TheiaStoreCdmProducer *producer){
	  tc_schema::TheiaQuery new_query_object;
	  uint64_t query_id = 593;
	  boost::array<uint8_t, 16> query_id_array = {};
	  size_t i,j;
	  for(i=0,j=15; i<sizeof(query_id); ++i,--j) {
		  query_id_array[j] = ((query_id >> (i*8)) & 0xff);
	  }
	  new_query_object.queryId = query_id_array;
	  new_query_object.type = tc_schema::TheiaQueryType::FORWARD;
	  uint64_t source_id = 4;
	  boost::array<uint8_t, 16> source_id_array = {};
	  for(i=0,j=15; i<sizeof(source_id); ++i,--j) {
		  source_id_array[j] = ((source_id >> (i*8)) & 0xff);
	  }
	  new_query_object.sourceId.set_UUID(source_id_array);
	  new_query_object.endTimestamp.set_long(99999);
	  tc_schema::TCCDMDatum new_query_object_record;
	  new_query_object_record.datum.set_TheiaQuery(new_query_object);
	  new_query_object_record.CDMVersion = "17";
	  producer->publish_record(DEFAULT_KEY, new_query_object_record);
	  printf("Published forward query.\n");
}

void generatePointToPointQuery(TheiaStoreCdmProducer *producer){
	  tc_schema::TheiaQuery new_query_object;
	  uint64_t query_id = 595;
	  boost::array<uint8_t, 16> query_id_array = {};
	  size_t i,j;
	  for(i=0,j=15; i<sizeof(query_id); ++i,--j) {
		  query_id_array[j] = ((query_id >> (i*8)) & 0xff);
	  }
	  new_query_object.queryId = query_id_array;
	  new_query_object.type = tc_schema::TheiaQueryType::POINT_TO_POINT;
	  uint64_t source_id = 6;
	  boost::array<uint8_t, 16> source_id_array = {};
	  for(i=0,j=15; i<sizeof(source_id); ++i,--j) {
		  source_id_array[j] = ((source_id >> (i*8)) & 0xff);
	  }
	  new_query_object.sourceId.set_UUID(source_id_array);
	  new_query_object.endTimestamp.set_long(99999);
	  uint64_t sink_id = 7;
	  boost::array<uint8_t, 16> sink_id_array = {};
	  for(i=0,j=15; i<sizeof(sink_id); ++i,--j) {
		  sink_id_array[j] = ((sink_id >> (i*8)) & 0xff);
	  }
	  new_query_object.sinkId.set_UUID(sink_id_array);
	  new_query_object.startTimestamp.set_long(0);
	  tc_schema::TCCDMDatum new_query_object_record;
	  new_query_object_record.datum.set_TheiaQuery(new_query_object);
	  new_query_object_record.CDMVersion = "17";
	  producer->publish_record(DEFAULT_KEY, new_query_object_record);
	  printf("Published point-to-point query.\n");
}


int main(int argc, char *argv[]){
  avro::ValidSchema writer_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  TheiaStoreCdmProducer *producer = new TheiaStoreCdmProducer("ta1-theia-q", "localhost:9092", writer_schema, "ta1-theia-q");
  producer->connect();
  generateBackwardQuery(producer);
  sleep(3);
  generateForwardQuery(producer);
  sleep(3);
  generatePointToPointQuery(producer);

  producer->shutdown();

}

