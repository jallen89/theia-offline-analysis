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

#include <iostream>
#include <set>

#include "branch_pred.h"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "syscall_desc64.h"
#include "tagmap64.h"
#include "dta_api64.h"

//Yang
#include <glib-2.0/glib.h>
#include "util.h"
#include "libdft_api.h"

//mf: added
#include "syscall_desc.h"

/* Syscall descriptors, defined in libdft. */
extern syscall_desc_t syscall_desc[SYSCALL_MAX];

/* default path for the log file (audit) */
#define LOGFILE_DFL	"/tmp/libdft-dta.log"

/* log file path (auditing) */
static KNOB<string> logpath(KNOB_MODE_WRITEONCE, "pintool", "l",
		LOGFILE_DFL, "");

/*
 * flag variables
 *
 * 0	: feature disabled
 * >= 1	: feature enabled
 */

/* track stdin (enabled by default) */
static KNOB<size_t> stin(KNOB_MODE_WRITEONCE, "pintool", "s", "1", "");

/* track fs (enabled by default) */
static KNOB<size_t> fs(KNOB_MODE_WRITEONCE, "pintool", "f", "1", "");

/* track net (enabled by default) */
static KNOB<size_t> net(KNOB_MODE_WRITEONCE, "pintool", "n", "1", "");

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
    cerr << "This tool monitors whether untrusted information (network/file io) can\n";
    cerr << "flow to the control flow instructions.\n";
    cerr << "\n" << KNOB_BASE::StringKnobSummary() << "\n";
}

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

		if (sysnum == SYS_open) {
			fprintf(out_fd, "try to open %s\n", (char *) syscallarg0);
		}
		if (sysnum == 31) {
			tdata->ignore_flag = (u_long) syscallarg1;
		}

#ifdef PLUS_TWO
		g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+2));
		fprintf (out_fd, "Add address %x\n", ip+2);
		g_hash_table_add(sysexit_addr_table, GINT_TO_POINTER(ip+11));
		fprintf (out_fd, "Add address %x\n", ip+11);
#endif	    
		if (sysnum == 45 || sysnum == 91 || sysnum == 120 || sysnum == 125 || sysnum == 174 || sysnum == 175 || sysnum == 190 || sysnum == 192) {
			//if (sysnum == 91 || sysnum == 120 || sysnum == 125 || sysnum == 175 || sysnum == 190 || sysnum == 192) {
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
      tdata->app_syscall = 0;  
    }
  } else {
    fprintf (out_fd, "syscall_after: NULL tdata\n");
  }
}



//Yang test shared memory read contents feedback
VOID shm_read_feeback(VOID *ip, BOOL if_mem_read, BOOL if_mem_write, 
	/*UINT32 mem_read_size, UINT32 mem_write_size,*/ VOID *address, CONTEXT *ctxt) {
	
	fprintf(out_fd, "for shm_read, ip: %p, address: %p, mem_read: %d,\
		mem_write: %d\n", ip, address, if_mem_read, if_mem_write);
	
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
  printf("hello there!\n");
	/* initialize symbol processing */
	PIN_InitSymbols();

	/* initialize Pin; optimized branch */
	if (unlikely(PIN_Init(argc, argv))) {
		/* Pin initialization failed */
		Usage();
        goto err;
    }

/************* start of replay compensation*********/
	PIN_AddFollowChildProcessFunction(follow_child, argv);
	INS_AddInstrumentFunction(track_inst, 0);

	// Register a notification handler that is called when the application
	// forks a new process
	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, AfterForkInChild, 0);

//	IMG_AddInstrumentFunction (ImageLoad, 0);
	TRACE_AddInstrumentFunction (track_trace, 0);

/************* end of replay compensation*********/

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
	(void)syscall_set_post(&syscall_desc[__NR_open], post_open_hook);
	//(void)syscall_set_post(&syscall_desc[__NR_read], post_read_hook);
	(void)syscall_set_post(&syscall_desc[__NR_write], post_write_hook);
	//(void)syscall_set_post(&syscall_desc[__NR_close], post_close_hook);
	(void)syscall_set_post(&syscall_desc[__NR_recvfrom], post_recvfrom_hook_test);

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
