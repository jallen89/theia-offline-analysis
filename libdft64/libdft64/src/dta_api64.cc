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
#include <assert.h>

#include "branch_pred.h"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "syscall_desc64.h"
#include "tagmap64.h"
#include "dta_api64.h"

#define QUAD_LEN    8
#define LONG_LEN	4	/* size in bytes of a word value */

/* default suffixes for dynamic shared libraries */
#define DLIB_SUFF	".so"
#define DLIB_SUFF_ALT	".so."

/* set of interesting descriptors (sockets) */
static set<int> fdset;

//mf: commented them out as they are not used so far
//static AFUNPTR alert_reg = NULL;
//static AFUNPTR alert_mem = NULL;
//static FILENAME_PREDICATE_CALLBACK filename_predicate = NULL;
//static NETWORK_PREDICATE_CALLBACK network_predicate = NULL;

//mf: commented
//#define DEBUG_PRINT_TRACE
#ifdef DEBUG_PRINT_TRACE
#include "debuglog.h"
#endif

/////////////////////////////////////////////////////////////////////
//mf: added or edited

/*
 * auxiliary (helper) function
 *
 * whenever open(2)/creat(2) is invoked,
 * add the descriptor inside the monitored
 * set of descriptors
 *
 * NOTE: it does not track dynamic shared
 * libraries
 */
void
post_open_hook(syscall_ctx_t *ctx)
{
#ifndef USE_CUSTOM_TAG
	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* ignore dynamic shared libraries */
	if (strstr((char *)ctx->arg[SYSCALL_ARG0], DLIB_SUFF) == NULL &&
		strstr((char *)ctx->arg[SYSCALL_ARG0], DLIB_SUFF_ALT) == NULL) {
#ifdef DEBUG_PRINT_TRACE
        logprintf("open syscall: %d %s\n", (int)ctx->ret, (char*)ctx->arg[SYSCALL_ARG0]);
        //fprintf(stderr, "open syscall: %d %s\n", (int)ctx->ret, (char*)ctx->arg[SYSCALL_ARG0]);
#endif
        if (strcmp((char*)ctx->arg[SYSCALL_ARG0], "/etc/localtime") == 0) {
#ifdef DEBUG_PRINT_TRACE
            logprintf("localtime, ignored due to address randomization!\n");
#endif
            return;
        }

		if (filename_predicate == NULL)
			fdset.insert((int)ctx->ret);
		else if (filename_predicate((char*)ctx->arg[SYSCALL_ARG0]))
			fdset.insert((int)ctx->ret);
    }
#else
	//mf: TODO implement
#endif
}

/*
 * read(2) handler (taint-source)
 */
void
post_read_hook(syscall_ctx_t *ctx)
{
#ifndef USE_CUSTOM_TAG
    /* read() was not successful; optimized branch */
     if (unlikely((long)ctx->ret <= 0))
                return;

	/* taint-source */
	if (fdset.find(ctx->arg[SYSCALL_ARG0]) != fdset.end()) {
        /* else set the tag markings */
#ifdef DEBUG_PRINT_TRACE
        logprintf("read syscall set address %lx ~ %lx with size %lu form fd %lu\n",
                ctx->arg[SYSCALL_ARG1], ctx->arg[SYSCALL_ARG1] + ctx->ret,
                (size_t)ctx->ret, ctx->arg[SYSCALL_ARG0]);
#endif
        tagmap_setn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
    }
	else {
        /* clear the tag markings */
	    tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
	}
#else
	//mf: TODO implement
#endif
}

/*
 * write(2) handler (taint-sink)
 */
void
post_write_hook(syscall_ctx_t *ctx)
{
#ifndef USE_CUSTOM_TAG
    /* write() was not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
            return;

    if (fdset.find(ctx->arg[SYSCALL_ARG0]) != fdset.end()) {
#ifdef DEBUG_PRINT_TRACE
    	logprintf("write syscall got address %lx ~ %lx with size %lu for fd %lu\n",
    			ctx->arg[SYSCALL_ARG1], ctx->arg[SYSCALL_ARG1] + ctx->ret,
    	        (size_t)ctx->ret, ctx->arg[SYSCALL_ARG0]);
#endif
    	//mf: iterate over addresses
    	ADDRINT base_address = (ADDRINT)ctx->arg[SYSCALL_ARG1];
    	size_t bytes_num = (size_t)ctx->ret;
    	for(size_t i=0; i<bytes_num; ++i){
    		ADDRINT curr_address = base_address+i;
    		size_t curr_tag_value = tagmap_getb(curr_address);
#ifdef DEBUG_PRINT_TRACE
    		logprintf("address %lx has tag %lu\n", curr_address, curr_tag_value);
#endif
    	}
    }
#else
	//mf: TODO implement
#endif
}

/*
 * auxiliary (helper) function
 *
 * whenever close(2) is invoked, check
 * the descriptor and remove if it was
 * inside the monitored set of descriptors
 */
void
post_close_hook(syscall_ctx_t *ctx)
{
#ifndef USE_CUSTOM_TAG
	/* iterator */
	set<int>::iterator it;

	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/*
	 * if the descriptor (argument) is
	 * interesting, remove it from the
	 * monitored set
	 */
	it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);
	if (likely(it != fdset.end()))
		fdset.erase(it);
#else
	//mf: TODO implement
#endif
}










