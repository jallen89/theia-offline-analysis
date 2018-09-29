/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in October 2010.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Columbia University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 * 	- add support for file descriptor duplication via fcntl(2)
 * 	- add support for non PF_INET* sockets
 * 	- add support for recvmmsg(2)
 */

#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <set>
#include <algorithm>

#include "TheiaDB.h"
#include "branch_pred.h"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "syscall_desc64.h"
#include "tagmap64.h"
#include "dta_api64.h"

//Yang
#include <glib-2.0/glib.h>
#include "libdft_api.h"
#include <sys/ioctl.h> // ioctl

//mf: added
//#include "syscall_desc.h"

#ifdef THEIA_REPLAY_COMPENSATION
#define SPECI_CHECK_BEFORE _IOR('u',3,int)
#define SPECI_CHECK_AFTER _IOR('u',4,int)
#define SPEC_DEV "/dev/spec0"

int check_clock_before_syscall (int fd_spec, int syscall)
{
    return ioctl (fd_spec, SPECI_CHECK_BEFORE, &syscall);
}

int check_clock_after_syscall (int fd_spec)
{
    return ioctl (fd_spec, SPECI_CHECK_AFTER);
}

int devspec_init (int* fd_spec) 
{
  // yysu: prepare for speculation
  *fd_spec = open (SPEC_DEV, O_RDWR);
  if (*fd_spec < 0) {
    printf ("cannot open spec device");
    return errno;
  }

    return 0;
}
#endif

/* Syscall descriptors, defined in libdft. */
extern syscall_desc_t syscall_desc[SYSCALL_MAX];

/* default path for the log file (audit) */
//#define LOGFILE_DFL	"/tmp/libdft-dta.log"

/* log file path (auditing) */
//static KNOB<string> logpath(KNOB_MODE_WRITEONCE, "pintool", "l",
//		LOGFILE_DFL, "");

/*
 * flag variables
 *
 * 0	: feature disabled
 * >= 1	: feature enabled
 */

/* track stdin (enabled by default) */
//static KNOB<size_t> stin(KNOB_MODE_WRITEONCE, "pintool", "s", "1", "");

/* track fs (enabled by default) */
//static KNOB<size_t> fs(KNOB_MODE_WRITEONCE, "pintool", "f", "1", "");

/* track net (enabled by default) */
//static KNOB<size_t> net(KNOB_MODE_WRITEONCE, "pintool", "n", "1", "");

static KNOB<string> EngagementConfig(KNOB_MODE_WRITEONCE, "pintool", "engagement_config", "false", "Specify whether to use engagement config");

//mf: publish to kafka
static KNOB<string> PublishToKafka(KNOB_MODE_WRITEONCE, "pintool", "publish_to_kafka", "false", "Specify if you want to publish results to kafka");

//mf: kafka server
static KNOB<string> KafkaServer(KNOB_MODE_WRITEONCE, "pintool", "kafka_server", "127.0.0.1:9092", "Specify address:port of kafka server");

//mf: kafka topic
static KNOB<string> KafkaTopic(KNOB_MODE_WRITEONCE, "pintool", "kafka_topic", "ta1-theia-q", "Specify kafka topic");

//mf: create avro binary file
static KNOB<string> CreateAvroFile(KNOB_MODE_WRITEONCE, "pintool", "create_avro_file", "false", "Specify if you want to create binary file for query results");

//mf: query id
static KNOB<string> QueryId(KNOB_MODE_WRITEONCE, "pintool", "query_id", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "Specify query id (UUID) for the query we are serving");

//mf: subject uuid
static KNOB<string> SubjectUUID(KNOB_MODE_WRITEONCE, "pintool", "subject_uuid", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "Specify subject uuid (UUID) of the process replayed");

//mf: local princial
static KNOB<string> LocalPrincipal(KNOB_MODE_WRITEONCE, "pintool", "local_principal", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "Specify local principal uuid (UUID) of the process replayed");

//mf: tag counter
static KNOB<string> TagCounter(KNOB_MODE_WRITEONCE, "pintool", "tag_counter", "0", "Specify tag counter");

//mf: global
avro::ValidSchema writer_schema = tc_serialization::utils::loadSchema(THEIA_DEFAULT_SCHEMA_FILE);

//mf: defining extern global
unsigned long long tag_counter_global = 0;
bool engagement_config = false;
std::string query_id_global = "";
std::string subject_uuid_global = "";
std::string local_principal_global = "";
CDM_UUID_Type *inbound_uuid_array_global = NULL;
CDM_UUID_Type *outbound_uuid_array_global = NULL;
int inbound_uuid_array_count_global = 0;
int outbound_uuid_array_count_global = 0;
std::string default_key_global = "";
bool publish_to_kafka_global = false;
bool create_avro_file_global = false;
tc_serialization::AvroGenericFileSerializer<tc_schema::TCCDMDatum> *file_serializer_global;
TheiaStoreCdmProducer *producer_global;

//mf: removed because not needed
//static bool detected = false;

//mf: removed because not needed
///*
// * DTA/DFT alert
// *
// * @ins:	address of the offending instruction
// */
//static void
//alert(ADDRINT ins, ADDRINT v)
//{
//	/* log file */
//	FILE *logfile;
//
//	/* auditing */
//	if (likely((logfile = fopen(logpath.Value().c_str(), "w")) != NULL)) {
//		/* hilarious :) */
//		(void)fprintf(logfile, " ____ ____ ____ ____\n");
//		(void)fprintf(logfile, "||w |||o |||o |||t ||\n");
//		(void)fprintf(logfile, "||__|||__|||__|||__||\t");
//		(void)fprintf(logfile, "[%d]: 0x%08lx\n",
//							getpid(), ins);
//
//		(void)fprintf(logfile, "|/__\\|/__\\|/__\\|/__\\|\n");
//
//		/* cleanup */
//		(void)fclose(logfile);
//	}
//	else
//		/* failed */
//		LOG(string(__func__) +
//			": failed while trying to open " +
//			logpath.Value().c_str() + " (" +
//			string(strerror(errno)) + ")\n");
//
//    detected = true;
//
//	/* terminate */
//	PIN_ExitApplication(EXIT_FAILURE);
//}

//mf: removed because not needed
//static void Fini(int32_t code, VOID *v) {
//    if (!detected) {
//        FILE *logfile = fopen(logpath.Value().c_str(), "w");
//        fprintf(logfile, "Normal exit with return code %d.\n", code);
//        fclose(logfile);
//    }
//}

void Usage() {
    cerr << "This tool performs dynamic information flow tracking\n";
    cerr << "\n" << KNOB_BASE::StringKnobSummary() << "\n";
}

#ifdef THEIA_REPLAY_COMPENSATION
//Yang
/******************replay compensation starts here*****************/

#define SYSCALL_CNT tdata->syscall_cnt

long global_syscall_cnt = 0;
int child = 0;

// Use a Pin virtual register to store the TLS pointer
REG tls_reg;

//struct thread_data {
//	u_long app_syscall; // Per thread address for specifying pin vs. non-pin system calls
//	int record_pid; 	// per thread record pid
//	int syscall_cnt;	// per thread count of syscalls
//	int sysnum;		// current syscall number
//	u_long ignore_flag;
//};

int fd_dev; // File descriptor for the replay device
TLS_KEY tls_key; // Key for accessing TLS. This is alternative to tls_reg. 

FILE* out_fd;

GHashTable* sysexit_addr_table; 

//int get_record_pid(void);

//inline void increment_syscall_cnt (struct thread_data* ptdata, int syscall_num)
//{
//	// ignore pthread syscalls, or deterministic system calls that we don't log (e.g. 123, 186, 243, 244)
//	if (!(syscall_num == 17 || syscall_num == 31 || syscall_num == 32 || syscall_num == 35 || 
//				syscall_num == 44 || syscall_num == 53 || syscall_num == 56 || syscall_num == 98 ||
//				syscall_num == 119 || syscall_num == 123 || syscall_num == 186 ||
//				syscall_num == 243 || syscall_num == 244)) {
//		if (ptdata->ignore_flag) {
//			if (!(*(int *)(ptdata->ignore_flag))) {
//				global_syscall_cnt++;
//				ptdata->syscall_cnt++;
//			}
//		} else {
//			global_syscall_cnt++;
//			ptdata->syscall_cnt++;
//		}
//	}
//}
//
//void inst_syscall_end(THREADID thread_id, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v)
//{
//#ifdef USE_TLS_SCRATCH
//	struct thread_data* tdata = (struct thread_data *) PIN_GetContextReg(ctxt, tls_reg);
//#else
//	struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
//#endif
//	if (tdata) {
//		if (tdata->app_syscall != 999) tdata->app_syscall = 0;
//	} else {
//		fprintf (out_fd, "inst_syscall_end: NULL tdata\n");
//	}	
//
//	increment_syscall_cnt(tdata, tdata->sysnum);
//	// reset the syscall number after returning from system call
//	tdata->sysnum = 0;
//	increment_syscall_cnt(tdata, tdata->sysnum);
//}

// called before every application system call
#ifdef USE_TLS_SCRATCH
#ifdef PLUS_TWO
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT tls_ptr, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2, ADDRINT ip)
#else
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT tls_ptr, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2)
#endif
#else
void set_address_one(ADDRINT syscall_num, ADDRINT ebx_value, ADDRINT syscallarg0, ADDRINT syscallarg1, ADDRINT syscallarg2)
#endif
{
#ifdef USE_TLS_SCRATCH
	struct thread_data* tdata = (struct thread_data *) tls_ptr;
#else
	struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
	if (tdata) {
		int sysnum = (int) syscall_num;

		fprintf (out_fd, "%ld Pid %d, tid %d, (record pid %d), %d: syscall num is %d\n", global_syscall_cnt, PIN_GetPid(), PIN_GetTid(), tdata->record_pid, tdata->syscall_cnt, (int) syscall_num);
    fflush(out_fd);

		if (sysnum == SYS_open) {
			fprintf(out_fd, "try to open %s\n", (char *) syscallarg0);
		}
		if (sysnum == 401) { //sys_pthread_log
			tdata->ignore_flag = (u_long) syscallarg1;
		}

#ifdef PLUS_TWO
		g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+2));
		fprintf (out_fd, "Add address %x\n", ip+2);
		g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+11));
		fprintf (out_fd, "Add address %x\n", ip+11);
#endif	    
		if (sysnum == 12 || sysnum == 11 || sysnum == 56 || sysnum == 10 || 
        sysnum == 13 || sysnum == 14 || sysnum == 58 || sysnum == 9) {
		fprintf (out_fd, "[%s|%d]%ld Pid %d, tid %d, (record pid %d), %d: syscall num is %d\n", __func__,__LINE__,global_syscall_cnt, PIN_GetPid(), PIN_GetTid(), tdata->record_pid, tdata->syscall_cnt, (int) syscall_num);
    fflush(out_fd);
			check_clock_before_syscall (fd_dev, (int) syscall_num);
		}
    //Yang: the following assignment makes the trick that test_app_syscall can distinguish syscall from pin and from execution
		tdata->app_syscall = syscall_num;
		tdata->sysnum = syscall_num;
    } else {
      fprintf (out_fd, "set_address_one: NULL tdata\n");
  }
}

#ifdef USE_TLS_SCRATCH
void syscall_after (ADDRINT ip, ADDRINT tls_ptr)
#else
void syscall_after (ADDRINT ip)
#endif
{
#ifdef USE_TLS_SCRATCH
  struct thread_data* tdata = (struct thread_data *) tls_ptr;
#else
  struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
  if (tdata) {
    if (tdata->app_syscall == 999) {
      //fprintf (out_fd, "Pid %d Waiting for clock after syscall,ip=%lx\n", PIN_GetPid(), (u_long) ip);
      //if (addr_save) g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip));
      if (check_clock_after_syscall (fd_dev) == 0) {
      } else {
        fprintf (out_fd, "Check clock failed\n");
      }
      //tdata->app_syscall = 0;  
      tdata->app_syscall = 997;  
    }
  } else {
    fprintf (out_fd, "syscall_after: NULL tdata\n");
  }
}



//Yang test shared memory read contents feedback
VOID shm_read_feeback(VOID *ip, BOOL if_mem_read, BOOL if_mem_write, 
	/*UINT32 mem_read_size, UINT32 mem_write_size,*/ VOID *address, CONTEXT *ctxt) {
	
//	fprintf(out_fd, "for shm_read, ip: %p, address: %p, mem_read: %d,
//		mem_write: %d\n", ip, address, if_mem_read, if_mem_write);
	
	if((UINT64)address == 0xb6b11004) {
		char new_content = 'a';
		fprintf(out_fd, "0xb6b11004 found: %d", *(char*)address);
		memcpy(address, &new_content, 1);
		fprintf(out_fd, "0xb6b11004 changed: %c", *(char*)address);
	}
	return;

}

void AfterForkInChild(THREADID threadid, const CONTEXT* ctxt, VOID* arg)
{
#ifdef USE_TLS_SCRATCH
  struct thread_data* tdata = (struct thread_data *) PIN_GetContextReg(ctxt, tls_reg);
#else
  struct thread_data* tdata = (struct thread_data *) PIN_GetThreadData(tls_key, PIN_ThreadId());
#endif
  int record_pid;
  fprintf(out_fd, "AfterForkInChild\n");
  record_pid = get_record_pid();
  fprintf(out_fd, "get record id %d\n", record_pid);
  tdata->record_pid = record_pid;

  // reset syscall index for thread
  tdata->syscall_cnt = 0;
}

void track_inst(INS ins, void* data) 
{
#ifdef USE_TLS_SCRATCH
  if(INS_IsSyscall(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(set_address_one), IARG_SYSCALL_NUMBER, 
        IARG_REG_VALUE, LEVEL_BASE::REG_EBX, 
        IARG_REG_VALUE, tls_reg,
        IARG_SYSARG_VALUE, 0, 
        IARG_SYSARG_VALUE, 1,
        IARG_SYSARG_VALUE, 2,
#ifdef PLUS_TWO
        IARG_INST_PTR,
#endif
        IARG_END);
  }
#else
  if(INS_IsSyscall(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(set_address_one), IARG_SYSCALL_NUMBER, 
        IARG_REG_VALUE, LEVEL_BASE::REG_EBX, 
        IARG_SYSARG_VALUE, 0, 
        IARG_SYSARG_VALUE, 1,
        IARG_SYSARG_VALUE, 2,
        IARG_END);
  }
#endif

	//Yang test shared memory read contents feedback
	UINT32 memOperands = INS_MemoryOperandCount(ins);
	if(memOperands > 0) {
		INS_InsertCall(
				ins, IPOINT_BEFORE, (AFUNPTR)shm_read_feeback,
				IARG_INST_PTR,
				IARG_BOOL, INS_MemoryOperandIsRead(ins, 0),
				IARG_BOOL, INS_MemoryOperandIsWritten(ins, 0),
				//		IARG_MEMORYREAD_SIZE,
				//		IARG_MEMORYWRITE_SIZE,
				IARG_MEMORYOP_EA, 0,
				IARG_CONTEXT,
				IARG_END);
	}

}

void track_trace(TRACE trace, void* data)
{
#ifdef PLUS_TWO
  ADDRINT addr = TRACE_Address (trace);
  if (!g_hash_table_contains (sysexit_addr_table, GINT_TO_POINTER(addr))) return;
#endif

//  if (addr_load) {
//    ADDRINT addr = TRACE_Address (trace);
//    if (!g_hash_table_contains (sysexit_addr_table, GINT_TO_POINTER(addr))) return;
//  }

#ifdef USE_TLS_SCRATCH
  TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_INST_PTR, IARG_REG_VALUE, tls_reg, IARG_END);
#else
  TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) syscall_after, IARG_INST_PTR, IARG_END);
#endif
}

BOOL follow_child(CHILD_PROCESS child, void* data)
{
	char** argv;
	char** prev_argv = (char**)data;
	int index = 0;

	fprintf(out_fd, "following child...\n");

	/* the format of pin command would be:
	 * pin_binary -follow_execv -t pin_tool new_addr*/
	int new_argc = 5;
	argv = (char**)malloc(sizeof(char*) * new_argc);

	argv[0] = prev_argv[index++];
	argv[1] = (char *) "-follow_execv";
	while(strcmp(prev_argv[index], "-t")) index++;
	argv[2] = prev_argv[index++];
	argv[3] = prev_argv[index++];
	argv[4] = (char *) "--";

	CHILD_PROCESS_SetPinCommandLine(child, new_argc, argv);

	fprintf(out_fd, "returning from follow child\n");
	fprintf(out_fd, "pin my pid is %d\n", PIN_GetPid());
	fprintf(out_fd, "%d is application thread\n", PIN_IsApplicationThread());

	return TRUE;
}

/**************************end of replay compensation**************/
#endif

void OnExit(INT32, void *) {
	printf("Inside fini function\n");

//	theia_store_cdm_query_result();

        //close kafka
        if(publish_to_kafka_global){
                producer_global->shutdown();
        }

        //close avro file
        if(create_avro_file_global){
                file_serializer_global->close();
        }
}

/*
 * DTA
 *
 * used for demonstrating how to implement
 * a practical dynamic taint analysis (DTA)
 * tool using libdft
 */
int
main(int argc, char **argv)
{
  printf("Using taint analysis!\n");

  std::string publish_to_kafka = "";                                               
  std::string kafka_server = "";                                                   
  std::string kafka_topic = "";                                                    
  std::string kafka_producer_id = "";                                              
  std::string create_avro_file = "";                                               
  std::string query_id = "";                                                       
  std::string subject_uuid = "";
  std::string local_principal = "";                                                
  std::string tag_counter = "";
  std::string avro_file_name = "";
  std::string engagement_config_string = "";
  engagement_config_string = EngagementConfig.Value();

#ifdef THEIA_REPLAY_COMPENSATION
  int rc;
  out_fd = fopen("pin_theia.output", "w");
  //Yang
	// Intialize the replay device
	rc = devspec_init (&fd_dev);
	if (rc < 0) return rc;

#endif


if(engagement_config_string == "true"){
  engagement_config = true;

	/* initialize symbol processing */
	PIN_InitSymbols();

	/* initialize Pin; optimized branch */
	if (unlikely(PIN_Init(argc, argv))) {
		/* Pin initialization failed */
		Usage();
        goto err;
    }


#ifdef THEIA_REPLAY_COMPENSATION
/************* start of replay compensation*********/
//	IMG_AddInstrumentFunction(ImageLoad, 0);
	PIN_AddFiniFunction(OnExit, 0);
#ifdef USE_TLS_SCRATCH
	// Claim a Pin virtual register to store the pointer to a thread's TLS
	tls_reg = PIN_ClaimToolRegister();
#else
	// Obtain a key for TLS storage
	tls_key = PIN_CreateThreadDataKey(0);
#endif

//	sysexit_addr_table = g_hash_table_new(g_direct_hash, g_direct_equal);

	PIN_AddFollowChildProcessFunction(follow_child, argv);
	INS_AddInstrumentFunction(track_inst, 0);

	// Register a notification handler that is called when the application
	// forks a new process
	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, AfterForkInChild, 0);

	TRACE_AddInstrumentFunction (track_trace, 0);

/************* end of replay compensation*********/
#endif

	/* initialize the core tagging engine */
	if (unlikely(libdft_init() != 0))
		/* failed */
		goto err;

	//mf: removed because not needed
	//(void) dta_sink_control_flow((AFUNPTR)alert, (AFUNPTR)alert);

	//mf: removed because not needed
    //bool track_fs, track_net, track_stdin;

	//mf: removed because not needed
    //track_fs = (fs.Value()!=0);
    //track_net = (net.Value()!=0);
    //track_stdin = (stin.Value()!=0);

    //mf: removed because not needed
    //dta_source(track_fs, NULL, track_net, NULL, track_stdin);
	//dta_source(true, NULL, false, NULL, false);

	//mf: removed because not needed
    //PIN_AddFiniFunction(Fini, 0);

	//mf: implementing basic tracking
  
  //get the inbound and outbound uuids here by querying the reachabilty results
  // CDM_UUID_Type *inb_uuid, *out_uuid; 
  //int num_inb = get_inbound_for_taint(query_id, subject_uuid, &inb_uuid);
  //int num_out = get_outbound_for_taint(query_id, subject_uuid, &out_uuid);
  // inb_uuid[0], ..., inb_uuid[num_inb-1]
  // out_uuid[0], ..., out_uuid[num_out-1]
  // /*convert uuid to tag and store to rdb*/
  // add_tags_from_uuid_to_db(inb_uuid, IN);
  // add_tags_from_uuid_to_db(out_uuid, OUT);

//	(void)syscall_set_post(&syscall_desc[__NR_open], post_open_hook);
	(void)syscall_set_post(&syscall_desc[__NR_read], post_read_hook);
	(void)syscall_set_post(&syscall_desc[__NR_recvfrom], post_recvfrom_hook);
	(void)syscall_set_post(&syscall_desc[__NR_write], post_write_hook);
	(void)syscall_set_post(&syscall_desc[__NR_sendto], post_sendto_hook);
//	(void)syscall_set_post(&syscall_desc[__NR_close], post_close_hook);

  publish_to_kafka = PublishToKafka.Value();
  kafka_server = KafkaServer.Value();
  kafka_topic = KafkaTopic.Value() + "r";
  kafka_producer_id = kafka_topic;
  create_avro_file = CreateAvroFile.Value();
  query_id = QueryId.Value();
  subject_uuid = SubjectUUID.Value();
  local_principal = LocalPrincipal.Value();
  tag_counter = TagCounter.Value();

  printf("Publish to kafka=%s\n", publish_to_kafka.c_str());
  printf("Kafka server=%s\n", kafka_server.c_str());
  printf("Kafka topic=%s\n", kafka_topic.c_str());
  printf("Kafka producer id=%s\n", kafka_producer_id.c_str());
  printf("Create avro file=%s\n", create_avro_file.c_str());
  printf("Query id=%s\n", query_id.c_str());
  printf("Subject UUID=%s\n", subject_uuid.c_str());
  printf("Local principal=%s\n", local_principal.c_str());
  printf("Tag counter=%s\n", tag_counter.c_str());

  //mf: take care of kafak and avro
  if(publish_to_kafka=="true"){
  	publish_to_kafka_global = true;
	producer_global = new TheiaStoreCdmProducer(kafka_topic, kafka_server, writer_schema, kafka_producer_id);
	producer_global->connect();
  }
  printf("Publish to kafka global=%s\n", publish_to_kafka_global ? "true" : "false");

  avro_file_name = query_id + "-" + subject_uuid + "-" + tag_counter + ".bin";
  std::replace(avro_file_name.begin(), avro_file_name.end(), ' ', '_');
  printf("Avro file name=%s\n", avro_file_name.c_str());
  if(create_avro_file=="true"){
  	create_avro_file_global = true;
  	file_serializer_global = new tc_serialization::AvroGenericFileSerializer<tc_schema::TCCDMDatum>(writer_schema, avro_file_name);
  }
  printf("Create avro file global=%s\n", create_avro_file_global ? "true" : "false");

  tag_counter_global = strtoul(tag_counter.c_str(), NULL, 0);
  query_id_global = query_id;
  subject_uuid_global= subject_uuid;
  local_principal_global = local_principal;
  printf("Tag counter global=%llu\n", tag_counter_global);
  printf("Query id global=%s\n", query_id_global.c_str());
  printf("Subject UUID global=%s\n", subject_uuid_global.c_str());
  printf("Local principal global=%s\n", local_principal_global.c_str());
  default_key_global = kafka_topic;
  printf("Default kafka key global=%s\n", default_key_global.c_str());

  //get inbound uuid
  inbound_uuid_array_count_global = get_inbound_for_taint(query_id_global, subject_uuid_global, &inbound_uuid_array_global);
  //get outbound uuid
  outbound_uuid_array_count_global = get_outbound_for_taint(query_id_global, subject_uuid_global, &outbound_uuid_array_global);
  printf("Number of inbound uuids=%d\n", inbound_uuid_array_count_global);
  printf("Number of outbound uuids=%d\n", outbound_uuid_array_count_global);

}
else{
  printf("Setting up taint analysis!\n");
  engagement_config = false;

    /* initialize symbol processing */
  PIN_InitSymbols();

  /* initialize Pin; optimized branch */
  if (unlikely(PIN_Init(argc, argv))) {
    /* Pin initialization failed */
    Usage();
        goto err;
    }

      /* initialize the core tagging engine */
  if (unlikely(libdft_init() != 0))
    /* failed */
    goto err;


  (void)syscall_set_post(&syscall_desc[__NR_read], post_read_hook);
  (void)syscall_set_post(&syscall_desc[__NR_recvfrom], post_recvfrom_hook);
  (void)syscall_set_post(&syscall_desc[__NR_write], post_write_hook);
  (void)syscall_set_post(&syscall_desc[__NR_sendto], post_sendto_hook);

  printf("Done setting up taint analysis!\n");
}

	/* start Pin */
	PIN_StartProgram();

	/* typically not reached; make the compiler happy */
	return EXIT_SUCCESS;

err:	/* error handling */

	/* detach from the process */
	libdft_die();

	/* return */
	return EXIT_FAILURE;
}
