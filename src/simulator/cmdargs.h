/*
 * Declarations for argument handling for Dinero IV's command interface.
 * Written by Jan Edler
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
 * This structure describes a command line arg for Dinero IV.
 * Some args require a cache attribute prefix, -ln-idu, where
 * 1 <= n <= MAX_LEV and idu is "i", "d", or "", coresponding to an instruction,
 * data, or unified cache; this is all handled by choice of match function.
 *
 * The arglist structure also supports the help message,
 * summary info, and option customization.
 */

#ifndef MAX_LEV
#define MAX_LEV	5		/* allow -ln prefix no larger than this */
#endif

typedef struct D4ArgList {
    const char *optstring;	  /* string to match, without -ln-idu if applicable */
    int pad;		  /* how many extra chars will help print? */
    void *var;		  /* scalar variable or array to modify */
    const char *defstr;		  /* default value, as a string */
    const char *customstring; /* arg to use for custom version */
    const char *helpstring;	  /* string for help line */

    /* function to recognize arg on command line */
    int (*match)(const char *opt, const struct D4ArgList *);
    /* valf is function to set value */
    void (*valf)(const char *opt, const char *arg, const struct D4ArgList *);
    /* customf produces definitions for custom version */
    void (*customf)(const struct D4ArgList *, FILE *);
    /* sumf prints summary line */
    void (*sumf)(const struct D4ArgList *, FILE *);
    /* help prints line for -help */
    void (*help)(const struct D4ArgList *);
} D4ArgList;

/** all option from command line are converted and stored into
 *  this format
 */
typedef struct {
    uint64_t skipcount;         /** skip initial U references */
    uint64_t flushcount;        /** flush cache every U reference */
    uint64_t maxcount;          /** stop simulation after U referennce */
    uint64_t stat_interval;     /** show statistics after ever U referce */
} D4Option;

/* Some globals, defined in cmdargs.c */
extern D4ArgList args[];	/* defined in cmdargs.c */
extern int maxlevel;		/* largest cache level specified */

extern D4Option g_d4opt;        /** global instance to keep optional value */

extern char *customname;	/* for -custom, name of executable */
extern long on_trigger;		/* for -on-trigger */
extern long off_trigger;	/* for -off-trigger */
extern int stat_idcombine;	/* for -stat-idcombine */

/*
 * The size of each cache is given by
 *	level_size[idu][level]
 * where idu=0 for ucache, 1 for icache, 2 for dcache,
 * and 0=closest to processor, MAX_LEV-1 = closest to memory)
 */
extern unsigned int level_size[3][MAX_LEV];

/*
 * Set argument-related things up after seeing all args
 */
void doargs (int argc, char **argv);
void verify_options (void);
void summarize_caches (void);
extern void initialize_caches (D4Cache **icachep, D4Cache **dcachep);
extern void init_1cache (D4Cache *, int, int);
