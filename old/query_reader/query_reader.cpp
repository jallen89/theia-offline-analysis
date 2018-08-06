#include <stdio.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <string>
#include <cstring>

#include "tc_schema/cdm.h"
#include "TheiaCdmConsumer.h"

#define THEIA_DEFAULT_SCHEMA_FILE "/usr/local/include/tc_schema/TCCDMDatum.avsc"

FILE *out_fd;
std::string kafka_ipport;
std::string kafka_r_topic;
std::string psql_cred;

int main(int argc, char *argv[]){
  if(argc!=3){
	  printf("usage: query_reader kafka_server kafka_topic\n");
	  exit(1);
  }

  if(const char* env_p = std::getenv("TA_TWO")) {
	  std::stringstream buff;
	  buff << "dbname=theia1 user=theia password=darpatheia1 host=ta1-theia-database-" << env_p << " port=5432";
	  psql_cred = buff.str();
  }
  else {
	  perror("failed to get env of TA_TWO. exit\n");
	  exit(1);
  }

  out_fd = fopen("query.log", "w");

  printf("Starting query reader!\n");
  std::string kafka_server(argv[1]);
  std::string kafka_topic(argv[2]);
  kafka_ipport = kafka_server;
  kafka_r_topic = kafka_topic;
  std::string consumer_group_id = kafka_topic;
  avro::ValidSchema writer_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  avro::ValidSchema reader_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  TheiaCdmConsumer *consumer = new TheiaCdmConsumer(kafka_topic, kafka_server, writer_schema, reader_schema, consumer_group_id);
  consumer->connect();
  consumer->run();
}
