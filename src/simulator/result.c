#include <stdio.h>
#include <stdlib.h>

#include "d4.h"
#include "result.h"

/*
 * Print out the stuff the user really wants
 */
void
dostats(D4Cache *levcache[3][MAX_LEV])
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
do1stats(const D4Cache *c)
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


