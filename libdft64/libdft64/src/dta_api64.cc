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


/* thread context */
extern REG thread_ctx_ptr;

/* ins descriptors */
extern ins_desc_t ins_desc[XED_ICLASS_LAST];

/* syscall descriptors */
extern syscall_desc_t syscall_desc[SYSCALL_MAX];

/* set of interesting descriptors (sockets) */
static set<int> fdset;

static AFUNPTR alert_reg = NULL;
static AFUNPTR alert_mem = NULL;

static FILENAME_PREDICATE_CALLBACK filename_predicate = NULL;

static NETWORK_PREDICATE_CALLBACK network_predicate = NULL;

#define DEBUG_PRINT_TRACE

#ifdef DEBUG_PRINT_TRACE
#include "debuglog.h"
#endif

/*
 * 64-bit register assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a register
 * for an indirect branch; returns a positive value
 * whenever the register value or the target address
 * are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_reg64(thread_ctx_t *thread_ctx, uint32_t reg, ADDRINT addr)
{
#ifdef DEBUG_PRINT_TRACE
    logprintf("Target address: %lx\n", addr);
    logprintf("Sanity: %x %lx\n", thread_ctx->vcpu.gpr[reg] & VCPU_MASK64, tagmap_getq(addr));
#endif
	/*
     * combine the register tag along with the tag
	 * markings of the target address
	 */
    return (thread_ctx->vcpu.gpr[reg] & VCPU_MASK64) | tagmap_getq(addr);
}

/*
 * 32-bit register assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a register
 * for an indirect branch; returns a positive value
 * whenever the register value or the target address
 * are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_reg32(thread_ctx_t *thread_ctx, uint32_t reg, ADDRINT addr)
{
#ifdef DEBUG_PRINT_TRACE
    logprintf("Target address: %lx\n", addr);
    logprintf("Sanity: %x %lx\n", thread_ctx->vcpu.gpr[reg] & VCPU_MASK32, tagmap_getw(addr));
#endif
	/*
	 * combine the register tag along with the tag
	 * markings of the target address
	 */
    return (thread_ctx->vcpu.gpr[reg] & VCPU_MASK32) | tagmap_getl(addr);
}

/*
 * 16-bit register assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a register
 * for an indirect branch; returns a positive value
 * whenever the register value or the target address
 * are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_reg16(thread_ctx_t *thread_ctx, uint32_t reg, ADDRINT addr)
{
#ifdef DEBUG_PRINT_TRACE
    logprintf("Target address: %lx\n", addr);
    logprintf("Sanity: %x %lx\n", thread_ctx->vcpu.gpr[reg] & VCPU_MASK16, tagmap_getw(addr));
#endif

	/*
	 * combine the register tag along with the tag
	 * markings of the target address
	 */
    return (thread_ctx->vcpu.gpr[reg] & VCPU_MASK16)
		| tagmap_getw(addr);
}

/*
 * 64-bit memory assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a memory
 * location for an indirect branch; returns a positive
 * value whenever the memory value (i.e., effective address),
 * or the target address, are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem64(ADDRINT paddr, ADDRINT taddr)
{
    return tagmap_getq(paddr) | tagmap_getq(taddr);
}

static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem64_1arg(ADDRINT paddr) {
    ADDRINT taddr = 0;
    size_t ret = PIN_SafeCopy(&taddr, (void*)paddr, 8);
    if (ret != 8)
        return tagmap_getq(paddr);
    else
        return tagmap_getq(paddr) | tagmap_getq(taddr);
}

/*
 * 32-bit memory assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a memory
 * location for an indirect branch; returns a positive
 * value whenever the memory value (i.e., effective address),
 * or the target address, are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem32(ADDRINT paddr, ADDRINT taddr)
{
    return tagmap_getl(paddr) | tagmap_getl(taddr);
}

static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem32_1arg(ADDRINT paddr) {
    ADDRINT taddr = 0;
    size_t ret = PIN_SafeCopy(&taddr, (void*)paddr, 4);
    if (ret != 4)
        return tagmap_getl(paddr);
    else
        return tagmap_getl(paddr) | tagmap_getl(taddr);
}


/*
 * 16-bit memory assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a memory
 * location for an indirect branch; returns a positive
 * value whenever the memory value (i.e., effective address),
 * or the target address, are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem16(ADDRINT paddr, ADDRINT taddr)
{
    return tagmap_getw(paddr) | tagmap_getw(taddr);
}

static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem16_1arg(ADDRINT paddr) {
    ADDRINT taddr = 0;
    size_t ret = PIN_SafeCopy(&taddr, (void*)paddr, 2);
    if (ret != 2)
        return tagmap_getw(paddr);
    else
        return tagmap_getw(paddr) | tagmap_getw(taddr);
}


/*
 * instrument the jmp/call instructions
 *
 * install the appropriate DTA/DFT logic (sinks)
 *
 * @ins:	the instruction to instrument
 */
void
dta_instrument_jmp_call(INS ins)
{
	/* temporaries */
	REG reg;

	/*
	 * we only care about indirect calls;
	 * optimized branch
	 */
	if (unlikely(INS_IsIndirectBranchOrCall(ins))) {
		/* perform operand analysis */

		/* call via register */
		if (INS_OperandIsReg(ins, 0)) {
			/* extract the register from the instruction */
			reg = INS_OperandReg(ins, 0);

			/* size analysis */

            /* 64-bit register */
            if (REG_is_gr64(reg))
				/*
				 * instrument assert_reg64() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_reg64,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg),
					IARG_REG_VALUE, reg,
					IARG_END);
			/* 32-bit register */
			else if (REG_is_gr32(reg))
				/*
				 * instrument assert_reg32() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_reg32,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg),
					IARG_REG_VALUE, reg,
					IARG_END);
			else
				/* 16-bit register */
				/*
				 * instrument assert_reg16() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_reg16,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg),
					IARG_REG_VALUE, reg,
					IARG_END);
            /*
             * instrument alert_reg() before branch;
             * conditional instrumentation -- then
             */
            INS_InsertThenCall(ins,
                IPOINT_BEFORE,
                (AFUNPTR)alert_reg,
                IARG_CONTEXT,
                IARG_BRANCH_TARGET_ADDR,
                IARG_END);
		}
		else {
		/* call via memory */
			/* size analysis */

            if (!INS_IsFarCall(ins) && !INS_IsFarJump(ins)) {
                if (INS_MemoryReadSize(ins) == QUAD_LEN)
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem64_1arg,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_END);
                else if (INS_MemoryReadSize(ins) == LONG_LEN)
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem32_1arg,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_END);
                else
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem16_1arg,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_END);
            }
            else {
                /* 64-bit */
                if (INS_MemoryReadSize(ins) == QUAD_LEN)
                    /*
                     * instrument assert_mem64() before branch;
                     * conditional instrumentation -- if
                     */
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem64,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_BRANCH_TARGET_ADDR,
                        IARG_END);
                /* 32-bit */
                else if (INS_MemoryReadSize(ins) == LONG_LEN)
                    /*
                     * instrument assert_mem32() before branch;
                     * conditional instrumentation -- if
                     */
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem32,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_BRANCH_TARGET_ADDR,
                        IARG_END);
                /* 16-bit */
                else
                    /*
                     * instrument assert_mem16() before branch;
                     * conditional instrumentation -- if
                     */
                    INS_InsertIfCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)assert_mem16,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_MEMORYREAD_EA,
                        IARG_BRANCH_TARGET_ADDR,
                        IARG_END);
            }
            /*
             * instrument alert() before branch;
             * conditional instrumentation -- then
             */
            INS_InsertThenCall(ins,
                IPOINT_BEFORE,
                (AFUNPTR)alert_mem,
                IARG_CONTEXT,
                IARG_MEMORYREAD_EA,
                IARG_END);
        }
	}
}

/*
 * instrument the ret instruction
 *
 * install the appropriate DTA/DFT logic (sinks)
 *
 * @ins:	the instruction to instrument
 */
void
dta_instrument_ret(INS ins)
{
	/* size analysis */

    /* 64-bit */
    if (INS_MemoryReadSize(ins) == QUAD_LEN)
		/*
		 * instrument assert_mem64() before ret;
		 * conditional instrumentation -- if
		 */
		INS_InsertIfCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)assert_mem64,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYREAD_EA,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);
	/* 32-bit */
	else if (INS_MemoryReadSize(ins) == LONG_LEN)
		/*
		 * instrument assert_mem32() before ret;
		 * conditional instrumentation -- if
		 */
		INS_InsertIfCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)assert_mem32,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYREAD_EA,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);
	/* 16-bit */
	else
		/*
		 * instrument assert_mem16() before ret;
		 * conditional instrumentation -- if
		 */
		INS_InsertIfCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)assert_mem16,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYREAD_EA,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);

	/*
	 * instrument alert() before ret;
	 * conditional instrumentation -- then
	 */
	INS_InsertThenCall(ins,
		IPOINT_BEFORE,
		(AFUNPTR)alert_mem,
        IARG_CONTEXT,
		IARG_MEMORYREAD_EA,
		IARG_END);
}


/*
 * readv(2) handler (taint-source)
 */
static void
post_readv_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	int i;
	struct iovec *iov;
	set<int>::iterator it;

	/* bytes copied in a iovec structure */
	size_t iov_tot;

	/* total bytes copied */
	size_t tot = (size_t)ctx->ret;

	/* readv() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* get the descriptor */
	it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);

	/* iterate the iovec structures */
	for (i = 0; i < (int)ctx->arg[SYSCALL_ARG2] && tot > 0; i++) {
		/* get an iovec  */
		iov = ((struct iovec *)ctx->arg[SYSCALL_ARG1]) + i;

		/* get the length of the iovec */
		iov_tot = (tot >= (size_t)iov->iov_len) ?
			(size_t)iov->iov_len : tot;

		/* taint interesting data and zero everything else */
		if (it != fdset.end()) {
#ifdef DEBUG_PRINT_TRACE
            logprintf("readv syscall set address %lx ~ %lx with size %lu\n",
                    (size_t)iov->iov_base, (size_t)iov->iov_base + iov_tot,
                    (size_t)iov_tot);
#endif

            /* set the tag markings */
            tagmap_setn((size_t)iov->iov_base, iov_tot);
        }
        else
            /* clear the tag markings */
            tagmap_clrn((size_t)iov->iov_base, iov_tot);

        /* housekeeping */
        tot -= iov_tot;
    }
}

static void
post_socket_hook(syscall_ctx_t *ctx) {
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;

    /*
     * PF_INET and PF_INET6 descriptors are
     * considered interesting
     */
    if (likely(ctx->arg[SYSCALL_ARG0] == PF_INET ||
        ctx->arg[SYSCALL_ARG0] == PF_INET6))
        /* add the descriptor to the monitored set */
        fdset.insert((int)ctx->ret);
}

static void
post_accept_accept4_hook(syscall_ctx_t *ctx) {
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret < 0))
        return;
    /*
     * if the socket argument is interesting,
     * the returned handle of accept(2) is also
     * interesting
     */
    if (likely(fdset.find(ctx->arg[SYSCALL_ARG0]) !=
                fdset.end()))
        /* add the descriptor to the monitored set */
        fdset.insert((int)ctx->ret);
}

static void
post_recvfrom_hook(syscall_ctx_t *ctx) {
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
        return;

    /* taint-source */
    if (fdset.find((int)ctx->arg[SYSCALL_ARG0]) != fdset.end()) {
#ifdef DEBUG_PRINT_TRACE
        logprintf("recvfrom syscall set address %lx ~ %lx with size %lu\n",
                ctx->arg[SYSCALL_ARG1], ctx->arg[SYSCALL_ARG1] + ctx->ret,
                (size_t)ctx->ret);
#endif

        /* set the tag markings */
        tagmap_setn(ctx->arg[SYSCALL_ARG1],
                (size_t)ctx->ret);
    }
    else
        /* clear the tag markings */
        tagmap_clrn(ctx->arg[SYSCALL_ARG1],
                (size_t)ctx->ret);

    /* sockaddr argument is specified */
    if ((void *)ctx->arg[SYSCALL_ARG4] != NULL) {
        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG4],
            *((socklen_t *)ctx->arg[SYSCALL_ARG5]));

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG5], sizeof(socklen_t));
    }
}

static void
post_recvmsg_hook(syscall_ctx_t *ctx) {
    /* not successful; optimized branch */
    if (unlikely((long)ctx->ret <= 0))
        return;

    /* get the descriptor */
    set<int>::iterator it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);

    /* extract the message header */
    struct msghdr *msg = (struct msghdr *)ctx->arg[SYSCALL_ARG1];

    /* source address specified */
    if (msg->msg_name != NULL) {
        /* clear the tag bits */
        tagmap_clrn((size_t)msg->msg_name,
            msg->msg_namelen);

        /* clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_namelen,
                sizeof(int));
    }

    /* ancillary data specified */
    if (msg->msg_control != NULL) {
        /* taint-source */
        if (it != fdset.end()) {
#ifdef DEBUG_PRINT_TRACE
            logprintf("recvmsg ancillary syscall set address %lx ~ %lx with size %lu\n",
                    (size_t)msg->msg_control, (size_t)msg->msg_control + msg->msg_controllen,
                    (size_t)msg->msg_controllen);
#endif
            /* set the tag markings */
            tagmap_setn((size_t)msg->msg_control,
                msg->msg_controllen);
        }
        else
            /* clear the tag markings */
            tagmap_clrn((size_t)msg->msg_control,
                msg->msg_controllen);

        /* clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_controllen,
                sizeof(int));
    }

    /* flags; clear the tag bits */
    tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));

    /* total bytes received */
    size_t tot = (size_t)ctx->ret;

    /* iterate the iovec structures */
    for (size_t i = 0; i < msg->msg_iovlen && tot > 0; i++) {
        /* get the next I/O vector */
        struct iovec *iov = &msg->msg_iov[i];

        /* get the length of the iovec */
        size_t iov_tot = (tot > (size_t)iov->iov_len) ?
                (size_t)iov->iov_len : tot;

        /* taint-source */
        if (it != fdset.end()) {
#ifdef DEBUG_PRINT_TRACE
            logprintf("readmsg syscall set address %lx ~ %lx with size %lu\n",
                    (size_t)iov->iov_base, (size_t)iov->iov_base + iov_tot,
                    (size_t)iov_tot);
#endif

            /* set the tag markings */
            tagmap_setn((size_t)iov->iov_base,
                        iov_tot);
        }
        else
            /* clear the tag markings */
            tagmap_clrn((size_t)iov->iov_base,
                        iov_tot);

        /* housekeeping */
        tot -= iov_tot;
    }
}

/*
 * auxiliary (helper) function
 *
 * duplicated descriptors are added into
 * the monitored set
 */
static void
post_dup_hook(syscall_ctx_t *ctx)
{
	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/*
	 * if the old descriptor argument is
	 * interesting, the returned handle is
	 * also interesting
	 */
	if (likely(fdset.find((int)ctx->arg[SYSCALL_ARG0]) != fdset.end()))
		fdset.insert((int)ctx->ret);
}

int dta_sink_control_flow(AFUNPTR alert_reg_funcp, AFUNPTR alert_mem_funcp) {
	/*
	 * handle control transfer instructions
	 *
	 * instrument the branch instructions, accordingly,
	 * for installing taint-sinks (DFT-logic) that check
	 * for tainted targets (i.e., tainted operands or
	 * tainted branch targets)
	 */

	/* instrument call */
	(void)ins_set_post(&ins_desc[XED_ICLASS_CALL_NEAR],
			dta_instrument_jmp_call);

	/* instrument jmp */
	(void)ins_set_post(&ins_desc[XED_ICLASS_JMP],
			dta_instrument_jmp_call);

	/* instrument ret */
	(void)ins_set_post(&ins_desc[XED_ICLASS_RET_NEAR],
			dta_instrument_ret);

    alert_reg = alert_reg_funcp;
    alert_mem = alert_mem_funcp;

    return 0;
}

int dta_source(bool track_file, FILENAME_PREDICATE_CALLBACK fpred,
               bool track_network, NETWORK_PREDICATE_CALLBACK npred,
               bool track_stdin) {
	/*
	 * install taint-sources
	 *
	 * all network-related I/O calls are
	 * assumed to be taint-sources; we
	 * install the appropriate wrappers
	 * for tagging the received data
	 * accordingly.
	 */

	/* read(2) */
	(void)syscall_set_post(&syscall_desc[__NR_read], post_read_hook);

	/* readv(2) */
	(void)syscall_set_post(&syscall_desc[__NR_readv], post_readv_hook);

	/* socket(2), accept(2), recv(2), recvfrom(2), recvmsg(2) */
	if (track_network) {
        network_predicate = npred;
        (void)syscall_set_post(&syscall_desc[__NR_socket], post_socket_hook);
        (void)syscall_set_post(&syscall_desc[__NR_accept], post_accept_accept4_hook);
        (void)syscall_set_post(&syscall_desc[__NR_accept4], post_accept_accept4_hook);
        (void)syscall_set_post(&syscall_desc[__NR_recvfrom], post_recvfrom_hook);
        (void)syscall_set_post(&syscall_desc[__NR_recvmsg], post_recvmsg_hook);
    }

	/* dup(2), dup2(2) */
	(void)syscall_set_post(&syscall_desc[__NR_dup], post_dup_hook);
	(void)syscall_set_post(&syscall_desc[__NR_dup2], post_dup_hook);

	/* close(2) */
	(void)syscall_set_post(&syscall_desc[__NR_close], post_close_hook);

	/* open(2), creat(2) */
	if (track_file) {
        filename_predicate = fpred;
		(void)syscall_set_post(&syscall_desc[__NR_open],
				post_open_hook);
		(void)syscall_set_post(&syscall_desc[__NR_creat],
				post_open_hook);
	}

    if (track_stdin)
    	fdset.insert(STDIN_FILENO);

    return 0;
}

bool dta_is_fd_interesting(int fd) {
    return fdset.count(fd) != 0;
}

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

        std::string file_name((char*)ctx->arg[SYSCALL_ARG0]);
        size_t found=file_name.find("test5.jpg");
        if(found!=std::string::npos){
        	 logprintf("opened file\n");

			if (filename_predicate == NULL)
				fdset.insert((int)ctx->ret);
			else if (filename_predicate((char*)ctx->arg[SYSCALL_ARG0]))
				fdset.insert((int)ctx->ret);
        }
    }
}

/*
 * read(2) handler (taint-source)
 */
void
post_read_hook(syscall_ctx_t *ctx)
{
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
        tagmap_setn_with_tag(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret, 5);
    }
	else {
        /* clear the tag markings */
	    tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
	}
}

/*
 * write(2) handler (taint-sink)
 */
void
post_write_hook(syscall_ctx_t *ctx)
{
#ifdef DEBUG_PRINT_TRACE
    logprintf("write syscall\n");
#endif
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
    		uint16_t curr_tag_value = tagmap_getb_tag(curr_address);
#ifdef DEBUG_PRINT_TRACE
    		logprintf("address %lx has tag %lu\n", curr_address, curr_tag_value);
#endif
    	}
    }
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
}

bool is_tagging = false;
int tagging_descriptor = -1;

void
post_recvfrom_hook_test(syscall_ctx_t *ctx) {
	  if (unlikely((long)ctx->ret <= 0))
		  return;

	  #ifdef DEBUG_PRINT_TRACE
	  	  ssize_t bytes_num = (ssize_t)ctx->ret;
	  	  int descriptor = (int) ctx->arg[SYSCALL_ARG0];
	  	  void *buf = (void *) ctx->arg[SYSCALL_ARG1];
	  	  int bytes_copied = 0;
	  	  if(bytes_num>1000){
	  		bytes_copied=1000;
	  	  }
	  	  std::string content;
	  	  content.assign((const char*)buf, bytes_copied);
  		  logprintf("recvfrom#descriptor:%d#string:%s#bytes:%d\n", descriptor, content.c_str(), bytes_num);

	  	  //if it is tagging and if the descriptor is the same check if I need to stop tagging
	  	  if(is_tagging && descriptor==tagging_descriptor){
	  		  size_t found=content.find("Last-Modified:");
	  		  if(found!=std::string::npos){
	  			  is_tagging=false;
	  			  tagging_descriptor=-1;
	  			  logprintf("stopped tagging\n");
	  		  }
	  	  }

	  	  if(!is_tagging){
	  		  size_t found = content.find("Last-Modified: Fri, 27 Oct 2017");
	  		  if(found!=std::string::npos){
	  			  is_tagging=true;
	  			  tagging_descriptor=descriptor;
	  			  logprintf("started tagging\n");
	  		  }
	  	  }
	  	  if(is_tagging){
	          logprintf("recvfrom syscall set address %lx ~ %lx with size %lu form fd %lu\n",
	                  ctx->arg[SYSCALL_ARG1], ctx->arg[SYSCALL_ARG1] + ctx->ret,
	                  (size_t)ctx->ret, ctx->arg[SYSCALL_ARG0]);
	          tagmap_setn_with_tag(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret, 5);
	  	  }
	  #endif

}










