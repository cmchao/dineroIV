/*
 * Main header file for Dinero IV.
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
#ifndef D4_H
#define D4_H

#include <stdbool.h>
#include <stdint.h>


/*
 * Miscellaneous definitions
 */

#define D4VERSION	"7"

typedef uint32_t d4addr;


/*
 * Full specification of a memory reference.
 * 0 size is allowd for D4XCOPYB and D4XINVAL only,
 * and indicated the operation applies to the whole cache.
 */
typedef struct {
    d4addr        address;     /** memorr address */
    int           accesstype;  /** D4Xefined */
    unsigned  int size;        /** of memory referenced, in bytes */
} D4MemRef;


/* Node for a stack of pending memrefs per cache */
typedef struct D4PendStack {
    D4MemRef m;
    struct D4PendStack *next;
} D4PendStack;


/*
 * The access types
 * D4PREFETCH is or'ed for prefetch types
 * D4_MULTIBLOCK is or'ed for split references
 */
#define	D4XREAD		0
#define D4XWRITE	1
#define D4XINSTRN	2
#define D4XMISC		3
#define D4XCOPYB	4	/* copy back dirty line(s) - no invalidate */
#define D4XINVAL	5	/* invalidate line(s) - no copyback */
#define D4NUMACCESSTYPES 8	/* how many basic access types + padding */
#define D4PREFETCH	D4NUMACCESSTYPES
#define D4_MULTIBLOCK	(2*D4PREFETCH)
#if D4PREFETCH==0 || (D4PREFETCH&(D4PREFETCH-1)) != 0
#error "D4PREFETCH must be a power of 2"
#endif

#define D4BASIC_ATYPE(x)	((x)&(D4PREFETCH-1)) /* just the basic part */




/**
 * Stack node in simulated cache,
 * Each stack is doubly linked, using up and down fields.
 * The list is circular, so top->up is the bottom.
 * Each stack always contains the full setsize nodes,
 * + 1 for replacement.  The valid ones are always first.
 * For caches with large associativity (>= D4HASH_THRESH),
 * the valid nodes are also on a hash bucket chain.
 * The following is 2-way example:
 *
 *                                 up
 *                   +----------------------------+
 *                   |                            v
 *                   |     down          down
 *+-------+      +---+--+ +---> +------+ +--> +------+
 *| stack | +--> | node0|       | node1|      | node2|    # set
 *+-------+      +------+ <---+ +------+ <--+ +---+--+
 *                          up            up      |
 *                   ^                            |
 *                   +----------------------------+
 *                                 down
 *
 */
typedef struct D4StackNode {
    d4addr             blockaddr;  /** byte address of block */
    unsigned int       valid;      /** bit for each subblock */
    unsigned int       referenced; /** bit for each subblock */
    unsigned int       dirty;      /** bit for each subblock */
    int                onstack;    /** which stack is node on? */
    struct D4Cache     *cachep;    /** which cache is this a part of */
    struct D4StackNode *down;      /** ptr to less recently used node */
    struct D4StackNode *up;        /** ptr to more recently used node */
    struct D4StackNode *bucket;    /** singly-linked for hash collisions */

#ifdef D4STACK_USERHOOK
    D4STACK_USERHOOK               /** allow additional stuff for user policies */
#endif
} D4StackNode;


/**
 * Head of a stack,
 * top points to the most recently used node in the stack.
 */
typedef struct {
    D4StackNode *top;    /** the "beginning" of the stack */
    int n;               /** size of stack (== 1 + assoc) */
} D4StackHead;


/*
 * Long stacks are indexed with a hash table
 * One hash table takes care of everything.
 * The hash key is based on the block address, stack number,
 * and cacheid.  Collisions are resolved by chaining.
 */
typedef struct {
    int size;		/* size of the hash table */
    D4StackNode **table;	/* the table itself, malloced */
} D4StackHash;

#define D4HASH_THRESH	8	/* stacks bigger than this size are hashed */


/*
 * We support infinite caches for classification of misses into
 * compulsory/capacity/conflict categories.
 * An infinite cache is made up of address ranges, where each
 * range has a bitmap showing which subblocks have been cached.
 */
typedef struct {
    d4addr	addr;		/* start address of range */
    char	*bitmap;
} D4Range;

#define D4_BITMAP_RSIZE	(8*1024*1024)	/* size of each range, in bits */
#if D4_BITMAP_RSIZE<=0 || (D4_BITMAP_RSIZE&(D4_BITMAP_RSIZE-1)) != 0
#error "D4_BITMAP_RSIZE must be a power of 2"
#endif


/**
 * full specification of a cache
 */
typedef struct D4Cache {
    char *name;            /** mostly for printing */
    int cacheid;           /** unique for each cache */
    int flags;             /** keep D4F_XXX hot bit information */
    D4StackHead *stack;    /** the priority stacks for this cache */
    D4PendStack *pending;  /** stack for prefetch etc. */
    struct D4Cache *link;  /** linked list of all caches */

    /*
     * Cache parameters
     */
    int lg2blocksize;      /** block size, set by the user */
    int lg2subblocksize;   /** sub-block size, set by the user */
    int lg2size;           /** size set by the user */
    int assoc;             /** way number, set by the user */

    int numsets;           /** this one is derived, not set by the user */

    /*
     * Interconnection of caches
     * Caches must form a tree, with the root
     * being memory and the leaves being closest
     * to the processors.
     */
    struct D4Cache *downstream;  /** pointer to next level cache instance */
    void (*ref)(struct D4Cache *, D4MemRef);  /** d4ref or custom version */

    /*
     * Cache policy functions and data:
     *    replacement, prefetch, write-alloc, write-back
     * These must be set by the user
     * (normally, each policy should have an initialization routine)
     */
    /** adjust priority stack */
    D4StackNode    *(*replacementf) (struct D4Cache *, int stacknum,
                                     D4MemRef, D4StackNode *ptr);
    /** indicate a prefetch with prefetch_pending */
    D4PendStack    *(*prefetchf) (struct D4Cache *, D4MemRef,
                                  int miss, D4StackNode *ptr);
    /** walloc returns true for write-allocate */
    int        (*wallocf) (struct D4Cache *, D4MemRef);
    /** wback returns true for write-back */
    int        (*wbackf) (struct D4Cache *, D4MemRef, int,
                          D4StackNode *ptr, int);

    int        prefetch_distance;     /** specific to built-in prefetch policies */
    int        prefetch_abortpercent;
    const char *name_replacement;     /** for printing */
    const char *name_prefetch;
    const char *name_walloc;
    const char *name_wback;

#ifdef D4CACHE_USERHOOK
    D4CACHE_USERHOOK                  /** allow additional stuff for user policies */
#endif

    /**
     * Infinite cache for compulsory/capacity/conflict classification
     */
    int        nranges;
    int        maxranges;
    D4Range    *ranges;


    /**
     * Cache statistics
     * Doubles are used as big integers
     * Index is accesstype + 0 or
     *          accesstype + D4PREFETCH for prefetch
     */
    double fetch          [2 * D4NUMACCESSTYPES];
    double miss           [2 * D4NUMACCESSTYPES];
    double blockmiss      [2 * D4NUMACCESSTYPES];
    double comp_miss      [2 * D4NUMACCESSTYPES];  /* compulsory misses */
    double comp_blockmiss [2 * D4NUMACCESSTYPES];
    double cap_miss       [2 * D4NUMACCESSTYPES];  /* capacity misses */
    double cap_blockmiss  [2 * D4NUMACCESSTYPES];
    double conf_miss      [2 * D4NUMACCESSTYPES];  /* conflict misses */
    double conf_blockmiss [2 * D4NUMACCESSTYPES];

    double multiblock;
    double bytes_read;
    double bytes_written;
} D4Cache;

/* flags for D4Cache */
#define D4F_MEM			0x1	/* for simulated memory only */
#define D4F_CCC			0x2	/* compulsory/capacity/conflict classification */
#define D4F_RO			0x4	/* cache is read-only (e.g., an instruction cache) */
#define D4F_USERFLAG1		0x8	/* first available flag bit */


/*
 * Miscellaneous pseudo-functions
 */
#define D4VAL(cache,field)	((cache)->field)

static inline d4addr D4LG2MASK(d4addr n)
{
    return ((((d4addr)1)<<(n))-1);
}

#define D4ADDR2BLOCK(cache,addr) /* byte address of block containing ref */   \
		((addr) & ~D4LG2MASK(D4VAL(cache,lg2blocksize)))

#define D4ADDR2SUBBLOCK(cache,addr) /* byte addr of subblock for ref */   \
		((addr) & ~D4LG2MASK(D4VAL(cache,lg2subblocksize)))

#define D4ADDR2SET(cache,addr)	/* which set does addr go in? */	      \
		(((addr)>>D4VAL(cache,lg2blocksize)) % D4VAL(cache,numsets))

#define D4REFNSB(cache,mref) /* how many subblocks will mref touch? */	      \
	((((mref).address+(mref).size-1) >> D4VAL(cache,lg2subblocksize)) -   \
	 ((mref).address>>D4VAL(cache,lg2subblocksize)) + 1)

#define D4ADDR2SBMASK(cache,mref) /* produce subblock bit mask for mref */    \
	(D4LG2MASK(D4REFNSB(cache,mref)) <<				      \
	 (((mref).address-D4ADDR2BLOCK(cache,(mref).address)) >>	      \
	  D4VAL(cache,lg2subblocksize)))


/** all option from command line are converted and stored into
 *  this format
 */
#ifndef MAX_LEV
#define MAX_LEV	5		/* allow -ln prefix no larger than this */
#endif

typedef struct {
    /** feature trigger related counter */
    uint64_t skipcount;         /** skip initial U references */
    uint64_t flushcount;        /** flush cache every U reference */
    uint64_t maxcount;          /** stop simulation after U referennce */
    uint64_t stat_interval;     /** show statistics after ever U referce */

    /** cache-related parameter */
    int maxlevel;               /** the higest level actually used */
    unsigned int level_size[3][MAX_LEV]; /** The size of each cache is given by
                                        * level_size[idu][level]
                                        * where idu=0 for ucache,
                                        *           1 for icache,
                                        *           2 for dcache,
                                        * and 0=closest to processor,
                                        *     MAX_LEV-1 = closest to memory)
                                        */
    /** misc */
    uint64_t on_trigger;        /** simulation start from this address */
    uint64_t off_trigger;       /** simulation stop after this address */
    bool stat_idcombine;        /** combine I$D cache stats */
    int informat;               /** input trace format */
    const char *progname;       /** the program name */
    const char *trace_file;     /** input tracefile path */
} D4Option;


/*
 * Global data declarations
 */
extern D4StackHash d4stackhash; /* hash table for all caches */
extern D4StackNode d4freelist; /* free list for stack nodes of all caches */
extern D4Option g_d4opt;        /** global instance to keep optional value */


/*
 * Global declarations for functions making up the Dinero IV
 * subroutine callable interface.
 */

/* top level user-callable functions */
/**
 * Create a new cache
 * The new cache sits "above" the indicated larger cache in the
 * memory hierarchy, with memory at the bottom and processors at the top.
 *
 * @param[in]  the downstream cache of the new cache. if NULL, it means memory
 * @return new cache instance or NULL if failed
 */
D4Cache	*d4new (D4Cache *);

/**
 * Check all caches, set up internal data structures.
 * Must be called exactly once, after all calls to d4new
 * and all necessary direct initialization of D4Cache structures.
 * The call to d4setup must occur before any calls to d4ref.
 *
 * @return  0 -> success
 */
int	d4setup (void);
void		d4ref (D4Cache *, D4MemRef); /* call generic version */
void		d4copyback (D4Cache *, const D4MemRef *, int);
void		d4invalidate (D4Cache *, const D4MemRef *, int);

/* replacement policies */
extern D4StackNode *d4rep_lru (D4Cache *, int stacknum, D4MemRef, D4StackNode *ptr);
extern D4StackNode *d4rep_fifo (D4Cache *, int stacknum, D4MemRef, D4StackNode *ptr);
extern D4StackNode *d4rep_random (D4Cache *, int stacknum, D4MemRef, D4StackNode *ptr);

/* prefetch policies */
extern D4PendStack *d4prefetch_none (D4Cache *, D4MemRef, int miss, D4StackNode *);
extern D4PendStack *d4prefetch_always (D4Cache *, D4MemRef, int miss, D4StackNode *);
extern D4PendStack *d4prefetch_loadforw (D4Cache *, D4MemRef, int miss, D4StackNode *);
extern D4PendStack *d4prefetch_subblock (D4Cache *, D4MemRef, int miss, D4StackNode *);
extern D4PendStack *d4prefetch_miss (D4Cache *, D4MemRef, int miss, D4StackNode *);
extern D4PendStack *d4prefetch_tagged (D4Cache *, D4MemRef, int miss, D4StackNode *);

/* write allocate policies */
extern int d4walloc_always (D4Cache *, D4MemRef);
extern int d4walloc_never (D4Cache *, D4MemRef);
extern int d4walloc_nofetch (D4Cache *, D4MemRef);
extern int d4walloc_impossible (D4Cache *, D4MemRef); /* for icaches */

/* write back/through policies */
extern int d4wback_always (D4Cache *, D4MemRef, int, D4StackNode *, int);
extern int d4wback_never (D4Cache *, D4MemRef, int, D4StackNode *, int);
extern int d4wback_nofetch (D4Cache *, D4MemRef, int, D4StackNode *, int);
extern int d4wback_impossible (D4Cache *, D4MemRef, int, D4StackNode *, int); /* for icaches */

/* initialization routines */
extern void d4init_rep_lru (D4Cache *);
extern void d4init_rep_fifo (D4Cache *);
extern void d4init_rep_random (D4Cache *);

extern void d4init_prefetch_none (D4Cache *);
extern void d4init_prefetch_always (D4Cache *, int, int);
extern void d4init_prefetch_loadforw (D4Cache *, int, int);
extern void d4init_prefetch_subblock (D4Cache *, int, int);
extern void d4init_prefetch_miss (D4Cache *, int, int);
extern void d4init_prefetch_tagged (D4Cache *, int, int);

extern void d4init_walloc_always (D4Cache *);
extern void d4init_walloc_never (D4Cache *);
extern void d4init_walloc_nofetch (D4Cache *);

extern void d4init_wback_always (D4Cache *);
extern void d4init_wback_never (D4Cache *);
extern void d4init_wback_nofetch (D4Cache *);


/* Miscellaneous functions users may or may not need */
extern D4PendStack *d4get_mref(void);	/* allocate struct for pending mref */
extern void d4put_mref (D4PendStack *);	/* deallocate pending mref */
extern void d4init_prefetch_generic (D4Cache *); /* helper routine for prefetch */

extern D4StackNode *d4findnth (D4Cache *, int stacknum, int n);
extern void d4movetotop (D4Cache *, int stacknum, D4StackNode *);
extern void d4movetobot (D4Cache *, int stacknum, D4StackNode *);

/** Insert the indicated node into the hash table
 * if way number + 1 > D4HASH_THRESH
 *
 * @param[in, out] c the related cache instance
 * @param[in] stacknum  set number
 * @param[in] s the node needed to be hashed
 */
void d4hash_insert (D4Cache * c, int stacknum, D4StackNode * s);


/**
 * Remove the indicated node from the hash table
 *
 * @param[in, out] c the related cache instance
 * @param[in] stacknum  set number
 * @param[in] s the node needed to be hashed
 */
void d4hash_remove (D4Cache *c, int stacknum, D4StackNode *s);



/*
 * Global declarations for internal Dinero IV use.
 */
extern int d4_infcache (D4Cache *, D4MemRef);
extern D4MemRef d4_splitm (D4Cache *, D4MemRef, d4addr);
extern void d4_dopending (D4Cache *, D4PendStack *);
extern void d4_unhash (D4Cache *c, int stacknum, D4StackNode *);
extern D4StackNode *d4_find (D4Cache *, int stacknum, d4addr blockaddr);
extern void d4_wbblock (D4Cache *, D4StackNode *, const int);

#endif
