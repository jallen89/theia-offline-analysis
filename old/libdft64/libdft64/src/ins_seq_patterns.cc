#include "pin.H"
#include "libdft_api64.h"
#include "libdft_core64.h"
#include "ins_seq_patterns.h"
#include "branch_pred.h"
#include <assert.h>
#include <string.h>

//#define DEBUG_PRINT_TRACE
#ifdef DEBUG_PRINT_TRACE
#include "debuglog.h"
#endif

extern REG thread_ctx_ptr;

typedef struct {
    ADDRINT test_nextinstp, jnz_target;
    int32_t reg_id;
    uint32_t update_value;
    REG test_reg;
} test_jnz_bsf_info_t;

static TLS_KEY test_jnz_bsf_key;

static void test_jnz_bsf_key_destruct(void* p) {
    free(p);
}

void test_jnz_bsf_pattern_init() {
    test_jnz_bsf_key = PIN_CreateThreadDataKey(test_jnz_bsf_key_destruct);
}

void test_jnz_bsf_pattern_thread_init(THREADID tid) {
    test_jnz_bsf_info_t *p;
    p = (test_jnz_bsf_info_t*)malloc(sizeof(test_jnz_bsf_info_t));
    memset(p, 0, sizeof(test_jnz_bsf_info_t));
    PIN_SetThreadData(test_jnz_bsf_key, p, tid);
}

static
void PIN_FAST_ANALYSIS_CALL
test_jnz_bsf_test_callback(ADDRINT nextinstp, ADDRINT reg) {
    test_jnz_bsf_info_t *p = (test_jnz_bsf_info_t*)PIN_GetThreadData(test_jnz_bsf_key, PIN_ThreadId());
    p->test_nextinstp = nextinstp;
    p->test_reg = (REG)reg;
}

static
void PIN_FAST_ANALYSIS_CALL
test_jnz_bsf_jnz_callback(ADDRINT instp, ADDRINT targetp) {
    test_jnz_bsf_info_t *p = (test_jnz_bsf_info_t*)PIN_GetThreadData(test_jnz_bsf_key, PIN_ThreadId());
    if (p->test_nextinstp != instp)
        p->jnz_target = 0;
    else
        p->jnz_target = targetp;
}

static
void PIN_FAST_ANALYSIS_CALL
test_jnz_bsf_bsf_callback1(ADDRINT instp, thread_ctx_t *thread_ctx, ADDRINT reg) {
//mf: customized
#ifndef USE_CUSTOM_TAG
    test_jnz_bsf_info_t *p = (test_jnz_bsf_info_t*)PIN_GetThreadData(test_jnz_bsf_key, PIN_ThreadId());
    if (unlikely(p->jnz_target == instp)) {
        REG reg1 = (REG)reg;
        if (REG_FullRegName(p->test_reg) != REG_FullRegName(reg1)) {
            p->reg_id = -1;
            return;
        }
        uint32_t mask1 = 0, mask2 = 0;
        if (REG_is_gr64(reg1)) {
            p->reg_id = REG64_INDX(reg1);
            mask1 = VCPU_MASK64;
        }
        else if (REG_is_gr32(reg1)) {
            p->reg_id = REG32_INDX(reg1);
            /* NOTE: Automaitcally clear upper 32-bit */
            mask1 = VCPU_MASK64;
        }
        else {
            assert(REG_is_gr16(reg1));
            p->reg_id = REG16_INDX(reg1);
            mask1 = VCPU_MASK16;
        }

        if (REG_is_gr64(p->test_reg))
            mask2 = VCPU_MASK64;
        else if (REG_is_gr32(p->test_reg))
            mask2 = VCPU_MASK32;
        else {
            assert(REG_is_gr16(p->test_reg));
            mask2 = VCPU_MASK16;
        }

        if ((thread_ctx->vcpu.gpr[p->reg_id] & mask2) != 0)
            p->update_value = (thread_ctx->vcpu.gpr[p->reg_id] & ~mask1) | 1;
        else
            p->update_value = (thread_ctx->vcpu.gpr[p->reg_id] & ~mask1);

#ifdef DEBUG_PRINT_TRACE
        logprintf("test_jnz_bsf matchs, reg_id %d and update taint value %u\n", p->reg_id, p->update_value);
#endif
    }
    else {
#ifdef DEBUG_PRINT_TRACE
        logprintf("test_jnz_bsf mismatchs\n");
#endif
        p->reg_id = -1;
    }
#else
        //mf: TODO implement propagation
#endif
}

static
void PIN_FAST_ANALYSIS_CALL
test_jnz_bsf_bsf_callback2(thread_ctx_t *thread_ctx) {
//mf: customized
#ifndef USE_CUSTOM_TAG
    test_jnz_bsf_info_t *p = (test_jnz_bsf_info_t*)PIN_GetThreadData(test_jnz_bsf_key, PIN_ThreadId());
    if (p->reg_id != -1) {
        thread_ctx->vcpu.gpr[p->reg_id] = p->update_value;
#ifdef DEBUG_PRINT_TRACE
        logprintf("reg %d with updated value %u\n", p->reg_id, thread_ctx->vcpu.gpr[p->reg_id]);
#endif
    }
#else
        //mf: TODO implement propagation
#endif
}

void test_jnz_bsf_pattern(INS ins) {
    xed_iclass_enum_t ins_opcode = (xed_iclass_enum_t)INS_Opcode(ins);
    if (ins_opcode == XED_ICLASS_TEST) {
        if (!INS_OperandIsReg(ins, OP_0) || !INS_OperandIsReg(ins, OP_1))
            return;
        REG reg_op0 = INS_OperandReg(ins, OP_0);
        REG reg_op1 = INS_OperandReg(ins, OP_1);
        if (reg_op0 != reg_op1)
            return;
        INS_InsertCall(ins,
            IPOINT_BEFORE,
            (AFUNPTR)test_jnz_bsf_test_callback,
            IARG_FAST_ANALYSIS_CALL,
            IARG_ADDRINT, INS_NextAddress(ins),
            IARG_ADDRINT, (ADDRINT)reg_op0,
            IARG_END);
    }
    else if (ins_opcode == XED_ICLASS_JNZ) {
        INS_InsertCall(ins,
            IPOINT_BEFORE,
            (AFUNPTR)test_jnz_bsf_jnz_callback,
            IARG_FAST_ANALYSIS_CALL,
            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);
    }
    else if (ins_opcode == XED_ICLASS_BSF) {
        if (!INS_OperandIsReg(ins, OP_0) || !INS_OperandIsReg(ins, OP_1))
            return;
        REG reg_op0 = INS_OperandReg(ins, OP_0);
        REG reg_op1 = INS_OperandReg(ins, OP_1);
        if (reg_op0 != reg_op1)
            return;
        INS_InsertCall(ins,
            IPOINT_BEFORE,
            (AFUNPTR)test_jnz_bsf_bsf_callback1,
            IARG_FAST_ANALYSIS_CALL,
            IARG_CALL_ORDER, CALL_ORDER_FIRST,
            IARG_INST_PTR,
            IARG_REG_VALUE, thread_ctx_ptr,
            IARG_ADDRINT, (ADDRINT)reg_op0,
            IARG_END);
        INS_InsertCall(ins,
            IPOINT_BEFORE,
            (AFUNPTR)test_jnz_bsf_bsf_callback2,
            IARG_FAST_ANALYSIS_CALL,
            IARG_CALL_ORDER, CALL_ORDER_LAST,
            IARG_REG_VALUE, thread_ctx_ptr,
            IARG_END);
    }
}
