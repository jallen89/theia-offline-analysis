#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include "pin.H"
#include <string>

//mf: needed for kafka and avro
#include "TheiaStoreCdmProducer.h"
#include "tc_schema/cdm.h"
#include "avro/ValidSchema.hh"
#include "services/kafka_producer_impl.h"
#include "services/kafka_client.h"
#include "services/kafka_callbacks.h"
#include "serialization/utils.h"
#include "serialization/avro_generic_file_serializer.h"
#include "serialization/avro_generic_serializer.h"

typedef bool (*FILENAME_PREDICATE_CALLBACK)(char*);
typedef bool (*NETWORK_PREDICATE_CALLBACK)(struct sockaddr*, uint32_t);

//dtracker
void post_open_hook(syscall_ctx_t *ctx);
void post_read_hook(syscall_ctx_t *ctx);
void post_recvfrom_hook(syscall_ctx_t *ctx);
void post_write_hook(syscall_ctx_t *ctx);
void post_sendto_hook(syscall_ctx_t *ctx);
void post_close_hook(syscall_ctx_t *ctx);


//mf: generate query result
void theia_store_cdm_query_result();

extern unsigned long long tag_counter_global;
extern bool engagement_config;
extern std::string query_id_global;
extern std::string subject_uuid_global;
extern std::string local_principal_global;
extern CDM_UUID_Type *inbound_uuid_array_global;
extern CDM_UUID_Type *outbound_uuid_array_global;
extern int inbound_uuid_array_count_global;                                         
extern int outbound_uuid_array_count_global;
extern std::string default_key_global;
extern bool publish_to_kafka_global;
extern bool create_avro_file_global;
extern tc_serialization::AvroGenericFileSerializer<tc_schema::TCCDMDatum> *file_serializer_global;
extern TheiaStoreCdmProducer *producer_global;

#define CURR_CDM_VERSION "19"
#define THEIA_DEFAULT_SCHEMA_FILE "/usr/local/taint/include/tc_schema/TCCDMDatum.avsc"
