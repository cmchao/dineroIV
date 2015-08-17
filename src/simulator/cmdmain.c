/*
 * Dinero IV
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
 *
 * This file contains the startup, overall driving code, and
 * miscellaneous stuff needed for Dinero IV when running as a
 * self-contained simulator.
 * All the really hard stuff is in the callable Dinero IV subroutines,
 * which may be used independently of this program.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "d4.h"
#include "cmdd4.h"
#include "cmdargs.h"
#include "tracein.h"


/* some global variables */
D4Cache *levcache[3][MAX_LEV];		/* to locate cache by level and type */
D4Cache *mem;				/* which cache represents simulated memory? */

/* private prototypes for this file */
extern void dostats (void);
extern void do1stats (D4Cache *);
extern D4MemRef next_trace_item (void);

/*
 * Print out the stuff the user really wants
 */
void
dostats()
{
    int lev;
    int i;

    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        if (g_d4opt.stat_idcombine && levcache[0][lev] == NULL) {
            D4Cache cc;	/* a bogus cache structure */
            char ccname[30];

            cc.name = ccname;
            sprintf (cc.name, "l%d-I/Dcaches", lev + 1);
            cc.prefetchf = NULL;
            if (levcache[1][lev]->prefetchf == d4prefetch_none && levcache[2][lev]->prefetchf == d4prefetch_none) {
                cc.prefetchf = d4prefetch_none;    /* controls prefetch printout stats */
            }

            /* add the i & d stats into the new bogus structure */
            for (i = 0;  i < 2 * D4NUMACCESSTYPES;  i++) {
                cc.fetch[i]          = levcache[1][lev]->fetch[i]          + levcache[2][lev]->fetch[i];
                cc.miss[i]           = levcache[1][lev]->miss[i]           + levcache[2][lev]->miss[i];
                cc.blockmiss[i]      = levcache[1][lev]->blockmiss[i]      + levcache[2][lev]->blockmiss[i];
                cc.comp_miss[i]      = levcache[1][lev]->comp_miss[i]      + levcache[2][lev]->comp_miss[i];
                cc.comp_blockmiss[i] = levcache[1][lev]->comp_blockmiss[i] + levcache[2][lev]->comp_blockmiss[i];
                cc.cap_miss[i]       = levcache[1][lev]->cap_miss[i]       + levcache[2][lev]->cap_miss[i];
                cc.cap_blockmiss[i]  = levcache[1][lev]->cap_blockmiss[i]  + levcache[2][lev]->cap_blockmiss[i];
                cc.conf_miss[i]      = levcache[1][lev]->conf_miss[i]      + levcache[2][lev]->conf_miss[i];
                cc.conf_blockmiss[i] = levcache[1][lev]->conf_blockmiss[i] + levcache[2][lev]->conf_blockmiss[i];
            }
            cc.multiblock    = levcache[1][lev]->multiblock    + levcache[2][lev]->multiblock;
            cc.bytes_read    = levcache[1][lev]->bytes_read    + levcache[2][lev]->bytes_read;
            cc.bytes_written = levcache[1][lev]->bytes_written + levcache[2][lev]->bytes_written;

            cc.flags = levcache[1][lev]->flags | levcache[2][lev]->flags; /* get D4F_CCC */

            /*
             * block and subblock size should match;
             * should be checked at startup time
             */
            cc.lg2subblocksize = levcache[1][lev]->lg2subblocksize;
            cc.lg2blocksize = levcache[1][lev]->lg2blocksize;

            do1stats (&cc);
        } else {
            if (levcache[0][lev] != NULL) {
                do1stats (levcache[0][lev]);
            }
            if (levcache[1][lev] != NULL) {
                do1stats (levcache[1][lev]);
            }
            if (levcache[2][lev] != NULL) {
                do1stats (levcache[2][lev]);
            }
        }
    }
}


#define NONZERO(i) (((i)==0.0) ? 1.0 : (double)(i)) /* avoid divide-by-zero exception */
/*
 * Print stats for 1 cache
 */
void
do1stats (D4Cache *c)
{
    double	demand_fetch_data,
            demand_fetch_alltype;
    double	prefetch_fetch_data,
            prefetch_fetch_alltype;

    double	demand_data,
            demand_alltype;
    double	prefetch_data,
            prefetch_alltype;

    double	demand_comp_data,
            demand_comp_alltype;
    double	demand_cap_data,
            demand_cap_alltype;
    double	demand_conf_data,
            demand_conf_alltype;

    double	floatnum;

    /* Used in bus traffic calculations even if no prefetching. */
    prefetch_fetch_alltype = 0;

    /*
     * Print Header
     */
    printf(	"%s\n", c->name);
    printf(	" Metrics		      Total	      Instrn	       Data	       Read	      Write	       Misc\n");
    printf(	" -----------------	      ------	      ------	      ------	      ------	      ------	      ------\n");

    /*
     * Print Fetch Numbers
     */
    demand_fetch_data = c->fetch[D4XMISC]
                        + c->fetch[D4XREAD]
                        + c->fetch[D4XWRITE];
    demand_fetch_alltype = demand_fetch_data
                           + c->fetch[D4XINSTRN];

    printf(	" Demand Fetches		%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
            demand_fetch_alltype,
            c->fetch[D4XINSTRN],
            demand_fetch_data,
            c->fetch[D4XREAD],
            c->fetch[D4XWRITE],
            c->fetch[D4XMISC]);

    floatnum = NONZERO(demand_fetch_alltype);

    printf(	"  Fraction of total	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
            demand_fetch_alltype / floatnum,
            c->fetch[D4XINSTRN] / floatnum,
            demand_fetch_data / floatnum,
            c->fetch[D4XREAD] / floatnum,
            c->fetch[D4XWRITE] / floatnum,
            c->fetch[D4XMISC] / floatnum);

    /*
     * Prefetching?
     */
    prefetch_fetch_data = c->fetch[D4PREFETCH + D4XMISC]
                          + c->fetch[D4PREFETCH + D4XREAD]
                          + c->fetch[D4PREFETCH + D4XWRITE];
    if (c->prefetchf != d4prefetch_none) {
        prefetch_fetch_alltype = prefetch_fetch_data
                                 + c->fetch[D4PREFETCH + D4XINSTRN];

        printf(	" Prefetch Fetches	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                prefetch_fetch_alltype,
                c->fetch[D4PREFETCH + D4XINSTRN],
                prefetch_fetch_data,
                c->fetch[D4PREFETCH + D4XREAD],
                c->fetch[D4PREFETCH + D4XWRITE],
                c->fetch[D4PREFETCH + D4XMISC]);

        floatnum = NONZERO(prefetch_fetch_alltype);

        printf(	"  Fraction		%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                prefetch_fetch_alltype / floatnum,
                c->fetch[D4PREFETCH + D4XINSTRN] / floatnum,
                prefetch_fetch_data / floatnum,
                c->fetch[D4PREFETCH + D4XREAD] / floatnum,
                c->fetch[D4PREFETCH + D4XWRITE] / floatnum,
                c->fetch[D4PREFETCH + D4XMISC] / floatnum);

        printf(	" Total Fetches		%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_fetch_alltype + prefetch_fetch_alltype,
                c->fetch[D4XINSTRN] + c->fetch[D4PREFETCH + D4XINSTRN],
                demand_fetch_data + prefetch_fetch_data,
                c->fetch[D4XREAD] + c->fetch[D4PREFETCH + D4XREAD],
                c->fetch[D4XWRITE] + c->fetch[D4PREFETCH + D4XWRITE],
                c->fetch[D4XMISC] + c->fetch[D4PREFETCH + D4XMISC]);

        floatnum = NONZERO(demand_fetch_alltype + prefetch_fetch_alltype);

        printf(	"  Fraction		%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                (demand_fetch_alltype + prefetch_fetch_alltype) / floatnum,
                (c->fetch[D4XINSTRN] + c->fetch[D4PREFETCH + D4XINSTRN]) / floatnum,
                (demand_fetch_data + prefetch_fetch_data) / floatnum,
                (c->fetch[D4XREAD] + c->fetch[D4PREFETCH + D4XREAD]) / floatnum,
                (c->fetch[D4XWRITE] + c->fetch[D4PREFETCH + D4XWRITE]) / floatnum,
                (c->fetch[D4XMISC] + c->fetch[D4PREFETCH + D4XMISC]) / floatnum);

    } /* End of prefetching. */
    printf("\n");

    /*
     * End of Fetch Numbers
     */

    /*
     * Print Miss Numbers
     */
    demand_data = c->miss[D4XMISC]
                  + c->miss[D4XREAD]
                  + c->miss[D4XWRITE];
    demand_alltype = demand_data
                     + c->miss[D4XINSTRN];

    printf(	" Demand Misses		%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
            demand_alltype,
            c->miss[D4XINSTRN],
            demand_data,
            c->miss[D4XREAD],
            c->miss[D4XWRITE],
            c->miss[D4XMISC]);


    printf(	"  Demand miss rate	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
            demand_alltype / NONZERO(demand_fetch_alltype),
            c->miss[D4XINSTRN] / NONZERO(c->fetch[D4XINSTRN]),
            demand_data / NONZERO(demand_fetch_data),
            c->miss[D4XREAD] / NONZERO(c->fetch[D4XREAD]),
            c->miss[D4XWRITE] / NONZERO(c->fetch[D4XWRITE]),
            c->miss[D4XMISC] / NONZERO(c->fetch[D4XMISC]));

    if (c->flags & D4F_CCC) {
        demand_comp_data = c->comp_miss[D4XMISC]
                           + c->comp_miss[D4XREAD]
                           + c->comp_miss[D4XWRITE];
        demand_comp_alltype = demand_comp_data
                              + c->comp_miss[D4XINSTRN];
        demand_cap_data = c->cap_miss[D4XMISC]
                          + c->cap_miss[D4XREAD]
                          + c->cap_miss[D4XWRITE];
        demand_cap_alltype = demand_cap_data
                             + c->cap_miss[D4XINSTRN];
        demand_conf_data = c->conf_miss[D4XMISC]
                           + c->conf_miss[D4XREAD]
                           + c->conf_miss[D4XWRITE];
        demand_conf_alltype = demand_conf_data
                              + c->conf_miss[D4XINSTRN];

        printf(	"   Compulsory misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_comp_alltype,
                c->comp_miss[D4XINSTRN],
                demand_comp_data,
                c->comp_miss[D4XREAD],
                c->comp_miss[D4XWRITE],
                c->comp_miss[D4XMISC]);

        printf(	"   Capacity misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_cap_alltype,
                c->cap_miss[D4XINSTRN],
                demand_cap_data,
                c->cap_miss[D4XREAD],
                c->cap_miss[D4XWRITE],
                c->cap_miss[D4XMISC]);

        printf(	"   Conflict misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_conf_alltype,
                c->conf_miss[D4XINSTRN],
                demand_conf_data,
                c->conf_miss[D4XREAD],
                c->conf_miss[D4XWRITE],
                c->conf_miss[D4XMISC]);

        printf(	"   Compulsory fraction	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                demand_comp_alltype / NONZERO(demand_alltype),
                c->comp_miss[D4XINSTRN] / NONZERO(c->miss[D4XINSTRN]),
                demand_comp_data / NONZERO(demand_data),
                c->comp_miss[D4XREAD] / NONZERO(c->miss[D4XREAD]),
                c->comp_miss[D4XWRITE] / NONZERO(c->miss[D4XWRITE]),
                c->comp_miss[D4XMISC] / NONZERO(c->miss[D4XMISC]));

        printf(	"   Capacity fraction	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                demand_cap_alltype / NONZERO(demand_alltype),
                c->cap_miss[D4XINSTRN]  / NONZERO(c->miss[D4XINSTRN]),
                demand_cap_data / NONZERO(demand_data),
                c->cap_miss[D4XREAD] / NONZERO(c->miss[D4XREAD]),
                c->cap_miss[D4XWRITE] / NONZERO(c->miss[D4XWRITE]),
                c->cap_miss[D4XMISC] / NONZERO(c->miss[D4XMISC]));

        printf(	"   Conflict fraction	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                demand_conf_alltype / NONZERO(demand_alltype),
                c->conf_miss[D4XINSTRN] / NONZERO(c->miss[D4XINSTRN]),
                demand_conf_data / NONZERO(demand_data),
                c->conf_miss[D4XREAD] / NONZERO(c->miss[D4XREAD]),
                c->conf_miss[D4XWRITE] / NONZERO(c->miss[D4XWRITE]),
                c->conf_miss[D4XMISC] / NONZERO(c->miss[D4XMISC]));
    }

    /*
     * Prefetch misses?
     */
    if (c->prefetchf != d4prefetch_none) {
        prefetch_data = c->miss[D4PREFETCH + D4XMISC]
                        + c->miss[D4PREFETCH + D4XREAD]
                        + c->miss[D4PREFETCH + D4XWRITE];
        prefetch_alltype = prefetch_data
                           + c->miss[D4PREFETCH + D4XINSTRN];

        printf(	" Prefetch Misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                prefetch_alltype,
                c->miss[D4PREFETCH + D4XINSTRN],
                prefetch_data,
                c->miss[D4PREFETCH + D4XREAD],
                c->miss[D4PREFETCH + D4XWRITE],
                c->miss[D4PREFETCH + D4XMISC]);

        printf(	"  PF miss rate		%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                prefetch_alltype / NONZERO(prefetch_fetch_alltype),
                c->miss[D4PREFETCH + D4XINSTRN] / NONZERO(c->fetch[D4PREFETCH + D4XINSTRN]),
                prefetch_data / NONZERO(prefetch_fetch_data),
                c->miss[D4PREFETCH + D4XREAD] / NONZERO(c->fetch[D4PREFETCH + D4XREAD]),
                c->miss[D4PREFETCH + D4XWRITE] / NONZERO(c->fetch[D4PREFETCH + D4XWRITE]),
                c->miss[D4PREFETCH + D4XMISC] / NONZERO(c->fetch[D4PREFETCH + D4XMISC]));

        if (c->flags & D4F_CCC) {
            demand_comp_data = c->comp_miss[D4PREFETCH + D4XMISC]
                               + c->comp_miss[D4PREFETCH + D4XREAD]
                               + c->comp_miss[D4PREFETCH + D4XWRITE];
            demand_comp_alltype = demand_comp_data
                                  + c->comp_miss[D4PREFETCH + D4XINSTRN];
            demand_cap_data = c->cap_miss[D4PREFETCH + D4XMISC]
                              + c->cap_miss[D4PREFETCH + D4XREAD]
                              + c->cap_miss[D4PREFETCH + D4XWRITE];
            demand_cap_alltype = demand_cap_data
                                 + c->cap_miss[D4PREFETCH + D4XINSTRN];
            demand_conf_data = c->conf_miss[D4PREFETCH + D4XMISC]
                               + c->conf_miss[D4PREFETCH + D4XREAD]
                               + c->conf_miss[D4PREFETCH + D4XWRITE];
            demand_conf_alltype = demand_conf_data
                                  + c->conf_miss[D4PREFETCH + D4XINSTRN];

            printf(	"   PF compulsory misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_comp_alltype,
                    c->comp_miss[D4PREFETCH + D4XINSTRN],
                    demand_comp_data,
                    c->comp_miss[D4PREFETCH + D4XREAD],
                    c->comp_miss[D4PREFETCH + D4XWRITE],
                    c->comp_miss[D4PREFETCH + D4XMISC]);

            printf(	"   PF capacity misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_cap_alltype,
                    c->cap_miss[D4PREFETCH + D4XINSTRN],
                    demand_cap_data,
                    c->cap_miss[D4PREFETCH + D4XREAD],
                    c->cap_miss[D4PREFETCH + D4XWRITE],
                    c->cap_miss[D4PREFETCH + D4XMISC]);

            printf(	"   PF conflict misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_conf_alltype,
                    c->conf_miss[D4PREFETCH + D4XINSTRN],
                    demand_conf_data,
                    c->conf_miss[D4PREFETCH + D4XREAD],
                    c->conf_miss[D4PREFETCH + D4XWRITE],
                    c->conf_miss[D4PREFETCH + D4XMISC]);

            printf(	"   PF compulsory fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_comp_alltype / NONZERO(prefetch_alltype),
                    c->comp_miss[D4PREFETCH + D4XINSTRN] / NONZERO(c->miss[D4PREFETCH + D4XINSTRN]),
                    demand_comp_data / NONZERO(prefetch_data),
                    c->comp_miss[D4PREFETCH + D4XREAD] / NONZERO(c->miss[D4PREFETCH + D4XREAD]),
                    c->comp_miss[D4PREFETCH + D4XWRITE] / NONZERO(c->miss[D4PREFETCH + D4XWRITE]),
                    c->comp_miss[D4PREFETCH + D4XMISC] / NONZERO(c->miss[D4PREFETCH + D4XMISC]));

            printf(	"   PF capacity fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_cap_alltype / NONZERO(prefetch_alltype),
                    c->cap_miss[D4PREFETCH + D4XINSTRN] / NONZERO(c->miss[D4PREFETCH + D4XINSTRN]),
                    demand_cap_data / NONZERO(prefetch_data),
                    c->cap_miss[D4PREFETCH + D4XREAD] / NONZERO(c->miss[D4PREFETCH + D4XREAD]),
                    c->cap_miss[D4PREFETCH + D4XWRITE] / NONZERO(c->miss[D4PREFETCH + D4XWRITE]),
                    c->cap_miss[D4PREFETCH + D4XMISC] / NONZERO(c->miss[D4PREFETCH + D4XMISC]));

            printf(	"   PF conflict fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_conf_alltype / NONZERO(prefetch_alltype),
                    c->conf_miss[D4PREFETCH + D4XINSTRN] / NONZERO(c->miss[D4PREFETCH + D4XINSTRN]),
                    demand_conf_data / NONZERO(prefetch_data),
                    c->conf_miss[D4PREFETCH + D4XREAD] / NONZERO(c->miss[D4PREFETCH + D4XREAD]),
                    c->conf_miss[D4PREFETCH + D4XWRITE] / NONZERO(c->miss[D4PREFETCH + D4XWRITE]),
                    c->conf_miss[D4PREFETCH + D4XMISC] / NONZERO(c->miss[D4PREFETCH + D4XMISC]));
        }

        printf(	" Total Misses		%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_alltype + prefetch_alltype,
                c->miss[D4XINSTRN] + c->miss[D4PREFETCH + D4XINSTRN],
                demand_data + prefetch_data,
                c->miss[D4XREAD] + c->miss[D4PREFETCH + D4XREAD],
                c->miss[D4XWRITE] + c->miss[D4PREFETCH + D4XWRITE],
                c->miss[D4XMISC] + c->miss[D4PREFETCH + D4XMISC]);

        printf(	"  Total miss rate	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                (demand_alltype + prefetch_alltype) / NONZERO(demand_fetch_alltype + prefetch_fetch_alltype),
                (c->miss[D4XINSTRN] + c->miss[D4PREFETCH + D4XINSTRN]) / NONZERO(c->fetch[D4XINSTRN] + c->fetch[D4PREFETCH + D4XINSTRN]),
                (demand_data + prefetch_data) / NONZERO(demand_fetch_data + prefetch_fetch_data),
                (c->miss[D4XREAD] + c->miss[D4PREFETCH + D4XREAD]) / NONZERO(c->fetch[D4XREAD] + c->fetch[D4PREFETCH + D4XREAD]),
                (c->miss[D4XWRITE] + c->miss[D4PREFETCH + D4XWRITE]) / NONZERO(c->fetch[D4XWRITE] + c->fetch[D4PREFETCH + D4XWRITE]),
                (c->miss[D4XMISC] + c->miss[D4PREFETCH + D4XMISC]) / NONZERO(c->fetch[D4XMISC] + c->fetch[D4PREFETCH + D4XMISC]));

    } /* End of prefetch misses. */
    printf("\n");

    /*
     * End of Misses Numbers
     */


    /*
     * Print Block Miss Numbers
     */
    if (c->lg2subblocksize != c->lg2blocksize) {
        demand_data = c->blockmiss[D4XMISC]
                      + c->blockmiss[D4XREAD]
                      + c->blockmiss[D4XWRITE];
        demand_alltype = demand_data
                         + c->blockmiss[D4XINSTRN];

        printf(	" Demand Block Misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                demand_alltype,
                c->blockmiss[D4XINSTRN],
                demand_data,
                c->blockmiss[D4XREAD],
                c->blockmiss[D4XWRITE],
                c->blockmiss[D4XMISC]);

        printf(	"  DB miss rate		%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                demand_alltype / NONZERO(demand_fetch_alltype),
                c->blockmiss[D4XINSTRN] / NONZERO(c->fetch[D4XINSTRN]),
                demand_data / NONZERO(demand_fetch_data),
                c->blockmiss[D4XREAD] / NONZERO(c->fetch[D4XREAD]),
                c->blockmiss[D4XWRITE] / NONZERO(c->fetch[D4XWRITE]),
                c->blockmiss[D4XMISC] / NONZERO(c->fetch[D4XMISC]));

        if (c->flags & D4F_CCC) {
            demand_comp_data = c->comp_blockmiss[D4XMISC]
                               + c->comp_blockmiss[D4XREAD]
                               + c->comp_blockmiss[D4XWRITE];
            demand_comp_alltype = demand_comp_data
                                  + c->comp_blockmiss[D4XINSTRN];
            demand_cap_data = c->cap_blockmiss[D4XMISC]
                              + c->cap_blockmiss[D4XREAD]
                              + c->cap_blockmiss[D4XWRITE];
            demand_cap_alltype = demand_cap_data
                                 + c->cap_blockmiss[D4XINSTRN];
            demand_conf_data = c->conf_blockmiss[D4XMISC]
                               + c->conf_blockmiss[D4XREAD]
                               + c->conf_blockmiss[D4XWRITE];
            demand_conf_alltype = demand_conf_data
                                  + c->conf_blockmiss[D4XINSTRN];

            printf(	"   DB compulsory misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_comp_alltype,
                    c->comp_blockmiss[D4XINSTRN],
                    demand_comp_data,
                    c->comp_blockmiss[D4XREAD],
                    c->comp_blockmiss[D4XWRITE],
                    c->comp_blockmiss[D4XMISC]);

            printf(	"   DB capacity misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_cap_alltype,
                    c->cap_blockmiss[D4XINSTRN],
                    demand_cap_data,
                    c->cap_blockmiss[D4XREAD],
                    c->cap_blockmiss[D4XWRITE],
                    c->cap_blockmiss[D4XMISC]);

            printf(	"   DB conflict misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_conf_alltype,
                    c->conf_blockmiss[D4XINSTRN],
                    demand_conf_data,
                    c->conf_blockmiss[D4XREAD],
                    c->conf_blockmiss[D4XWRITE],
                    c->conf_blockmiss[D4XMISC]);

            printf(	"   DB compulsory fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_comp_alltype / NONZERO(demand_alltype),
                    c->comp_blockmiss[D4XINSTRN] / NONZERO(c->blockmiss[D4XINSTRN]),
                    demand_comp_data / NONZERO(demand_data),
                    c->comp_blockmiss[D4XREAD] / NONZERO(c->blockmiss[D4XREAD]),
                    c->comp_blockmiss[D4XWRITE] / NONZERO(c->blockmiss[D4XWRITE]),
                    c->comp_blockmiss[D4XMISC] / NONZERO(c->blockmiss[D4XMISC]));

            printf(	"   DB capacity fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_cap_alltype / NONZERO(demand_alltype),
                    c->cap_blockmiss[D4XINSTRN] / NONZERO(c->blockmiss[D4XINSTRN]),
                    demand_cap_data / NONZERO(demand_data),
                    c->cap_blockmiss[D4XREAD] / NONZERO(c->blockmiss[D4XREAD]),
                    c->cap_blockmiss[D4XWRITE] / NONZERO(c->blockmiss[D4XWRITE]),
                    c->cap_blockmiss[D4XMISC] / NONZERO(c->blockmiss[D4XMISC]));

            printf(	"   DB conflict fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    demand_conf_alltype / NONZERO(demand_alltype),
                    c->conf_blockmiss[D4XINSTRN] / NONZERO(c->blockmiss[D4XINSTRN]),
                    demand_conf_data / NONZERO(demand_data),
                    c->conf_blockmiss[D4XREAD] / NONZERO(c->blockmiss[D4XREAD]),
                    c->conf_blockmiss[D4XWRITE] / NONZERO(c->blockmiss[D4XWRITE]),
                    c->conf_blockmiss[D4XMISC] / NONZERO(c->blockmiss[D4XMISC]));
        }

        /*
         * Prefetch block misses?
         */
        if (c->prefetchf != d4prefetch_none) {
            prefetch_data = c->blockmiss[D4PREFETCH + D4XMISC]
                            + c->blockmiss[D4PREFETCH + D4XREAD]
                            + c->blockmiss[D4PREFETCH + D4XWRITE];
            prefetch_alltype = prefetch_data
                               + c->blockmiss[D4PREFETCH + D4XINSTRN];

            printf(	" Prefetch Block Misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    prefetch_alltype,
                    c->blockmiss[D4PREFETCH + D4XINSTRN],
                    prefetch_data,
                    c->blockmiss[D4PREFETCH + D4XREAD],
                    c->blockmiss[D4PREFETCH + D4XWRITE],
                    c->blockmiss[D4PREFETCH + D4XMISC]);

            printf(	"  PFB miss rate		%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    prefetch_alltype / NONZERO(prefetch_fetch_alltype),
                    c->blockmiss[D4PREFETCH + D4XINSTRN] / NONZERO(c->fetch[D4PREFETCH + D4XINSTRN]),
                    prefetch_data / NONZERO(prefetch_fetch_data),
                    c->blockmiss[D4PREFETCH + D4XREAD] / NONZERO(c->fetch[D4PREFETCH + D4XREAD]),
                    c->blockmiss[D4PREFETCH + D4XWRITE] / NONZERO(c->fetch[D4PREFETCH + D4XWRITE]),
                    c->blockmiss[D4PREFETCH + D4XMISC] / NONZERO(c->fetch[D4PREFETCH + D4XMISC]));

            if (c->flags & D4F_CCC) {
                demand_comp_data = c->comp_blockmiss[D4PREFETCH + D4XMISC]
                                   + c->comp_blockmiss[D4PREFETCH + D4XREAD]
                                   + c->comp_blockmiss[D4PREFETCH + D4XWRITE];
                demand_comp_alltype = demand_comp_data
                                      + c->comp_blockmiss[D4PREFETCH + D4XINSTRN];
                demand_cap_data = c->cap_blockmiss[D4PREFETCH + D4XMISC]
                                  + c->cap_blockmiss[D4PREFETCH + D4XREAD]
                                  + c->cap_blockmiss[D4PREFETCH + D4XWRITE];
                demand_cap_alltype = demand_cap_data
                                     + c->cap_blockmiss[D4PREFETCH + D4XINSTRN];
                demand_conf_data = c->conf_blockmiss[D4PREFETCH + D4XMISC]
                                   + c->conf_blockmiss[D4PREFETCH + D4XREAD]
                                   + c->conf_blockmiss[D4PREFETCH + D4XWRITE];
                demand_conf_alltype = demand_conf_data
                                      + c->conf_blockmiss[D4PREFETCH + D4XINSTRN];

                printf(	"   PFB comp misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                        demand_comp_alltype,
                        c->comp_blockmiss[D4PREFETCH + D4XINSTRN],
                        demand_comp_data,
                        c->comp_blockmiss[D4PREFETCH + D4XREAD],
                        c->comp_blockmiss[D4PREFETCH + D4XWRITE],
                        c->comp_blockmiss[D4PREFETCH + D4XMISC]);

                printf(	"   PFB cap misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                        demand_cap_alltype,
                        c->cap_blockmiss[D4PREFETCH + D4XINSTRN],
                        demand_cap_data,
                        c->cap_blockmiss[D4PREFETCH + D4XREAD],
                        c->cap_blockmiss[D4PREFETCH + D4XWRITE],
                        c->cap_blockmiss[D4PREFETCH + D4XMISC]);

                printf(	"   PFB conf misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                        demand_conf_alltype,
                        c->conf_blockmiss[D4PREFETCH + D4XINSTRN],
                        demand_conf_data,
                        c->conf_blockmiss[D4PREFETCH + D4XREAD],
                        c->conf_blockmiss[D4PREFETCH + D4XWRITE],
                        c->conf_blockmiss[D4PREFETCH + D4XMISC]);

                printf(	"   PFB comp fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                        demand_comp_alltype / NONZERO(prefetch_alltype),
                        c->comp_blockmiss[D4PREFETCH + D4XINSTRN] / NONZERO(c->blockmiss[D4PREFETCH + D4XINSTRN]),
                        demand_comp_data / NONZERO(prefetch_data),
                        c->comp_blockmiss[D4PREFETCH + D4XREAD] / NONZERO(c->blockmiss[D4PREFETCH + D4XREAD]),
                        c->comp_blockmiss[D4PREFETCH + D4XWRITE] / NONZERO(c->blockmiss[D4PREFETCH + D4XWRITE]),
                        c->comp_blockmiss[D4PREFETCH + D4XMISC] / NONZERO(c->blockmiss[D4PREFETCH + D4XMISC]));

                printf(	"   PFB cap fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                        demand_cap_alltype / NONZERO(prefetch_alltype),
                        c->cap_blockmiss[D4PREFETCH + D4XINSTRN] / NONZERO(c->blockmiss[D4PREFETCH + D4XINSTRN]),
                        demand_cap_data / NONZERO(prefetch_data),
                        c->cap_blockmiss[D4PREFETCH + D4XREAD] / NONZERO(c->blockmiss[D4PREFETCH + D4XREAD]),
                        c->cap_blockmiss[D4PREFETCH + D4XWRITE] / NONZERO(c->blockmiss[D4PREFETCH + D4XWRITE]),
                        c->cap_blockmiss[D4PREFETCH + D4XMISC] / NONZERO(c->blockmiss[D4PREFETCH + D4XMISC]));

                printf(	"   PFB conf fract	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                        demand_conf_alltype / NONZERO(prefetch_alltype),
                        c->conf_blockmiss[D4PREFETCH + D4XINSTRN] / NONZERO(c->blockmiss[D4PREFETCH + D4XINSTRN]),
                        demand_conf_data / NONZERO(prefetch_data),
                        c->conf_blockmiss[D4PREFETCH + D4XREAD] / NONZERO(c->blockmiss[D4PREFETCH + D4XREAD]),
                        c->conf_blockmiss[D4PREFETCH + D4XWRITE] / NONZERO(c->blockmiss[D4PREFETCH + D4XWRITE]),
                        c->conf_blockmiss[D4PREFETCH + D4XMISC] / NONZERO(c->blockmiss[D4PREFETCH + D4XMISC]));
            }

            printf(	" Total Block Misses	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f	%12.0f\n",
                    demand_alltype + prefetch_alltype,
                    c->blockmiss[D4XINSTRN] + c->blockmiss[D4PREFETCH + D4XINSTRN],
                    demand_data + prefetch_data,
                    c->blockmiss[D4XREAD] + c->blockmiss[D4PREFETCH + D4XREAD],
                    c->blockmiss[D4XWRITE] + c->blockmiss[D4PREFETCH + D4XWRITE],
                    c->blockmiss[D4XMISC] + c->blockmiss[D4PREFETCH + D4XMISC]);

            printf(	"  Tot blk miss rate	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f	%12.4f\n",
                    (demand_alltype + prefetch_alltype) / NONZERO(demand_fetch_alltype + prefetch_fetch_alltype),
                    (c->blockmiss[D4XINSTRN] + c->blockmiss[D4PREFETCH + D4XINSTRN]) / NONZERO(c->fetch[D4XINSTRN] + c->fetch[D4PREFETCH + D4XINSTRN]),
                    (demand_data + prefetch_data) / NONZERO(demand_fetch_data + prefetch_fetch_data),
                    (c->blockmiss[D4XREAD] + c->blockmiss[D4PREFETCH + D4XREAD]) / NONZERO(c->fetch[D4XREAD] + c->fetch[D4PREFETCH + D4XREAD]),
                    (c->blockmiss[D4XWRITE] + c->blockmiss[D4PREFETCH + D4XWRITE]) / NONZERO(c->fetch[D4XWRITE] + c->fetch[D4PREFETCH + D4XWRITE]),
                    (c->blockmiss[D4XMISC] + c->blockmiss[D4PREFETCH + D4XMISC]) / NONZERO(c->fetch[D4XMISC] + c->fetch[D4PREFETCH + D4XMISC]));
        } /* End of prefetch block misses */
        printf("\n");
    } /* End of block misses */
    /*
     * End of Block Misses Numbers
     */

    /*
     * Report multiblock and traffic to/from memory
     */
    printf( " Multi-block refs      %12.0f\n",
            c->multiblock);
    printf(	" Bytes From Memory	%12.0f\n",
            c->bytes_read);
    printf(	" ( / Demand Fetches)	%12.4f\n",
            c->bytes_read / NONZERO(demand_fetch_alltype));
    printf(	" Bytes To Memory	%12.0f\n",
            c->bytes_written);
    printf(	" ( / Demand Writes)	%12.4f\n",
            c->bytes_written / NONZERO(c->fetch[D4XWRITE]));
    printf(	" Total Bytes r/w Mem	%12.0f\n",
            c->bytes_read + c->bytes_written);
    printf(	" ( / Demand Fetches)	%12.4f\n",
            (c->bytes_read + c->bytes_written) / NONZERO(demand_fetch_alltype));
    printf("\n");
}

#undef NONZERO


/*
 * Called to produce each address trace record
 */
D4MemRef
next_trace_item()
{
    D4MemRef r;
    static int once = 1;
    static int discard = 0;
    static int hastoggled = 0;

    if (once) {
        once = 0;
        if (g_d4opt.on_trigger != 0) {
            discard = 1;    /* initially discard until trigger address seen */
        }
        if (g_d4opt.skipcount > 0) {
            uint64_t tskipcount = g_d4opt.skipcount;
            do {
                r = input_function();
                if (r.accesstype == D4TRACE_END) {
                    fprintf (stderr, "%s warning: input ended "
                             "before -skipcount satisfied\n", g_d4opt.progname);
                    return r;
                }
            } while ((tskipcount -= 1) > 0);
        }
    }
    while (1) {
        r = input_function();
        if (r.accesstype == D4TRACE_END) {
            if ((g_d4opt.on_trigger != 0 || g_d4opt.off_trigger != 0) && !hastoggled)
                fprintf (stderr, "%s warning: trace discard "
                         "trigger addresses were not matched\n", g_d4opt.progname);
            else if (discard == 0 && g_d4opt.off_trigger != 0) {
                fprintf (stderr, "%s warning: tail end of trace not discarded\n",
                         g_d4opt.progname);
            }
            return r;
        }
        if (r.address != 0) {	/* valid triggers must be != 0 */
            if ((discard != 0 && g_d4opt.on_trigger == r.address) ||
                    (discard == 0 && g_d4opt.off_trigger == r.address)) {
                discard ^= 1;	/* toggle */
                hastoggled = 1;
                continue;	/* discard the trigger itself */
            }
        }
        if (!discard) {
            return r;
        }
    }
}


/*
 * Called to initialize all caches based on args
 * Die with an error message if there are serious problems.
 */
void
initialize_caches (D4Cache **icachep, D4Cache **dcachep)
{
    static char memname[] = "memory";
    int i, lev, idu;
    D4Cache	*c = NULL,	/* avoid `may be used uninitialized' warning in gcc */
             *ci,
             *cd;

    mem = cd = ci = d4new(NULL);
    if (ci == NULL) {
        die ("cannot create simulated memory\n");
    }
    ci->name = memname;

    for (lev = g_d4opt.maxlevel - 1;  lev >= 0;  lev--) {
        for (idu = 0;  idu < 3;  idu++) {
            if (g_d4opt.level_size[idu][lev] != 0) {
                switch (idu) {
                case 0:
                    cd = ci = c = d4new (ci);
                    break;	/* u */
                case 1:
                    ci = c = d4new (ci);
                    break;	/* i */
                case 2:
                    cd = c = d4new (cd);
                    break;	/* d */
                }
                if (c == NULL)
                    die ("cannot create level %d %ccache\n",
                         lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
                init_1cache (c, lev, idu);
                levcache[idu][lev] = c;
            }
        }
    }
    i = d4setup();
    if (i != 0) {
        die ("cannot complete cache initializations; d4setup = %d\n", i);
    }
    *icachep = ci;
    *dcachep = cd;
}


/*
 * Everything starts here
 */
int
main (int argc, char **argv)
{
    D4MemRef r;
    D4Cache *ci, *cd;
    uint64_t tmaxcount = 0, tintcount;
    uint64_t flcount;

    doargs (argc, argv);
    verify_options();
    initialize_caches (&ci, &cd);

    if (cd == NULL) {
        cd = ci;    /* for unified L1 cache */
    }

    printf ("---Dinero IV cache simulator, version %s\n", D4VERSION);
    printf ("---Written by Jan Edler and Mark D. Hill\n");
    printf ("---Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.\n");
    printf ("---All rights reserved.\n");
    printf ("---Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.\n");
    printf ("---See -copyright option for details\n");

    summarize_caches();

    printf ("\n---Simulation begins.\n");
    tintcount = g_d4opt.stat_interval;
    flcount = g_d4opt.flushcount;
    while (1) {
        r = next_trace_item();
        if (r.accesstype == D4TRACE_END) {
            goto done;
        }
        if (g_d4opt.maxcount != 0 && tmaxcount >= g_d4opt.maxcount) {
            printf ("---Maximum address count exceeded.\n");
            break;
        }
        switch (r.accesstype) {
        case D4XINSTRN:
            d4ref (ci, r);
            break;
        case D4XINVAL:
            d4ref (ci, r);  /* fall through */
        default:
            d4ref (cd, r);
            break;
        }
        tmaxcount += 1;
        if (tintcount > 0 && (tintcount -= 1) <= 0) {
            dostats();
            tintcount = g_d4opt.stat_interval;
        }
        if (flcount > 0 && (flcount -= 1) <= 0) {
            /* flush cache = copy back and invalidate */
            r.accesstype = D4XCOPYB;
            r.address = 0;
            r.size = 0;
            d4ref (cd, r);
            r.accesstype = D4XINVAL;
            d4ref (ci, r);
            if (ci != cd) {
                d4ref (cd, r);
            }
            flcount = g_d4opt.flushcount;
        }
    }
done:
    /* copy everything back at the end -- is this really a good idea? XXX */
    r.accesstype = D4XCOPYB;
    r.address = 0;
    r.size = 0;
    d4ref (cd, r);
    printf ("---Simulation complete.\n");
    dostats();
    printf ("---Execution complete.\n");
    return 0;
}
