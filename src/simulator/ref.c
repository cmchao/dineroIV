/*
 * Memory reference handling functions for Dinero IV
 * Written by Jan Edler and Mark D. Hill
 *
 * Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.
 * All rights reserved.
 * Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its associated documentation for non-commercial purposes is hereby
 * granted (for commercial purposes see below), provided that the above
 * copyright notice appears in all copies, derivative works or modified
 * versions of the software and any portions thereof, and that both the
 * copyright notice and this permission notice appear in the documentation.
 * NEC Research Institute Inc. and Mark D. Hill shall be given a copy of
 * any such derivative work or modified version of the software and NEC
 * Research Institute Inc.  and any of its affiliated companies (collectively
 * referred to as NECI) and Mark D. Hill shall be granted permission to use,
 * copy, modify, and distribute the software for internal use and research.
 * The name of NEC Research Institute Inc. and its affiliated companies
 * shall not be used in advertising or publicity related to the distribution
 * of the software, without the prior written consent of NECI.  All copies,
 * derivative works, or modified versions of the software shall be exported
 * or reexported in accordance with applicable laws and regulations relating
 * to export control.  This software is experimental.  NECI and Mark D. Hill
 * make no representations regarding the suitability of this software for
 * any purpose and neither NECI nor Mark D. Hill will support the software.
 *
 * Use of this software for commercial purposes is also possible, but only
 * if, in addition to the above requirements for non-commercial use, written
 * permission for such use is obtained by the commercial user from NECI or
 * Mark D. Hill prior to the fabrication and distribution of the software.
 *
 * THE SOFTWARE IS PROVIDED AS IS.  NECI AND MARK D. HILL DO NOT MAKE
 * ANY WARRANTEES EITHER EXPRESS OR IMPLIED WITH REGARD TO THE SOFTWARE.
 * NECI AND MARK D. HILL ALSO DISCLAIM ANY WARRANTY THAT THE SOFTWARE IS
 * FREE OF INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS OF OTHERS.
 * NO OTHER LICENSE EXPRESS OR IMPLIED IS HEREBY GRANTED.  NECI AND MARK
 * D. HILL SHALL NOT BE LIABLE FOR ANY DAMAGES, INCLUDING GENERAL, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, ARISING OUT OF THE USE OR INABILITY
 * TO USE THE SOFTWARE.
 *
 */



/*
 * This file contains the primary functions
 * for handling memory references in Dinero IV.
 *
 *	Replacement policy functions
 * 	Prefetch policy functions
 *	Write allocate and back/through policy functions
 *	Other support functions
 *	Primary user-callable function: d4ref
 */


/*
 * The first group of stuff in this file is only done once
 * if we are customizing
 */
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "d4.h"

/*
 * LRU replacement policy
 * With inlining, this is also good for direct-mapped caches
 */
inline
D4StackNode *
d4rep_lru (D4Cache *c, int stacknum, D4MemRef m, D4StackNode *ptr)
{
    if (ptr != NULL) {	/* hits */
        if (ptr != c->stack[stacknum].top) {
            d4movetotop (c, stacknum, ptr);
        }
    } else {			/* misses */
        ptr = c->stack[stacknum].top->up;
        assert (ptr->valid == 0);
        ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
        d4hash_insert (c, stacknum, ptr);
        c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
    }
    return ptr;
}


/*
 * FIFO replacement policy
 */
inline
D4StackNode *
d4rep_fifo (D4Cache *c, int stacknum, D4MemRef m, D4StackNode *ptr)
{
    if (ptr == NULL) {	/* misses */
        ptr = c->stack[stacknum].top->up;
        assert (ptr->valid == 0);
        ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
        d4hash_insert (c, stacknum, ptr);
        c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
    }
    return ptr;
}


/*
 * Random replacement policy.
 */
inline
D4StackNode *
d4rep_random (D4Cache *c, int stacknum, D4MemRef m, D4StackNode *ptr)
{
    if (ptr == NULL) {	/* misses */
        int setsize = c->stack[stacknum].n - 1;
        ptr = c->stack[stacknum].top->up;
        assert (ptr->valid == 0);
        ptr->blockaddr = D4ADDR2BLOCK (c, m.address);
        d4hash_insert (c, stacknum, ptr);
        c->stack[stacknum].top = ptr;	/* quicker than d4movetotop */
        if (ptr->up->valid != 0) {	/* set is full */
            d4movetobot (c, stacknum, d4findnth (c, stacknum, 2 + (random() % setsize)));
        }
    }
    return ptr;
}


/*
 * Demand fetch only policy, no prefetching
 */
inline
D4PendStack *
d4prefetch_none (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    /* no prefetch, nothing to do */
    return NULL;
}


/*
 * Always prefetch policy
 */
inline
D4PendStack *
d4prefetch_always (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    D4PendStack *pf;

    pf = d4get_mref();
    pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
    pf->m.accesstype = m.accesstype | D4PREFETCH;
    pf->m.size = 1 << D4VAL(c, lg2subblocksize);
    return pf;
}


/*
 * Load forward prefetch policy
 * Don't prefetch into next block.
 */
inline
D4PendStack *
d4prefetch_loadforw (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    D4PendStack *pf;

    if (D4ADDR2BLOCK(c, m.address + c->prefetch_distance) != D4ADDR2BLOCK(c, m.address)) {
        return NULL;
    }

    pf = d4get_mref();
    pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
    pf->m.accesstype = m.accesstype | D4PREFETCH;
    pf->m.size = 1 << D4VAL(c, lg2subblocksize);
    return pf;
}


/*
 * Subblock prefetch policy
 * Don't prefetch into next block; wrap around within block instead.
 */
inline
D4PendStack *
d4prefetch_subblock (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    D4PendStack *pf;

    pf = d4get_mref();
    pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
    pf->m.accesstype = m.accesstype | D4PREFETCH;
    pf->m.size = 1 << D4VAL(c, lg2subblocksize);

    if (D4ADDR2BLOCK(c, pf->m.address) != D4ADDR2BLOCK(c, m.address)) {
        pf->m.address -= 1 << D4VAL(c, lg2blocksize);
    }
    return pf;
}


/*
 * Miss prefetch policy
 * Prefetch only on misses
 */
inline
D4PendStack *
d4prefetch_miss (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    D4PendStack *pf;

    if (!miss) {
        return NULL;
    }

    pf = d4get_mref();
    pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
    pf->m.accesstype = m.accesstype | D4PREFETCH;
    pf->m.size = 1 << D4VAL(c, lg2subblocksize);
    return pf;
}


/*
 * Tagged prefetch (see Smith, "cache Memories," ~p.20) initiates
 * a prefetch on the first demand reference to a (sub)-block.  Thus,
 * a prefetch is initiated on a demand miss or the first demand
 * reference to a (sub)-block that was brought into the cache by a
 * prefetch.
 *
 * Tagged prefetching is implemented using demand reference bits
 * in the cache entry.  A prefetch is started on a demand miss and
 * on a refernce to a (sub)-block whose reference bit was not previously set.
 */
inline
D4PendStack *
d4prefetch_tagged (D4Cache *c, D4MemRef m, int miss, D4StackNode *stackptr)
{
    D4PendStack *pf;
    int sbbits;

    sbbits = D4ADDR2SBMASK(c, m);
    if (!miss && (sbbits & stackptr->referenced) != 0) {
        return NULL;
    }

    pf = d4get_mref();
    pf->m.address = D4ADDR2SUBBLOCK (c, m.address + c->prefetch_distance);
    pf->m.accesstype = m.accesstype | D4PREFETCH;
    pf->m.size = 1 << D4VAL(c, lg2subblocksize);
    return pf;
}


/*
 * Always write allocate
 */
inline
int
d4walloc_always (D4Cache *c, D4MemRef m)
{
    return 1;
}


/*
 * Never write allocate
 */
inline
int
d4walloc_never (D4Cache *c, D4MemRef m)
{
    return 0;
}


/*
 * Write allocate if no fetch is required
 * (write exactly fills an integral number of subblocks)
 */
inline
int
d4walloc_nofetch (D4Cache *c, D4MemRef m)
{
    return m.size == D4REFNSB(c, m) << D4VAL (c, lg2subblocksize);
}


/* this is for the walloc policy of an icache */
int
d4walloc_impossible (D4Cache *c, D4MemRef m)
{
    fprintf (stderr, "Dinero IV: impossible walloc policy routine called for %s!!!\n",
             c->name);
    exit (9);
    return 0;	/* can't really get here, but some compilers get upset if we don't have a return value */
}



/*
 * Always write back
 */
inline
int
d4wback_always (D4Cache *c, D4MemRef m, int setnumber, D4StackNode *ptr, int walloc)
{
    return 1;
}


/*
 * Never write back (i.e., always write through)
 */
inline
int
d4wback_never (D4Cache *c, D4MemRef m, int setnumber, D4StackNode *ptr, int walloc)
{
    return 0;
}


/*
 * Write back if no fetch is required
 * The actual test is for every affected subblock to be valid or
 * for the write to completely cover all affected subblocks.
 */
inline
int
d4wback_nofetch (D4Cache *c, D4MemRef m, int setnumber, D4StackNode *ptr, int walloc)
{
    return (D4ADDR2SBMASK(c, m) & ~ptr->valid) == 0 ||
           m.size == (D4REFNSB(c, m) << D4VAL (c, lg2subblocksize));
}


/* this is for the wback policy of an icache */
int
d4wback_impossible (D4Cache *c, D4MemRef m, int setnumber, D4StackNode *ptr, int walloc)
{
    fprintf (stderr, "Dinero IV: impossible wback policy routine called for %s!!!\n",
             c->name);
    exit (9);
    return 0;	/* can't really get here, but some compilers get upset if we don't have a return value */
}


/*
 * This function implements an infinite-sized cache, used
 * when classifying cache misses into compulsory, capacity,
 * and conflict misses.
 *
 * Return value:
 *	-1 if at least 1 affected subblock (but not the whole block)
 *		misses in the infinite cache
 *	0 if all affected subblocks hit in the infinite cache
 *	1 if the whole block misses in the infinite cache
 * Note we require that the number of subblocks per block be a
 *	divisor of D4_BITMAP_RSIZE, so blocks are not split across bitmaps
 */

static int
d4infcache (D4Cache *c, D4MemRef m)
{
    const unsigned int sbsize = 1 << D4VAL (c, lg2subblocksize);
    const d4addr sbaddr = D4ADDR2SUBBLOCK (c, m.address);
    const int nsb = D4REFNSB (c, m);
    unsigned int bitoff; /* offset of bit in bitmap */
    int hi, lo, i, b;
    static int totranges = 0, totbitmaps = 0;

    bitoff = (sbaddr & (D4_BITMAP_RSIZE - 1)) / sbsize;

    /* binary search for range containing our address */
    hi = c->nranges - 1;
    lo = 0;
    while (lo <= hi) {
        i = lo + (hi - lo) / 2;
        if (c->ranges[i].addr + D4_BITMAP_RSIZE <= sbaddr) {
            lo = i + 1;    /* need to look higher */
        } else if (c->ranges[i].addr > sbaddr) {
            hi = i - 1;    /* need to look lower */
        } else { /* found the right range */
            const int sbpb = 1 << (D4VAL (c, lg2blocksize) - D4VAL (c, lg2subblocksize));
            int nb;
            /* count affected bits we've seen */
            for (nb = 0, b = 0;  b < nsb;  b++)
                nb += ((c->ranges[i].bitmap[(bitoff + b) / CHAR_BIT] &
                        (1 << ((bitoff + b) % CHAR_BIT))) != 0);
            if (nb == nsb) {
                return 0;    /* we've seen it all before */
            }
            /* consider the whole block */
            if (sbpb != 1 && nsb != sbpb) {
                unsigned int bbitoff = (D4ADDR2BLOCK (c, m.address) &
                                        (D4_BITMAP_RSIZE - 1)) / sbsize;
                for (nb = 0, b = 0;  b < sbpb;  b++)
                    nb += ((c->ranges[i].bitmap[(bbitoff + b) / CHAR_BIT] &
                            (1 << ((bbitoff + b) % CHAR_BIT))) != 0);
            }
            /* set the bits */
            for (b = 0;  b < nsb;  b++)
                c->ranges[i].bitmap[(bitoff + b) / CHAR_BIT] |=
                    (1 << ((bitoff + b) % CHAR_BIT));
            return nb == 0 ? 1 : -1;
        }
    }
    /* lo > hi: range not found; find position and insert new range */
    if (c->nranges >= c->maxranges - 1) {
        /* ran out of range pointers; allocate some more */
        int oldmaxranges = c->maxranges;
        c->maxranges = (c->maxranges + 10) * 2;
        if (c->ranges == NULL) { /* don't trust realloc(NULL,...) */
            c->ranges = malloc (c->maxranges * sizeof(*c->ranges));
        } else {
            c->ranges = realloc (c->ranges, c->maxranges * sizeof(*c->ranges));
        }
        if (c->ranges == NULL) {
            fprintf (stderr, "DineroIV: can't allocate more "
                     "bitmap pointers for cache %s (%d so far, total %d)\n",
                     c->name, oldmaxranges, totranges);
            exit(1);
        }
        totranges++;
    }
    for (i = c->nranges++ - 1;  i >= 0;  i--) {
        if (c->ranges[i].addr < sbaddr) {
            break;
        }
        c->ranges[i + 1] = c->ranges[i];
    }
    c->ranges[i + 1].addr = sbaddr & ~(D4_BITMAP_RSIZE - 1);
    c->ranges[i + 1].bitmap = calloc ((((D4_BITMAP_RSIZE + sbsize - 1)
                                        / sbsize) + CHAR_BIT - 1) / CHAR_BIT, 1);
    if (c->ranges[i + 1].bitmap == NULL) {
        fprintf (stderr, "DineroIV: can't allocate another bitmap "
                 "(currently %d, total %d, each mapping 0x%x bytes)\n",
                 c->nranges - 1, totbitmaps, D4_BITMAP_RSIZE);
        exit(1);
    }
    totbitmaps++;
    for (b = 0;  b < nsb;  b++, bitoff++) {
        c->ranges[i + 1].bitmap[bitoff / CHAR_BIT] |= (1 << (bitoff % CHAR_BIT));
    }
    return 1; /* we've not seen it before */
}


/*
 * Split a memory reference if it crosses a block boundary.
 * The remainder, if any, is queued for processing later.
 */
inline
D4MemRef
d4_splitm (D4Cache *c, D4MemRef mr, d4addr ba)
{
    const int bsize = 1 << D4VAL (c, lg2blocksize);
    const int bmask = bsize - 1;
    int newsize;
    D4PendStack *pf;

    if (ba == D4ADDR2BLOCK (c, mr.address + mr.size - 1)) {
        return mr;
    }
    pf = d4get_mref();
    pf->m.address = ba + bsize;
    pf->m.accesstype = mr.accesstype | D4_MULTIBLOCK;
    newsize = bsize - (mr.address & bmask);
    pf->m.size = mr.size - newsize;
    pf->next = c->pending;
    c->pending = pf;
    c->multiblock++;
    mr.size = newsize;
    return mr;
}


/*
 * Handle a memory reference for the given cache.
 * The user calls this function for the cache closest to
 * the processor; other caches are handled automatically.
 */
void
d4ref (D4Cache *c, D4MemRef mr)
{
    /* special cases first */
    if ((D4VAL (c, flags) & D4F_MEM) != 0) { /* Special case for simulated memory */
        c->fetch[mr.accesstype]++;
    } else if (mr.accesstype == D4XCOPYB || mr.accesstype == D4XINVAL) {
        D4MemRef m = mr;	/* dumb compilers might de-optimize if we take addr of mr */
        if (m.accesstype == D4XCOPYB) {
            d4copyback (c, &m, 1);
        } else {
            d4invalidate (c, &m, 1);
        }
    } else {				 /* Everything else */
        const d4addr blockaddr = D4ADDR2BLOCK (c, mr.address);
        const D4MemRef m = d4_splitm (c, mr, blockaddr);
        const int atype = D4BASIC_ATYPE (m.accesstype);
        const int setnumber = D4ADDR2SET (c, m.address);
        const int ronly = 0; /* conservative */
        const int walloc = !ronly && atype == D4XWRITE && D4VAL (c, wallocf) (c, m);
        const int sbbits = D4ADDR2SBMASK (c, m);
        int miss, blockmiss, wback;
        D4StackNode *ptr;

        if ((D4VAL (c, flags) & D4F_RO) != 0 && atype == D4XWRITE) {
            fprintf (stderr, "Dinero IV: write to read-only cache %d (%s)\n",
                     c->cacheid, c->name);
            exit (9);
        }

        /*
         * Find address in the cache.
         * Quickly check for top of stack.
         */
        ptr = c->stack[setnumber].top;
        if (ptr->blockaddr == blockaddr && ptr->valid != 0)
            ; /* found it */
        else {
            ptr = d4_find (c, setnumber, blockaddr);
        }

        blockmiss = (ptr == NULL);
        miss = blockmiss || (sbbits & ptr->valid) != sbbits;

        /*
         * Prefetch on reads and instruction fetches, but not on
         * writes, misc, and prefetch references.
         * Optionally, some percentage may be thrown away.
         */
        if ((m.accesstype == D4XREAD || m.accesstype == D4XINSTRN)) {
            D4PendStack *pf = D4VAL (c, prefetchf) (c, m, miss, ptr);
            if (pf != NULL) {
                /* Note: 0 <= random() <= 2^31-1 and 0 <= random()/(INT_MAX/100) < 100. */
                if (D4VAL (c, prefetch_abortpercent) > 0 &&
                        random() / (INT_MAX / 100) < D4VAL (c, prefetch_abortpercent)) {
                    d4put_mref (pf);    /* throw it away */
                } else {
                    pf->next = c->pending;	/* add to pending list */
                    c->pending = pf;
                }
            }
        }

        /*
         * Update the cache
         * Don't do it for non-write-allocate misses
         */
        wback = 0;
        if (ronly || atype != D4XWRITE || !blockmiss || walloc) {
            /*
             * Adjust priority stack as necessary
             */
            ptr = D4VAL (c, replacementf) (c, setnumber, m, ptr);
            /*
             * Update state bits
             */
            if (blockmiss) {
                assert (ptr->valid == 0);
                ptr->referenced = 0;
                ptr->dirty = 0;
            }
            ptr->valid |= sbbits;
            if ((m.accesstype & D4PREFETCH) == 0) {
                ptr->referenced |= sbbits;
            }

            /*
             * For writes, decide if write-back or write-through.
             * Set the dirty bits if write-back is going to be used.
             */
            wback = !ronly &&
                    (atype == D4XWRITE) &&
                    D4VAL (c, wbackf) (c, m, setnumber, ptr, walloc);
            if (wback) {
                ptr->dirty |= sbbits;
            }

            /*
             * Take care of replaced block
             * including write-back if necessary
             */
            if (blockmiss) {
                D4StackNode *rptr = c->stack[setnumber].top->up;
                if (rptr->valid != 0) {
                    if (!ronly && (rptr->valid & rptr->dirty) != 0) {
                        d4_wbblock (c, rptr, D4VAL (c, lg2subblocksize));
                    }
                    d4hash_remove (c, setnumber, rptr);
                    rptr->valid = 0;
                }
            }
        }

        /*
         * Prepare reference for downstream cache.
         * We do this for write-throughs, read-type misses,
         * and fetches for incompletely written subblocks
         * when a write misses and write-allocate is being used.
         * In some cases, a write can generate two downstream references:
         * a fetch to load the complete subblock and a write-through store.
         */
        if (!ronly && atype == D4XWRITE && !wback) {
            D4PendStack *newm = d4get_mref();
            newm->m = m;
            newm->next = c->pending;
            c->pending = newm;
        }
        if (miss && (ronly || atype != D4XWRITE ||
                     (walloc && m.size != D4REFNSB (c, m) << D4VAL (c, lg2subblocksize)))) {
            D4PendStack *newm = d4get_mref();
            /* note, we drop prefetch attribute */
            newm->m.accesstype = (atype == D4XWRITE) ? D4XREAD : atype;
            newm->m.address = D4ADDR2SUBBLOCK (c, m.address);
            newm->m.size = D4REFNSB (c, m) << D4VAL (c, lg2subblocksize);
            newm->next = c->pending;
            c->pending = newm;
        }

        /*
         * Do fully associative and infinite sized caches too.
         * This allows classifying misses into {compulsory,capacity,conflict}.
         * An extra "set" is provided (==c->numsets) for the fully associative
         * simulation.
         */
        if ((c->flags & D4F_CCC) != 0) {
            /* set to use for fully assoc cache */
            const int fullset = D4VAL(c, numsets);
            /* number of blocks in fully assoc cache */
            int fullmiss, fullblockmiss;	/* like miss and blockmiss, but for fully assoc cache */

            ptr = c->stack[fullset].top;
            if (ptr->blockaddr != blockaddr) {
                ptr = d4_find (c, fullset, blockaddr);
            } else if (ptr->valid == 0) {
                ptr = NULL;
            }

            fullblockmiss = (ptr == NULL);
            fullmiss = fullblockmiss || (sbbits & ptr->valid) != sbbits;

            /* take care of stack update */
            if (ronly || atype != D4XWRITE || !fullblockmiss || walloc) {
                ptr = D4VAL (c, replacementf) (c, fullset, m, ptr);
                assert (!fullblockmiss || ptr->valid == 0);
                ptr->valid |= sbbits;
            }

            /* classify misses */
            if (miss) {
                int infmiss = 0; /* assume hit in infinite cache */
                if (!fullmiss) { /* hit in fully assoc: conflict miss */
                    c->conf_miss[m.accesstype]++;
                } else {
                    infmiss = d4infcache (c, m);
                    if (infmiss != 0) { /* first miss: compulsory */
                        c->comp_miss[m.accesstype]++;
                    } else {	/* hit in infinite cache: capacity miss */
                        c->cap_miss[m.accesstype]++;
                    }
                }
                if (blockmiss) {
                    if (!fullblockmiss) { /* block hit in full assoc */
                        c->conf_blockmiss[m.accesstype]++;
                    } else if (infmiss == 1) { /* block miss in full and inf */
                        c->comp_blockmiss[m.accesstype]++;
                    } else { /* part of block hit in infinite cache */
                        c->cap_blockmiss[m.accesstype]++;
                    }
                }
            }

            /* take care of replaced block */
            if (fullblockmiss) {
                D4StackNode *rptr = c->stack[fullset].top->up;
                if (rptr->valid != 0) {
                    d4hash_remove (c, fullset, rptr);
                    rptr->valid = 0;
                }
            }
        }

        /*
         * Update non-ccc metrics.
         */
        c->fetch[m.accesstype]++;
        if (miss) {
            c->miss[m.accesstype]++;
            if (blockmiss) {
                c->blockmiss[m.accesstype]++;
            }
        }

        /*
         * Now make recursive calls for pending references
         */
        if (c->pending) {
            d4_dopending (c, c->pending);
        }
    }
}
