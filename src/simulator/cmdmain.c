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
#include "result.h"


/* some global variables */
static D4Cache *levcache[3][MAX_LEV];		/* to locate cache by level and type */

/* private prototypes for this file */
extern D4MemRef next_trace_item (void);


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
 * Everything starts here
 */
int
main (int argc, char **argv)
{
    D4MemRef r;
    D4Cache *ci, *cd;
    D4Cache *mem;				/* which cache represents simulated memory? */
    uint64_t tmaxcount = 0, tintcount;
    uint64_t flcount;

    doargs (argc, argv);
    verify_options();
    initialize_caches (levcache, &ci, &cd, &mem);

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
            dostats(levcache);
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
    dostats(levcache);
    printf ("---Execution complete.\n");
    return 0;
}
