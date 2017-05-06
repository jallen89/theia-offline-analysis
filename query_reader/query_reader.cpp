#include <stdio.h>

#include "tc_schema/cdm.h"
#include "TheiaCdmConsumer.h"

#define THEIA_DEFAULT_SCHEMA_FILE "/usr/local/include/tc_schema/TCCDMDatum.avsc"

FILE *out_fd;

int main(int argc, char *argv[]){
  if(argc!=3){
	  printf("usage: query_reader kafka_server kafka_topic");
	  exit(1);
  }

  out_fd = fopen("query.log", "w");

  printf("Starting query reader!\n");
  std::string kafka_server(argv[1]);
  std::string kafka_topic(argv[2]);
  std::string consumer_group_id = kafka_topic;
  avro::ValidSchema writer_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  avro::ValidSchema reader_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  TheiaCdmConsumer *consumer = new TheiaCdmConsumer(kafka_topic, kafka_server, writer_schema, reader_schema, consumer_group_id);
  consumer->connect();
  consumer->run();
}
