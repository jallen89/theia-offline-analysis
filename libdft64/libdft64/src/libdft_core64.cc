/*-
 * Coiyright (c) 2010, 2011, 2012, 2013, Columbia University
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

/* 01/10/2013:
 * 	_xchg_m2r_op{b_u, b_l, w, l} and _xadd_r2m_op{b_u, b_l, w, l} did
 * 	not propagate the tags correctly; proposed fix by Remco Vermeulen
 * 	(r.vermeulen@vu.nl)
 * 09/10/2010:
 * 	r2r_xfer_oplb() was erroneously invoked without a third argument in
 *	MOVSX and MOVZX; proposed fix by Kangkook Jee (jikk@cs.columbia.edu)
 */

/*
 * TODO:
 * 	- optimize rep prefixed MOVS{B, W, D}
 */

#include <errno.h>
#include <string.h>

#include "pin.H"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "tagmap64.h"
#include "ins_seq_patterns.h"
#include "branch_pred.h"

//#define DEBUG_PRINT_TRACE
#ifdef DEBUG_PRINT_TRACE
#include "debuglog.h"
#endif

/* thread context */
extern REG	thread_ctx_ptr;

//mf: customized
#ifndef USE_CUSTOM_TAG
//do nothing
#else
extern tag_dir_t tag_dir;
#endif

//mf: customized
#ifndef USE_CUSTOM_TAG
static uint32_t SE_HELPER_32[] = {0, 0x0c};

static uint32_t SE_HELPER_64[] = {0, 0xf0};

/* fast tag extension (helper); [0] -> 0, [1] -> VCPU_MASK16 */
static uint32_t	MAP_8L_16[] = {0, VCPU_MASK16};

/* fast tag extension (helper); [0] -> 0, [2] -> VCPU_MASK16 */
static uint32_t	MAP_8H_16[] = {0, 0, VCPU_MASK16};

/* fast tag extension (helper); [0] -> 0, [1] -> VCPU_MASK32 */
static uint32_t	MAP_8L_32[] = {0, VCPU_MASK32};

/* fast tag extension (helper); [0] -> 0, [2] -> VCPU_MASK32 */
static uint32_t	MAP_8H_32[] = {0, 0, VCPU_MASK32};

static uint32_t MAP_8L_64[] = {0, VCPU_MASK64};

static uint32_t MAP_8H_64[] = {0, 0, VCPU_MASK64};
#else
/* Register reference helper macro's */

// Quickly reference the tags of register, only valid in a context where
// thread_ctx is defined!
#define RTAG thread_ctx->vcpu.gpr

// Quickly create arrays of register tags, only valid in a context where RTAG is valid!
#define R16TAG(RIDX) \
    {RTAG[(RIDX)][0], RTAG[(RIDX)][1]}


#define R32TAG(RIDX) \
    {RTAG[(RIDX)][0], RTAG[(RIDX)][1], RTAG[(RIDX)][2], RTAG[(RIDX)][3]}


#define R64TAG(RIDX) \
    {RTAG[(RIDX)][0], RTAG[(RIDX)][1], RTAG[(RIDX)][2], RTAG[(RIDX)][3], RTAG[(RIDX)][4], RTAG[(RIDX)][5], RTAG[(RIDX)][6], RTAG[(RIDX)][7]}

// Quickly create arrays of memory tags, only valid in a context where tag_dir_getb is valid!
// Note: Unlike the R*TAG macros, the M*TAG macros cannot be used to assign tags!
#define M8TAG(ADDR) \
    tag_dir_getb(tag_dir, (ADDR))

#define M16TAG(ADDR) \
    {M8TAG(ADDR), M8TAG(ADDR+1)}

#define M32TAG(ADDR) \
    {M8TAG(ADDR), M8TAG(ADDR+1), M8TAG(ADDR+2), M8TAG(ADDR+3) }

#define M64TAG(ADDR) \
    {M8TAG(ADDR), M8TAG(ADDR+1), M8TAG(ADDR+2), M8TAG(ADDR+3), M8TAG(ADDR+4), M8TAG(ADDR+5), M8TAG(ADDR+6), M8TAG(ADDR+7) }
#endif

/*
 * tag propagation (analysis function)
 *
 * extend the tag as follows: t[upper(eax)] = t[ax]
 *
 * NOTE: special case for the CWDE instruction
 *
 * @thread_ctx:	the thread context
 */
static void PIN_FAST_ANALYSIS_CALL
_cwde(thread_ctx_t *thread_ctx)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[7] & VCPU_MASK16;

	/* extension; 16-bit to 32-bit */
	src_tag |= SE_HELPER_32[src_tag >> 1];

	/* update the destination */
	thread_ctx->vcpu.gpr[7] =
        (thread_ctx->vcpu.gpr[7] & ~VCPU_MASK32) | src_tag;
#else
    //mf: correct propagation according to us as C3EE becomes FFFFC3EE (i.e., FFFF gets added at the beginning)
	RTAG[GPR_EAX][2] = tag_traits<tag_t>::cleared_val;
	RTAG[GPR_EAX][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * extend the tag as follows: t[upper(rax)] = t[eax]
 *
 * NOTE: special case for the CDQE instruction
 *
 * @thread_ctx:	the thread context
 */
static void PIN_FAST_ANALYSIS_CALL
_cdqe(thread_ctx_t *thread_ctx)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[7] & VCPU_MASK32;

	/* extension; 32-bit to 64-bit */
	src_tag |= SE_HELPER_64[src_tag >> 3];

	/* update the destination */
	thread_ctx->vcpu.gpr[7] = src_tag;
#else
    //mf: correct propagation according to us as 3344C3EE becomes FFFFFFFF3344C3EE (i.e., FFFFFFFF gets added at the beginning)
	RTAG[GPR_EAX][4] = tag_traits<tag_t>::cleared_val;
	RTAG[GPR_EAX][5] = tag_traits<tag_t>::cleared_val;
	RTAG[GPR_EAX][6] = tag_traits<tag_t>::cleared_val;
	RTAG[GPR_EAX][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_opwb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8H_16[src_tag];
#else
    //mf: correct propagation according to us as EE becomes FFEE (i.e., FF gets added at the beginning)
	tag_t src_tag = thread_ctx->vcpu.gpr[src][1];
	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_opwb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag =
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8L_16[src_tag];
#else
    //mf: correct propagation according to us as EE becomes FFEE (i.e., FF gets added at the beginning)
	tag_t src_tag = thread_ctx->vcpu.gpr[src][0];
	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_oplb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
        (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK32) | MAP_8H_32[src_tag];
#else
    //mf: correct propagation according to us as 0000CCEE becomes FFFFFFCC
	tag_t src_tag = thread_ctx->vcpu.gpr[src][1];
	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_oplb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
    /* NOTE: The upper 32-bit is automatically clear */
	thread_ctx->vcpu.gpr[dst] = MAP_8L_32[src_tag];
#else
    //mf: correct propagation according to us as 0000CCEE becomes FFFFFFEE
	tag_t src_tag = thread_ctx->vcpu.gpr[src][0];
	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	//mf: TODO investigate why fl changed this
	/* extension; 16-bit to 32-bit */
	src_tag |= SE_HELPER_32[src_tag >> 1];

	/* update the destination (xfer) */
    /* NOTE: The upper 32-bit is automatically clear */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us as 0000CCEE becomes FFFFCCEE
	tag_t src_tags[] = R16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_opqb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8H_64[src_tag];
#else
    //mf: correct propagation according to us as 0000CCEE becomes CC
	tag_t src_tag = thread_ctx->vcpu.gpr[src][1];

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_opqb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("movsx_opqb_l: src %x\n", thread_ctx->vcpu.gpr[src]);
#endif
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8L_64[src_tag];
#ifdef DEBUG_PRINT_TRACE
    logprintf("movsx_opqb_l: dst %x\n", thread_ctx->vcpu.gpr[dst]);
#endif

#else
    //mf: correct propagation according to us as 0000CCEE becomes EE
	tag_t src_tag = thread_ctx->vcpu.gpr[src][0];

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_r2r_opqw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* extension; 16-bit to 64-bit */
	src_tag |= SE_HELPER_32[src_tag >> 1] | SE_HELPER_64[src_tag >> 1];

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us as 0000CCEE becomes CCEE
	tag_t src_tags[] = R16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 32-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVSXD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsxd_r2r_opql(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK32;

#ifdef DEBUG_PRINT_TRACE
    logprintf("movsxd src tag %lx\n", src_tag);
#endif

	/* extension; 32-bit to 64-bit */
	src_tag |= SE_HELPER_64[src_tag >> 3];

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us as 1122CCEE becomes 1122CCEE
	tag_t src_tags[] = R32TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = src_tags[2];
	thread_ctx->vcpu.gpr[dst][3] = src_tags[3];
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_m2r_opwb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

#ifdef DEBUG_PRINT_TRACE
    logprintf("movsxd src tag %lx\n", src_tag);
#endif

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8L_16[src_tag];
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_m2r_oplb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (xfer) */
	/* NOTE: The upper 32-bit is automatically clear */
    thread_ctx->vcpu.gpr[dst] = MAP_8L_32[src_tag];
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_m2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK16;

	/* extension; 16-bit to 32-bit */
	src_tag |= SE_HELPER_32[src_tag >> 1];

	/* update the destination (xfer) */
    /* NOTE: The upper 32-bit is automatically clear */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = M16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_m2r_opqb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8L_64[src_tag];
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 16-bit memory location as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsx_m2r_opqw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK16;

	/* extension; 16-bit to 64-bit */
	src_tag |= SE_HELPER_64[src_tag >> 1] | SE_HELPER_32[src_tag >> 1];

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = M16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 32-bit memory location as t[dst] = t[src]
 *
 * NOTE: special case for MOVSXD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movsxd_m2r_opql(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK32;

#ifdef DEBUG_PRINT_TRACE
    logprintf("Movsxd mem: %lx\n", src_tag);
#endif

	/* extension; 32-bit to 64-bit */
	src_tag |= SE_HELPER_64[src_tag >> 3];

#ifdef DEBUG_PRINT_TRACE
    logprintf("Result tag: %lx\n", src_tag);
#endif

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = M32TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = src_tags[3];
	thread_ctx->vcpu.gpr[dst][3] = src_tags[4];
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_opwb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag =
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_high_tag = thread_ctx->vcpu.gpr[src][1];
	thread_ctx->vcpu.gpr[dst][0] = src_high_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_opwb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag =
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_low_tag = thread_ctx->vcpu.gpr[src][0];
	thread_ctx->vcpu.gpr[dst][0] = src_low_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_oplb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag =
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

	/* update the destination (xfer) */
	/* NOTE: It automatically clear uppper 32-bit */
    thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_high_tag = thread_ctx->vcpu.gpr[src][1];
	thread_ctx->vcpu.gpr[dst][0] = src_high_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_oplb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
    /* note: It automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_low_tag = thread_ctx->vcpu.gpr[src][0];
	thread_ctx->vcpu.gpr[dst][0] = src_low_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
#ifdef DEBUG_PRINT_TRACE
    logprintf("movzx_r2r_oplw dst %u src %u\n", dst, src);
#endif
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* update the destination (xfer) */
    /* NOTE: It automatically clears upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = R16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_opqb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag =
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_high_tag = thread_ctx->vcpu.gpr[src][1];
	thread_ctx->vcpu.gpr[dst][0] = src_high_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_opqb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_low_tag = thread_ctx->vcpu.gpr[src][0];
	thread_ctx->vcpu.gpr[dst][0] = src_low_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_r2r_opqw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = R16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_m2r_opwb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_m2r_oplb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (xfer) */
    /* NOTE: It automatically clears upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_m2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK16;

	/* update the destination (xfer) */
    /* NOTE: It automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = M16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_m2r_opqb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tag = M8TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tag;
	thread_ctx->vcpu.gpr[dst][1] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 64-bit register
 * and a 16-bit memory location as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_movzx_m2r_opqw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	size_t src_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK16;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
#else
    //mf: correct propagation according to us
	tag_t src_tags[] = M16TAG(src);

	thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
	thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
	thread_ctx->vcpu.gpr[dst][2] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][3] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][4] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][5] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][6] = tag_traits<tag_t>::cleared_val;
	thread_ctx->vcpu.gpr[dst][7] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[RAX] = t[src]; return
 * the result of RAX == src and also
 * store the original tag value of
 * EAX in the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	RAX register value
 * @src:	source register index (VCPU)
 * @src_val:	source register value
 */
//mf: the fast path is the else case for cmpxchg instruction
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opq_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, uint32_t src,
							ADDRINT src_val)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

	/* update */
	thread_ctx->vcpu.gpr[7] = thread_ctx->vcpu.gpr[src];

	/* compare the dst and src values */
	return (dst_val == src_val);
#else
	//mf: propagation according to dtracker (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R64TAG(GPR_EAX);

    for (size_t i = 0; i < 8; i++)
        RTAG[GPR_SCRATCH][i] = save_tags[i];

	/* update */
    //src is the index of the destination register
    tag_t src_tags[] = R64TAG(src);

    for (size_t i = 0; i < 8; i++)
        RTAG[GPR_EAX][i] = src_tags[i];

    //mf: dst_val is the value of RAX and src_val is the value of destination register
	return (dst_val == src_val);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[dst] = t[src]; restore the
 * value of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
//mf: the slow path is the then case for cmpxchg instruction
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opq_slow(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

	/* update */
	thread_ctx->vcpu.gpr[dst] =
        thread_ctx->vcpu.gpr[src];
#else
	//mf: propagation according to dtracker (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	/* restore the tag value from the scratch register */
    tag_t saved_tags[] = R64TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 8; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R64TAG(src);

    for (size_t i = 0; i < 8; i++)
        thread_ctx->vcpu.gpr[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[EAX] = t[src]; return
 * the result of EAX == src and also
 * store the original tag value of
 * EAX in the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	EAX register value
 * @src:	source register index (VCPU)
 * @src_val:	source register value
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opl_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, uint32_t src,
							ADDRINT src_val)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

	/* update */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[7] = thread_ctx->vcpu.gpr[src] & VCPU_MASK32;
//        (thread_ctx->vcpu.gpr[7] & ~VCPU_MASK32) |
//		(thread_ctx->vcpu.gpr[src] & VCPU_MASK32);

	/* compare the dst and src values */
	return (dst_val == src_val);
#else
	//mf: propagation according to dtracker (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R32TAG(GPR_EAX);

    for (size_t i = 0; i < 4; i++)
        RTAG[GPR_SCRATCH][i] = save_tags[i];

	/* update */
    tag_t src_tags[] = R32TAG(src);

    for (size_t i = 0; i < 4; i++)
        RTAG[GPR_EAX][i] = src_tags[i];

    /* compare the dst and src values */
    return (dst_val == src_val);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[dst] = t[src]; restore the
 * value of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opl_slow(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

	/* update */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = thread_ctx->vcpu.gpr[src] & VCPU_MASK32;
//        (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK32) |
//        (thread_ctx->vcpu.gpr[src] & VCPU_MASK32);
#else
	//mf: propagation according to dtracker (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	/* restore the tag value from the scratch register */
    tag_t saved_tags[] = R32TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 4; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R32TAG(src);

    for (size_t i = 0; i < 4; i++)
        thread_ctx->vcpu.gpr[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[AX] = t[src]; return
 * the result of AX == src and also
 * store the original tag value of
 * AX in the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	AX register value
 * @src:	source register index (VCPU)
 * @src_val:	source register value
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opw_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, uint32_t src,
						ADDRINT src_val)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

	/* update */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);

	/* compare the dst and src values */
	return (dst_val == src_val);
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R16TAG(GPR_EAX);

    for (size_t i = 0; i < 2; i++)
        RTAG[GPR_SCRATCH][i] = save_tags[i];

    tag_t src_tags[] = R16TAG(src);
    RTAG[GPR_EAX][0] = src_tags[0];
    RTAG[GPR_EAX][1] = src_tags[1];

    /* compare the dst and src values */
    return (dst_val == src_val);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[dst] = t[src]; restore the
 * value of AX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2r_opw_slow(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

	/* update */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);
#else
	/* restore the tag value from the scratch register */
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
    tag_t saved_tags[] = R16TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 2; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R16TAG(src);
    thread_ctx->vcpu.gpr[dst][0] = src_tags[0];
    thread_ctx->vcpu.gpr[dst][1] = src_tags[1];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location
 * as t[EAX] = t[src]; return the result
 * of EAX == src and also store the
 * original tag value of EAX in
 * the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	destination register value
 * @src:	source memory address
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_m2r_opq_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* update */
	thread_ctx->vcpu.gpr[7] = (*entry >> VIRT2BIT(src)) & VCPU_MASK64;

	/* compare the dst and src values; the original values the tag bits */
	return (((uint64_t)dst_val) == *(uint64_t *)src);
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R64TAG(GPR_EAX);

    for (size_t i = 0; i < 8; i++)
        thread_ctx->vcpu.gpr[GPR_SCRATCH][i] = save_tags[i];

    tag_t src_tags[] = M64TAG(src);

    for (size_t i = 0; i < 8; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = src_tags[i];

    return (((uint64_t)dst_val) == *(uint64_t *)src);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location
 * as t[dst] = t[src]; restore the value
 * of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2m_opq_slow(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	/* update */
	*entry =
		(*entry & ~(QUAD_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK64) <<
        VIRT2BIT(dst));
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	/* restore the tag value from the scratch register */
    tag_t saved_tags[] = R64TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 8; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R64TAG(src);

    for (size_t i = 0; i < 8; i++)
        tag_dir_setb(tag_dir, dst + i, src_tags[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location
 * as t[EAX] = t[src]; return the result
 * of EAX == src and also store the
 * original tag value of EAX in
 * the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	destination register value
 * @src:	source memory address
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_m2r_opl_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* update */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[7] = ((*entry >> VIRT2BIT(src)) & VCPU_MASK32);
//        (thread_ctx->vcpu.gpr[7] & ~VCPU_MASK32) |
//		((*entry >> VIRT2BIT(src)) & VCPU_MASK32);

	/* compare the dst and src values; the original values the tag bits */
	return (((uint32_t)dst_val) == *(uint32_t *)src);
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R32TAG(GPR_EAX);

    for (size_t i = 0; i < 4; i++)
        thread_ctx->vcpu.gpr[GPR_SCRATCH][i] = save_tags[i];

    tag_t src_tags[] = M32TAG(src);

    for (size_t i = 0; i < 4; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = src_tags[i];

    return (((uint32_t)dst_val) == *(uint32_t *)src);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location
 * as t[dst] = t[src]; restore the value
 * of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2m_opl_slow(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	/* update */
	*entry =
		(*entry & ~(LONG_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst));
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	/* restore the tag value from the scratch register */
    tag_t saved_tags[] = R32TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 4; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R32TAG(src);

    for (size_t i = 0; i < 4; i++)
        tag_dir_setb(tag_dir, dst + i, src_tags[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location
 * as t[AX] = t[src]; return the result
 * of AX == src and also store the
 * original tag value of AX in
 * the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @dst_val:	destination register value
 * @src:	source memory address
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
_cmpxchg_m2r_opw_fast(thread_ctx_t *thread_ctx, ADDRINT dst_val, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[SCRATCH_REG] =
		thread_ctx->vcpu.gpr[7];

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* update */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK16);

	/* compare the dst and src values; the original values the tag bits */
	return (((uint16_t)dst_val) == *(uint16_t *)src);
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	//reinitialize scratch
	for (size_t i = 0; i < 8; i++)
	        RTAG[GPR_SCRATCH][i] = tag_traits<tag_t>::cleared_val;

	/* save the tag value of dst in the scratch register */
    tag_t save_tags[] = R16TAG(GPR_EAX);

    for (size_t i = 0; i < 2; i++)
        thread_ctx->vcpu.gpr[GPR_SCRATCH][i] = save_tags[i];

    tag_t src_tags[] = M16TAG(src);

    for (size_t i = 0; i < 2; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = src_tags[i];

    return (((uint16_t)dst_val) == *(uint16_t *)src);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location
 * as t[dst] = t[src]; restore the value
 * of AX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 * @res:	restore register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_cmpxchg_r2m_opw_slow(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[SCRATCH_REG];

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	/* update */
	*entry =
		(*entry & ~(WORD_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst));
#else
	//mf: propagation according to us (GPR_SCRATCH is a temporary register, ZF flag is not handled)
	/* restore the tag value from the scratch register */
    tag_t saved_tags[] = R16TAG(GPR_SCRATCH);

    for (size_t i = 0; i < 2; i++)
        thread_ctx->vcpu.gpr[GPR_EAX][i] = saved_tags[i];

	/* update */
    tag_t src_tags[] = R16TAG(src);

    for (size_t i = 0; i < 2; i++)
        tag_dir_setb(tag_dir, dst + i, src_tags[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] = t[lower(src)]
 * and t[lower(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		 (thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);

	thread_ctx->vcpu.gpr[src] =
		 (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | (tmp_tag >> 1);
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = RTAG[src][0];
    tag_t tmp_tag = RTAG[dst][1];

	/* swap */
    RTAG[src][0] = tmp_tag;
    RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] = t[upper(src)]
 * and t[upper(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);

	thread_ctx->vcpu.gpr[src] =
	 (thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | (tmp_tag << 1);
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = RTAG[src][1];
    tag_t tmp_tag = RTAG[dst][0];

	/* swap */
    RTAG[src][1] = tmp_tag;
    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] = t[upper(src)]
 * and t[upper(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | tmp_tag;
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = RTAG[src][1];
    tag_t tmp_tag = RTAG[dst][1];

	/* swap */
    RTAG[src][1] = tmp_tag;
    RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] = t[lower(src)]
 * and t[lower(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | tmp_tag;
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = RTAG[src][0];
    tag_t tmp_tag = RTAG[dst][0];

	/* swap */
    RTAG[src][0] = tmp_tag;
    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[dst] = t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16;

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK16) | tmp_tag;
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = R16TAG(src);
    tag_t tmp_tags[] = R16TAG(dst);

	/* swap */
    for (size_t i = 0; i < 2; i++)
    	RTAG[src][i] = tmp_tags[i];

    for (size_t i = 0; i < 2; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[dst] = t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opl(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK32;

	/* swap */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[src] & VCPU_MASK32);
//		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK32) |
//		(thread_ctx->vcpu.gpr[src] & VCPU_MASK32);

	thread_ctx->vcpu.gpr[src] = tmp_tag;
//		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK32) | tmp_tag;
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = R32TAG(src);
    tag_t tmp_tags[] = R32TAG(dst);

	/* swap */
    for (size_t i = 0; i < 4; i++)
    	RTAG[src][i] = tmp_tags[i];

    for (size_t i = 0; i < 4; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[dst] = t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_r2r_opq(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst];

	/* swap */
	thread_ctx->vcpu.gpr[dst] = thread_ctx->vcpu.gpr[src];

	thread_ctx->vcpu.gpr[src] = tmp_tag;
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = R64TAG(src);
    tag_t tmp_tags[] = R64TAG(dst);

	/* swap */
    for (size_t i = 0; i < 8; i++)
    	RTAG[src][i] = tmp_tags[i];

    for (size_t i = 0; i < 8; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_m2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(((*entry >> VIRT2BIT(src)) << 1) &
		(VCPU_MASK8 << 1));

	*entry =
		(*entry & ~(BYTE_MASK << VIRT2BIT(src))) |
		((tmp_tag >> 1) << VIRT2BIT(src));
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = M8TAG(src);
    tag_t tmp_tag = RTAG[dst][1];

	/* swap */
    tag_dir_setb(tag_dir, src, tmp_tag);
    RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_m2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK8);

	*entry =
		(*entry & ~(BYTE_MASK << VIRT2BIT(src))) |
		(tmp_tag << VIRT2BIT(src));
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tag = M8TAG(src);
    tag_t tmp_tag = RTAG[dst][0];

	/* swap */
    tag_dir_setb(tag_dir, src, tmp_tag);
    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_m2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16;

    uint16_t *entry;
    VIRT2ENTRY(src, entry);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK16);

	*entry =
		(*entry & ~(WORD_MASK << VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) << VIRT2BIT(src));
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = M16TAG(src);
    tag_t tmp_tags[] = R16TAG(dst);

	/* swap */
    for (size_t i = 0; i < 2; i++)
    	tag_dir_setb(tag_dir, src+i, tmp_tags[i]);

    for (size_t i = 0; i < 2; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_m2r_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK32;

    uint16_t *entry;
    VIRT2ENTRY(src, entry);

	/* swap */
    /* Automatically clear upper 32-bit*/
	thread_ctx->vcpu.gpr[dst] =	((*entry >> VIRT2BIT(src)) & VCPU_MASK32);
//        (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK32) |
//		((*entry >> VIRT2BIT(src)) & VCPU_MASK32);

    *entry =
		(*entry & ~(LONG_MASK << VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) << VIRT2BIT(src));
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = M32TAG(src);
    tag_t tmp_tags[] = R32TAG(dst);

	/* swap */
    for (size_t i = 0; i < 4; i++)
    	tag_dir_setb(tag_dir, src+i, tmp_tags[i]);

    for (size_t i = 0; i < 4; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
_xchg_m2r_opq(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst];

    uint16_t *entry;
    VIRT2ENTRY(src, entry);

	/* swap */
	thread_ctx->vcpu.gpr[dst] = (*entry >> VIRT2BIT(src)) & VCPU_MASK64;

    *entry =
		(*entry & ~(QUAD_MASK << VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) << VIRT2BIT(src));
#else
	//mf: propagation according to dtracker (xchg is an exchange operation)
	/* temporary tag value */
    tag_t src_tags[] = M64TAG(src);
    tag_t tmp_tags[] = R64TAG(dst);

	/* swap */
    for (size_t i = 0; i < 8; i++)
    	tag_dir_setb(tag_dir, src+i, tmp_tags[i]);

    for (size_t i = 0; i < 8; i++)
    	RTAG[dst][i] = src_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] |= t[lower(src)]
 * and t[lower(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] |=
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);

	thread_ctx->vcpu.gpr[src] =
		 (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | (tmp_tag >> 1);
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t tmp_tag = RTAG[dst][1];

    RTAG[dst][1] = tag_combine(RTAG[dst][1], RTAG[src][0]);
    RTAG[src][0] = tmp_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] |= t[upper(src)]
 * and t[upper(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] |=
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);

	thread_ctx->vcpu.gpr[src] =
	 (thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | (tmp_tag << 1);
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t tmp_tag = RTAG[dst][0];

    RTAG[dst][0] = tag_combine(RTAG[dst][0], RTAG[src][1]);
    RTAG[src][1] = tmp_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] |= t[upper(src)]
 * and t[upper(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | tmp_tag;
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t tmp_tag = RTAG[dst][1];

    RTAG[dst][1] = tag_combine(RTAG[dst][1], RTAG[src][1]);
    RTAG[src][1] = tmp_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] |= t[lower(src)]
 * and t[lower(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | tmp_tag;
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t tmp_tag = RTAG[dst][0];

    RTAG[dst][0] = tag_combine(RTAG[dst][0], RTAG[src][0]);
    RTAG[src][0] = tmp_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[dst] |= t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16;

	/* swap */
	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);

	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK16) | tmp_tag;
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t src_tags[] = R16TAG(src);
	tag_t dst_tags[] = R16TAG(dst);

    for (size_t i = 0; i < 2; i++)
    	RTAG[dst][i] = tag_combine(dst_tags[i], src_tags[i]);

    for (size_t i = 0; i < 2; i++)
    	RTAG[src][i] = dst_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[dst] |= t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opl(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK32;

	/* swap */
    /* NOTE: Automatically clear 32-bit */
	thread_ctx->vcpu.gpr[dst] =
		((thread_ctx->vcpu.gpr[dst] | thread_ctx->vcpu.gpr[src]) & VCPU_MASK32);

	thread_ctx->vcpu.gpr[src] = tmp_tag;
//		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK32) | tmp_tag;
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t src_tags[] = R32TAG(src);
	tag_t dst_tags[] = R32TAG(dst);

    for (size_t i = 0; i < 4; i++)
    	RTAG[dst][i] = tag_combine(dst_tags[i], src_tags[i]);

    for (size_t i = 0; i < 4; i++)
    	RTAG[src][i] = dst_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[dst] |= t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2r_opq(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst];

	/* swap */
	thread_ctx->vcpu.gpr[dst] |= thread_ctx->vcpu.gpr[src];

	thread_ctx->vcpu.gpr[src] = tmp_tag;
#else
	//mf: propagation according to dtracker (does not handle propagation to eflags)
    tag_t src_tags[] = R64TAG(src);
	tag_t dst_tags[] = R64TAG(dst);

    for (size_t i = 0; i < 8; i++)
    	RTAG[dst][i] = tag_combine(dst_tags[i], src_tags[i]);

    for (size_t i = 0; i < 8; i++)
    	RTAG[src][i] = dst_tags[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 * @dst:	destination memory address
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * Exchanges the first operand (destination operand) with the second operand (source operand),
 * then loads the sum of the two values into the destination operand.
 * The destination operand can be a register or a memory location; the source operand is a register.
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2m_opb_u(thread_ctx_t *thread_ctx, uint32_t src, ADDRINT dst)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);

    /* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(dst)) & BYTE_MASK;

    *entry |=
        (((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1) << VIRT2BIT(dst));

    thread_ctx->vcpu.gpr[src] =
        (thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) |
        (tmp_tag << 1);
#else
	//mf: propagation according to us (does not handle propagation to eflags)
    //mf: src is the second operand, dst is the first operand
    tag_t src_tag = RTAG[src][1];
    tag_t dst_tag = M8TAG(dst);

    RTAG[src][1] = dst_tag;
    tag_dir_setb(tag_dir, dst, tag_combine(src_tag, dst_tag));
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 * @dst:	destination memory address
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * Exchanges the first operand (destination operand) with the second operand (source operand),
 * then loads the sum of the two values into the destination operand.
 * The destination operand can be a register or a memory location; the source operand is a register.
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2m_opb_l(thread_ctx_t *thread_ctx, uint32_t src, ADDRINT dst)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);

    /* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(dst)) & BYTE_MASK;

    *entry |=
        ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << VIRT2BIT(dst));

    thread_ctx->vcpu.gpr[src] =
        (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) |
        tmp_tag;
#else
	//mf: propagation according to us (does not handle propagation to eflags)
    //mf: src is the second operand, dst is the first operand
    tag_t src_tag = RTAG[src][0];
    tag_t dst_tag = M8TAG(dst);

    RTAG[src][0] = dst_tag;
    tag_dir_setb(tag_dir, dst, tag_combine(src_tag, dst_tag));
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 * @dst:	destination memory address
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * Exchanges the first operand (destination operand) with the second operand (source operand),
 * then loads the sum of the two values into the destination operand.
 * The destination operand can be a register or a memory location; the source operand is a register.
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2m_opw(thread_ctx_t *thread_ctx, uint32_t src, ADDRINT dst)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);

    /* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(dst)) & WORD_MASK;

    *entry |=
        ((thread_ctx->vcpu.gpr[src] & VCPU_MASK16) << VIRT2BIT(dst));

    thread_ctx->vcpu.gpr[src] =
        (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK16) |
        tmp_tag;
#else
	//mf: propagation according to us (does not handle propagation to eflags)
    //mf: src is the second operand, dst is the first operand
    tag_t src_tags[] = R16TAG(src);
    tag_t dst_tags[] = M16TAG(dst);

    for (size_t i = 0; i < 2; i++)
    	RTAG[src][i] = dst_tags[i];

    for (size_t i = 0; i < 2; i++)
    	tag_dir_setb(tag_dir, dst+i, tag_combine(src_tags[i], dst_tags[i]));
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 * @dst:	destination memory address
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * Exchanges the first operand (destination operand) with the second operand (source operand),
 * then loads the sum of the two values into the destination operand.
 * The destination operand can be a register or a memory location; the source operand is a register.
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2m_opl(thread_ctx_t *thread_ctx, uint32_t src, ADDRINT dst)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);

    /* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(dst)) & LONG_MASK;

    *entry |=
        ((thread_ctx->vcpu.gpr[src] & VCPU_MASK32) << VIRT2BIT(dst));

    /* NOTE: Automatically clear upper 32-bit */
    thread_ctx->vcpu.gpr[src] = tmp_tag;
//        (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK32) |
//        tmp_tag;
#else
	//mf: propagation according to us (does not handle propagation to eflags)
    //mf: src is the second operand, dst is the first operand
    tag_t src_tags[] = R32TAG(src);
    tag_t dst_tags[] = M32TAG(dst);

    for (size_t i = 0; i < 4; i++)
    	RTAG[src][i] = dst_tags[i];

    for (size_t i = 0; i < 4; i++)
    	tag_dir_setb(tag_dir, dst+i, tag_combine(src_tags[i], dst_tags[i]));
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 * @dst:	destination memory address
 */
/* mf: instruction details
 * TEMP ← SRC + DEST;
 * SRC ← DEST;
 * DEST ← TEMP;
 *
 * Exchanges the first operand (destination operand) with the second operand (source operand),
 * then loads the sum of the two values into the destination operand.
 * The destination operand can be a register or a memory location; the source operand is a register.
 *
 * The CF, PF, AF, SF, ZF, and OF flags are set according to the result of the addition, which is stored in the destination operand.
 */
static void PIN_FAST_ANALYSIS_CALL
_xadd_r2m_opq(thread_ctx_t *thread_ctx, uint32_t src, ADDRINT dst)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);

    /* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(dst)) & QUAD_MASK;

    *entry |=
        ((thread_ctx->vcpu.gpr[src] & VCPU_MASK64) << VIRT2BIT(dst));

    thread_ctx->vcpu.gpr[src] = tmp_tag;
#else
	//mf: propagation according to us (does not handle propagation to eflags)
    //mf: src is the second operand, dst is the first operand
    tag_t src_tags[] = R64TAG(src);
    tag_t dst_tags[] = M64TAG(dst);

    for (size_t i = 0; i < 8; i++)
    	RTAG[src][i] = dst_tags[i];

    for (size_t i = 0; i < 8; i++)
    	tag_dir_setb(tag_dir, dst+i, tag_combine(src_tags[i], dst_tags[i]));
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between three 16-bit
 * registers as t[dst] = t[base] | t[index]
 *
 * NOTE: special case for the LEA instruction
 *
 * @thread_ctx: the thread context
 * @dst:        destination register
 * @base:       base register
 * @index:      index register
 */
static void PIN_FAST_ANALYSIS_CALL
_lea_r2r_opw(thread_ctx_t *thread_ctx,
		uint32_t dst,
		uint32_t base,
		uint32_t index)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* update the destination */
	thread_ctx->vcpu.gpr[dst] =
		((thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[base] & VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[index] & VCPU_MASK16));
#else
    //mf: TODO double check propagation because lea clears the content of the remaining part of the register
	//mf: propagation according to dtracker
    tag_t base_tag[] = R16TAG(base);
    tag_t idx_tag[] = R16TAG(index);

    for (size_t i = 0; i < 2; i++)
    	RTAG[dst][i] = tag_combine(base_tag[i], idx_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between three 32-bit
 * registers as t[dst] = t[base] | t[index]
 *
 * NOTE: special case for the LEA instruction
 *
 * @thread_ctx: the thread context
 * @dst:        destination register
 * @base:       base register
 * @index:      index register
 */
static void PIN_FAST_ANALYSIS_CALL
_lea_r2r_opl(thread_ctx_t *thread_ctx,
		uint32_t dst,
		uint32_t base,
		uint32_t index)
{
//mf: customized
#ifndef USE_CUSTOM_TAG
	/* update the destination */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] =
         (thread_ctx->vcpu.gpr[base] & VCPU_MASK32) |
         (thread_ctx->vcpu.gpr[index] & VCPU_MASK32);
#else
	//mf: TODO double check propagation because lea clears the content of the remaining part of the register
	//mf: propagation according to dtracker
    tag_t base_tag[] = R16TAG(base);
    tag_t idx_tag[] = R16TAG(index);

    for (size_t i = 0; i < 4; i++)
    	RTAG[dst][i] = tag_combine(base_tag[i], idx_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between three 64-bit
 * registers as t[dst] = t[base] | t[index]
 *
 * NOTE: special case for the LEA instruction
 *
 * @thread_ctx: the thread context
 * @dst:        destination register
 * @base:       base register
 * @index:      index register
 */
static void PIN_FAST_ANALYSIS_CALL
_lea_r2r_opq(thread_ctx_t *thread_ctx,
		uint32_t dst,
		uint32_t base,
		uint32_t index)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("LEA: base %x idx %x\n", thread_ctx->vcpu.gpr[base], thread_ctx->vcpu.gpr[index]);
#endif
	/* update the destination */
	thread_ctx->vcpu.gpr[dst] =
		thread_ctx->vcpu.gpr[base] | thread_ctx->vcpu.gpr[index];
#else
	//mf: TODO double check propagation because lea clears the content of the remaining part of the register
	//mf: propagation according to dtracker
    tag_t base_tag[] = R16TAG(base);
    tag_t idx_tag[] = R16TAG(index);

    for (size_t i = 0; i < 8; i++)
    	RTAG[dst][i] = tag_combine(base_tag[i], idx_tag[i]);
#endif
}

// FIXME: this set of functions need to consider the influence between
// AX and DX.
/*
 * tag propagation (analysis function)
 *
 * propagate tag among three 8-bit registers as t[dst] |= t[upper(src)];
 * dst is AX, whereas src is an upper 8-bit register (e.g., CH, BH, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_ternary_opb_u(thread_ctx_t *thread_ctx, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8H_16[tmp_tag];
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV. Why also propagating to RTAG[GPR_EAX][1]?
	//mf: propagation according to dtracker
    tag_t tmp_tag = RTAG[src][1];

    RTAG[GPR_EAX][0] = tag_combine(RTAG[GPR_EAX][0], tmp_tag);
    RTAG[GPR_EAX][1] = tag_combine(RTAG[GPR_EAX][1], tmp_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among three 8-bit registers as t[dst] |= t[lower(src)];
 * dst is AX, whereas src is a lower 8-bit register (e.g., CL, BL, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_ternary_opb_l(thread_ctx_t *thread_ctx, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8L_16[tmp_tag];
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV. Why also propagating to RTAG[GPR_EAX][1]?
	//mf: propagation according to dtracker
    tag_t tmp_tag = RTAG[src][0];

    RTAG[GPR_EAX][0] = tag_combine(RTAG[GPR_EAX][0], tmp_tag);
    RTAG[GPR_EAX][1] = tag_combine(RTAG[GPR_EAX][1], tmp_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between among three 16-bit
 * registers as t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is DX, dst2 is AX, and src is a 16-bit register
 * (e.g., CX, BX, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_ternary_opw(thread_ctx_t *thread_ctx, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag;
	thread_ctx->vcpu.gpr[7] |= tmp_tag;
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = R16TAG(src);
    tag_t dst1_tag[] = R16TAG(GPR_EDX);
    tag_t dst2_tag[] = R16TAG(GPR_EAX);

    RTAG[GPR_EDX][0] = tag_combine(dst1_tag[0], tmp_tag[0]);
    RTAG[GPR_EDX][1] = tag_combine(dst1_tag[1], tmp_tag[1]);

    RTAG[GPR_EAX][0] = tag_combine(dst2_tag[0], tmp_tag[0]);
    RTAG[GPR_EAX][1] = tag_combine(dst2_tag[1], tmp_tag[1]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between among three 32-bit
 * registers as t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is EDX, dst2 is EAX, and src is a 32-bit register
 * (e.g., ECX, EBX, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_ternary_opl(thread_ctx_t *thread_ctx, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    size_t tmp_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK32;

	/* update the destinations */
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[5] = (thread_ctx->vcpu.gpr[5] & VCPU_MASK32) | tmp_tag;
	thread_ctx->vcpu.gpr[7] = (thread_ctx->vcpu.gpr[7] & VCPU_MASK32) | tmp_tag;
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = R32TAG(src);
    tag_t dst1_tag[] = R32TAG(GPR_EDX);
    tag_t dst2_tag[] = R32TAG(GPR_EAX);

    for (size_t i = 0; i < 4; i++)
    {
        RTAG[GPR_EDX][i] = tag_combine(dst1_tag[i], tmp_tag[i]);
        RTAG[GPR_EAX][i] = tag_combine(dst2_tag[i], tmp_tag[i]);
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between among three 64-bit
 * registers as t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is RDX, dst2 is RAX, and src is a 64-bit register
 * (e.g., RCX, RBX, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_ternary_opq(thread_ctx_t *thread_ctx, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= thread_ctx->vcpu.gpr[src];
	thread_ctx->vcpu.gpr[7] |= thread_ctx->vcpu.gpr[src];
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = R64TAG(src);
    tag_t dst1_tag[] = R64TAG(GPR_EDX);
    tag_t dst2_tag[] = R64TAG(GPR_EAX);

    for (size_t i = 0; i < 8; i++)
    {
        RTAG[GPR_EDX][i] = tag_combine(dst1_tag[i], tmp_tag[i]);
        RTAG[GPR_EAX][i] = tag_combine(dst2_tag[i], tmp_tag[i]);
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 8-bit registers
 * and an 8-bit memory location as t[dst] |= t[src];
 * dst is AX, whereas src is an 8-bit memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_ternary_opb(thread_ctx_t *thread_ctx, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK8;

	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8L_16[tmp_tag];
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV. Why R16TAG and not R8TAG?
	//mf: propagation according to dtracker
    tag_t tmp_tag = M8TAG(src);
    tag_t dst_tag[] = R16TAG(GPR_EAX);

    RTAG[GPR_EAX][0] = tag_combine(dst_tag[0], tmp_tag);
    RTAG[GPR_EAX][1] = tag_combine(dst_tag[1], tmp_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 16-bit registers
 * and a 16-bit memory address as
 * t[dst1] |= t[src] and t[dst1] |= t[src];
 *
 * dst1 is DX, dst2 is AX, and src is a 16-bit
 * memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_ternary_opw(thread_ctx_t *thread_ctx, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK16;

	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag;
	thread_ctx->vcpu.gpr[7] |= tmp_tag;
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = M16TAG(src);
    tag_t dst1_tag[] = R16TAG(GPR_EDX);
    tag_t dst2_tag[] = R16TAG(GPR_EAX);

    for (size_t i = 0; i < 2; i++)
    {
        RTAG[GPR_EDX][i] = tag_combine(dst1_tag[i], tmp_tag[i]);
        RTAG[GPR_EAX][i] = tag_combine(dst2_tag[i], tmp_tag[i]);
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 32-bit
 * registers and a 32-bit memory as
 * t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is EDX, dst2 is EAX, and src is a 32-bit
 * memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_ternary_opl(thread_ctx_t *thread_ctx, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK32;

	/* update the destinations */
    /* NOTE: Automatically clear 32-bit */
	thread_ctx->vcpu.gpr[5] = (thread_ctx->vcpu.gpr[5] & VCPU_MASK32) | tmp_tag;
	thread_ctx->vcpu.gpr[7] = (thread_ctx->vcpu.gpr[7] & VCPU_MASK32) | tmp_tag;
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = M32TAG(src);
    tag_t dst1_tag[] = R32TAG(GPR_EDX);
    tag_t dst2_tag[] = R32TAG(GPR_EAX);

    for (size_t i = 0; i < 4; i++)
    {
        RTAG[GPR_EDX][i] = tag_combine(dst1_tag[i], tmp_tag[i]);
        RTAG[GPR_EAX][i] = tag_combine(dst2_tag[i], tmp_tag[i]);
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 64-bit
 * registers and a 64-bit memory as
 * t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is RDX, dst2 is RAX, and src is a 64-bit
 * memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_ternary_opq(thread_ctx_t *thread_ctx, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* temporary tag value */
	size_t tmp_tag = (*entry >> VIRT2BIT(src)) & VCPU_MASK64;

	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag;
	thread_ctx->vcpu.gpr[7] |= tmp_tag;
#else
	//mf: TODO double check propagation because does it currently does not consider the effects of the operation on EDX for DIV
	//mf: propagation according to dtracker
    tag_t tmp_tag[] = M64TAG(src);
    tag_t dst1_tag[] = R64TAG(GPR_EDX);
    tag_t dst2_tag[] = R64TAG(GPR_EAX);

    for (size_t i = 0; i < 8; i++)
    {
        RTAG[GPR_EDX][i] = tag_combine(dst1_tag[i], tmp_tag[i]);
        RTAG[GPR_EAX][i] = tag_combine(dst2_tag[i], tmp_tag[i]);
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] |= t[lower(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][0];
    tag_t dst_tag = RTAG[dst][1];

    RTAG[dst][1] = tag_combine(dst_tag, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] |= t[upper(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];
    tag_t dst_tag = RTAG[dst][0];

    RTAG[dst][0] = tag_combine(dst_tag, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] |= t[upper(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];
    tag_t dst_tag = RTAG[dst][1];

    RTAG[dst][1] = tag_combine(dst_tag, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] |= t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][0];
    tag_t dst_tag = RTAG[dst][0];

    RTAG[dst][0] = tag_combine(dst_tag, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit registers
 * as t[dst] |= t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & VCPU_MASK16;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R16TAG(src);
    tag_t dst_tag[] = R16TAG(dst);

    RTAG[dst][0] = tag_combine(dst_tag[0], src_tag[0]);
    RTAG[dst][1] = tag_combine(dst_tag[1], src_tag[1]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[dst] |= t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opl(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & VCPU_MASK32) | (thread_ctx->vcpu.gpr[src] & VCPU_MASK32);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R32TAG(src);
    tag_t dst_tag[] = R32TAG(dst);

    RTAG[dst][0] = tag_combine(dst_tag[0], src_tag[0]);
    RTAG[dst][1] = tag_combine(dst_tag[1], src_tag[1]);
    RTAG[dst][2] = tag_combine(dst_tag[2], src_tag[2]);
    RTAG[dst][3] = tag_combine(dst_tag[3], src_tag[3]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[dst] |= t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_binary_opq(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] |= thread_ctx->vcpu.gpr[src];
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R64TAG(src);
    tag_t dst_tag[] = R64TAG(dst);

    RTAG[dst][0] = tag_combine(dst_tag[0], src_tag[0]);
    RTAG[dst][1] = tag_combine(dst_tag[1], src_tag[1]);
    RTAG[dst][2] = tag_combine(dst_tag[2], src_tag[2]);
    RTAG[dst][3] = tag_combine(dst_tag[3], src_tag[3]);
    RTAG[dst][4] = tag_combine(dst_tag[4], src_tag[4]);
    RTAG[dst][5] = tag_combine(dst_tag[5], src_tag[5]);
    RTAG[dst][6] = tag_combine(dst_tag[6], src_tag[6]);
    RTAG[dst][7] = tag_combine(dst_tag[7], src_tag[7]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[upper(dst)] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_binary_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] |=
		((*entry >> VIRT2BIT(src)) & VCPU_MASK8) << 1;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = M8TAG(src);
    tag_t dst_tag = RTAG[dst][1];

    RTAG[dst][1] = tag_combine(src_tag, dst_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[lower(dst)] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_binary_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] |=
		(*entry >> VIRT2BIT(src)) & VCPU_MASK8;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = M8TAG(src);
    tag_t dst_tag = RTAG[dst][0];

    RTAG[dst][0] = tag_combine(src_tag, dst_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_binary_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
	VIRT2ENTRY(src, entry);
    thread_ctx->vcpu.gpr[dst] |=
		(*entry >> VIRT2BIT(src)) & VCPU_MASK16;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = M16TAG(src);
    tag_t dst_tag[] = R16TAG(dst);

    RTAG[dst][0] = tag_combine(src_tag[0], dst_tag[0]);
    RTAG[dst][1] = tag_combine(src_tag[1], dst_tag[1]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_binary_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & VCPU_MASK32) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK32);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = M32TAG(src);
    tag_t dst_tag[] = R32TAG(dst);

    RTAG[dst][0] = tag_combine(src_tag[0], dst_tag[0]);
    RTAG[dst][1] = tag_combine(src_tag[1], dst_tag[1]);
    RTAG[dst][2] = tag_combine(src_tag[2], dst_tag[2]);
    RTAG[dst][3] = tag_combine(src_tag[3], dst_tag[3]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_binary_opq(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] |=
		(*entry >> VIRT2BIT(src)) & VCPU_MASK64;
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = M64TAG(src);
    tag_t dst_tag[] = R64TAG(dst);

    RTAG[dst][0] = tag_combine(src_tag[0], dst_tag[0]);
    RTAG[dst][1] = tag_combine(src_tag[1], dst_tag[1]);
    RTAG[dst][2] = tag_combine(src_tag[2], dst_tag[2]);
    RTAG[dst][3] = tag_combine(src_tag[3], dst_tag[3]);
    RTAG[dst][4] = tag_combine(src_tag[4], dst_tag[4]);
    RTAG[dst][5] = tag_combine(src_tag[5], dst_tag[5]);
    RTAG[dst][6] = tag_combine(src_tag[6], dst_tag[6]);
    RTAG[dst][7] = tag_combine(src_tag[7], dst_tag[7]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[dst] |= t[upper(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_binary_opb_u(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
    *entry |=
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1)
		<< VIRT2BIT(dst);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];
    tag_t dst_tag = M8TAG(dst);

    tag_t res_tag = tag_combine(dst_tag, src_tag);
    tag_dir_setb(tag_dir, dst, res_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[dst] |= t[lower(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_binary_opb_l(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
    *entry |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << VIRT2BIT(dst);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][0];
    tag_t dst_tag = M8TAG(dst);

    tag_t res_tag = tag_combine(dst_tag, src_tag);
    tag_dir_setb(tag_dir, dst, res_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] |= t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_binary_opw(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
	VIRT2ENTRY(dst, entry);
    *entry |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R16TAG(src);
    tag_t dst_tag[] = M16TAG(dst);

    tag_t res_tag[] = {tag_combine(dst_tag[0], src_tag[0]), tag_combine(dst_tag[1], src_tag[1])};
    tag_dir_setb(tag_dir, dst, res_tag[0]);
    tag_dir_setb(tag_dir, dst+1, res_tag[1]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] |= t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_binary_opl(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	*entry |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R32TAG(src);
    tag_t dst_tag[] = M32TAG(dst);

    tag_t res_tag[] = {tag_combine(dst_tag[0], src_tag[0]), tag_combine(dst_tag[1], src_tag[1]),
        tag_combine(dst_tag[2], src_tag[2]), tag_combine(dst_tag[3], src_tag[3])};

    tag_dir_setb(tag_dir, dst, res_tag[0]);
    tag_dir_setb(tag_dir, dst+1, res_tag[1]);
    tag_dir_setb(tag_dir, dst+2, res_tag[2]);
    tag_dir_setb(tag_dir, dst+3, res_tag[3]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] |= t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_binary_opq(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	*entry |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK64) <<
		VIRT2BIT(dst);
#else
	//mf: TODO double check propagation, do the other bytes of dst needs to be cleared?
	//mf: propagation according to dtracker
    tag_t src_tag[] = R64TAG(src);
    tag_t dst_tag[] = M64TAG(dst);

    tag_t res_tag[] = {tag_combine(dst_tag[0], src_tag[0]), tag_combine(dst_tag[1], src_tag[1]),
        tag_combine(dst_tag[2], src_tag[2]), tag_combine(dst_tag[3], src_tag[3]),
		tag_combine(dst_tag[4], src_tag[4]), tag_combine(dst_tag[5], src_tag[5]),
		        tag_combine(dst_tag[6], src_tag[6]), tag_combine(dst_tag[7], src_tag[7])};

    tag_dir_setb(tag_dir, dst, res_tag[0]);
    tag_dir_setb(tag_dir, dst+1, res_tag[1]);
    tag_dir_setb(tag_dir, dst+2, res_tag[2]);
    tag_dir_setb(tag_dir, dst+3, res_tag[3]);
    tag_dir_setb(tag_dir, dst+4, res_tag[4]);
    tag_dir_setb(tag_dir, dst+5, res_tag[5]);
    tag_dir_setb(tag_dir, dst+6, res_tag[6]);
    tag_dir_setb(tag_dir, dst+7, res_tag[7]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of EAX, EBX, ECX, EDX
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrl4(thread_ctx_t *thread_ctx)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[4] = 0;
	thread_ctx->vcpu.gpr[5] = 0;
	thread_ctx->vcpu.gpr[6] = 0;
	thread_ctx->vcpu.gpr[7] = 0;
#else
	//mf: propagation according to us (clared upper part of register as well)
    for (size_t i = 0; i < 8; i++)
    {
        thread_ctx->vcpu.gpr[GPR_EBX][i] = tag_traits<tag_t>::cleared_val;
        thread_ctx->vcpu.gpr[GPR_EDX][i] = tag_traits<tag_t>::cleared_val;
        thread_ctx->vcpu.gpr[GPR_ECX][i] = tag_traits<tag_t>::cleared_val;
        thread_ctx->vcpu.gpr[GPR_EAX][i] = tag_traits<tag_t>::cleared_val;
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of EAX, EDX
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrl2(thread_ctx_t *thread_ctx)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[5] = 0;
	thread_ctx->vcpu.gpr[7] = 0;
#else
	//mf: propagation according to us (clared upper part of register as well)
    for (size_t i = 0; i < 8; i++)
    {
        thread_ctx->vcpu.gpr[GPR_EDX][i] = tag_traits<tag_t>::cleared_val;
        thread_ctx->vcpu.gpr[GPR_EAX][i] = tag_traits<tag_t>::cleared_val;
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of a 64-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrq(thread_ctx_t *thread_ctx, uint32_t reg)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[reg] = 0;
#else
	//mf: propagation according to us (clared upper part of register as well)
    for (size_t i = 0; i < 8; i++)
    {
        thread_ctx->vcpu.gpr[reg][i] = tag_traits<tag_t>::cleared_val;
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of a 32-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrl(thread_ctx_t *thread_ctx, uint32_t reg)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[reg] = 0;
#else
	//mf: TODO double check propagation (check if upper 32 bits need to be cleared as well)
	//mf: propagation according to us
    for (size_t i = 0; i < 4; i++)
    {
        thread_ctx->vcpu.gpr[reg][i] = tag_traits<tag_t>::cleared_val;
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of a 16-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrw(thread_ctx_t *thread_ctx, uint32_t reg)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[reg] &= ~VCPU_MASK16;
#else
	//mf: propagation according to us
    for (size_t i = 0; i < 2; i++)
    {
        thread_ctx->vcpu.gpr[reg][i] = tag_traits<tag_t>::cleared_val;
    }
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of an upper 8-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrb_u(thread_ctx_t *thread_ctx, uint32_t reg)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[reg] &= ~(VCPU_MASK8 << 1);
#else
	//mf: propagation according to us
	thread_ctx->vcpu.gpr[reg][1] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of a lower 8-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r_clrb_l(thread_ctx_t *thread_ctx, uint32_t reg)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[reg] &= ~VCPU_MASK8;
#else
	//mf: propagation according to us
	thread_ctx->vcpu.gpr[reg][0] = tag_traits<tag_t>::cleared_val;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] = t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	 thread_ctx->vcpu.gpr[dst] =
		 (thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);
#else
	 //mf: TODO double check as it is also used be CBW instruction
	 //mf: propagation according to dtracker
     tag_t src_tag = RTAG[src][0];
     RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] = t[upper(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];

    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[upper(dst)] = t[upper(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];

    RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * registers as t[lower(dst)] = t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][0];

    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[dst] = t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("xferw callback: dst %u %u src %u %u\n", dst, thread_ctx->vcpu.gpr[dst], src, thread_ctx->vcpu.gpr[src]);
#endif
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R16TAG(src);

    RTAG[dst][0] = src_tag[0];
    RTAG[dst][1] = src_tag[1];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[dst] = t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opl(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("xferl callback: dst %u %u src %u %u\n", dst, thread_ctx->vcpu.gpr[dst], src, thread_ctx->vcpu.gpr[src]);
#endif
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] =
        (thread_ctx->vcpu.gpr[src] & VCPU_MASK32);
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R32TAG(src);

    RTAG[dst][0] = src_tag[0];
    RTAG[dst][1] = src_tag[1];
    RTAG[dst][2] = src_tag[2];
    RTAG[dst][3] = src_tag[3];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * registers as t[dst] = t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_xfer_opq(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("xferq callback: dst %u %u src %u %u\n", dst, thread_ctx->vcpu.gpr[dst], src, thread_ctx->vcpu.gpr[src]);
#endif
	thread_ctx->vcpu.gpr[dst] =
		thread_ctx->vcpu.gpr[src];
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R64TAG(src);

    RTAG[dst][0] = src_tag[0];
    RTAG[dst][1] = src_tag[1];
    RTAG[dst][2] = src_tag[2];
    RTAG[dst][3] = src_tag[3];
    RTAG[dst][4] = src_tag[4];
    RTAG[dst][5] = src_tag[5];
    RTAG[dst][6] = src_tag[6];
    RTAG[dst][7] = src_tag[7];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_xfer_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(((*entry >> VIRT2BIT(src)) << 1) & (VCPU_MASK8 << 1));
#else
	//mf: propagation according to dtracker
    tag_t src_tag = M8TAG(src);

    RTAG[dst][1] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_xfer_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK8);
#else
	//mf: propagation according to dtracker
    tag_t src_tag = M8TAG(src);

    RTAG[dst][0] = src_tag;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_xfer_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		((*entry >> VIRT2BIT(src)) & VCPU_MASK16);
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M16TAG(src);

    RTAG[dst][0] = src_tag[0];
    RTAG[dst][1] = src_tag[1];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_xfer_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
#ifdef DEBUG_PRINT_TRACE
    logprintf("mem tag %x\n", ((*entry >> VIRT2BIT(src)) & VCPU_MASK32));
#endif
    /* NOTE: Automatically clear upper 32-bit */
	thread_ctx->vcpu.gpr[dst] =
		((*entry >> VIRT2BIT(src)) & VCPU_MASK32);
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M32TAG(src);

    for (size_t i = 0; i < 4; i++)
        RTAG[dst][i] = src_tag[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_xfer_opq(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
#ifdef DEBUG_PRINT_TRACE
    logprintf("m2r_xfer_opq: mem %x\n", (*entry >> VIRT2BIT(src)) & VCPU_MASK64);
#endif
    thread_ctx->vcpu.gpr[dst] =
		((*entry >> VIRT2BIT(src)) & VCPU_MASK64);
#ifdef DEBUG_PRINT_TRACE
    logprintf("m2r_xfer_opq: reg %d %x\n", dst, thread_ctx->vcpu.gpr[dst]);
#endif
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M64TAG(src);

    for (size_t i = 0; i < 8; i++)
        RTAG[dst][i] = src_tag[i];
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a n-memory locations as
 * t[dst] = t[src]; src is AL
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @count:	memory bytes
 * @eflags:	the value of the EFLAGS register
 *
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opbn(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK8)
			tagmap_setn(dst, count);
		/* the source register is clear */
		else
			tagmap_clrn(dst, count);
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK8)
			tagmap_setn(dst - count + 1, count);
		/* the source register is clear */
		else
			tagmap_clrn(dst - count + 1, count);

	}
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[GPR_EAX][0];
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

        for (size_t i = 0; i < count; i++)
        {
            tag_dir_setb(tag_dir, dst+i, src_tag);

        }
	}
	else {
		/* EFLAGS.DF = 1 */

        for (size_t i = 0; i < count; i++)
        {
            size_t dst_addr = dst - count + 1 + i;
            tag_dir_setb(tag_dir, dst_addr, src_tag);

        }
	}
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[dst] = t[upper(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opb_u(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	*entry =
		(*entry & ~(BYTE_MASK << VIRT2BIT(dst))) |
		(((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1)
		<< VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][1];

    tag_dir_setb(tag_dir, dst, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit
 * register and a memory location as
 * t[dst] = t[lower(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opb_l(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	*entry =
		(*entry & ~(BYTE_MASK << VIRT2BIT(dst))) |
		((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag = RTAG[src][0];

    tag_dir_setb(tag_dir, dst, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a n-memory locations as
 * t[dst] = t[src]; src is AX
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @count:	memory words
 * @eflags:	the value of the EFLAGS register
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opwn(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK16)
			tagmap_setn(dst, (count << 1));
		/* the source register is clear */
		else
			tagmap_clrn(dst, (count << 1));
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK16)
			tagmap_setn(dst - (count << 1) + 1, (count << 1));
		/* the source register is clear */
		else
			tagmap_clrn(dst - (count << 1) + 1, (count << 1));
	}
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R16TAG(GPR_EAX);
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

        for (size_t i = 0; i < (count << 1); i++)
        {
            tag_dir_setb(tag_dir, dst+i, src_tag[i%2]);

        }
	}
	else {
		/* EFLAGS.DF = 1 */

        for (size_t i = 0; i < (count << 1); i++)
        {
            size_t dst_addr = dst - (count << 1) + 1 + i;
            tag_dir_setb(tag_dir, dst_addr, src_tag[i%2]);

        }
	}
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location as
 * t[dst] = t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opw(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	*entry =
		(*entry & ~(WORD_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R16TAG(src);

    tag_dir_setb(tag_dir, dst, src_tag[0]);
    tag_dir_setb(tag_dir, dst+1, src_tag[1]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a n-memory locations as
 * t[dst] = t[src]; src is EAX
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 * @count:	memory double words
 * @eflags:	the value of the EFLAGS register
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opln(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK32)
			tagmap_setn(dst, (count << 2));
		/* the source register is clear */
		else
			tagmap_clrn(dst, (count << 2));
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK32)
			tagmap_setn(dst - (count << 2) + 1, (count << 2));
		/* the source register is clear */
		else
			tagmap_clrn(dst - (count << 2) + 1, (count << 2));
	}
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R32TAG(GPR_EAX);
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

        for (size_t i = 0; i < (count << 2); i++)
        {
            tag_dir_setb(tag_dir, dst+i, src_tag[i%4]);

        }
	}
	else {
		/* EFLAGS.DF = 1 */

        for (size_t i = 0; i < (count << 2); i++)
        {
            size_t dst_addr = dst - (count << 2) + 1 + i;
            tag_dir_setb(tag_dir, dst_addr, src_tag[i%4]);

        }
	}
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location as
 * t[dst] = t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opl(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
//mf: customized
#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("reg tag %x\n", thread_ctx->vcpu.gpr[src]);
#endif
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
    *entry =
		(*entry & ~(LONG_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R32TAG(src);

    for (size_t i = 0; i < 4; i++)
        tag_dir_setb(tag_dir, dst + i, src_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a n-memory locations as
 * t[dst] = t[src]; src is RAX
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 * @count:	memory quad words
 * @eflags:	the value of the EFLAGS register
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opqn(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK64)
			tagmap_setn(dst, (count << 3));
		/* the source register is clear */
		else
			tagmap_clrn(dst, (count << 3));
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK64)
			tagmap_setn(dst - (count << 3) + 1, (count << 3));
		/* the source register is clear */
		else
			tagmap_clrn(dst - (count << 3) + 1, (count << 3));
	}
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R64TAG(GPR_EAX);
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

        for (size_t i = 0; i < (count << 3); i++)
        {
            tag_dir_setb(tag_dir, dst+i, src_tag[i%8]);

        }
	}
	else {
		/* EFLAGS.DF = 1 */

        for (size_t i = 0; i < (count << 3); i++)
        {
            size_t dst_addr = dst - (count << 3) + 1 + i;
            tag_dir_setb(tag_dir, dst_addr, src_tag[i%8]);

        }
	}
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 64-bit
 * register and a memory location as
 * t[dst] = t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_xfer_opq(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("r2m_xfer_opq: reg tag %x\n", thread_ctx->vcpu.gpr[src] & VCPU_MASK64);
#endif
    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
    *entry =
		(*entry & ~(QUAD_MASK << VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK64) <<
		VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = R64TAG(src);

    for (size_t i = 0; i < 8; i++)
        tag_dir_setb(tag_dir, dst + i, src_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2m_xfer_opw(ADDRINT dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_dst, *entry_src;
    VIRT2ENTRY(src, entry_src);
    VIRT2ENTRY(dst, entry_dst);
	*entry_dst =
		(*entry_dst & ~(WORD_MASK << VIRT2BIT(dst))) |
		(((*entry_src >> VIRT2BIT(src)) & WORD_MASK) << VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M16TAG(src);

    for (size_t i = 0; i < 2; i++)
        tag_dir_setb(tag_dir, dst + i, src_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2m_xfer_opb(ADDRINT dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_dst, *entry_src;
    VIRT2ENTRY(src, entry_src);
    VIRT2ENTRY(dst, entry_dst);
	*entry_dst =
		(*entry_dst & ~(BYTE_MASK << VIRT2BIT(dst))) |
		(((*entry_src >> VIRT2BIT(src)) & BYTE_MASK) << VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag = M8TAG(src);

    tag_dir_setb(tag_dir, dst, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2m_xfer_opl(ADDRINT dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_dst, *entry_src;
    VIRT2ENTRY(src, entry_src);
    VIRT2ENTRY(dst, entry_dst);
	*entry_dst =
		(*entry_dst & ~(LONG_MASK << VIRT2BIT(dst))) |
		(((*entry_src >> VIRT2BIT(src)) & LONG_MASK) << VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M32TAG(src);

    for (size_t i = 0; i < 4; i++)
        tag_dir_setb(tag_dir, dst + i, src_tag[i]);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 64-bit
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2m_xfer_opq(ADDRINT dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_dst, *entry_src;
    VIRT2ENTRY(src, entry_src);
    VIRT2ENTRY(dst, entry_dst);
	*entry_dst =
		(*entry_dst & ~(QUAD_MASK << VIRT2BIT(dst))) |
		(((*entry_src >> VIRT2BIT(src)) & QUAD_MASK) << VIRT2BIT(dst));
#else
	//mf: propagation according to dtracker
    tag_t src_tag[] = M64TAG(src);

    for (size_t i = 0; i < 8; i++)
        tag_dir_setb(tag_dir, dst + i, src_tag[i]);
#endif
}


/*
 * bit scan instructions BSF and BSR from memory to 64 bit reg these
 * instruction only touch the least significant byte fo dst. Clean the rest
 * byte.
 *
 * @dst:    destination register
 * @src:    source memory address
 */

static void PIN_FAST_ANALYSIS_CALL
m2r_bitscan_opq(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_src;
    VIRT2ENTRY(src, entry_src);
    if (((*entry_src >> VIRT2BIT(src)) & QUAD_MASK) != 0)
        thread_ctx->vcpu.gpr[dst] = 1;
    else
        thread_ctx->vcpu.gpr[dst] = 0;
#else
	//mf: TODO implement propagation
#endif
}

/*
 * bit scan instructions BSF and BSR from memory to 32 bit reg these
 * instruction only touch the least significant byte fo dst. Clean the rest
 * byte.
 *
 * @dst:    destination register
 * @src:    source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_bitscan_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_src;
    VIRT2ENTRY(src, entry_src);
    /* NOTE: Automatically clear upper 32-bit */
    if (((*entry_src >> VIRT2BIT(src)) & LONG_MASK) != 0)
        thread_ctx->vcpu.gpr[dst] = 1;
    else
        thread_ctx->vcpu.gpr[dst] = 0;
#else
	//mf: TODO implement propagation
#endif
}

/*
 * bit scan instructions BSF and BSR from memory to 16 bit reg. these
 * instruction only touch the least significant byte fo dst. Clean the rest
 * byte.
 *
 * @dst:    destination register
 * @src:    source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_bitscan_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry_src;
    VIRT2ENTRY(src, entry_src);
    if (((*entry_src >> VIRT2BIT(src)) & WORD_MASK) != 0)
        thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | 1;
    else
        thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16);
#else
	//mf: TODO implement propagation
#endif
}

/*
 * bit scan instructions BSF and BSR from reg to reg (64bit). These instructions
 * only touch the least significant bytpe of dst. Clean the rest byte.
 *
 * @dst: destination register
 * @src: source register
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_bitscan_opq(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("regular bitscan called, dst %u src %u\n", dst, src);
#endif
    if ((thread_ctx->vcpu.gpr[src] & VCPU_MASK64) != 0) {
#ifdef DEBUG_PRINT_TRACE
        logprintf("updated to 1\n");
#endif
        thread_ctx->vcpu.gpr[dst] = 1;
    }
    else {
#ifdef DEBUG_PRINT_TRACE
        logprintf("updated to 0\n");
#endif
        thread_ctx->vcpu.gpr[dst] = 0;
    }
#else
	//mf: TODO implement propagation
#endif
}

/* bit scan instructions BSF and BSR from reg to reg (32bit). These instructions
 * only touch the least significant bytpe of dst. Clean the rest byte.
 *
 * @dst: destination register
 * @src: source register
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_bitscan_opl(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    /* NOTE: Automatically clear upper 32-bit */
    if ((thread_ctx->vcpu.gpr[src] & VCPU_MASK32) != 0)
        thread_ctx->vcpu.gpr[dst] = 1;
    else
        thread_ctx->vcpu.gpr[dst] = 0;
#else
	//mf: TODO implement propagation
#endif
}

/* bit scan instructions BSF and BSR from reg to reg (16bit). These instructions
 * only touch the least significant bytpe of dst. Clean the rest byte.
 *
 * @dst: destination register
 * @src: source register
 */
static void PIN_FAST_ANALYSIS_CALL
r2r_bitscan_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    if ((thread_ctx->vcpu.gpr[src] & VCPU_MASK16) != 0)
        thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | 1;
    else
        thread_ctx->vcpu.gpr[dst] = (thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16);
#else
	//mf: TODO implement propagation
#endif
}

/*
 * tag propagation (analysis function)
 *
 * instrumentation helper; returns the flag that
 * takes as argument -- seems lame, but it is
 * necessary for aiding conditional analysis to
 * be inlined. Typically used with INS_InsertIfCall()
 * in order to return true (i.e., allow the execution
 * of the function that has been instrumented with
 * INS_InsertThenCall()) only once
 *
 * first_iteration:	flag; indicates whether the rep-prefixed instruction is
 * 			executed for the first time or not
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
rep_predicate(BOOL first_iteration)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

	/* return the flag; typically this is true only once */
	return first_iteration;
#else
	//mf: TODO implement propagation
	return first_iteration;
#endif
}

/*
 * tag propagation (analysis function)
 *
 * restore the tag values for all the
 * 16-bit general purpose registers from
 * the memory
 *
 * NOTE: special case for POPA instruction
 *
 * @thread_ctx:	the thread context
 * @src:	the source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_restore_opw(thread_ctx_t *thread_ctx, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

	/* tagmap value */
    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	uint16_t src_val = *entry;

	/* restore DI */
	thread_ctx->vcpu.gpr[0] =
		(thread_ctx->vcpu.gpr[0] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK16);

	/* restore SI */
	thread_ctx->vcpu.gpr[1] =
		(thread_ctx->vcpu.gpr[1] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 2)) & VCPU_MASK16);

	/* restore BP */
	thread_ctx->vcpu.gpr[2] =
		(thread_ctx->vcpu.gpr[2] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK16);

	/* update the tagmap value */
	src	+= 8;
    VIRT2ENTRY(src, entry);
	src_val	= *entry;

	/* restore BX */
	thread_ctx->vcpu.gpr[4] =
		(thread_ctx->vcpu.gpr[4] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK16);

	/* restore DX */
	thread_ctx->vcpu.gpr[5] =
		(thread_ctx->vcpu.gpr[5] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 2)) & VCPU_MASK16);

	/* restore CX */
	thread_ctx->vcpu.gpr[6] =
		(thread_ctx->vcpu.gpr[6] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK16);

	/* restore AX */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 6)) & VCPU_MASK16);

#else
	//mf: TODO implement propagation
#endif

}

/*
 * tag propagation (analysis function)
 *
 * restore the tag values for all the
 * 32-bit general purpose registers from
 * the memory
 *
 * NOTE: special case for POPAD instruction
 *
 * @thread_ctx:	the thread context
 * @src:	the source memory address
 */
static void PIN_FAST_ANALYSIS_CALL
m2r_restore_opl(thread_ctx_t *thread_ctx, ADDRINT src)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(src, entry);
	/* tagmap value */
	uint16_t src_val = *entry;

	/* restore EDI */
	thread_ctx->vcpu.gpr[0] =
        (thread_ctx->vcpu.gpr[0] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK32);

	/* restore ESI */
	thread_ctx->vcpu.gpr[1] =
        (thread_ctx->vcpu.gpr[1] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32);

	/* update the tagmap value */
	src	+= 8;
    VIRT2ENTRY(src, entry);
	src_val	= *entry;

	/* restore EBP */
	thread_ctx->vcpu.gpr[2] =
        (thread_ctx->vcpu.gpr[2] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK32);

	/* update the tagmap value */
	src	+= 8;
    VIRT2ENTRY(src, entry);
	src_val	= *entry;

	/* restore EBX */
	thread_ctx->vcpu.gpr[4] =
        (thread_ctx->vcpu.gpr[4] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK32);

	/* restore EDX */
	thread_ctx->vcpu.gpr[5] =
        (thread_ctx->vcpu.gpr[5] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32);

	/* update the tagmap value */
	src	+= 8;
    VIRT2ENTRY(src, entry);
	src_val	= *entry;

	/* restore ECX */
	thread_ctx->vcpu.gpr[6] =
        (thread_ctx->vcpu.gpr[6] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK32);

	/* restore EAX */
	thread_ctx->vcpu.gpr[7] =
        (thread_ctx->vcpu.gpr[7] & ~VCPU_MASK32) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32);

#else
	//mf: TODO implement propagation
#endif
}

/*
 * tag propagation (analysis function)
 *
 * save the tag values for all the 16-bit
 * general purpose registers into the memory
 *
 * NOTE: special case for PUSHA instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	the destination memory address
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_save_opw(thread_ctx_t *thread_ctx, ADDRINT dst)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	/* save DI */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[0] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save SI */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[1] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save BP */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[2] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save SP */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[3] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save BX */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[4] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save DX */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[5] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save CX */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[6] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

    VIRT2ENTRY(dst, entry);
	/* save AX */
	*entry =
		(*entry & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[7] & VCPU_MASK16) <<
		VIRT2BIT(dst));
#else
	//mf: TODO implement propagation
#endif
}

/*
 * tag propagation (analysis function)
 *
 * save the tag values for all the 32-bit
 * general purpose registers into the memory
 *
 * NOTE: special case for PUSHAD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	the destination memory address
 */
static void PIN_FAST_ANALYSIS_CALL
r2m_save_opl(thread_ctx_t *thread_ctx, ADDRINT dst)
{
	//mf: customized
	#ifndef USE_CUSTOM_TAG

    uint16_t *entry;
    VIRT2ENTRY(dst, entry);
	/* save EDI */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[0] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save ESI */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[1] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save EBP */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[2] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save ESP */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[3] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save EBX */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[4] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save EDX */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[5] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save ECX */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[6] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

    VIRT2ENTRY(dst, entry);
	/* save EAX */
	*entry =
		(*entry & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[7] & VCPU_MASK32) <<
		VIRT2BIT(dst));
#else
	//mf: TODO implement propagation
#endif
}

/* special callback for clear memory related to sse instruction */
static
void PIN_FAST_ANALYSIS_CALL
m_clrn(ADDRINT addr, uint32_t n) {
	//mf: customized
	#ifndef USE_CUSTOM_TAG

#ifdef DEBUG_PRINT_TRACE
    logprintf("m_clrn sse instruction %lx %u\n", addr, n);
#endif
    tagmap_clrn(addr, n);
#else
	//mf: TODO implement propagation
#endif
}

/*
 * instruction inspection (instrumentation function)
 *
 * analyze every instruction and instrument it
 * for propagating the tag bits accordingly
 *
 * @ins:	the instruction to be instrumented
 */
void
ins_inspect(INS ins)
{
	/*
	 * temporaries;
	 * source, destination, base, and index registers
	 */
	REG reg_dst, reg_src, reg_base, reg_indx;

	/* use XED to decode the instruction and extract its opcode */
	xed_iclass_enum_t ins_indx = (xed_iclass_enum_t)INS_Opcode(ins);

	/* sanity check */
	if (unlikely(ins_indx <= XED_ICLASS_INVALID ||
				ins_indx >= XED_ICLASS_LAST)) {
		LOG(string(__func__) + ": unknown opcode (opcode=" +
				decstr(ins_indx) + ")\n");

		/* done */
		return;
	}

	/* analyze the instruction */
	switch (ins_indx) {
		/* adc */
		case XED_ICLASS_ADC:
		/* add */
		case XED_ICLASS_ADD:
		/* and */
		case XED_ICLASS_AND:
		/* or */
		case XED_ICLASS_OR:
		/* xor */
		case XED_ICLASS_XOR:
		/* sbb */
		case XED_ICLASS_SBB:
		/* sub */
		case XED_ICLASS_SUB:
			/*
			 * the general format of these instructions
			 * is the following: dst {op}= src, where
			 * op can be +, -, &, |, etc. We tag the
			 * destination if the source is also taged
			 * (i.e., t[dst] |= t[src])
			 */
			/* 2nd operand is immediate; do nothing */
			if (INS_OperandIsImmediate(ins, OP_1))
				break;

			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src)
							{
								/* clear */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
								(AFUNPTR)r_clrq,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG64_INDX(reg_dst),
								IARG_END);

								/* done */
								break;
							}
						/* default behavior */
						default:
							/*
							 * propagate the tag
							 * markings accordingly
							 */
							INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opq,
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE,
							thread_ctx_ptr,
							IARG_UINT32,
							REG64_INDX(reg_dst),
							IARG_UINT32,
							REG64_INDX(reg_src),
							IARG_END);
					}
                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst)) {
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src)
							{
								/* clear */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
								(AFUNPTR)r_clrl,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG32_INDX(reg_dst),
								IARG_END);

								/* done */
								break;
							}
						/* default behavior */
						default:
							/*
							 * propagate the tag
							 * markings accordingly
							 */
							INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opl,
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE,
							thread_ctx_ptr,
							IARG_UINT32,
							REG32_INDX(reg_dst),
							IARG_UINT32,
							REG32_INDX(reg_src),
							IARG_END);
					}
				}
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst)) {
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src)
							{
								/* clear */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
								(AFUNPTR)r_clrw,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG16_INDX(reg_dst),
								IARG_END);

								/* done */
								break;
							}
						/* default behavior */
						default:
						/* propagate tags accordingly */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opw,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG16_INDX(reg_dst),
								IARG_UINT32,
							REG16_INDX(reg_src),
								IARG_END);
					}
				}
				/* 8-bit operands */
				else {
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src)
							{
							/* 8-bit upper */
						if (REG_is_Upper8(reg_dst))
								/* clear */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
							(AFUNPTR)r_clrb_u,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG8_INDX(reg_dst),
								IARG_END);
							/* 8-bit lower */
						else
								/* clear */
							INS_InsertCall(ins,
								IPOINT_BEFORE,
							(AFUNPTR)r_clrb_l,
							IARG_FAST_ANALYSIS_CALL,
								IARG_REG_VALUE,
								thread_ctx_ptr,
								IARG_UINT32,
							REG8_INDX(reg_dst),
								IARG_END);

								/* done */
								break;
							}
						/* default behavior */
						default:
						/* propagate tags accordingly */
					if (REG_is_Lower8(reg_dst) &&
							REG_is_Lower8(reg_src))
						/* lower 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_binary_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if(REG_is_Upper8(reg_dst) &&
							REG_is_Upper8(reg_src))
						/* upper 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_binary_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if (REG_is_Lower8(reg_dst))
						/*
						 * destination register is a
						 * lower 8-bit register and
						 * source register is an upper
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_binary_opb_lu,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
						/*
						 * destination register is an
						 * upper 8-bit register and
						 * source register is a lower
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_binary_opb_ul,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					}
				}
			}
			/*
			 * 2nd operand is memory;
			 * we optimize for that case, since most
			 * instructions will have a register as
			 * the first operand -- leave the result
			 * into the reg and use it later
			 */
			else if (INS_OperandIsMemory(ins, OP_1)) {
				/* extract the register operand */
				reg_dst = INS_OperandReg(ins, OP_0);
                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_binary_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_binary_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_binary_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operand (upper) */
				else if (REG_is_Upper8(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_binary_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_binary_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
			}
			/* 1st operand is memory */
			else {
				/* extract the register operand */
				reg_src = INS_OperandReg(ins, OP_1);
                /* 64-bit operands */
                if (REG_is_gr64(reg_src)) {
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_binary_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_binary_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_binary_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_binary_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_binary_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
			}

			/* done */
			break;
		/* bsf */
		case XED_ICLASS_BSF:
		/* bsr */
		case XED_ICLASS_BSR:
            if (INS_IsMemoryRead(ins)) {
                reg_dst = INS_OperandReg(ins, OP_0);
                if (REG_is_gr64(reg_dst))
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)m2r_bitscan_opq,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                        IARG_MEMORYREAD_EA,
                        IARG_END);
                else if (REG_is_gr32(reg_dst))
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)m2r_bitscan_opl,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                        IARG_MEMORYREAD_EA,
                        IARG_END);
                else {
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)m2r_bitscan_opw,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG16_INDX(reg_dst),
                        IARG_MEMORYREAD_EA,
                        IARG_END);
                }
            }
            else {
                reg_dst = INS_OperandReg(ins, OP_0);
                reg_src = INS_OperandReg(ins, OP_1);
                if (REG_is_gr64(reg_dst))
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)r2r_bitscan_opq,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                        IARG_UINT32, REG64_INDX(reg_src),
                        IARG_END);
                else if (REG_is_gr32(reg_dst))
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)r2r_bitscan_opl,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                        IARG_UINT32, REG32_INDX(reg_src),
                        IARG_END);
                else
                    INS_InsertCall(ins,
                        IPOINT_BEFORE,
                        (AFUNPTR)r2r_bitscan_opw,
                        IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG16_INDX(reg_dst),
                        IARG_UINT32, REG16_INDX(reg_src),
                        IARG_END);
            }
            break;
		/* mov */
		case XED_ICLASS_MOV:
			/*
			 * the general format of these instructions
			 * is the following: dst = src. We move the
			 * tag of the source to the destination
			 * (i.e., t[dst] = t[src])
			 */
			/*
			 * 2nd operand is immediate or segment register;
			 * clear the destination
			 *
			 * NOTE: When the processor moves a segment register
			 * into a 32-bit general-purpose register, it assumes
			 * that the 16 least-significant bits of the
			 * general-purpose register are the destination or
			 * source operand. If the register is a destination
			 * operand, the resulting value in the two high-order
			 * bytes of the register is implementation dependent.
			 * For the Pentium 4, Intel Xeon, and P6 family
			 * processors, the two high-order bytes are filled with
			 * zeros; for earlier 32-bit IA-32 processors, the two
			 * high order bytes are undefined.
			 */
			if (INS_OperandIsImmediate(ins, OP_1) ||
				(INS_OperandIsReg(ins, OP_1) &&
				REG_is_seg(INS_OperandReg(ins, OP_1)))) {
				/* destination operand is a memory address */
				if (INS_OperandIsMemory(ins, OP_0)) {
					/* clear n-bytes */
					switch (INS_OperandWidth(ins, OP_0)) {
                        /* 8 bytes */
                        case MEM_QUAD_LEN:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)tagmap_clrq,
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);

                            /* done */
                            break;
						/* 4 bytes */
						case MEM_LONG_LEN:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)tagmap_clrl,
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);

							/* done */
							break;
						/* 2 bytes */
						case MEM_WORD_LEN:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)tagmap_clrw,
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);

							/* done */
							break;
						/* 1 byte */
						case MEM_BYTE_LEN:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)tagmap_clrb,
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);

							/* done */
							break;
						/* make the compiler happy */
						default:
						LOG(string(__func__) +
						": unhandled operand width (" +
						INS_Disassemble(ins) + ")\n");

							/* done */
							return;
					}
				}
				/* destination operand is a register */
				else if (INS_OperandIsReg(ins, OP_0)) {
					/* extract the operand */
					reg_dst = INS_OperandReg(ins, OP_0);

                    /* 64-bit operand */
                    if (REG_is_gr64(reg_dst)) {
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r_clrq,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
							IARG_END);
                    }
					/* 32-bit operand */
					else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r_clrl,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
							IARG_END);
					/* 16-bit operand */
					else if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r_clrw,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
							IARG_END);
					/* 8-bit operand (upper) */
					else if (REG_is_Upper8(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r_clrb_u,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
							IARG_END);
					/* 8-bit operand (lower) */
					else
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r_clrb_l,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
							IARG_END);
				}
			}
			/* both operands are registers */
			else if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operands */
				else if (REG_is_gr8(reg_dst)) {
					/* propagate tag accordingly */
					if (REG_is_Lower8(reg_dst) &&
							REG_is_Lower8(reg_src))
						/* lower 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if(REG_is_Upper8(reg_dst) &&
							REG_is_Upper8(reg_src))
						/* upper 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if (REG_is_Lower8(reg_dst))
						/*
						 * destination register is a
						 * lower 8-bit register and
						 * source register is an upper
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opb_lu,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
						/*
						 * destination register is an
						 * upper 8-bit register and
						 * source register is a lower
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opb_ul,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
			}
			/*
			 * 2nd operand is memory;
			 * we optimize for that case, since most
			 * instructions will have a register as
			 * the first operand -- leave the result
			 * into the reg and use it later
			 */
			else if (INS_OperandIsMemory(ins, OP_1)) {
				/* extract the register operand */
				reg_dst = INS_OperandReg(ins, OP_0);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operands (upper) */
				else if (REG_is_Upper8(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operands (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
			}
			/* 1st operand is memory */
			else {
				/* extract the register operand */
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					    IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					    IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					    IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operands (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				/* 8-bit operands (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
			}

			/* done */
			break;
		/* conditional movs */
		case XED_ICLASS_CMOVB:
		case XED_ICLASS_CMOVBE:
		case XED_ICLASS_CMOVL:
		case XED_ICLASS_CMOVLE:
		case XED_ICLASS_CMOVNB:
		case XED_ICLASS_CMOVNBE:
		case XED_ICLASS_CMOVNL:
		case XED_ICLASS_CMOVNLE:
		case XED_ICLASS_CMOVNO:
		case XED_ICLASS_CMOVNP:
		case XED_ICLASS_CMOVNS:
		case XED_ICLASS_CMOVNZ:
		case XED_ICLASS_CMOVO:
		case XED_ICLASS_CMOVP:
		case XED_ICLASS_CMOVS:
		case XED_ICLASS_CMOVZ:
			/*
			 * the general format of these instructions
			 * is the following: dst = src iff cond. We
			 * move the tag of the source to the destination
			 * iff the corresponding condition is met
			 * (i.e., t[dst] = t[src])
			 */
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operands */
				else
					/* propagate tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
			}
			/*
			 * 2nd operand is memory;
			 * we optimize for that case, since most
			 * instructions will have a register as
			 * the first operand -- leave the result
			 * into the reg and use it later
			 */
			else {
				/* extract the register operand */
				reg_dst = INS_OperandReg(ins, OP_0);

                /* 64-bit operands */
				if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operands */
				else
					/* propagate the tag accordingly */
					INS_InsertPredicatedCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
			}

			/* done */
			break;
		/*
		 * cbw;
		 * move the tag associated with AL to AH
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_CBW:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2r_xfer_opb_ul,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG8_INDX(REG_AH),
				IARG_UINT32, REG8_INDX(REG_AL),
				IARG_END);

			/* done */
			break;
		/*
		 * cwd;
		 * move the tag associated with AX to DX
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_CWD:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2r_xfer_opw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG16_INDX(REG_DX),
				IARG_UINT32, REG16_INDX(REG_AX),
				IARG_END);

			/* done */
			break;
		/*
		 * cwde;
		 * move the tag associated with AX to EAX
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_CWDE:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)_cwde,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_END);

			/* done */
			break;
        /*
         * cdqe;
         * move the tag associated with EAX to RAX
         *
         * NOTE: sign extension generates data that
         * are dependent to the source operand
         */

        case XED_ICLASS_CDQE:
            /* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)_cdqe,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_END);

            /* done */
            break;
		/*
		 * cdq;
		 * move the tag associated with EAX to EDX
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_CDQ:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2r_xfer_opl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG32_INDX(REG_EDX),
				IARG_UINT32, REG32_INDX(REG_EAX),
				IARG_END);

			/* done */
			break;

		/*
		 * cqo;
		 * move the tag associated with RAX to RDX
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_CQO:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2r_xfer_opq,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG64_INDX(REG_RDX),
				IARG_UINT32, REG64_INDX(REG_RAX),
				IARG_END);

			/* done */
			break;

		/*
		 * movsx;
		 *
		 * NOTE: sign extension generates data that
		 * are dependent to the source operand
		 */
		case XED_ICLASS_MOVSX:
			/*
			 * the general format of these instructions
			 * is the following: dst = src. We move the
			 * tag of the source to the destination
			 * (i.e., t[dst] = t[src]) and we extend the
			 * tag bits accordingly
			 */
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

				/* 16-bit & 8-bit operands */
				if (REG_is_gr16(reg_dst)) {
					/* upper 8-bit */
					if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movsx_r2r_opwb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movsx_r2r_opwb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
				/* 32-bit & 16-bit operands */
				else if (REG_is_gr32(reg_dst)) {
                    if (REG_is_gr16(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_oplw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                        IARG_UINT32, REG16_INDX(reg_src),
                            IARG_END);
                    /* 32-bit & 8-bit operands (upper 8-bit) */
                    else if (REG_is_Upper8(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_oplb_u,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                    /* 32-bit & 8-bit operands (lower 8-bit) */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_oplb_l,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                }
                /* 64-bit operands */
                else {
                    if (REG_is_gr16(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_opqw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                        IARG_UINT32, REG16_INDX(reg_src),
                            IARG_END);
                    /* 64-bit & 8-bit operands (upper 8-bit) */
                    else if (REG_is_Upper8(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_opqb_u,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                    /* 64-bit & 8-bit operands (lower 8-bit) */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_r2r_opqb_l,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                }
			}
			/* 2nd operand is memory */
			else {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);

				/* 16-bit & 8-bit operands */
				if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movsx_m2r_opwb,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit & 16-bit operands */
				else if (REG_is_gr32(reg_dst)) {
                    if (INS_MemoryReadSize(ins) ==
						BIT2BYTE(MEM_WORD_LEN))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_m2r_oplw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                    /* 32-bit & 8-bit operands */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_m2r_oplb,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                }
                /* 64-bit operands */
                else {
                    if (INS_MemoryReadSize(ins) ==
						BIT2BYTE(MEM_WORD_LEN))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_m2r_opqw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                    /* 64-bit & 8-bit operands */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movsx_m2r_opqb,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                }
			}

			/* done */
			break;
        /* movsxd */
        case XED_ICLASS_MOVSXD:
            if (INS_MemoryOperandCount(ins) == 0) {
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);
                INS_InsertCall(ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)_movsxd_r2r_opql,
                    IARG_FAST_ANALYSIS_CALL,
                    IARG_REG_VALUE, thread_ctx_ptr,
                IARG_UINT32, REG64_INDX(reg_dst),
                    IARG_UINT32, REG32_INDX(reg_src),
                    IARG_END);
            }
            else {
                reg_dst = INS_OperandReg(ins, OP_0);
                INS_InsertCall(ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)_movsxd_m2r_opql,
                    IARG_FAST_ANALYSIS_CALL,
                    IARG_REG_VALUE, thread_ctx_ptr,
                IARG_UINT32, REG64_INDX(reg_dst),
                    IARG_MEMORYREAD_EA,
                    IARG_END);
            }

            /* done */
            break;
		/*
		 * movzx;
		 *
		 * NOTE: zero extension always results in
		 * clearing the tags associated with the
		 * higher bytes of the destination operand
		 */
		case XED_ICLASS_MOVZX:
			/*
			 * the general format of these instructions
			 * is the following: dst = src. We move the
			 * tag of the source to the destination
			 * (i.e., t[dst] = t[src]) and we extend the
			 * tag bits accordingly
			 */
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

				/* 16-bit & 8-bit operands */
				if (REG_is_gr16(reg_dst)) {
					/* upper 8-bit */
					if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movzx_r2r_opwb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movzx_r2r_opwb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
				/* 32-bit & 16-bit operands */
				else if (REG_is_gr32(reg_dst)) {
                    if (REG_is_gr16(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_oplw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                        IARG_UINT32, REG16_INDX(reg_src),
                            IARG_END);
                    /* 32-bit & 8-bit operands (upper 8-bit) */
                    else if (REG_is_Upper8(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_oplb_u,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                    /* 32-bit & 8-bit operands (lower 8-bit) */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_oplb_l,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                }
                /* 64-bit operands */
                else {
                    if (REG_is_gr16(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_opqw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                        IARG_UINT32, REG16_INDX(reg_src),
                            IARG_END);
                    /* 64-bit & 8-bit operands (upper 8-bit) */
                    else if (REG_is_Upper8(reg_src))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_opqb_u,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                    /* 64-bit & 8-bit operands (lower 8-bit) */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_r2r_opqb_l,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_UINT32, REG8_INDX(reg_src),
                            IARG_END);
                }
			}
			/* 2nd operand is memory */
			else {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);

				/* 16-bit & 8-bit operands */
				if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_movzx_m2r_opwb,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit & 16-bit operands */
				else if (REG_is_gr32(reg_dst)) {
                    if (INS_MemoryReadSize(ins) ==
						BIT2BYTE(MEM_WORD_LEN))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_m2r_oplw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                    /* 32-bit & 8-bit operands */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_m2r_oplb,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                }
                /* 64-bit operands */
                else {
                    if (INS_MemoryReadSize(ins) ==
						BIT2BYTE(MEM_WORD_LEN))
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_m2r_opqw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                    /* 32-bit & 8-bit operands */
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)_movzx_m2r_opqb,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                        IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_MEMORYREAD_EA,
                            IARG_END);
                }
			}

			/* done */
			break;
		/* div */
		case XED_ICLASS_DIV:
		/* idiv */
		case XED_ICLASS_IDIV:
		/* mul */
		case XED_ICLASS_MUL:
			/*
			 * the general format of these brain-dead and
			 * totally corrupted instructions is the following:
			 * dst1:dst2 {*, /}= src. We tag the destination
			 * operands if the source is also taged
			 * (i.e., t[dst1]:t[dst2] |= t[src])
			 */
			/* memory operand */
			if (INS_OperandIsMemory(ins, OP_0))
				/* differentiate based on the memory size */
				switch (INS_MemoryWriteSize(ins)) {
                    /* 8 bytes */
                    case BIT2BYTE(MEM_QUAD_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opq,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);
                        /* done */
                        break;
					/* 4 bytes */
					case BIT2BYTE(MEM_LONG_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opl,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
					/* 2 bytes */
					case BIT2BYTE(MEM_WORD_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opw,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
					/* 1 byte */
					case BIT2BYTE(MEM_BYTE_LEN):
					default:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opb,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
				}
			/* register operand */
			else {
				/* extract the operand */
				reg_src = INS_OperandReg(ins, OP_0);
                /* 64-bit operand */
                if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operand */
				else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operand */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
			}

			/* done */
			break;
		/*
		 * imul;
		 * I'm still wondering how brain-damaged the
		 * ISA architect should be in order to come
		 * up with something so ugly as the IMUL
		 * instruction
		 */
		case XED_ICLASS_IMUL:
			/* one-operand form */
			if (INS_OperandIsImplicit(ins, OP_1)) {
				/* memory operand */
				if (INS_OperandIsMemory(ins, OP_0))
				/* differentiate based on the memory size */
				switch (INS_MemoryWriteSize(ins)) {
                    case BIT2BYTE(MEM_QUAD_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opq,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;

					/* 4 bytes */
					case BIT2BYTE(MEM_LONG_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opl,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
					/* 2 bytes */
					case BIT2BYTE(MEM_WORD_LEN):
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opw,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
					/* 1 byte */
					case BIT2BYTE(MEM_BYTE_LEN):
					default:
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)m2r_ternary_opb,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
							IARG_MEMORYREAD_EA,
							IARG_END);

						/* done */
						break;
				}
			/* register operand */
			else {
				/* extract the operand */
				reg_src = INS_OperandReg(ins, OP_0);

                /* 64-bit operand */
				if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operand */
				else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operand */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_ternary_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
			}
			/* two/three-operands form
             * if it is a three-operand imul, the third must be an immediate.
             */
			else {
				/* 2nd operand is immediate; do nothing */
				if (INS_OperandIsImmediate(ins, OP_1))
					break;

				/* both operands are registers */
				if (INS_MemoryOperandCount(ins) == 0) {
					/* extract the operands */
					reg_dst = INS_OperandReg(ins, OP_0);
					reg_src = INS_OperandReg(ins, OP_1);

                    /* 64-bit operands */
                    if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opq,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
							IARG_END);
					/* 32-bit operands */
					else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opl,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
							IARG_END);
					/* 16-bit operands */
					else
					/* propagate tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)r2r_binary_opw,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
							IARG_END);
				}
				/*
				 * 2nd operand is memory;
				 * we optimize for that case, since most
				 * instructions will have a register as
				 * the first operand -- leave the result
				 * into the reg and use it later
				 */
				else {
					/* extract the register operand */
					reg_dst = INS_OperandReg(ins, OP_0);

                    /* 64-bit operands */
                    if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)m2r_binary_opq,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
							IARG_MEMORYREAD_EA,
							IARG_END);
					/* 32-bit operands */
					else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)m2r_binary_opl,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
							IARG_MEMORYREAD_EA,
							IARG_END);
					/* 16-bit operands */
					else
					/* propagate the tag accordingly */
						INS_InsertCall(ins,
							IPOINT_BEFORE,
							(AFUNPTR)m2r_binary_opw,
							IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
							IARG_MEMORYREAD_EA,
							IARG_END);
				}
			}

			/* done */
			break;
		/* conditional sets */
		case XED_ICLASS_SETB:
		case XED_ICLASS_SETBE:
		case XED_ICLASS_SETL:
		case XED_ICLASS_SETLE:
		case XED_ICLASS_SETNB:
		case XED_ICLASS_SETNBE:
		case XED_ICLASS_SETNL:
		case XED_ICLASS_SETNLE:
		case XED_ICLASS_SETNO:
		case XED_ICLASS_SETNP:
		case XED_ICLASS_SETNS:
		case XED_ICLASS_SETNZ:
		case XED_ICLASS_SETO:
		case XED_ICLASS_SETP:
		case XED_ICLASS_SETS:
		case XED_ICLASS_SETZ:
			/*
			 * clear the tag information associated with the
			 * destination operand
			 */
			/* register operand */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operand */
				reg_dst = INS_OperandReg(ins, OP_0);

				/* 8-bit operand (upper) */
				if (REG_is_Upper8(reg_dst))
					/* propagate tag accordingly */
					INS_InsertPredicatedCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)r_clrb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate tag accordingly */
					INS_InsertPredicatedCall(ins,
							IPOINT_BEFORE,
						(AFUNPTR)r_clrb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_END);
			}
			/* memory operand */
			else
				/* propagate the tag accordingly */
				INS_InsertPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)tagmap_clrb,
					IARG_FAST_ANALYSIS_CALL,
					IARG_MEMORYWRITE_EA,
					IARG_END);

			/* done */
			break;
		/*
		 * stmxcsr;
		 * clear the destination operand (register only)
		 */
		case XED_ICLASS_STMXCSR:
			/* propagate tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)tagmap_clrl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* smsw */
		case XED_ICLASS_SMSW:
		/* str */
		case XED_ICLASS_STR:
			/*
			 * clear the tag information associated with
			 * the destination operand
			 */
			/* register operand */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operand */
				reg_dst = INS_OperandReg(ins, OP_0);

				/* 16-bit register */
				if (REG_is_gr16(reg_dst))
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_END);
				/* 32-bit register */
				else if (REG_is_gr32(reg_dst))
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_END);
                else
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_END);

			}
			/* memory operand */
			else
				/* propagate tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)tagmap_clrw,
					IARG_FAST_ANALYSIS_CALL,
					IARG_MEMORYWRITE_EA,
					IARG_END);

			/* done */
			break;
		/*
		 * lar;
		 * clear the destination operand (register only)
		 */
		case XED_ICLASS_LAR:
			/* extract the 1st operand */
			reg_dst = INS_OperandReg(ins, OP_0);

			/* 16-bit register */
			if (REG_is_gr16(reg_dst))
				/* propagate tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r_clrw,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_END);
			/* 32-bit register */
			else if (REG_is_gr32(reg_dst))
				/* propagate tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r_clrl,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_END);
            else
                /* propagate tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r_clrq,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_END);

			/* done */
			break;
		/* rdpmc */
		case XED_ICLASS_RDPMC:
		/* rdtsc */
		case XED_ICLASS_RDTSC:
			/*
			 * clear the tag information associated with
			 * EAX and EDX
			 */
			/* propagate tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r_clrl2,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_END);

			/* done */
			break;
		/*
		 * cpuid;
		 * clear the tag information associated with
		 * EAX, EBX, ECX, and EDX
		 */
		case XED_ICLASS_CPUID:
			/* propagate tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r_clrl4,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_END);

			/* done */
			break;
		/*
		 * lahf;
		 * clear the tag information of AH
		 */
		case XED_ICLASS_LAHF:
			/* propagate tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r_clrb_u,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG8_INDX(REG_AH),
				IARG_END);

			/* done */
			break;
		/*
		 * cmpxchg;
		 * t[dst] = t[src] iff EAX/AX/AL == dst, else
		 * t[EAX/AX/AL] = t[dst] -- yes late-night coding again
		 * and I'm really tired to comment this crap...
         *
         * It it fun to read your comment in my late night coding.
         * --Fan.
		 */
		case XED_ICLASS_CMPXCHG:
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opq_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_RAX,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_REG_VALUE, reg_dst,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opq_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);

                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opl_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_EAX,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_REG_VALUE, reg_dst,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opl_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				}
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opw_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_AX,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_REG_VALUE, reg_dst,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2r_opw_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				}
				/* 8-bit operands */
				else
				LOG(string(__func__) +
					": unhandled opcode (opcode=" +
					decstr(ins_indx) + ")\n");
			}
			/* 1st operand is memory */
			else {
				/* extract the operand */
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_src)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_m2r_opq_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_RAX,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2m_opq_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
                }
				/* 32-bit operands */
                else if (REG_is_gr32(reg_src)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_m2r_opl_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_EAX,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2m_opl_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				}
				/* 16-bit operands */
				else if (REG_is_gr16(reg_src)) {
				/* propagate tag accordingly; fast path */
					INS_InsertIfCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_m2r_opw_fast,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_REG_VALUE, REG_AX,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* propagate tag accordingly; slow path */
					INS_InsertThenCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_cmpxchg_r2m_opw_slow,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				}
				/* 8-bit operands */
				else
				LOG(string(__func__) +
					": unhandled opcode (opcode=" +
					decstr(ins_indx) + ")\n");
			}

			/* done */
			break;
		/*
		 * xchg;
		 * exchange the tag information of the two operands
		 */
        case XED_ICLASS_XCHG:
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
                }
				/* 32-bit operands */
                else if (REG_is_gr32(reg_dst)) {
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				}
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operands */
				else if (REG_is_gr8(reg_dst)) {
					/* propagate tag accordingly */
					if (REG_is_Lower8(reg_dst) &&
						REG_is_Lower8(reg_src))
						/* lower 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if(REG_is_Upper8(reg_dst) &&
						REG_is_Upper8(reg_src))
						/* upper 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if (REG_is_Lower8(reg_dst))
						/*
						 * destination register is a
						 * lower 8-bit register and
						 * source register is an upper
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opb_lu,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
						/*
						 * destination register is an
						 * upper 8-bit register and
						 * source register is a lower
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_r2r_opb_ul,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
			}
			/*
			 * 2nd operand is memory;
			 * we optimize for that case, since most
			 * instructions will have a register as
			 * the first operand -- leave the result
			 * into the reg and use it later
			 */
			else if (INS_OperandIsMemory(ins, OP_1)) {
				/* extract the register operand */
				reg_dst = INS_OperandReg(ins, OP_0);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operands (upper) */
				else if (REG_is_Upper8(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 8-bit operands (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
			}
			/* 1st operand is memory */
			else {
				/* extract the register operand */
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
				if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
                /* 32-bit operands */
				else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 8-bit operands (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG8_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 8-bit operands (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xchg_m2r_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG8_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
			}

			/* done */
			break;
		/*
		 * xadd;
		 * xchg + add. We instrument this instruction  using the tag
		 * logic of xchg and add (see above)
		 */
        case XED_ICLASS_XADD:
			/* both operands are registers */
			if (INS_MemoryOperandCount(ins) == 0) {
				/* extract the operands */
				reg_dst = INS_OperandReg(ins, OP_0);
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst)) {
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				}
				/* 16-bit operands */
				else if (REG_is_gr16(reg_dst))
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
				/* 8-bit operands */
				else if (REG_is_gr8(reg_dst)) {
					/* propagate tag accordingly */
					if (REG_is_Lower8(reg_dst) &&
						REG_is_Lower8(reg_src))
						/* lower 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if(REG_is_Upper8(reg_dst) &&
						REG_is_Upper8(reg_src))
						/* upper 8-bit registers */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else if (REG_is_Lower8(reg_dst))
						/*
						 * destination register is a
						 * lower 8-bit register and
						 * source register is an upper
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opb_lu,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
					else
						/*
						 * destination register is an
						 * upper 8-bit register and
						 * source register is a lower
						 * 8-bit register
						 */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2r_opb_ul,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_dst),
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_END);
				}
			}
			/* 1st operand is memory */
			else {
				/* extract the register operand */
				reg_src = INS_OperandReg(ins, OP_1);

                /* 64-bit operands */
                if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
                	//mf: changed
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2m_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 32-bit operands */
                else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2m_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 16-bit operands */
				else if (REG_is_gr16(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2m_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 8-bit operand (upper) */
				else if (REG_is_Upper8(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2m_opb_u,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
				/* 8-bit operand (lower) */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_xadd_r2m_opb_l,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32, REG8_INDX(reg_src),
						IARG_MEMORYWRITE_EA,
						IARG_END);
			}

			/* done */
			break;
		/* xlat; similar to a mov between a memory location and AL */
		case XED_ICLASS_XLAT:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_xfer_opb_l,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG8_INDX(REG_AL),
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* lodsb; similar to a mov between a memory location and AL */
		case XED_ICLASS_LODSB:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_xfer_opb_l,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG8_INDX(REG_AL),
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* lodsw; similar to a mov between a memory location and AX */
		case XED_ICLASS_LODSW:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_xfer_opw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG16_INDX(REG_AX),
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* lodsd; similar to a mov between a memory location and EAX */
		case XED_ICLASS_LODSD:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_xfer_opl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG32_INDX(REG_EAX),
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
        /* lodsq; similar to a mov between a memory location and RAX */
        case XED_ICLASS_LODSQ:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_xfer_opq,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG64_INDX(REG_RAX),
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;


		/*
		 * stosb;
		 * the opposite of lodsb; however, since the instruction can
		 * also be prefixed with 'rep', the analysis code moves the
		 * tag information, accordingly, only once (i.e., before the
		 * first repetition) -- typically this will not lead in
		 * inlined code
		 */
		case XED_ICLASS_STOSB:
			/* the instruction is rep prefixed */
			if (INS_RepPrefix(ins)) {
				/* propagate the tag accordingly */
				INS_InsertIfPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)rep_predicate,
					IARG_FAST_ANALYSIS_CALL,
					IARG_FIRST_REP_ITERATION,
					IARG_END);
				INS_InsertThenPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opbn,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
				IARG_REG_VALUE, INS_RepCountRegister(ins),
				IARG_REG_VALUE, INS_OperandReg(ins, OP_4),
					IARG_END);
			}
			/* no rep prefix */
			else
				/* the instruction is not rep prefixed */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opb_l,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG8_INDX(REG_AL),
					IARG_END);

			/* done */
			break;
		/*
		 * stosw;
		 * the opposite of lodsw; however, since the instruction can
		 * also be prefixed with 'rep', the analysis code moves the
		 * tag information, accordingly, only once (i.e., before the
		 * first repetition) -- typically this will not lead in
		 * inlined code
		 */
		case XED_ICLASS_STOSW:
			/* the instruction is rep prefixed */
			if (INS_RepPrefix(ins)) {
				/* propagate the tag accordingly */
				INS_InsertIfPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)rep_predicate,
					IARG_FAST_ANALYSIS_CALL,
					IARG_FIRST_REP_ITERATION,
					IARG_END);
				INS_InsertThenPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opwn,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
				IARG_REG_VALUE, INS_RepCountRegister(ins),
				IARG_REG_VALUE, INS_OperandReg(ins, OP_4),
					IARG_END);
			}
			/* no rep prefix */
			else
				/* the instruction is not rep prefixed */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opw,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG16_INDX(REG_AX),
					IARG_END);

			/* done */
			break;
		/*
		 * stosd;
		 * the opposite of lodsd; however, since the instruction can
		 * also be prefixed with 'rep', the analysis code moves the
		 * tag information, accordingly, only once (i.e., before the
		 * first repetition) -- typically this will not lead in
		 * inlined code
		 */
		case XED_ICLASS_STOSD:
			/* the instruction is rep prefixed */
			if (INS_RepPrefix(ins)) {
				/* propagate the tag accordingly */
				INS_InsertIfPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)rep_predicate,
					IARG_FAST_ANALYSIS_CALL,
					IARG_FIRST_REP_ITERATION,
					IARG_END);
				INS_InsertThenPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opln,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
				IARG_REG_VALUE, INS_RepCountRegister(ins),
				IARG_REG_VALUE, INS_OperandReg(ins, OP_4),
					IARG_END);
			}
			/* no rep prefix */
			else
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opl,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG32_INDX(REG_EAX),
					IARG_END);

			/* done */
			break;
		/*
		 * stosq;
		 * the opposite of lodsd; however, since the instruction can
		 * also be prefixed with 'rep', the analysis code moves the
		 * tag information, accordingly, only once (i.e., before the
		 * first repetition) -- typically this will not lead in
		 * inlined code
		 */
		case XED_ICLASS_STOSQ:
			/* the instruction is rep prefixed */
			if (INS_RepPrefix(ins)) {
				/* propagate the tag accordingly */
				INS_InsertIfPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)rep_predicate,
					IARG_FAST_ANALYSIS_CALL,
					IARG_FIRST_REP_ITERATION,
					IARG_END);
				INS_InsertThenPredicatedCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opqn,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
				IARG_REG_VALUE, INS_RepCountRegister(ins),
				IARG_REG_VALUE, INS_OperandReg(ins, OP_4),
					IARG_END);
			}
			/* no rep prefix */
			else
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2m_xfer_opq,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG64_INDX(REG_RAX),
					IARG_END);

			/* done */
			break;
		/* movsq */
		case XED_ICLASS_MOVSQ:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2m_xfer_opq,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* movsd */
		case XED_ICLASS_MOVSD:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2m_xfer_opl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* movsw */
		case XED_ICLASS_MOVSW:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2m_xfer_opw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* movsb */
		case XED_ICLASS_MOVSB:
			/* propagate the tag accordingly */
			INS_InsertPredicatedCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2m_xfer_opb,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* sal */
		case XED_ICLASS_SALC:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r_clrb_l,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_UINT32, REG8_INDX(REG_AL),
				IARG_END);

			/* done */
			break;
		/* TODO: shifts are not handled (yet) */
		/* rcl */
		case XED_ICLASS_RCL:
		/* rcr */
		case XED_ICLASS_RCR:
		/* rol */
		case XED_ICLASS_ROL:
		/* ror */
		case XED_ICLASS_ROR:
		/* sal/shl */
		case XED_ICLASS_SHL:
		/* sar */
		case XED_ICLASS_SAR:
		/* shr */
		case XED_ICLASS_SHR:
		/* shld */
		case XED_ICLASS_SHLD:
		case XED_ICLASS_SHRD:

			/* done */
			break;
		/* pop; mov equivalent (see above) */
		case XED_ICLASS_POP:
			/* register operand */
			if (INS_OperandIsReg(ins, OP_0)) {
				/* extract the operand */
				reg_dst = INS_OperandReg(ins, OP_0);

                /* 64-bit operand */
				if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
                /* 32-bit operand */
                else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operand */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
						IARG_MEMORYREAD_EA,
						IARG_END);
			}
			/* memory operand */
			else if (INS_OperandIsMemory(ins, OP_0)) {
                /* 64-bit operand */
                if (INS_MemoryWriteSize(ins) == BIT2BYTE(MEM_QUAD_LEN))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit operand */
                else if (INS_MemoryWriteSize(ins) ==
						BIT2BYTE(MEM_LONG_LEN))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operand */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
			}

			/* done */
			break;
		/* push; mov equivalent (see above) */
		case XED_ICLASS_PUSH:
			/* register operand */
			if (INS_OperandIsReg(ins, OP_0)) {
				/* extract the operand */
				reg_src = INS_OperandReg(ins, OP_0);

                /* 64-bit operand */
                if (REG_is_gr64(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG64_INDX(reg_src),
						IARG_END);
				/* 32-bit operand */
                else if (REG_is_gr32(reg_src))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG32_INDX(reg_src),
						IARG_END);
				/* 16-bit operand */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2m_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_MEMORYWRITE_EA,
					IARG_UINT32, REG16_INDX(reg_src),
						IARG_END);
			}
			/* memory operand */
			else if (INS_OperandIsMemory(ins, OP_0)) {
                /* 64-bit operand */
                if (INS_MemoryWriteSize(ins) == BIT2BYTE(MEM_QUAD_LEN))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 32-bit operand */
                else if (INS_MemoryWriteSize(ins) ==
						BIT2BYTE(MEM_LONG_LEN))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
				/* 16-bit operand */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)m2m_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYREAD_EA,
						IARG_END);
			}
			/* immediate or segment operand; clean */
			else {
				/* clear n-bytes */
				switch (INS_OperandWidth(ins, OP_0)) {
                    /* 8 bytes */
                    case MEM_QUAD_LEN:
				/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)tagmap_clrq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_END);

                        break;
					/* 4 bytes */
					case MEM_LONG_LEN:
				/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)tagmap_clrl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_END);

						/* done */
						break;
					/* 2 bytes */
					case MEM_WORD_LEN:
				/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)tagmap_clrw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_END);

						/* done */
						break;
					/* 1 byte */
					case MEM_BYTE_LEN:
				/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)tagmap_clrb,
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_END);

						/* done */
						break;
					/* make the compiler happy */
					default:
						/* done */
						break;
				}
			}

			/* done */
			break;
		/* popa;
		 * similar to pop but for all the 16-bit
		 * general purpose registers
		 */
		case XED_ICLASS_POPA:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_restore_opw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* popad;
		 * similar to pop but for all the 32-bit
		 * general purpose registers
		 */
		case XED_ICLASS_POPAD:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)m2r_restore_opl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_MEMORYREAD_EA,
				IARG_END);

			/* done */
			break;
		/* pusha;
		 * similar to push but for all the 16-bit
		 * general purpose registers
		 */
		case XED_ICLASS_PUSHA:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2m_save_opw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* pushad;
		 * similar to push but for all the 32-bit
		 * general purpose registers
		 */
		case XED_ICLASS_PUSHAD:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)r2m_save_opl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, thread_ctx_ptr,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* pushf; clear a memory word (i.e., 16-bits) */
		case XED_ICLASS_PUSHF:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)tagmap_clrw,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* pushfd; clear a double memory word (i.e., 32-bits) */
		case XED_ICLASS_PUSHFD:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)tagmap_clrl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* pushfq; clear a quad memory word (i.e., 64-bits) */
		case XED_ICLASS_PUSHFQ:
			/* propagate the tag accordingly */
			INS_InsertCall(ins,
				IPOINT_BEFORE,
				(AFUNPTR)tagmap_clrq,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA,
				IARG_END);

			/* done */
			break;
		/* call (near); similar to push (see above) */
		case XED_ICLASS_CALL_NEAR:
            if (INS_MemoryWriteSize(ins) == BIT2BYTE(MEM_QUAD_LEN))
                /* propagate the tag accordingly */
                INS_InsertCall(ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)tagmap_clrq,
                    IARG_FAST_ANALYSIS_CALL,
                    IARG_MEMORYWRITE_EA,
                    IARG_END);
            else if (INS_MemoryWriteSize(ins) == BIT2BYTE(MEM_LONG_LEN))
                /* propagate the tag accordingly */
                INS_InsertCall(ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)tagmap_clrl,
                    IARG_FAST_ANALYSIS_CALL,
                    IARG_MEMORYWRITE_EA,
                    IARG_END);
            else
                /* propagate the tag accordingly */
                INS_InsertCall(ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)tagmap_clrw,
                    IARG_FAST_ANALYSIS_CALL,
                    IARG_MEMORYWRITE_EA,
                    IARG_END);
			/* done */
			break;
		/*
		 * leave;
		 * similar to a mov between ESP/SP and EBP/BP, and a pop
		 */
		case XED_ICLASS_LEAVE:
			/* extract the operands */
			reg_dst = INS_OperandReg(ins, OP_3);
			reg_src = INS_OperandReg(ins, OP_2);

            /* 64-bit operands */
            if (REG_is_gr64(reg_dst)) {
				/* propagate the tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2r_xfer_opq,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_src),
					IARG_END);
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)m2r_xfer_opq,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_src),
					IARG_MEMORYREAD_EA,
					IARG_END);
            }
			/* 32-bit operands */
			if (REG_is_gr32(reg_dst)) {
				/* propagate the tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2r_xfer_opl,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_src),
					IARG_END);
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)m2r_xfer_opl,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_src),
					IARG_MEMORYREAD_EA,
					IARG_END);
			}
			/* 16-bit operands */
			else {
				/* propagate the tag accordingly */
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)r2r_xfer_opw,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_src),
					IARG_END);
				INS_InsertCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)m2r_xfer_opw,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_src),
					IARG_MEMORYREAD_EA,
					IARG_END);
			}

			/* done */
			break;
		/* lea */
		case XED_ICLASS_LEA:
			/*
			 * the general format of this instruction
			 * is the following: dst = src_base | src_indx.
			 * We move the tags of the source base and index
			 * registers to the destination
			 * (i.e., t[dst] = t[src_base] | t[src_indx])
			 */

			/* extract the operands */
			reg_base	= INS_MemoryBaseReg(ins);
			reg_indx	= INS_MemoryIndexReg(ins);
			reg_dst		= INS_OperandReg(ins, OP_0);

			/* no base or index register; clear the destination */
			if (reg_base == REG_INVALID() &&
					reg_indx == REG_INVALID()) {
                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* clear */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32,
						REG64_INDX(reg_dst),
						IARG_END);
				/* 32-bit operands */
                else if (REG_is_gr32(reg_dst))
					/* clear */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32,
						REG32_INDX(reg_dst),
						IARG_END);
				/* 16-bit operands */
				else
					/* clear */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r_clrw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
						IARG_UINT32,
						REG16_INDX(reg_dst),
						IARG_END);
			}
			/* base register exists; no index register */
			if (reg_base != REG_INVALID() &&
					reg_indx == REG_INVALID()) {
                /* 64-bit operands */
                if (REG_is_gr64(reg_dst)) {
                    if (reg_base == REG_RIP)
                        /* if base is rip */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r_clrq,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_END);
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r2r_xfer_opq,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG64_INDX(reg_dst),
                            IARG_UINT32, REG64_INDX(reg_base),
                            IARG_END);
                }
				/* 32-bit operands */
				else if (REG_is_gr32(reg_dst)) {
					if (reg_base == REG_EIP)
                        /* if base is eip */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r_clrl,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_END);
                    else
                        /* propagate the tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r2r_xfer_opl,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG32_INDX(reg_dst),
                            IARG_UINT32, REG32_INDX(reg_base),
                            IARG_END);
                }
				/* 16-bit operands */
				else {
					if (reg_base == REG_IP)
                        /* if base is ip */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r_clrw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG16_INDX(reg_dst),
                            IARG_END);
                    else
                        /* propagate tag accordingly */
                        INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)r2r_xfer_opw,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_REG_VALUE, thread_ctx_ptr,
                            IARG_UINT32, REG16_INDX(reg_dst),
                            IARG_UINT32, REG16_INDX(reg_base),
                            IARG_END);
                }
			}
			/* index register exists; no base register */
			if (reg_base == REG_INVALID() &&
					reg_indx != REG_INVALID()) {
                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_indx),
						IARG_END);
				/* 32-bit operands */
                else if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_indx),
						IARG_END);
				/* 16-bit operands */
				else
					/* propagate tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)r2r_xfer_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_indx),
						IARG_END);
			}
			/* base and index registers exist */
			if (reg_base != REG_INVALID() &&
					reg_indx != REG_INVALID()) {
                /* 64-bit operands */
                if (REG_is_gr64(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_lea_r2r_opq,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG64_INDX(reg_dst),
					IARG_UINT32, REG64_INDX(reg_base),
					IARG_UINT32, REG64_INDX(reg_indx),
						IARG_END);
				/* 32-bit operands */
				if (REG_is_gr32(reg_dst))
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_lea_r2r_opl,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg_dst),
					IARG_UINT32, REG32_INDX(reg_base),
					IARG_UINT32, REG32_INDX(reg_indx),
						IARG_END);
				/* 16-bit operands */
				else
					/* propagate the tag accordingly */
					INS_InsertCall(ins,
						IPOINT_BEFORE,
						(AFUNPTR)_lea_r2r_opw,
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg_dst),
					IARG_UINT32, REG16_INDX(reg_base),
					IARG_UINT32, REG16_INDX(reg_indx),
						IARG_END);
			}

			/* done */
			break;
		/* cmpxchg */
		case XED_ICLASS_CMPXCHG8B:
		/* enter */
		case XED_ICLASS_ENTER:
			LOG(string(__func__) +
				": unhandled opcode (opcode=" +
				decstr(ins_indx) + ")\n");

			/* done */
			break;
		/*
		 * default handler
		 */
		default:
            // FIXME: Here we assume all the sse instruction will not touch tainted data,
            // this probably won't be true in some specific applications. We need to handle
            // SSE instruction one by one.
            bool is_sse = false;

            unsigned int i = 0;
            while (INS_RegR(ins, i) != 0) {
                if (REG_is_xmm(INS_RegR(ins, i)) || REG_is_ymm(INS_RegR(ins, i)) || REG_is_mm(INS_RegR(ins, i))) {
                    is_sse = true;
                    break;
                }
                i ++;
            }

            if (is_sse) {
                if (INS_IsMemoryWrite(ins)) {
                    uint32_t n = INS_MemoryWriteSize(ins);
                    //fprintf(stderr, "sse: %s size: %u\n", INS_Disassemble(ins).c_str(), n);
                    INS_InsertCall(ins,
                            IPOINT_BEFORE,
                            (AFUNPTR)m_clrn,
                            IARG_FAST_ANALYSIS_CALL,
                            IARG_MEMORYWRITE_EA,
                            IARG_UINT32, n,
                            IARG_END);
                }
                i = 0;
                while (INS_RegW(ins, i) != 0) {
                    REG wreg = INS_RegW(ins, i);
                    if (REG_is_gr(REG_FullRegName(wreg))) {
                        if (REG_is_gr64(wreg))
                            INS_InsertCall(ins,
                                    IPOINT_BEFORE,
                                    (AFUNPTR)r_clrq,
                                    IARG_FAST_ANALYSIS_CALL,
                                    IARG_REG_VALUE, thread_ctx_ptr,
                                    IARG_UINT32, REG64_INDX(wreg),
                                    IARG_END);
                        else if (REG_is_gr32(wreg))
                            INS_InsertCall(ins,
                                    IPOINT_BEFORE,
                                    (AFUNPTR)r_clrl,
                                    IARG_FAST_ANALYSIS_CALL,
                                    IARG_REG_VALUE, thread_ctx_ptr,
                                    IARG_UINT32, REG32_INDX(wreg),
                                    IARG_END);
                        else if (REG_is_gr16(wreg))
                            INS_InsertCall(ins,
                                    IPOINT_BEFORE,
                                    (AFUNPTR)r_clrw,
                                    IARG_FAST_ANALYSIS_CALL,
                                    IARG_REG_VALUE, thread_ctx_ptr,
                                    IARG_UINT32, REG16_INDX(wreg),
                                    IARG_END);
                        else if (REG_is_Upper8(wreg))
                            INS_InsertCall(ins,
                                    IPOINT_BEFORE,
                                    (AFUNPTR)r_clrb_u,
                                    IARG_FAST_ANALYSIS_CALL,
                                    IARG_REG_VALUE, thread_ctx_ptr,
                                    IARG_UINT32, REG8_INDX(wreg),
                                    IARG_END);
                        else
                            INS_InsertCall(ins,
                                    IPOINT_BEFORE,
                                    (AFUNPTR)r_clrb_l,
                                    IARG_FAST_ANALYSIS_CALL,
                                    IARG_REG_VALUE, thread_ctx_ptr,
                                    IARG_UINT32, REG8_INDX(wreg),
                                    IARG_END);
                    }
                    i++;
                }
            }
            /*unsigned int wreg_count, mwop_count;
            wreg_count = 0;
            mwop_count = 0;
            unsigned int i = 0;
            while (INS_RegW(ins, i) != 0) {
                if (REG_is_gr(REG_FullRegName(INS_RegW(ins, i))) || REG_is_xmm(INS_RegW(ins, i)) || REG_is_mm(INS_RegW(ins, i)) || REG_is_ymm(INS_RegW(ins, i)))
                    wreg_count ++;
                i++;
            }

            for (i = 0; i < INS_MemoryOperandCount(ins); i++) {
                if (INS_MemoryOperandIsWritten(ins, i))
                    mwop_count ++;
            }

            if ((wreg_count != 0 || mwop_count != 0) && (ins_indx != XED_ICLASS_INC) && (ins_indx != XED_ICLASS_RET_NEAR) && (ins_indx != XED_ICLASS_DEC)
                    && (ins_indx != XED_ICLASS_NOT) && (ins_indx != XED_ICLASS_NEG)) {
                (void)fprintf(stderr, "Unhandled %s\n",
                    INS_Disassemble(ins).c_str());
                (void)fprintf(stderr, "WRegcount %u WMemcount %u\n", wreg_count, mwop_count);
            }*/
            break;
	}

    // Now we are going to handle nasty patterns
    // that the compilers might generate which causes
    // some indirect dataflow that we cannot simply
    // ignore!
    test_jnz_bsf_pattern(ins);
}
