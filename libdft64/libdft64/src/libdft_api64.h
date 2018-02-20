/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in June 2010.
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

#ifndef __LIBDFT_API_H__
#define __LIBDFT_API_H__

#include <sys/syscall.h>
#include <linux/version.h>
#include <vector>
#include <algorithm>

#include "pin.H"
#include "config.h"

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)
#define SYSCALL_MAX	__NR_perf_event_open+1
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
#error "Your kernel is tool old and this version of libdft does not support it"
#else
#define SYSCALL_MAX	__NR_process_vm_writev+1		/* max syscall number */
#endif

#define GPR_NUM		16			/* general purpose registers */
#define SCRATCH_REG 16

/* FIXME: turn off the EFLAGS.AC bit by applying the corresponding mask */
#define CLEAR_EFLAGS_AC(eflags)	((eflags & 0xfffbffff))

//Yang: for defining replay compensation used for attaching pin during replay
//mf: disabled to test taint without replay
//to mf: please add/remove the -DTHEIA_REPLAY_COMPENSATION in src,tools/Makefile.am
//#define THEIA_REPLAY_COMPENSATION


enum {
/* #define */ SYSCALL_ARG0 = 0,			/* 1st argument in syscall */
/* #define */ SYSCALL_ARG1 = 1,			/* 2nd argument in syscall */
/* #define */ SYSCALL_ARG2 = 2,			/* 3rd argument in syscall */
/* #define */ SYSCALL_ARG3 = 3,			/* 4th argument in syscall */
/* #define */ SYSCALL_ARG4 = 4,			/* 5th argument in syscall */
/* #define */ SYSCALL_ARG5 = 5,			/* 6th argument in syscall */
/* #define */ SYSCALL_ARG_NUM = 6		/* syscall arguments */
};

enum {						 /* {en,dis}able (ins_desc_t) */
/* #define */ INSDFL_ENABLE	= 0,
/* #define */ INSDFL_DISABLE	= 1
};

#ifdef USE_CUSTOM_TAG

// NOTE: This uses the same mapping as the vcpu_ctx_t struct defined below!
enum gpr {GPR_EDI, GPR_ESI, GPR_EBP, GPR_ESP, GPR_EBX, GPR_EDX, GPR_ECX, GPR_EAX, R8, R9, R10, R11, R12, R13, R14, R15, GPR_SCRATCH};

#define TAGS_PER_GPR 8

// The gpr_reg_idx struct specifies an individual byte of a gpr reg.
struct gpr_idx
{
    gpr reg;
    uint8_t idx;
};

#endif
/*
 * virtual CPU (VCPU) context definition;
 * x86/x86_32/i386 arch
 */
typedef struct {
	/*
	 * general purpose registers (GPRs)
	 *
	 * we assign one bit of tag information for
	 * for every byte of addressable memory; the 64-bit
	 * GPRs of the x86 architecture will be represented
	 * with 8 bits each (the lower 8 bits of a 32-bit
	 * unsigned integer)
	 *
	 * NOTE the mapping:
	 * 	0: RDI
	 * 	1: RSI
	 * 	2: RBP
	 * 	3: RSP
	 * 	4: RBX
	 * 	5: RDX
	 * 	6: RCX
	 * 	7: RAX
     * 	8: R8
     * 	9: R9
     * 	10: R10
     * 	11: R11
     * 	12: R12
     * 	13: R13
     * 	14: R14
     * 	15: R15
	 * 	16: scratch (not a real register; helper)
	 */
#ifdef USE_CUSTOM_TAG
    tag_t gpr[GPR_NUM + 1][TAGS_PER_GPR];
#else
	uint32_t gpr[GPR_NUM + 1];
#endif
} vcpu_ctx_t;

/*
 * system call context definition
 *
 * only up to SYSCALL_ARGS (i.e., 6) are saved
 */
typedef struct {
	int 	nr;			/* syscall number */
	ADDRINT arg[SYSCALL_ARG_NUM];	/* arguments */
	ADDRINT ret;			/* return value */
	void	*aux;			/* auxiliary data (processor state) */
/* 	ADDRINT errno; */		/* error code */
} syscall_ctx_t;

/* thread context definition */
typedef struct {
	vcpu_ctx_t	vcpu;		/* VCPU context */
	syscall_ctx_t	syscall_ctx;	/* syscall context */
	void		*uval;		/* local storage */
} thread_ctx_t;

/* instruction (ins) descriptor */
typedef struct {
	void	(* pre)(INS ins);	/* pre-ins instrumentation callback */
	void	(* post)(INS ins);	/* post-ins instrumentation callback */
	size_t	dflact;                 /* default instrumentation predicate */
} ins_desc_t;


/* libdft API */
int	libdft_init(ADDRINT version_mask = 0);
void	libdft_die(void);

/* ins API */
int	ins_set_pre(ins_desc_t*, void (*)(INS));
int	ins_clr_pre(ins_desc_t*);
int	ins_set_post(ins_desc_t*, void (*)(INS));
int	ins_clr_post(ins_desc_t*);
int	ins_set_dflact(ins_desc_t *desc, size_t action);

/* REG API */
size_t  REG64_INDX(REG);
size_t	REG32_INDX(REG);
size_t	REG16_INDX(REG);
size_t	REG8_INDX(REG);

//Yang
int get_record_pid();

struct thread_data {
	unsigned long app_syscall; // Per thread address for specifying pin vs. non-pin system calls
	int record_pid; 	// per thread record pid
	int syscall_cnt;	// per thread count of syscalls
	int sysnum;		// current syscall number
	unsigned long ignore_flag;
};

#ifdef THEIA_REPLAY_COMPENSATION
#include "boost/any.hpp"
#include "boost/array.hpp"

typedef boost::array<uint8_t, 16> CDM_UUID_Type; //128 bit long

int get_inode_for_pin (int fd_spec, u_long inode);
CDM_UUID_Type get_current_uuid(void);
#endif

#endif /* __LIBDFT_API_H__ */
