/*
 * Miscellaneous functions for Dinero IV.
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "d4.h"


/*
 * Global variable definitions
 */
D4StackHash d4stackhash;
D4StackNode d4freelist;
D4PendStack *d4pendfree;
D4Cache *d4_allcaches;


/**
 * create the hash value for node related to specific cache
 *
 * @param[in] ba blockaddr
 * @param[in] sn stacknum
 * @param[in] cid cacheid
 * @return  hash value
 */
static inline int d4hash_make(d4addr ba, int sn, int cid) {
    return (ba + sn + cid) % d4stackhash.size;
}


D4Cache *
d4new (D4Cache *larger)
{
    static int nextcacheid = 1;
    D4Cache *c = calloc (1, sizeof(D4Cache));

    if (c == NULL) {
        return NULL;
    }
    c->cacheid = nextcacheid++;
    c->downstream = larger;
    c->ref = d4ref;	/* may get altered for custom version */
    if (larger == NULL) {	/* simulated memory */
        c->flags = D4F_MEM;
        c->assoc = 1;	/* not used, but helps avoid compiler warnings */
    }
    c->link = d4_allcaches;
    d4_allcaches = c;	/* d4customize depends on this LIFO order */
    return c;
}


int
d4setup(void)
{
    int i, nnodes;
    int r = 0;
    D4Cache *c, *cc;
    D4StackNode *nodes = NULL, *ptr;

    for (c = d4_allcaches;  c != NULL;  c = c->link) {

        /* Check some stuff the user shouldn't muck with */
        if (c->stack != NULL || c->pending != NULL ||
                c->cacheid < 1 ||
                (c->link == NULL && c->cacheid != 1) ||
                (c->flags != D4F_MEM && c->downstream == NULL) ||
                c->numsets != 0 ||
                c->ranges != NULL || c->nranges != 0 || c->maxranges != 0) {
            goto fail1;
        }

        if ((c->flags & D4F_MEM) != 0) {
            c->numsets = 1;    /* not used, but helps avoid compiler warnings */
        } else {
            /* check the things the user should have set */
            if (c->lg2blocksize < 0) {
                goto fail2;
            }
            if (c->lg2subblocksize < 0 || c->lg2subblocksize > c->lg2blocksize) {
                goto fail3;
            }
            if (c->lg2size < c->lg2blocksize) {
                goto fail4;
            }
            if (c->assoc <= 0) {
                goto fail5;
            }
            if (c->replacementf == NULL || c->name_replacement == NULL) {
                goto fail6;
            }
            if (c->prefetchf == NULL || c->name_prefetch == NULL) {
                goto fail7;
            }
            if (c->wallocf == NULL || c->name_walloc == NULL) {
                goto fail8;
            }
            if (c->wbackf == NULL || c->name_wback == NULL) {
                goto fail9;
            }

            /* we don't try to check per-policy cache state */

            /* it looks ok, now initialize */
            c->numsets = (1 << c->lg2size) / ((1 << c->lg2blocksize) * c->assoc);

            c->stack = calloc (c->numsets + ((c->flags & D4F_CCC) != 0),
                               sizeof(D4StackHead));
            if (c->stack == NULL) {
                goto fail10;
            }
            nnodes = c->numsets * (1 + c->assoc) +
                     (c->numsets * c->assoc + 1) * ((c->flags & D4F_CCC) != 0);
            nodes = calloc (nnodes, sizeof(D4StackNode));
            if (nodes == NULL) {
                goto fail11;
            }
            for (i = 0;  i < nnodes;  i++) {
                nodes[i].cachep = c;
            }
            ptr = nodes;
            /* set up circular list for each stack */
            for (i = 0;  i < c->numsets + ((c->flags & D4F_CCC) != 0);  i++) {
                int j, n;
                n = 1 + c->assoc * ((i < c->numsets) ? 1 : c->numsets);
                c->stack[i].top = ptr;
                c->stack[i].n = n;
                for (j = 1;  j < n - 1;  j++) {
                    ptr[j].onstack = i;
                    ptr[j].down = &ptr[j + 1];
                    ptr[j].up = &ptr[j - 1];
                }
                ptr[0].onstack = i;
                ptr[0].down = &ptr[1];
                ptr[0].up = &ptr[n - 1];
                ptr[n - 1].onstack = i;
                ptr[n - 1].down = &ptr[0];
                ptr[n - 1].up = &ptr[n - 2];
                ptr += n;
            }
            assert (ptr - nodes == nnodes);
            d4stackhash.size += c->numsets * c->assoc;
        }

        /* make a printable name if the user didn't pick one */
        if (c->name == NULL) {
            c->name = malloc (30);
            if (c->name == NULL) {
                goto fail12;
            }
            sprintf (c->name, "%s%d",
                     (c->flags & D4F_MEM) != 0 ? "memory" : "cache", c->cacheid);
        }
    }

    d4stackhash.table = calloc (d4stackhash.size, sizeof(D4StackNode*));
    if (d4stackhash.table == NULL) {
        goto fail13;
    }
    return 0;

    /* Try to undo stuff so (in principle) the user could try again */
fail13:
    r++;
    /* don't bother trying to deallocate c->name */
fail12:
    r++;
    free (nodes);
fail11:
    r++;
    free (c->stack);
fail10:
    r++;
fail9:
    r++;
fail8:
    r++;
fail7:
    r++;
fail6:
    r++;
fail5:
    r++;
fail4:
    r++;
fail3:
    r++;
fail2:
    r++;
fail1:
    r++;

    for (cc = d4_allcaches;  cc != c;  cc = cc->link) {
        /* don't bother trying to deallocate c->name */
        free (c->stack[0].top);
        free (c->stack);
        c->stack = NULL;
        c->numsets = 0;
    }
    return r;
}


#if 0	/* not used normally, but can be useful for debugging */
/*
 * Perform a consistency check on a priority stack
 */
void d4checkstack (D4Cache *, int, char *); /* prototype avoids warnings */
void
d4checkstack (D4Cache *c, int stacknum, char *msg)
{
    D4StackNode *sp, *top;
    int i, ii;
    static int tentimes = 10;

    top = c->stack[stacknum].top;
    i = 0;
    sp = top;
    do {
        i++;
        sp = sp->down;
    } while (sp != top);
    if (i != c->stack[stacknum].n) {
        if (tentimes-- < 0) {
            return;
        }
        fprintf (stderr, "%s: cache %d stack %d actual forward count=%d, shouldbe=%d\n",
                 msg, c->cacheid, stacknum, i, c->stack[stacknum].n);
    }
    ii = 0;
    sp = top;
    do {
        ii++;
        sp = sp->up;
    } while (sp != top);
    if (ii != i) {
        if (tentimes-- < 0) {
            return;
        }
        fprintf (stderr, "%s: cache %d stack %d actual forward count=%d, actual reverse count=%d\n",
                 msg, c->cacheid, stacknum, i, ii);
    }
}
#endif


/*
 * Find address in stack.
 */
D4StackNode *
d4_find (D4Cache *c, int stacknum, d4addr blockaddr)
{
    D4StackNode *ptr;

    if (c->stack[stacknum].n > D4HASH_THRESH) {
        int buck = d4hash_make (blockaddr, stacknum, c->cacheid);
        for (ptr = d4stackhash.table[buck];
                ptr != NULL && (ptr->blockaddr != blockaddr || ptr->cachep != c || ptr->onstack != stacknum);
                ptr = ptr->bucket) {
            assert (ptr->valid != 0);
        }
        return ptr;
    }

    /*
     * Don't hash, search the stack linearly.
     * The search will terminate,
     * because the last node is guaranteed to have valid==0.
     */
    for (ptr = c->stack[stacknum].top;
            ptr->blockaddr != blockaddr && ptr->valid != 0;
            ptr = ptr->down) {
        continue;
    }

    if (ptr->valid != 0) {
        return ptr;
    }

    return NULL;	/* not found */
}


/* find the nth element from the top (1 origin) */
D4StackNode *
d4findnth (D4Cache *c, int stacknum, int n)
{
    D4StackNode *p;
    int i, stacksize;

    stacksize = c->stack[stacknum].n;
    assert (n <= stacksize);

    /* go in the shortest direction to find node */
    p = c->stack[stacknum].top;
    if (n <= stacksize / 2)		/* search from front */
        for (i = 1;  i < n;  i++) {
            p = p->down;
        }
    else				/* search from rear */
        for (i = stacksize + 1;  i > n;  i--) {
            p = p->up;
        }
    return p;
}


/* Move node to top (most recently used position) of stack */
void
d4movetotop (D4Cache *c, int stacknum, D4StackNode *ptr)
{
    D4StackNode *top = c->stack[stacknum].top;
    D4StackNode *bot;

    /* nothing to do if node is already at top */
    if (ptr != top) {
        bot = top->up;
        if (bot != ptr)	{	/* general case */
            ptr->down->up = ptr->up;	/* remove */
            ptr->up->down = ptr->down;
            ptr->up = bot;			/* insert between top & bot */
            ptr->down = top;
            bot->down = ptr;
            top->up = ptr;
        }
        c->stack[stacknum].top = ptr;
    }
}


/* Move node to bottom (least recently used, actually spare) position */
void
d4movetobot (D4Cache *c, int stacknum, D4StackNode *ptr)
{
    D4StackNode *top = c->stack[stacknum].top;
    D4StackNode *bot = top->up;

    /* nothing to do if node is already at bottom */
    if (ptr != bot) {
        if (top == ptr) {	/* common and favorable: move from top to bot */
            c->stack[stacknum].top = top->down;
        } else {
            ptr->down->up = ptr->up;	/* remove */
            ptr->up->down = ptr->down;
            ptr->up = bot;			/* insert between top & bot */
            ptr->down = top;
            bot->down = ptr;
            top->up = ptr;
        }
    }
}


inline void
d4hash_insert (D4Cache *c, int stacknum, D4StackNode *s)
{
    int buck = d4hash_make (s->blockaddr, stacknum, s->cachep->cacheid);

    assert(c != NULL);
    assert(s != NULL);

    if (c->stack[stacknum].n > D4HASH_THRESH) {
        s->bucket = d4stackhash.table[buck];
        d4stackhash.table[buck] = s;
    }
}


void
d4hash_remove (D4Cache *c, int stacknum, D4StackNode *s)
{
    assert(c != NULL);
    assert(s != NULL);

    int buck = d4hash_make (s->blockaddr, stacknum, c->cacheid);
    D4StackNode *p = d4stackhash.table[buck];

    if (c->stack[stacknum].n > D4HASH_THRESH) {
        if (p == s) {
            d4stackhash.table[buck] = s->bucket;
        } else {
            while (p->bucket != s) {
                assert (p->bucket != NULL);
                p = p->bucket;
            }
            p->bucket = s->bucket;
        }
    }
}


/* Allocate a structure describing a pending memory reference */
D4PendStack *
d4get_mref()
{
    D4PendStack *m;

    m = d4pendfree;
    if (m != NULL) {
        d4pendfree = m->next;
        return m;
    }
    m = malloc (sizeof(*m));	/* no need to get too fancy here */
    if (m != NULL) {
        return m;
    }
    fprintf (stderr, "DineroIV ***error: no memory for pending mref\n");
    exit (9);
    return NULL;	/* can't really get here, but some compilers get upset if we don't have a return value */
}


/* Deallocate the structure used to describe a pending memory reference */
void
d4put_mref (D4PendStack *m)
{
    m->next = d4pendfree;
    d4pendfree = m;
}


/*
 * Make recursive calls for pending references
 * to own cache or towards memory
 */
void
d4_dopending (D4Cache *c, D4PendStack *newm)
{
    do {
        c->pending = newm->next;
        if ((newm->m.accesstype & D4PREFETCH) != 0) {
            c->ref (c, newm->m);
        } else if ((newm->m.accesstype & D4_MULTIBLOCK) != 0) {
            newm->m.accesstype &= ~D4_MULTIBLOCK;
            c->ref (c, newm->m);
        } else {
            switch (D4BASIC_ATYPE(newm->m.accesstype)) {
            default:
                fprintf (stderr, "Dinero IV: missing case %d in d4_dopending\n",
                         D4BASIC_ATYPE(newm->m.accesstype));
                exit (9);
            case D4XMISC:
            case D4XREAD:
            case D4XINSTRN:
                c->bytes_read += newm->m.size;
                break;
            case D4XWRITE:
                c->bytes_written += newm->m.size;
                break;
            case D4XCOPYB:
            case D4XINVAL:	/* don't count these */
                break;
            }
            c->downstream->ref (c->downstream, newm->m);
        }
        d4put_mref(newm);
    } while ((newm = c->pending) != NULL);
}


/*
 * Initiate write back for the dirty parts of a block.
 * Each contiguous bunch of subblocks is written in one operation.
 */
void
d4_wbblock (D4Cache *c, D4StackNode *ptr, const int lg2sbsize)
{
    d4addr a;
    unsigned int b, dbits;
    D4PendStack *newm;
    const int sbsize = 1 << lg2sbsize;

    b = 1;
    dbits = ptr->valid & ptr->dirty;
    a = ptr->blockaddr;
    do {
        newm = d4get_mref();
        newm->m.accesstype = D4XWRITE;
        for (;  (dbits & b) == 0;  b <<= 1) {
            a += sbsize;
        }
        newm->m.address = a;
        for (;  (dbits & b) != 0;  b <<= 1) {
            a += sbsize;
            dbits &= ~b;
        }
        newm->m.size = a - newm->m.address;
        newm->next = c->pending;
        c->pending = newm;
    } while (dbits != 0);
    ptr->dirty = 0;
}


/* invalidate and deallocate a block, as indicated by ptr */
static inline void
d4_invblock (D4Cache *c, int stacknum, D4StackNode *ptr)
{
    assert (ptr->valid != 0);
    ptr->valid = 0;
    d4movetobot (c, stacknum, ptr);
    d4hash_remove (c, stacknum, ptr);
}


/*
 * Copy any dirty stuff from the cache back towards memory.  The operation
 * affects the whole cache or just 1 block, depending on m:
 * if m == NULL or m->size == 0, it affects the whole cache.
 * If prop is true, the same copyback operation will be propagated
 * to other caches towards memory.
 * This function can be invoked directly by the subroutine-calling user,
 * or indirectly by passing a D4XCOPYB access type to d4ref
 * (in which case d4ref passes prop == 1).
 *
 * NOTE: this function does not invalidate!
 */
void
d4copyback (D4Cache *c, const D4MemRef *m, int prop)
{
    int stacknum;
    D4StackNode *ptr;
    D4PendStack *newm;

    if (m != NULL) {
        assert (m->accesstype == D4XCOPYB);
    }
    if (prop) {
        newm = d4get_mref();
        if (m != NULL) {
            newm->m = *m;
        } else {
            newm->m.accesstype = D4XCOPYB;
            newm->m.address = 0;
            newm->m.size = 0;	/* affect the whole cache */
        }
        newm->next = c->pending;
        c->pending = newm;
    }
    if (m != NULL && m->size > 0) {		/* copy back just 1 block */
        ptr = d4_find (c, D4ADDR2SET (c, m->address), D4ADDR2BLOCK (c, m->address));
        if (ptr != NULL && (ptr->dirty & ptr->valid) != 0) {
            d4_wbblock (c, ptr, c->lg2subblocksize);
        }
    } else for (stacknum = 0;  stacknum < c->numsets;  stacknum++) {
            D4StackNode *top = c->stack[stacknum].top;
            assert (top->up->valid == 0); /* this loop skips the bottom node */
            for (ptr = top;  ptr->down != top;  ptr = ptr->down)
                if ((ptr->dirty & ptr->valid) != 0) {
                    d4_wbblock (c, ptr, c->lg2subblocksize);
                }
        }
    if ((newm = c->pending) != NULL) {
        d4_dopending (c, newm);
    }
}


/*
 * Handle invalidation for infinite cache
 */
static void
d4_invinfcache (D4Cache *c, const D4MemRef *m)
{
    int i;

    if (m == NULL || m->size == 0) {	/* invalidate whole cache */
        for (i = 0;  i < c->nranges;  i++) {
            assert (c->ranges[i].bitmap != NULL);
            free (c->ranges[i].bitmap);
            c->ranges[i].bitmap = NULL;
        }
        c->nranges = 0;
        c->maxranges = 0;
    } else {					/* invalidate just one block */
        const unsigned int sbsize = 1 << D4VAL (c, lg2subblocksize);
        const unsigned int baddr = D4ADDR2BLOCK (c, m->address);
        unsigned int bitoff;	/* offset of bit in bitmap */
        int hi, lo, nsb;

        bitoff = (baddr & (D4_BITMAP_RSIZE - 1)) / sbsize;

        /* binary search for range containing our address */
        hi = c->nranges - 1;
        lo = 0;
        while (lo <= hi) {
            i = lo + (hi - lo) / 2;
            if (c->ranges[i].addr + D4_BITMAP_RSIZE <= baddr) {
                lo = i + 1;    /* need to look higher */
            } else if (c->ranges[i].addr > baddr) {
                hi = i - 1;    /* need to look lower */
            } else {				/* found the right range */
                for (nsb = c->lg2blocksize - c->lg2subblocksize;
                        nsb-- > 0;
                        bitoff++)
                    c->ranges[i].bitmap[bitoff / CHAR_BIT] &=
                        ~(1 << (bitoff % CHAR_BIT));
                break;
            }
        }
    }
}

/*
 * Invalidate cache contents.  The operation affects the whole cache
 * or just 1 block, depending on m:
 * if m == NULL or m->size == 0, it affects the whole cache.
 * If prop is true, the same invalidate operation will be propagated
 * to other caches towards memory.
 * This function can be invoked directly by the subroutine-calling user,
 * or indirectly by passing a D4XINVAL access type to d4ref
 * (in which case d4ref passes prop == 1).
 *
 * NOTE: this does not copy back dirty stuff!
 *	you have to call d4copyback first for that.
 */
void
d4invalidate (D4Cache *c, const D4MemRef *m, int prop)
{
    int stacknum;
    D4StackNode *ptr;
    D4PendStack *newm;

    if (m != NULL) {
        assert (m->accesstype == D4XINVAL);
    }
    if (prop) {
        newm = d4get_mref();
        if (m != NULL) {
            newm->m = *m;
        } else {
            newm->m.accesstype = D4XINVAL;
            newm->m.address = 0;
            newm->m.size = 0;	/* affect the whole cache */
        }
        newm->next = c->pending;
        c->pending = newm;
    }
    if (m != NULL && m->size > 0) {		/* invalidate just one block */
        d4addr blockaddr = D4ADDR2BLOCK (c, m->address);
        stacknum = D4ADDR2SET (c, m->address);
        ptr = d4_find (c, stacknum, blockaddr);
        if (ptr != NULL) {
            d4_invblock (c, stacknum, ptr);
        }
        if ((c->flags & D4F_CCC) != 0 &&	/* fully assoc cache */
                (ptr = d4_find (c, c->numsets, blockaddr)) != NULL) {
            d4_invblock (c, c->numsets, ptr);
        }
    } else for (stacknum = 0;  stacknum < c->numsets + ((c->flags & D4F_CCC) != 0);  stacknum++) {
            D4StackNode *top = c->stack[stacknum].top;
            assert (top->up->valid == 0); /* all invalid nodes are at bottom; at least 1 */
            for (ptr = top;  ptr->down != top;  ptr = ptr->down) {
                if (ptr->valid == 0) {
                    break;
                }
                d4_invblock (c, stacknum, ptr);
            }
        }
    if ((c->flags & D4F_CCC) != 0) {
        d4_invinfcache (c, m);
    }
    if ((newm = c->pending) != NULL) {
        d4_dopending (c, newm);
    }
}

