#pragma once

#include "pin.H"
#include "branch_pred.h"
#include <map>

/*
 * the size of a segment in bit width.
 *
 * we maintain shadow memory information by segment.
 * each segment has 64MB.
 */
#define SEG_SHIFT 26

/*
 * the bitmap size for each segment. for every byte in
 * the memory, we maintain a bit in the shadow memory.
 */
#define SEGMAP_SZ 8*1024*1024
/*
 * the size of the segment table. we need one entry for
 * each segment */
#define STAB_SZ 4*1024*1024

/*
 * in the 64bit machine, the lower 48bit can uniquely
 * distinguish an address */
#define ADDRBIT_SZ 48

#define ADDR_MASK 0x0000FFFFFFFFFFFFULL
#define SEG_MASK 0x03FFFFFFULL

#define BYTE_MASK	0x01U		/* byte mask; 1 bit */
#define WORD_MASK	0x0003U		/* word mask; 2 sequential bits */
#define LONG_MASK	0x000FU		/* long mask; 4 sequential bits */
#define QUAD_MASK	0x00FFU		/* quad mask; 8 sequential bits */
#define _3BYTE_MASK	0x0007U		/* 3 bytes mask; 3 sequential bits */
#define _5BYTE_MASK	0x001FU		/* 5 bytes mask; 5 sequential bits */
#define _6BYTE_MASK	0x003FU		/* 6 bytes mask; 6 sequential bits */
#define _7BYTE_MASK	0x007FU		/* 7 bytes mask; 7 sequential bits */

#define VIRT2ENTRY(addr, entry) \
do { \
    if (unlikely((STAB[((addr) & ADDR_MASK) >> SEG_SHIFT]) == 0)) \
        _tagmap_alloc_seg(((addr) & ADDR_MASK) >> SEG_SHIFT); \
    entry = (uint16_t*)(((uint8_t*)STAB[((addr) & ADDR_MASK) >> SEG_SHIFT]) + (((addr) & SEG_MASK) >> 3)); \
    if ((((addr) & SEG_MASK) >> 3) == 0x7FFFFF) \
        fprintf(stderr, "dangerous address, accessed %p!\n", (void*)addr); \
} \
while (0)

#define VIRT2BIT(addr) ((addr) & 0x7)

/*
 * the segment table for the file. */
extern uint16_t **STAB;

/* tagmap API */
int				tagmap_alloc(void);
void				tagmap_free(void);
void                tagmap_clean(void);
void                _tagmap_alloc_seg(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_setb(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_clrb(size_t);
size_t PIN_FAST_ANALYSIS_CALL tagmap_getb(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_setw(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_clrw(size_t);
size_t PIN_FAST_ANALYSIS_CALL tagmap_getw(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_setl(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_clrl(size_t);
size_t PIN_FAST_ANALYSIS_CALL tagmap_getl(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_setq(size_t);
void PIN_FAST_ANALYSIS_CALL tagmap_clrq(size_t);
size_t PIN_FAST_ANALYSIS_CALL tagmap_getq(size_t);
size_t				tagmap_issetn(size_t, size_t);
void				tagmap_setn(size_t, size_t);
void                tagmap_clrn(size_t, size_t);


//mf: added
void PIN_FAST_ANALYSIS_CALL tagmap_setb_with_tag(size_t, uint16_t tag);
void				tagmap_setn_with_tag(size_t, size_t, uint16_t tag);
uint16_t PIN_FAST_ANALYSIS_CALL tagmap_getb_tag(size_t);

typedef std::map<void*, size_t> MemblockListTy;

MemblockListTy get_tainted_memblock_list();
