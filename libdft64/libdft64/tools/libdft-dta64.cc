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
