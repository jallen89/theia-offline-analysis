#include <stdio.h>

#include "tc_schema/cdm.h"
#include "TheiaCdmConsumer.h"

#define THEIA_DEFAULT_SCHEMA_FILE "/usr/local/include/tc_schema/TCCDMDatum.avsc"

int main(int argc, char *argv[]){
  printf("Hello World!\n");
  avro::ValidSchema writer_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  avro::ValidSchema reader_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);
  std::string DEFAULT_KEY = "ta1-theia-q";
  TheiaCdmConsumer *consumer = new TheiaCdmConsumer("ta1-theia-q", "localhost:9092", writer_schema, reader_schema, "ta1-theia-q");
  //consumer->setAutoOffsetReset("earliest");
  consumer->connect();
  consumer->run();
  printf("Done!\n");
}

