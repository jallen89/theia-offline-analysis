#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "tagmap64.h"
#include "branch_pred.h"

uint16_t **STAB = NULL;

#define ALIGN_OFF_MAX 8
#define ASSERT_FAST 32

/*
 * initialize the tagmap */
int tagmap_alloc(void) {
    size_t len = STAB_SZ * sizeof(uint16_t*);
    STAB = (uint16_t**)malloc(len);
    if (unlikely(STAB == 0))
        return 1;

    // we clear all the mapping at the begin
    for (size_t i = 0; i < STAB_SZ; i++)
        STAB[i] = NULL;
    return 0;
}

void tagmap_free(void) {
    // Sometimes free() will be called without alloc()
    if (STAB == 0) return;

    for (size_t i = 0; i < STAB_SZ; i++)
        if (STAB[i] != NULL)
            (void)free(STAB[i]);
    (void)free(STAB);
    STAB = NULL;
}

void tagmap_clean(void) {
    if (STAB == 0) return;

    for (size_t i = 0; i < STAB_SZ; i++)
        if (STAB[i] != NULL) {
            (void)free(STAB[i]);
            STAB[i] = NULL;
        }
}

void _tagmap_alloc_seg(size_t idx) {
    assert(STAB[idx] == 0);

    STAB[idx] = (uint16_t*)malloc(SEGMAP_SZ);
    if (unlikely(STAB[idx] == 0))
        assert(0);

    memset(STAB[idx], 0, SEGMAP_SZ);
}

/*
 * tag a byte on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_setb(size_t addr)
{
	/* assert the bit that corresponds to the given address */
	uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
    *mapp |= (BYTE_MASK << VIRT2BIT(addr));
}

/*
 * untag a byte on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_clrb(size_t addr)
{
	/* clear the bit that corresponds to the given address */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*mapp &= ~(BYTE_MASK << VIRT2BIT(addr));
}

/*
 * get the tag value of a byte from the tagmap
 *
 * @addr:	the virtual address
 *
 * returns:	the tag value (e.g., 0, 1,...)
 */
size_t PIN_FAST_ANALYSIS_CALL
tagmap_getb(size_t addr)
{
	/* get the bit that corresponds to the address */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	return (*mapp) & (BYTE_MASK << VIRT2BIT(addr));
}

/*
 * tag a word (i.e., 2 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_setw(size_t addr)
{
	/*
	 * assert the bits that correspond to the addresses of the word
	 *
	 * NOTE: we use 16-bit words for referring to the bitmap in order
	 * to avoid checking for cases where we need to set cross-byte bits
	 * (e.g., 2 bits starting from address 0x00000007)
	 */
	uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
    *(mapp) |= WORD_MASK << VIRT2BIT(addr);
}

/*
 * untag a word (i.e., 2 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_clrw(size_t addr)
{
	/* clear the bits that correspond to the addresses of the word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*(mapp) &= ~(WORD_MASK << VIRT2BIT(addr));
}

/*
 * get the tag value of a word (i.e., 2 bytes) from the tagmap
 *
 * @addr:	the virtual address
 *
 * returns:	the tag value (e.g., 0, 1,...)
 */
size_t PIN_FAST_ANALYSIS_CALL
tagmap_getw(size_t addr)
{
	/* get the bits that correspond to the addresses of the word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	return (*mapp) & (WORD_MASK << VIRT2BIT(addr));
}

/*
 * tag a long word (i.e., 4 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_setl(size_t addr)
{
	/*
	 * assert the bits that correspond to the addresses of the long word
	 *
	 * NOTE: we use 16-bit words for referring to the bitmap in order
	 * to avoid checking for cases where we need to set cross-byte bits
	 * (e.g., 4 bits starting from address 0x00000006)
	 */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*mapp |= (LONG_MASK << VIRT2BIT(addr));
}

/*
 * untag a long word (i.e., 4 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_clrl(size_t addr)
{
	/* clear the bits that correspond to the addresses of the long word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*(mapp) &= ~(LONG_MASK << VIRT2BIT(addr));
}

/*
 * get the tag value of a long word (i.e., 4 bytes) from the tagmap
 *
 * @addr:	the virtual address
 *
 * returns:	the tag value (e.g., 0, 1,...)
 */
size_t PIN_FAST_ANALYSIS_CALL
tagmap_getl(size_t addr)
{
	/* get the bits that correspond to the addresses of the long word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	return (*mapp) & (LONG_MASK << VIRT2BIT(addr));
}

/*
 * tag a quad word (i.e., 8 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_setq(size_t addr)
{
	/*
	 * assert the bits that correspond to the addresses of the quad word
	 *
	 * NOTE: we use 16-bit words for referring to the bitmap in order
	 * to avoid checking for cases where we need to set cross-byte bits
	 * (e.g., 8 bits starting from address 0x00000002)
	 */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*mapp |= (QUAD_MASK << VIRT2BIT(addr));
}

/*
 * untag a quad word (i.e., 8 bytes) on the virtual address space
 *
 * @addr:	the virtual address
 */
void PIN_FAST_ANALYSIS_CALL
tagmap_clrq(size_t addr)
{
	/* assert the bits that correspond to the addresses of the quad word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	*(mapp) &= ~(QUAD_MASK << VIRT2BIT(addr));
}

/*
 * get the tag value of a quad word (i.e., 8 bytes) from the tagmap
 *
 * @addr:	the virtual address
 *
 * returns:	the tag value (e.g., 0, 1,...)
 */
size_t PIN_FAST_ANALYSIS_CALL
tagmap_getq(size_t addr)
{
	/* get the bits that correspond to the addresses of the quad word */
    uint16_t *mapp;
    VIRT2ENTRY(addr, mapp);
	return (*mapp) & (QUAD_MASK << VIRT2BIT(addr));
}

size_t tagmap_issetn(size_t addr, size_t num) {
	/* alignment offset */
	int alg_off;
	size_t tag;
    uint16_t *mapp;

	/* fast path for small writes (i.e., ~8 bytes) */
	if (num <= ALIGN_OFF_MAX) {
		switch (num) {
			/* tag 1 byte; similar to tagmap_setb() */
			case 1:
				tag = tagmap_getb(addr);
				break;
			/* tag 2 bytes; similar to tagmap_setw() */
			case 2:
				tag = tagmap_getw(addr);
				break;
			/* tag 3 bytes */
			case 3:
				/*
				 * check the bits that correspond to
				 * the addresses of the 3 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag = (*mapp) & (_3BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 4 bytes; similar to tagmap_setl() */
			case 4:
				tag = tagmap_getl(addr);
				break;
			/* tag 5 bytes */
			case 5:
				/*
				 * check the bits that correspond to
				 * the addresses of the 5 bytes
				 */
				VIRT2ENTRY(addr, mapp);
                tag = (*mapp) & (_5BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 6 bytes */
			case 6:
				/*
				 * check the bits that correspond to
				 * the addresses of the 6 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag = (*mapp) & (_6BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 7 bytes */
			case 7:
				/*
				 * check the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag = (*mapp) & (_7BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 8 bytes; similar to tagmap_setq() */
			case 8:
				tag = tagmap_getq(addr);
				break;
			default:
				/* nothing to do */
				tag = 0;
				break;
		}

		/* done */
		return tag;
	}

	/*
	 * estimate the address alignment offset;
	 * how many bits we need to check in
	 * order to align the address
	 */
	alg_off = ALIGN_OFF_MAX - VIRT2BIT(addr);

	/*
	 * check the appropriate number of bits
	 * in order to align the address
	 */
	switch (alg_off) {
		/* tag 1 byte; similar to tagmap_setb() */
		case 1:
			tag = tagmap_getb(addr);
			break;
		/* tag 2 bytes; similar to tagmap_setw() */
		case 2:
			tag = tagmap_getw(addr);
			break;
		/* tag 3 bytes */
		case 3:
			/*
			 * check the bits that correspond to
			 * the addresses of the 3 bytes
			 */
			VIRT2ENTRY(addr, mapp);
            tag = (*mapp) & (_3BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 4 bytes; similar to tagmap_setl() */
		case 4:
			tag = tagmap_getl(addr);
			break;
		/* tag 5 bytes */
		case 5:
			/*
			 * check the bits that correspond to
			 * the addresses of the 5 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			tag = (*mapp) & (_5BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 6 bytes */
		case 6:
			/*
			 * check the bits that correspond to
			 * the addresses of the 6 bytes
			 */
			VIRT2ENTRY(addr, mapp);
            tag = (*mapp) & (_6BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 7 bytes */
		case 7:
			/*
			 * check the bits that correspond to
			 * the addresses of the 7 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			tag = (*mapp) & (_7BYTE_MASK << VIRT2BIT(addr));
			break;
		/* the address is already aligned */
		case 8:
			/* fix the alg_offset */
			alg_off = 0;
		default:
			/* nothing to do */
			tag = 0;
			break;
	}

	if (tag)
		return tag;

	/* patch the address and bytes left */
	addr	+= alg_off;
	num	-= alg_off;

	/*
	 * fast path; check a 32 bits chunk at a time
	 * we assume that in most cases the tags are going to be clear, so we
	 * only check the tag after the loop
	 */
	for (tag = 0; num >= ASSERT_FAST; num -= ASSERT_FAST,
			addr += ASSERT_FAST) {
        VIRT2ENTRY(addr, mapp);
		tag |= *((uint32_t *)(mapp));
    }
	if (tag)
		return tag;

	/* slow path; check whatever is left */
	while (num > 0) {
		switch (num) {
			/* tag 1 byte; similar to tagmap_setb() */
			case 1:
				tag |= tagmap_getb(addr);
				num--; addr++;
				break;
			/* tag 2 bytes; similar to tagmap_setw() */
			case 2:
				tag |= tagmap_getw(addr);
				num -= 2; addr += 2;
				break;
			/* tag 3 bytes */
			case 3:
				/*
				 * check the bits that correspond to
				 * the addresses of the 3 bytes
				 */
				VIRT2ENTRY(addr, mapp);
                tag |= (*mapp) & (_3BYTE_MASK << VIRT2BIT(addr));
				num -= 3; addr += 3;
				break;
			/* tag 4 bytes; similar to tagmap_setl() */
			case 4:
				tag |= tagmap_getl(addr);
				num -= 4; addr += 4;
				break;
			/* tag 5 bytes */
			case 5:
				/*
				 * check the bits that correspond to
				 * the addresses of the 5 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag |= (*mapp) & (_5BYTE_MASK << VIRT2BIT(addr));
				num -= 5; addr += 5;
				break;
			/* tag 6 bytes */
			case 6:
				/*
				 * check the bits that correspond to
				 * the addresses of the 6 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag |= (*mapp) & (_6BYTE_MASK << VIRT2BIT(addr));
				num -= 6; addr += 6;
				break;
			/* tag 7 bytes */
			case 7:
				/*
				 * check the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				tag |= (*mapp) & (_7BYTE_MASK << VIRT2BIT(addr));
				num -= 7; addr += 7;
				break;
			/* tag 8 bytes; similar to tagmap_setq() */
			default:
				tag |= tagmap_getq(addr);
				num -= 8; addr += 8;
				break;
		}
	}

	return tag;
}

void tagmap_setn(size_t addr, size_t num) {
	/* alignment offset */
	int alg_off;
    uint16_t *mapp;

	/* fast path for small writes (i.e., ~8 bytes) */
	if (num <= ALIGN_OFF_MAX) {
		switch (num) {
			/* tag 1 byte; similar to tagmap_setb() */
			case 1:
				tagmap_setb(addr);
				break;
			/* tag 2 bytes; similar to tagmap_setw() */
			case 2:
				tagmap_setw(addr);
				break;
			/* tag 3 bytes */
			case 3:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 3 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_3BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 4 bytes; similar to tagmap_setl() */
			case 4:
				tagmap_setl(addr);
				break;
			/* tag 5 bytes */
			case 5:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 5 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_5BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 6 bytes */
			case 6:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 6 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_6BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 7 bytes */
			case 7:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_7BYTE_MASK << VIRT2BIT(addr));
				break;
			/* tag 8 bytes; similar to tagmap_setq() */
			case 8:
				tagmap_setq(addr);
				break;
			default:
				/* nothing to do */
				break;
		}

		/* done */
		return;
	}

	/*
	 * estimate the address alignment offset;
	 * how many bits we need to assert in
	 * order to align the address
	 */
	alg_off = ALIGN_OFF_MAX - VIRT2BIT(addr);

	/*
	 * assert the appropriate number of bits
	 * in order to align the address
	 */
	switch (alg_off) {
		/* tag 1 byte; similar to tagmap_setb() */
		case 1:
			tagmap_setb(addr);
			break;
		/* tag 2 bytes; similar to tagmap_setw() */
		case 2:
			tagmap_setw(addr);
			break;
		/* tag 3 bytes */
		case 3:
			/*
			 * assert the bits that correspond to
			 * the addresses of the 3 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) |= (_3BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 4 bytes; similar to tagmap_setl() */
		case 4:
			tagmap_setl(addr);
			break;
		/* tag 5 bytes */
		case 5:
			/*
			 * assert the bits that correspond to
			 * the addresses of the 5 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) |= (_5BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 6 bytes */
		case 6:
			/*
			 * assert the bits that correspond to
			 * the addresses of the 6 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) |= (_6BYTE_MASK << VIRT2BIT(addr));
			break;
		/* tag 7 bytes */
		case 7:
			/*
			 * assert the bits that correspond to
			 * the addresses of the 7 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) |= (_7BYTE_MASK << VIRT2BIT(addr));
			break;
		/* the address is already aligned */
		case 8:
			/* fix the alg_offset */
			alg_off = 0;
		default:
			/* nothing to do */
			break;
	}

	/* patch the address and bytes left */
	addr	+= alg_off;
	num	-= alg_off;

	/*
	 * fast path; assert a 32 bits chunk at a time
	 */
	for (; num >= ASSERT_FAST; num -= ASSERT_FAST, addr += ASSERT_FAST) {
        VIRT2ENTRY(addr, mapp);
		*((uint32_t *)(mapp)) = ~0x0U;
    }

	/* slow path; assert whatever is left */
	while (num > 0) {
		switch (num) {
			/* tag 1 byte; similar to tagmap_setb() */
			case 1:
				tagmap_setb(addr);
				num--; addr++;
				break;
			/* tag 2 bytes; similar to tagmap_setw() */
			case 2:
				tagmap_setw(addr);
				num -= 2; addr += 2;
				break;
			/* tag 3 bytes */
			case 3:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 3 bytes
				 */
				VIRT2ENTRY(addr, mapp);
                *(mapp) |= (_3BYTE_MASK << VIRT2BIT(addr));
				num -= 3; addr += 3;
				break;
			/* tag 4 bytes; similar to tagmap_setl() */
			case 4:
				tagmap_setl(addr);
				num -= 4; addr += 4;
				break;
			/* tag 5 bytes */
			case 5:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 5 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_5BYTE_MASK << VIRT2BIT(addr));
				num -= 5; addr += 5;
				break;
			/* tag 6 bytes */
			case 6:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 6 bytes
				 */
				VIRT2ENTRY(addr, mapp);
                *(mapp) |= (_6BYTE_MASK << VIRT2BIT(addr));
				num -= 6; addr += 6;
				break;
			/* tag 7 bytes */
			case 7:
				/*
				 * assert the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) |= (_7BYTE_MASK << VIRT2BIT(addr));
				num -= 7; addr += 7;
				break;
			/* tag 8 bytes; similar to tagmap_setq() */
			default:
				tagmap_setq(addr);
				num -= 8; addr += 8;
				break;
		}
	}
}

void
tagmap_clrn(size_t addr, size_t num) {
	/* alignment offset */
	int alg_off;
    uint16_t *mapp;

	/* fast path for small writes (i.e., ~8 bytes) */
	if (num <= ALIGN_OFF_MAX) {
		switch (num) {
			/* untag 1 byte; similar to tagmap_clrb() */
			case 1:
				tagmap_clrb(addr);
				break;
			/* untag 2 bytes; similar to tagmap_clrw() */
			case 2:
				tagmap_clrw(addr);
				break;
			/* untag 3 bytes */
			case 3:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 3 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_3BYTE_MASK << VIRT2BIT(addr));
				break;
			/* untag 4 bytes; similar to tagmap_clrl() */
			case 4:
				tagmap_clrl(addr);
				break;
			/* untag 5 bytes */
			case 5:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 5 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_5BYTE_MASK << VIRT2BIT(addr));
				break;
			/* untag 6 bytes */
			case 6:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 6 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_6BYTE_MASK << VIRT2BIT(addr));
				break;
			/* untag 7 bytes */
			case 7:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_7BYTE_MASK << VIRT2BIT(addr));
				break;
			/* untag 8 bytes; similar to tagmap_clrq() */
			case 8:
				tagmap_clrq(addr);
				break;
			default:
				/* nothing to do */
				break;
		}

		/* done */
		return;
	}

	/*
	 * estimate the address alignment offset;
	 * how many bits we need to assert in
	 * order to align the address
	 */
	alg_off = ALIGN_OFF_MAX - VIRT2BIT(addr);

	/*
	 * clear the appropriate number of bits
	 * in order to align the address
	 */
	switch (alg_off) {
		/* untag 1 byte; similar to tagmap_crlb() */
		case 1:
			tagmap_clrb(addr);
			break;
		/* untag 2 bytes; similar to tagmap_clrw() */
		case 2:
			tagmap_clrw(addr);
			break;
		/* untag 3 bytes */
		case 3:
			/*
			 * clear the bits that correspond to
			 * the addresses of the 3 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) &= ~(_3BYTE_MASK << VIRT2BIT(addr));
			break;
		/* untag 4 bytes; similar to tagmap_clrl() */
		case 4:
			tagmap_clrl(addr);
			break;
		/* untag 5 bytes */
		case 5:
			/*
			 * clear the bits that correspond to
			 * the addresses of the 5 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) &= ~(_5BYTE_MASK << VIRT2BIT(addr));
			break;
		/* untag 6 bytes */
		case 6:
			/*
			 * clear the bits that correspond to
			 * the addresses of the 6 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) &= ~(_6BYTE_MASK << VIRT2BIT(addr));
			break;
		/* untag 7 bytes */
		case 7:
			/*
			 * clear the bits that correspond to
			 * the addresses of the 7 bytes
			 */
            VIRT2ENTRY(addr, mapp);
			*(mapp) &= ~(_7BYTE_MASK << VIRT2BIT(addr));
			break;
		/* the address is already aligned */
		case 8:
			/* fix the alg_offset */
			alg_off = 0;
		default:
			/* nothing to do */
			break;
	}

	/* patch the address and bytes left */
	addr	+= alg_off;
	num	-= alg_off;

	/*
	 * fast path; clear a 32 bits chunk at a time
	 */
	for (; num >= ASSERT_FAST; num -= ASSERT_FAST, addr += ASSERT_FAST) {
        VIRT2ENTRY(addr, mapp);
		*((uint32_t *)(mapp)) = 0x0U;
    }

	/* slow path; clear whatever is left */
	while (num > 0) {
		switch (num) {
			/* untag 1 byte; similar to tagmap_clrb() */
			case 1:
				tagmap_clrb(addr);
				num--;
				break;
			/* untag 2 bytes; similar to tagmap_clrw() */
			case 2:
				tagmap_clrw(addr);
				num -= 2;
				break;
			/* untag 3 bytes */
			case 3:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 3 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_3BYTE_MASK << VIRT2BIT(addr));
				num -= 3;
				break;
			/* untag 4 bytes; similar to tagmap_clrl() */
			case 4:
				tagmap_clrl(addr);
				num -= 4;
				break;
			/* untag 5 bytes */
			case 5:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 5 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) &= ~(_5BYTE_MASK << VIRT2BIT(addr));
				num -= 5;
				break;
			/* untag 6 bytes */
			case 6:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 6 bytes
				 */
				VIRT2ENTRY(addr, mapp);
                *(mapp) &= ~(_6BYTE_MASK << VIRT2BIT(addr));
				num -= 6;
				break;
			/* untag 7 bytes */
			case 7:
				/*
				 * clear the bits that correspond to
				 * the addresses of the 7 bytes
				 */
                VIRT2ENTRY(addr, mapp);
				*(mapp) *= ~(_7BYTE_MASK << VIRT2BIT(addr));
				num -= 7;
				break;
			/* untag 8 bytes; similar to tagmap_clrq() */
			default:
				tagmap_clrq(addr);
				num -= 8;
				break;
		}
	}
}

static inline size_t ijk2address(size_t i, size_t j, size_t k) {
    return (i << SEG_SHIFT) + (j << 3) + k;
}

MemblockListTy get_tainted_memblock_list() {
    MemblockListTy ret;
    ret.clear();
    for (size_t i = 0; i < STAB_SZ; i++)
        if (STAB[i] != NULL) {
            uint8_t *p = (uint8_t*)STAB[i];
            size_t trailing_bits = 0;
            for (size_t j = 0; j < SEGMAP_SZ; j++)
                if (*(p+j) == 0) {
                    if (trailing_bits != 0)
                        ret.insert(std::make_pair((void*)(ijk2address(i, j, 0) - trailing_bits), trailing_bits));
                    trailing_bits = 0;
                }
                else if (*(p+j) == 0xff)
                    trailing_bits += 8;
                else {
                    for (size_t k = 0; k < 8; k++)
                        if (((*(p+j)) & (1 << k)) == 0) {
                            if (trailing_bits != 0)
                                ret.insert(std::make_pair((void*)(ijk2address(i, j, k) - trailing_bits), trailing_bits));
                            trailing_bits = 0;
                        }
                        else
                            trailing_bits++;
                }
            if (trailing_bits != 0)
                ret.insert(std::make_pair((void*)(ijk2address(i, SEGMAP_SZ, 0) - trailing_bits), trailing_bits));
        }
    return ret;
}
