/*
 * Command line argument stuff for Dinero IV.
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

#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d4.h"
#include "cmdd4.h"
#include "cmdargs.h"
#include "tracein.h"


/*
 * The idea is to confine as much argument knowledge as possible to this file.
 * Generic argument handling functions are in cmdmain.c
 */

/*
 * Define the variables and arrays to receive command line argument values.
 * Those coresponding to arguments with -ln-idu prefixes are actually
 * 2-d arrays; for the 1st index 0=unified, 1=instruction, and 2=data.
 * The 2nd index is the level - 1.
 *
 * The basic strategy is to process all the command line args, fill in these
 * variables and arrays, check them for consistency, and then
 * allocate and initialize all necessary cache structures for simulation.
 *
 * For those options subject to customization, the customized version
 * of the array is defined elsewhere.
 */
/* keep value parsed from command argument string */
unsigned int level_blocksize[3][MAX_LEV];
unsigned int level_subblocksize[3][MAX_LEV];
unsigned int level_size[3][MAX_LEV];
unsigned int level_assoc[3][MAX_LEV];
int level_doccc[3][MAX_LEV];
int level_replacement[3][MAX_LEV];
int level_fetch[3][MAX_LEV];
int level_walloc[3][MAX_LEV];
int level_wback[3][MAX_LEV];
int level_prefetch_abortpercent[3][MAX_LEV];
int level_prefetch_distance[3][MAX_LEV];

static int optstringmax;     /** the max length of string in help_* functions */

D4Option g_d4opt;            /** the global Option singleton */

/*
 * command line defaults.
 * Make sure the DEFVAL and DEFSTR versions match
 * We don't really have to use fancy macro stuff for this do we?
 */
#define DEFVAL_assoc 1
#define  DEFSTR_assoc "1"
#define DEFVAL_repl 'l'
#define  DEFSTR_repl "l"
#define DEFVAL_fetch 'd'
#define  DEFSTR_fetch "d"
#define DEFVAL_walloc 'a'
#define  DEFSTR_walloc "a"
#define DEFVAL_wback 'a'
#define  DEFSTR_wback "a"
#define DEFVAL_informat 'D'
#define  DEFSTR_informat "D"

int informat = DEFVAL_informat;
long on_trigger;
long off_trigger;
int stat_idcombine;

/*
 * Produce a help line for a possible command line option with
 * a single char value.
 */
static void
help_char (const D4ArgList *adesc)
{
    printf ("%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "C",
            adesc->helpstring);
}


/*
 * Special help routine for -informat.
 * List the choices on subsequent lines.
 */
static void
help_informat (const D4ArgList *adesc)
{
    help_char (adesc);
    help_trace_format (optstringmax);	/* look for this in tracein.c */
}


/*
 * Generic functions for handling command line arguments.
 * Most argument-specific knowledge is in cmdargs.c.
 */

/*
 * Get the level and idu portion of a -ln-idu prefix.
 * The return value is a pointer to the next character after the prefix,
 * or NULL if the prefix is not recognized.
 */
static char *
level_idu (const char *opt, int *levelp, int *idup)
{
    int level;
    char *nextc;

    if (*opt++ != '-' || *opt++ != 'l') {
        return NULL;    /* no initial -l */
    }
    if (*opt == '-' || *opt == '+') {
        return NULL;    /* we don't accept a sign here */
    }
    level = strtol (opt, &nextc, 10);
    if (nextc == opt) {
        return NULL;    /* no digits */
    }
    if (level <= 0 || level > MAX_LEV) {
        return NULL;    /* level out of range */
    }
    if (*nextc++ != '-') {	/* missing - after level */
        return NULL;
    }
    switch (*nextc++) {
    default:
        return NULL;	/* bad idu value */
    case 'u':
        *idup = 0;
        break;
    case 'i':
        *idup = 1;
        break;
    case 'd':
        *idup = 2;
        break;
    }
    *levelp = level - 1;
    if (level > g_d4opt.maxlevel) {
        g_d4opt.maxlevel = level;
    }
    return nextc;
}


/*
 * Helper for scaled unsigned integer arguments
 */
static unsigned int
argscale_uint (const char *arg, unsigned int *var)
{
    char *nextc;
    unsigned long x;

    errno = 0;
    if (!isdigit ((int)*arg)) {
        return 0;    /* no good: doesn't start with a number */
    }
    x = strtoul (arg, &nextc, 10);
    if (nextc == arg) {
        return 0;    /* no good: no chars consumed */
    }
    if (x == ULONG_MAX && errno == ERANGE) {
        return 0;    /* no good: overflow */
    }
    switch (nextc[0]) {
    default:
        return 0;		/* no good; some other trailing char */
    case 0:
        break;		/* ok: no scale factor */
    case 'k':
    case 'K':
        if ((x >> (sizeof(x)*CHAR_BIT - 10)) != 0) {
            return 0;    /* no good: overflow */
        }
        x <<= 10;
        break;
    case 'm':
    case 'M':
        if ((x >> (sizeof(x)*CHAR_BIT - 20)) != 0) {
            return 0;    /* no good: overflow */
        }
        x <<= 20;
        break;
    case 'g':
    case 'G':
        if ((x >> (sizeof(x)*CHAR_BIT - 30)) != 0) {
            return 0;    /* no good: overflow */
        }
        x <<= 30;
        break;
    }
    if (nextc[0] != 0 && nextc[1] != 0) {
        return 0;    /* no good: trailing junk */
    }
    *var = x;
    return 1;
}


/**
 * @brief Helper for scaled unsigned integer arguments, but using double for extra range
 *        We go through various difficulties just to allow use of strtod
 *        while still preventing non-integer use.
 * @param[in] arg argument string
 * @param[in,out] var pointer to keep the value
 *
 * @return  0 -> success !0 -> failed
 */
static int
argscale_uintd (const char *arg, uint64_t *var)
{
    char *nextc;
    double x, ipart;

    errno = 0;
    x = strtod (arg, &nextc);
    if (nextc == arg) {
        return 1;    /* no good: no chars consumed */
    }
    if (x == HUGE_VAL && errno == ERANGE) {
        return 1;    /* no good: overflow */
    }
    /* make sure value is an integer */
    switch (nextc[0]) {
    case 'T':
        x *= 1024;
        /* fall through */
    case 'g':
    case 'G':
        x *= 1024;
        /* fall through */
    case 'm':
    case 'M':
        x *= 1024;
        /* fall through */
    case 'k':
    case 'K':
        x *= 1024;
        /* fall through */
    case 0:
        break;		/* ok: no scale factor */
    default:
        return 1;		/* no good; some other trailing char */
    }
    if (nextc[0] != 0 && nextc[1] != 0) {
        return 0;    /* no good: trailing junk */
    }
    /* make sure number was unsigned integer; no decimal pt, exponent, or sign */
    for (;  arg < nextc;  arg++)
        if (!isdigit ((int)*arg)) {
            return 1;    /* no good: non-digits */
        }
    /* make sure we don't have a fractional part due to scaling */
    if (modf (x, &ipart) != 0) {
        return 0;    /* no good: fraction != 0 */
    }

    *var = x;
    return 0;
}


/*
 * Recognize an option with no args
 */
static int
match_0arg (const char *opt, const D4ArgList *adesc)
{
    return strcmp (opt, adesc->optstring) == 0;
}


/*
 * Recognize an option with no args and the -ln-idu prefix
 */
static int
pmatch_0arg (const char *opt, const D4ArgList *adesc)
{
    int level;
    int idu;
    const char *nextc = level_idu (opt, &level, &idu);
    if (nextc == NULL) {
        return 0;    /* not recognized */
    }
    return 1 * (strcmp (nextc, adesc->optstring) == 0);
}


/*
 * Recognize an option with 1 arg
 */
static int
match_1arg (const char *opt, const D4ArgList *adesc)
{
    return 2 * (strcmp (opt, adesc->optstring) == 0);
}


/*
 * Recognize an option with 1 arg and the -ln-idu prefix
 */
static int
pmatch_1arg (const char *opt, const D4ArgList *adesc)
{
    int level;
    int idu;
    const char *nextc = level_idu (opt, &level, &idu);
    if (nextc == NULL) {
        return 0;    /* not recognized */
    }
    return 2 * (strcmp (nextc, adesc->optstring) == 0);
}

static D4ArgList args[];   /** forward declaration */
/**
 * @brief callback function for D4ArgList to
 *        Produce help message in response to -help
 *        The function exits directly
 * @param[in] opt unused
 * @param[in] arg unused
 * @param[in] adesc unused
 */
static void
val_help (const char *opt, const char *arg, const D4ArgList *adesc)
{
    const D4ArgList *iter;

    printf ("Usage: %s [options]\nValid options:\n", progname);

    for (iter = args;  iter != NULL;  iter++) {
        if (iter->help != NULL) {
            putchar (' ');
            iter->help (iter);
            if (iter->defstr != NULL) {
                printf (" (default %s)", iter->defstr);
            }
            putchar ('\n');
        }
    }
    printf ("Key:\n");
    printf (" U unsigned decimal integer\n");
    printf (" S like U but with optional [kKmMgG] scaling suffix\n");
    printf (" P like S but must be a power of 2\n");
    printf (" C single character\n");
    printf (" A hexadecimal address\n");
    printf (" F string\n");
    printf (" N cache level (1 <= N <= %d)\n", MAX_LEV);
    printf (" T cache type (u=unified, i=instruction, d=data)\n");
    exit(0);
}


/*
 * @brief Produce help message in response to -copyright
 * @param[in] opt unused
 * @param[in] arg unused
 * @param[in] adesc unused
 */
static void
val_helpcr (const char *opt, const char *arg, const D4ArgList *adesc)
{
    printf ("Dinero IV is copyrighted software\n");
    printf ("\n");
    printf ("Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.\n");
    printf ("All rights reserved.\n");
    printf ("Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.\n");
    printf ("\n");
    printf ("Permission to use, copy, modify, and distribute this software and\n");
    printf ("its associated documentation for non-commercial purposes is hereby\n");
    printf ("granted (for commercial purposes see below), provided that the above\n");
    printf ("copyright notice appears in all copies, derivative works or modified\n");
    printf ("versions of the software and any portions thereof, and that both the\n");
    printf ("copyright notice and this permission notice appear in the documentation.\n");
    printf ("NEC Research Institute Inc. and Mark D. Hill shall be given a copy of\n");
    printf ("any such derivative work or modified version of the software and NEC\n");
    printf ("Research Institute Inc.  and any of its affiliated companies (collectively\n");
    printf ("referred to as NECI) and Mark D. Hill shall be granted permission to use,\n");
    printf ("copy, modify, and distribute the software for internal use and research.\n");
    printf ("The name of NEC Research Institute Inc. and its affiliated companies\n");
    printf ("shall not be used in advertising or publicity related to the distribution\n");
    printf ("of the software, without the prior written consent of NECI.  All copies,\n");
    printf ("derivative works, or modified versions of the software shall be exported\n");
    printf ("or reexported in accordance with applicable laws and regulations relating\n");
    printf ("to export control.  This software is experimental.  NECI and Mark D. Hill\n");
    printf ("make no representations regarding the suitability of this software for\n");
    printf ("any purpose and neither NECI nor Mark D. Hill will support the software.\n");
    printf ("\n");
    printf ("Use of this software for commercial purposes is also possible, but only\n");
    printf ("if, in addition to the above requirements for non-commercial use, written\n");
    printf ("permission for such use is obtained by the commercial user from NECI or\n");
    printf ("Mark D. Hill prior to the fabrication and distribution of the software.\n");
    printf ("\n");
    printf ("THE SOFTWARE IS PROVIDED AS IS.  NECI AND MARK D. HILL DO NOT MAKE\n");
    printf ("ANY WARRANTEES EITHER EXPRESS OR IMPLIED WITH REGARD TO THE SOFTWARE.\n");
    printf ("NECI AND MARK D. HILL ALSO DISCLAIM ANY WARRANTY THAT THE SOFTWARE IS\n");
    printf ("FREE OF INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS OF OTHERS.\n");
    printf ("NO OTHER LICENSE EXPRESS OR IMPLIED IS HEREBY GRANTED.  NECI AND MARK\n");
    printf ("D. HILL SHALL NOT BE LIABLE FOR ANY DAMAGES, INCLUDING GENERAL, SPECIAL,\n");
    printf ("INCIDENTAL, OR CONSEQUENTIAL DAMAGES, ARISING OUT OF THE USE OR INABILITY\n");
    printf ("TO USE THE SOFTWARE.\n");
    exit(0);
}


/**
 * @brief Produce help message in response to -contact
 * @param[in] opt unused
 * @param[in] arg unused
 * @param[in] adesc unused
 */
static void
val_helpw (const char *opt, const char *arg, const D4ArgList *adesc)
{
    printf ("Dinero IV was written by Jan Edler and Mark D. Hill.\n");
    printf ("\n");
    printf ("    Dr. Jan Edler                    Prof. Mark D. Hill\n");
    printf ("    NEC Research Institute           Computer Sciences Dept.\n");
    printf ("    4 Independence Way               Univ. of Wisconsin\n");
    printf ("    Princeton, NJ 08540              Madison, WI 53706\n");
    printf ("    edler@research.nj.nec.com        markhill@cs.wisc.edu\n");
    printf ("    edler@acm.org\n");
    printf ("    edler@computer.org\n");
    printf ("\n");
    printf ("The latest version of Dinero IV can be obtained\n");
    printf ("from the following places:\n");
    printf ("       http://www.neci.nj.nec.com/homepages/edler/d4\n");
    printf ("       http://www.cs.wisc.edu/~markhill/DineroIV\n");
    printf ("       ftp://ftp.nj.nec.com/pub/edler/d4\n");
    printf ("       ftp://ftp.cs.wisc.edu/markhill/DineroIV\n");
    printf ("Copyright terms are explained by the -copyright option.\n");
    exit(0);
}


/**
 * @brief Explain DineroIII->DineroIV option mappings
 * @param[in] opt unused
 * @param[in] arg unused
 * @param[in] adesc unused
 */
static void
val_helpd3 (const char *opt, const char *arg, const D4ArgList *adesc)
{
    printf ("Summary of DineroIV replacements for DineroIII options:\n\n");
    printf ("DineroIII    DineroIV\n");
    printf ("-u           -l1-usize\n");
    printf ("-i           -l1-isize\n");
    printf ("-d           -l1-dsize\n");
    printf ("-b           -l1-ubsize, -l1-ibsize, or -l1-dbsize\n");
    printf ("-S           -l1-usbsize, -l1-isbsize, or -l1-dsbsize\n");
    printf ("-a           -l1-uassoc, -l1-iassoc, or -l1-dassoc\n");
    printf ("-r           -l1-urepl, -l1-irepl, or -l1-drepl\n");
    printf ("-f           -l1-ufetch, -l1-ifetch, or -l1-dfetch\n");
    printf ("-p           -l1-upfdist, -l1-ipfdist, or -l1-dpfdist\n");
    printf ("-P           -l1-upfabort, -l1-ipfabort, or -l1-dpfabort\n");
    printf ("-w           -l1-uwback or -l1-dwback\n");
    printf ("-A           -l1-uwalloc or -l1-dwalloc\n");
    printf ("-Q           -flushcount\n");
    printf ("-z           -maxcount\n");
    printf ("-Z           -skipcount\n");
    printf ("\n");
    printf ("DineroIII input format: -informat d\n");
    exit(0);
}


/*
 * Handle an option with no args (i.e., a boolean option).
 */
static void
val_0arg (const char *opt, const char *arg, const D4ArgList *adesc)
{
    *(int *) adesc->var = 1;
}


/*
 * Handle an option with no args (i.e., a boolean option) and the -ln-idu-prefix.
 */
static void
pval_0arg (const char *opt, const char *arg, const D4ArgList *adesc)
{
    int (*var)[3][MAX_LEV] = adesc->var;
    int level;
    int idu;

    (void) level_idu (opt, &level, &idu);
    (*var)[idu][level] = 1;
}


/*
 * Handle -ln-idu-stuff with unsigned integer arg.
 * Note the match function has already verified the form,
 * so level_idu() can't return NULL.
 */
static void
pval_uint (const char *opt, const char *arg, const D4ArgList *adesc)
{
    unsigned int (*var)[3][MAX_LEV] = adesc->var;
    unsigned int argui;
    int level;
    int idu;
    char *nextc;

    (void) level_idu (opt, &level, &idu);
    argui = strtoul (arg, &nextc, 10);
    if (*nextc != 0) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
    (*var)[idu][level] = argui;
}


/*
 * Handle unsigned integer arg with optional scaling (k,m,g, etc).
 * Here we use double for extra integer range.
 */
static void
val_scale_uintd (const char *opt, const char *arg, const D4ArgList *adesc)
{
    if (argscale_uintd (arg, adesc->var)) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
}


/*
 * Handle -ln-idu-stuff with unsigned integer arg restricted to
 * a power of 2 and with optional scaling (k,m,g, etc).
 * Note the match function has already verified the form,
 * so level_idu() can't return NULL.
 */
static void
pval_scale_pow2 (const char *opt, const char *arg, const D4ArgList *adesc)
{
    unsigned int (*var)[3][MAX_LEV] = adesc->var;
    unsigned int argui;
    int level;
    int idu;

    (void) level_idu (opt, &level, &idu);
    if (!argscale_uint (arg, &argui)) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
    if (argui == 0 || (argui & (argui - 1)) != 0) {
        shorthelp ("option %s arg must be power of 2\n", opt);
    }
    (*var)[idu][level] = argui;
}


/*
 * Handle an option with a single character as arg
 */
static void
val_char (const char *opt, const char *arg, const D4ArgList *adesc)
{
    int *var = adesc->var;

    if (strlen (arg) != 1) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
    *var = *arg;
}


/*
 * Handle an option with level/idu prefix and a single character as arg
 */
static void
pval_char (const char *opt, const char *arg, const D4ArgList *adesc)
{
    int (*var)[3][MAX_LEV] = adesc->var;
    int level;
    int idu;

    (void) level_idu (opt, &level, &idu);
    if (strlen (arg) != 1) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
    (*var)[idu][level] = *arg;
}


/*
 * Handle an option with a hexadecimal address as arg.
 */
static void
val_addr (const char *opt, const char *arg, const D4ArgList *adesc)
{
    long *var = adesc->var;
    long argl;
    char *nextc;

    argl = strtoul (arg, &nextc, 16);
    if (*nextc != 0) {
        shorthelp ("bad option: %s %s\n", opt, arg);
    }
    *var = argl;
}


/*
 * Output an array initializer for the level/idu array whose name is given
 * in the customstring field of the arglist structure,
 * when the values in question are boolean.
 */
static void
pcustom_0arg (const D4ArgList *adesc, FILE *hfile)
{
    int i, j;
    int (*var)[3][MAX_LEV] = adesc->var;

    fprintf (hfile, "int %s[3][MAX_LEV] = {\n",
             adesc->customstring);
    for (i = 0;  i < 3;  i++) {
        fprintf (hfile, " { ");
        for (j = 0;  j < g_d4opt.maxlevel;  j++)
            fprintf (hfile, "%d%s ", (*var)[i][j],
                     j < g_d4opt.maxlevel - 1 ? "," : "");
        fprintf (hfile, "}%s\n", i < 2 ? "," : "");
    }
    fprintf (hfile, "};\n");
}


/*
 * Output an array initializer for the level/idu array whose name is given
 * in the customstring field of the arglist structure,
 * when the values in question are unsigned ints.
 */
static void
pcustom_uint (const D4ArgList *adesc, FILE *hfile)
{
    int i, j;
    unsigned int (*var)[3][MAX_LEV] = adesc->var;

    fprintf (hfile, "unsigned int %s[3][MAX_LEV] = {\n",
             adesc->customstring);
    for (i = 0;  i < 3;  i++) {
        fprintf (hfile, " { ");
        for (j = 0;  j < g_d4opt.maxlevel;  j++)
            fprintf (hfile, "%u%s ", (*var)[i][j],
                     j < g_d4opt.maxlevel - 1 ? "," : "");
        fprintf (hfile, "}%s\n", i < 2 ? "," : "");
    }
    fprintf (hfile, "};\n");
}


/*
 * Output an array initializer for the level/idu array whose name is given
 * in the customstring field of the arglist structure,
 * when the values in question are chars.
 */
static void
pcustom_char (const D4ArgList *adesc, FILE *hfile)
{
    int i, j;
    int (*var)[3][MAX_LEV] = adesc->var;

    fprintf (hfile, "int %s[3][MAX_LEV] = {\n",
             adesc->customstring);
    for (i = 0;  i < 3;  i++) {
        fprintf (hfile, " { ");
        for (j = 0;  j < g_d4opt.maxlevel;  j++)
            fprintf (hfile, "%d%s ", (*var)[i][j],
                     j < g_d4opt.maxlevel - 1 ? "," : "");
        fprintf (hfile, "}%s\n", i < 2 ? "," : "");
    }
    fprintf (hfile, "};\n");
}


/*
 * Produce a summary line for parameters with no arg (i.e., boolean parameters).
 */
static void
summary_0arg (const D4ArgList *adesc, FILE *f)
{
    if (*(int *)adesc->var != 0) {
        fprintf (f, "%s\n", adesc->optstring);
    }
}


/*
 * Produce a summary line for parameters with level/idu prefix but no arg.
 */
static void
psummary_0arg (const D4ArgList *adesc, FILE *f)
{
    int idu, lev;
    int (*var)[3][MAX_LEV] = adesc->var;

    for (idu = 0;  idu < 3;  idu++) {
        for (lev = 0;  lev <= g_d4opt.maxlevel;  lev++) {
            if ((*var)[idu][lev] != 0)
                fprintf (f, "-l%d-%c%s\n", lev + 1,
                         idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'),
                         adesc->optstring);
        }
    }
}


/*
 * Produce a summary line for parameters with typical unsigned integer value,
 * handled as a double for extended range.
 */
static void
summary_uintd (const D4ArgList *adesc, FILE *f)
{
    fprintf (f, "%s %" PRIu64 "\n", adesc->optstring, *(uint64_t *)adesc->var);
}


/*
 * Produce a summary line for parameters with level/idu prefix and typical
 * unsigned integer value.
 */
static void
psummary_uint (const D4ArgList *adesc, FILE *f)
{
    int idu, lev;
    unsigned int (*var)[3][MAX_LEV] = adesc->var;

    for (idu = 0;  idu < 3;  idu++) {
        for (lev = 0;  lev <= g_d4opt.maxlevel;  lev++) {
            if ((*var)[idu][lev] != 0) {
                fprintf (f, "-l%d-%c%s %u\n", lev + 1,
                         idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'),
                         adesc->optstring, (*var)[idu][lev]);
            }
        }
    }
}


/*
 * Produce a summary line for parameters with level/idu prefix and typical
 * unsigned power-of-2 integer value, remembered as its log.
 */
static void
psummary_luint (const D4ArgList *adesc, FILE *f)
{
    int idu, lev;
    unsigned int (*var)[3][MAX_LEV] = adesc->var;

    for (idu = 0;  idu < 3;  idu++) {
        for (lev = 0;  lev <= g_d4opt.maxlevel;  lev++) {
            if ((*var)[idu][lev] != 0) {
                fprintf (f, "-l%d-%c%s %u\n", lev + 1,
                         idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'),
                         adesc->optstring,
                         (*var)[idu][lev]);
            }
        }
    }
}


/*
 * Produce a summary line for parameters with typical single char argument.
 */
static void
summary_char (const D4ArgList *adesc, FILE *f)
{
    fprintf (f, "%s %c\n", adesc->optstring, *(int *)adesc->var);
}


/*
 * Produce a summary line for parameters with level/idu prefix and typical
 * single char value.
 */
static void
psummary_char (const D4ArgList *adesc, FILE *f)
{
    int idu, lev;
    int (*var)[3][MAX_LEV] = adesc->var;

    for (idu = 0;  idu < 3;  idu++) {
        for (lev = 0;  lev <= g_d4opt.maxlevel;  lev++) {
            if ((*var)[idu][lev] != 0) {
                fprintf (f, "-l%d-%c%s %c\n", lev + 1,
                         idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'),
                         adesc->optstring,
                         (*var)[idu][lev]);
            }
        }
    }
}


/*
 * Produce a summary line for parameters with typical hexadecimal address
 * as argument.
 */
static void
summary_addr (const D4ArgList *adesc, FILE *f)
{
    fprintf (f, "%s 0x%lx\n", adesc->optstring, *(long *)adesc->var);
}


/*
 * Produce a help line for a possible command line option taking no args.
 */
static void
help_0arg (const D4ArgList *adesc)
{
    printf ("%-*s %s", optstringmax, adesc->optstring, adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option with
 * level/idu prefix and no args.
 * We don't bother trying to show the default values; there are too many.
 */
static void
phelp_0arg (const D4ArgList *adesc)
{
    printf ("-lN-T%-*s %s", optstringmax - adesc->pad, adesc->optstring, adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option taking
 * an unsigned int value.
 */
#if 0
static void
help_uint (const D4ArgList *adesc)
{
    printf ("%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "U",
            adesc->helpstring);
}
#endif


/*
 * Produce a help line for a possible command line option taking
 * a scaled unsigned int value, but using double for extra range.
 */
static void
help_scale_uintd (const D4ArgList *adesc)
{
    printf ("%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "U",
            adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option with
 * level/idu prefix and unsigned int value.
 * We don't bother trying to show the default values; there are too many.
 */
static void
phelp_uint (const D4ArgList *adesc)
{
    printf ("-lN-T%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "U",
            adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option with
 * level/idu prefix and scaled power-of-2 unsigned int value.
 * We don't bother trying to show the default values; there are too many.
 */
static void
phelp_scale_pow2 (const D4ArgList *adesc)
{
    printf ("-lN-T%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "P",
            adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option with
 * level/idu prefix and single char value.
 * We don't bother trying to show the default values; there are too many.
 */
static void
phelp_char (const D4ArgList *adesc)
{
    printf ("-lN-T%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "C",
            adesc->helpstring);
}


/*
 * Produce a help line for a possible command line option with
 * a hexidecimal address value.
 */
static void
help_addr (const D4ArgList *adesc)
{
    printf ("%s %-*s %s", adesc->optstring,
            optstringmax - (int)strlen(adesc->optstring) - adesc->pad + 1, "A",
            adesc->helpstring);
    if (*(long *)adesc->var != 0) {
        printf (" (0x%lx)", *(long *)adesc->var);
    }
}



/*
 * Special help routine for -lN-Treplacement.
 * List the choices on subsequent lines.
 */
static void
phelp_replacement (const D4ArgList *adesc)
{
    phelp_char (adesc);
    printf ("\n %*s (l=LRU, f=FIFO, r=random)",
            optstringmax, " ");
}


/*
 * Special help routine for -lN-Tfetch.
 * List the choices on subsequent lines.
 */
static void
phelp_fetch (const D4ArgList *adesc)
{
    phelp_char (adesc);
    printf ("\n %*s (d=demand, a=always, m=miss, t=tagged,\n"
            " %*s  l=load forward, s=subblock)",
            optstringmax, " ", optstringmax, " ");
}


/*
 * Special help routine for -lN-Twalloc.
 * List the choices on subsequent lines.
 */
static void
phelp_walloc (const D4ArgList *adesc)
{
    phelp_char (adesc);
    printf ("\n %*s (a=always, n=never, f=nofetch)",
            optstringmax, " ");
}


/*
 * Special help routine for -lN-Twback.
 * List the choices on subsequent lines.
 */
static void
phelp_wback (const D4ArgList *adesc)
{
    phelp_char (adesc);
    printf ("\n %*s (a=always, n=never, f=nofetch)",
            optstringmax, " ");
}


/*
 * Complain about an unspecified option
 */
static void
unspec (int lev, int idu, const char *name, void *var, const char *suggest)
{
    int iduchar = idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd');
    D4ArgList *argl;

    for (argl = args;  argl->var != var;  argl++)
        if (argl->optstring == NULL) {
            die ("internal problem with arglist table\n");
        }

    fprintf (stderr, "level %d %ccache %s must be specified, "
             "e.g., -l%d-%c%s %s\n",
             lev + 1, iduchar, name, lev + 1, iduchar, argl->optstring, suggest);
}


/* Initialize argument table to specify acceptable arguments */
static D4ArgList args[] = {
    {
        "-help", 0, NULL, NULL,
        NULL,
        "Print this help message",
        match_0arg, val_help, NULL,
        NULL, help_0arg
    },
    {
        "-copyright", 0, NULL, NULL,
        NULL,
        "Give details on copyright and lack of warranty",
        match_0arg, val_helpcr, NULL,
        NULL, help_0arg
    },
    {
        "-contact", 0, NULL, NULL,
        NULL,
        "Where to get the latest version or contact the authors",
        match_0arg, val_helpw, NULL,
        NULL, help_0arg
    },
    {
        "-dineroIII", 0, NULL, NULL,
        NULL,
        "Explain replacements for Dinero III options",
        match_0arg, val_helpd3, NULL,
        NULL, help_0arg
    },
    {
        "size", 7, &level_size[0][0], NULL,
        "level_size",
        "Size",
        pmatch_1arg, pval_scale_pow2, pcustom_uint,
        psummary_luint, phelp_scale_pow2
    },
    {
        "bsize", 7, &level_blocksize[0][0], NULL,
        "level_blocksize",
        "Block size",
        pmatch_1arg, pval_scale_pow2, pcustom_uint,
        psummary_luint, phelp_scale_pow2
    },
    {
        "sbsize", 7, &level_subblocksize[0][0], "same as block size",
        "level_subblocksize",
        "Sub-block size",
        pmatch_1arg, pval_scale_pow2, pcustom_uint,
        psummary_luint, phelp_scale_pow2
    },
    {
        "assoc", 7, &level_assoc[0][0], DEFSTR_assoc,
        "level_assoc",
        "Associativity",
        pmatch_1arg, pval_uint, pcustom_uint,
        psummary_uint, phelp_uint
    },
    {
        "repl", 7, &level_replacement[0][0], DEFSTR_repl,
        "level_replacement",
        "Replacement policy",
        pmatch_1arg, pval_char, pcustom_char,
        psummary_char, phelp_replacement
    },
    {
        "fetch", 7, &level_fetch[0][0], DEFSTR_fetch,
        "level_fetch",
        "Fetch policy",
        pmatch_1arg, pval_char, pcustom_char,
        psummary_char, phelp_fetch
    },
    {
        "pfdist", 7, &level_prefetch_distance[0][0], "1",
        NULL,
        "Prefetch distance (in sub-blocks)",
        pmatch_1arg, pval_uint, NULL,
        psummary_uint, phelp_uint
    },
    {
        "pfabort", 7, &level_prefetch_abortpercent[0][0], "0",
        "level_prefetch_abortpercent",
        "Prefetch abort percentage (0-100)",
        pmatch_1arg, pval_uint, pcustom_uint,
        psummary_uint, phelp_uint
    },
    {
        "walloc", 7, &level_walloc[0][0], DEFSTR_walloc,
        "level_walloc",
        "Write allocate policy",
        pmatch_1arg, pval_char, pcustom_char,
        psummary_char, phelp_walloc
    },
    {
        "wback", 7, &level_wback[0][0], DEFSTR_wback,
        "level_wback",
        "Write back policy",
        pmatch_1arg, pval_char, pcustom_char,
        psummary_char, phelp_wback
    },
    {
        "ccc", 5, &level_doccc[0][0], NULL,
        "level_doccc",
        "Compulsory/Capacity/Conflict miss statistics",
        pmatch_0arg, pval_0arg, pcustom_0arg,
        psummary_0arg, phelp_0arg
    },
    {
        "-skipcount", 2, &g_d4opt.skipcount, NULL,
        NULL,
        "Skip initial U references",
        match_1arg, val_scale_uintd, NULL,
        summary_uintd, help_scale_uintd
    },
    {
        "-flushcount", 2, &g_d4opt.flushcount, NULL,
        NULL,
        "Flush cache every U references",
        match_1arg, val_scale_uintd, NULL,
        summary_uintd, help_scale_uintd
    },
    {
        "-maxcount", 2, &g_d4opt.maxcount, NULL,
        NULL,
        "Stop simulation after U references",
        match_1arg, val_scale_uintd, NULL,
        summary_uintd, help_scale_uintd
    },
    {
        "-stat-interval", 2, &g_d4opt.stat_interval, NULL,
        NULL,
        "Show statistics after every U references",
        match_1arg, val_scale_uintd, NULL,
        summary_uintd, help_scale_uintd
    },
    {
        "-informat", 2, &informat, DEFSTR_informat,
        NULL,
        "Input trace format",
        match_1arg, val_char, NULL,
        summary_char, help_informat
    },
    {
        "-on-trigger", 2, &on_trigger, NULL,
        NULL,
        "Trigger address to start simulation",
        match_1arg, val_addr, NULL,
        summary_addr, help_addr
    },
    {
        "-off-trigger", 2, &off_trigger, NULL,
        NULL,
        "Trigger address to stop simulation",
        match_1arg, val_addr, NULL,
        summary_addr, help_addr
    },
    {
        "-stat-idcombine", 2, &stat_idcombine, NULL,
        NULL,
        "Combine I&D cache stats",
        match_0arg, val_0arg, NULL,
        summary_0arg, help_0arg
    },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};


/*
 * Internal function to handle one command line option.
 * Return number of command line args consumed.
 * (Doesn't really support more than 1 or 2 consumed.)
 */
static int
do1arg (const char *opt, const char *arg)
{
    D4ArgList *adesc;

    for (adesc = args;  adesc->optstring != NULL;  adesc++) {
        int eaten = adesc->match (opt, adesc);
        if (eaten > 0) {
            if (eaten > 1 && (arg == NULL || *arg == '-')) {
                shorthelp ("\"%s\" option requires additional argument\n", opt);
            }
            adesc->valf (opt, arg, adesc);

            return eaten;
        }
    }

    /* does it look like a possible Dinero III option? */
    if (opt[0] == '-' && strchr ("uidbSarfpPwAQzZ", opt[1]) != NULL)
        shorthelp ("\"%s\" option not recognized for Dinero IV;\n"
                   "try \"%s -dineroIII\" for Dinero III --> IV option correspondence.\n",
                   opt, progname);

    shorthelp ("\"%s\" option not recognized.\n", opt);
    return 0;	/* can't really get here, but some compilers get upset if we don't have a return value */
}


/*
 * Process all the command line args
 */
void
doargs (int argc, char **argv)
{
    D4ArgList *adesc;
    char **v = argv + 1;
    int x;

    memset(&g_d4opt, 0, sizeof(D4Option));

    for (adesc = args;  adesc->optstring != NULL;  adesc++) {
        if (optstringmax < (int)strlen(adesc->optstring) + adesc->pad) {
            optstringmax = strlen(adesc->optstring) + adesc->pad;
        }
    }

    while (argc > 1) {
        const char *opt = v[0];
        const char *arg = (argc > 1) ? v[1] : NULL;
        x = do1arg (opt, arg);
        v += x;
        argc -= x;
    }
}

/*
 * @brief Print info about how the caches are set up
 */
void
summarize_caches (void)
{
    D4ArgList *adesc;

    printf ("\n---Summary of options "
            "(-help option gives usage information).\n\n");

    for (adesc = args;  adesc->optstring != NULL;  adesc++)
        if (adesc->sumf != (void (*)())NULL) {
            adesc->sumf (adesc, stdout);
        }
}


/*
 * Called after all the options and args are consumed.
 * Check them for consistency and reasonableness.
 * Die with an error message if there are serious problems.
 */
void
verify_options()
{
    int lev, idu;

    /*
     * Allow some default values
     *	subblocksize (default to blocksize)
     *	prefetch distance (default to 1)
     *	other defaults defined as DEFVAL_xxx
     */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            if (level_blocksize[idu][lev] != 0 && level_subblocksize[idu][lev] == 0) {
                level_subblocksize[idu][lev] = level_blocksize[idu][lev];
            }
            if (level_size[idu][lev] != 0 && level_assoc[idu][lev] == 0) {
                level_assoc[idu][lev] = DEFVAL_assoc;
            }
            if (level_size[idu][lev] != 0 && level_replacement[idu][lev] == 0) {
                level_replacement[idu][lev] = DEFVAL_repl;
            }
            if (level_size[idu][lev] != 0 && level_fetch[idu][lev] == 0) {
                level_fetch[idu][lev] = DEFVAL_fetch;
            }
            if (level_size[idu][lev] != 0 &&
                    level_fetch[idu][lev] != 'd' && level_prefetch_distance[idu][lev] == 0) {
                level_prefetch_distance[idu][lev] = 1;
            }
            if (idu != 1 && level_size[idu][lev] != 0 && level_walloc[idu][lev] == 0) {
                level_walloc[idu][lev] = DEFVAL_walloc;
            }
            if (idu != 1 && level_size[idu][lev] != 0 && level_wback[idu][lev] == 0) {
                level_wback[idu][lev] = DEFVAL_wback;
            }
        }
    }

    /*
     * check for missing required parameters
     */
    if (g_d4opt.maxlevel <= 0)
        shorthelp ("cache size and block size must be specified,\n"
                   "e.g.: -l1-isize 16k -l1-dsize 8192 "
                   "-l1-ibsize 32 -l1-dbsize 16\n");
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        int nerr = 0, nidu = 0;
        for (idu = 0;  idu < 3;  idu++) {
            int nparams = (level_blocksize[idu][lev] != 0) +
                          (level_subblocksize[idu][lev] != 0) +
                          (level_size[idu][lev] != 0) +
                          (level_assoc[idu][lev] != 0) +
                          (level_replacement[idu][lev] != 0) +
                          (level_fetch[idu][lev] != 0) +
                          (level_walloc[idu][lev] != 0) +	/* only for u or d */
                          (level_wback[idu][lev] != 0);	/* only for u or d */
            int active = nparams != 0 || level_doccc[idu][lev] != 0;
            nidu += active;
            if (active && nparams != (6 + 2 * (idu != 1))) {
                if (level_blocksize[idu][lev] == 0) {
                    unspec (lev, idu, "block size", &level_blocksize[0][0], "16");
                }
                if (level_size[idu][lev] == 0) {
                    unspec (lev, idu, "size", &level_size[0][0], "16k");
                }
                nerr++;
            }
        }
        if (nidu == 0) {
            shorthelp ("no level %d cache specified\n", lev + 1);
        } else if (nerr != 0) {
            shorthelp ("level %d cache parameters incomplete\n", lev + 1);
        }
    }

    verify_trace_format();	/* look for this in tracein.c */

    /* allowable replacement policies */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            if (level_replacement[idu][lev] != 0 &&
                    level_replacement[idu][lev] != 'l' &&	/* LRU */
                    level_replacement[idu][lev] != 'f' &&	/* FIFO */
                    level_replacement[idu][lev] != 'r')	/* random */
                shorthelp ("level %d %ccache replacement policy unrecognized\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
        }
    }

    /* allowable fetch policies and prefetch parameters */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            if (level_fetch[idu][lev] != 0 &&
                    level_fetch[idu][lev] != 'd' &&	/* demand fetch */
                    level_fetch[idu][lev] != 'a' &&	/* always prefetch */
                    level_fetch[idu][lev] != 'm' &&	/* miss prefetch */
                    level_fetch[idu][lev] != 't' &&	/* tagged prefetch */
                    level_fetch[idu][lev] != 'l' &&	/* load forward prefetch */
                    level_fetch[idu][lev] != 's')		/* subblock prefetch */
                shorthelp ("level %d %ccache fetch policy unrecognized\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
            if ((level_fetch[idu][lev] == 'l' || level_fetch[idu][lev] == 's') &&
                    level_prefetch_distance[idu][lev] >
                    level_blocksize[idu][lev] / (level_subblocksize[idu][lev]
                                                 ? level_subblocksize[idu][lev] : level_blocksize[idu][lev]))
                shorthelp ("level %d %ccache prefetch distance > block size\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
            if (level_fetch[idu][lev] == 'd' &&
                    level_prefetch_abortpercent[idu][lev] != 0)
                shorthelp ("level %d %ccache abort %% not allowed "
                           "with demand fetch policy\n", lev + 1,
                           idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
            if (level_prefetch_abortpercent[idu][lev] < 0 ||
                    level_prefetch_abortpercent[idu][lev] > 100)
                shorthelp ("level %d %ccache abort %% out of range\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
        }
    }

    /* allowable walloc policies */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        if (level_walloc[1][lev] != 0) {
            shorthelp ("level %d icache cannot have write allocate policy\n", lev + 1);
        }
        for (idu = 0;  idu < 3;  idu++) {
            if (level_walloc[idu][lev] != 0 &&
                    level_walloc[idu][lev] != 'a' &&	/* always write allocate */
                    level_walloc[idu][lev] != 'n' &&	/* never write allocate */
                    level_walloc[idu][lev] != 'f')	/* walloc only w/o fetch */
                shorthelp ("level %d %ccache write allocate policy unrecognized\n",
                           lev + 1, idu == 0 ? 'u' : 'd');
        }
    }

    /* allowable wback policies */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        if (level_wback[1][lev] != 0) {
            shorthelp ("level %d icache cannot have write back policy\n", lev + 1);
        }
        for (idu = 0;  idu < 3;  idu++) {
            if (level_wback[idu][lev] != 0 &&
                    level_wback[idu][lev] != 'a' &&	/* always write back */
                    level_wback[idu][lev] != 'n' &&	/* never write back (i.e., write through) */
                    level_wback[idu][lev] != 'f')		/* wback only w/o fetch */
                shorthelp ("level %d %ccache write back policy unrecognized\n",
                           lev + 1, idu == 0 ? 'u' : 'd');
        }
    }

    /* the sub-block size is limited by size specified in memory reference */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            D4MemRef x;
            /* put a 1 in MSB position; x.size is unsigned */
            x.size = ~0;
            x.size = ~(x.size >> 1);
            if (level_subblocksize[idu][lev] > x.size)
                shorthelp ("level %d %ccache sub-block size must be <= %u\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'), x.size);
        }
    }

    /* block size/sub-block size */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            D4StackNode *xp;
            if (level_blocksize[idu][lev] != 0 &&
                    level_subblocksize[idu][lev] > level_blocksize[idu][lev])
                shorthelp ("level %d %ccache has sub-blocksize > blocksize\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
            if (level_subblocksize[idu][lev] != 0 &&
                    level_blocksize[idu][lev] / level_subblocksize[idu][lev] > sizeof(xp->valid)*CHAR_BIT)
                shorthelp ("level %d %ccache must have no more than %lu sub-blocks per block\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'), sizeof(xp->valid)*CHAR_BIT);
            if (level_subblocksize[idu][lev] != 0 && level_doccc[idu][lev] != 0 &&
                    D4_BITMAP_RSIZE < level_blocksize[idu][lev] / level_subblocksize[idu][lev])
                shorthelp ("level %d %ccache must have no more than %u sub-blocks per block for CCC\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'), D4_BITMAP_RSIZE);
        }
    }

    /* block and sub-block sizes must match for -stat-idcombine */
    if (stat_idcombine) {
        for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
            if (level_blocksize[1][lev] != level_blocksize[2][lev]) {
                shorthelp ("level %d i & d cache block sizes must match for -stat-idcombine\n", lev + 1);
            }
            if (level_subblocksize[1][lev] != level_subblocksize[2][lev]) {
                shorthelp ("level %d i & d cache sub-block sizes must match for -stat-idcombine\n", lev + 1);
            }
        }
    }

    /*
     * Check for u and (i or d) at each level
     */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        if (0 != (level_blocksize[0][lev]    |
                  level_subblocksize[0][lev] |
                  level_size[0][lev]         |
                  level_assoc[0][lev]        |
                  level_replacement[0][lev]  |
                  level_fetch[0][lev]        |
                  level_walloc[0][lev]       |
                  level_wback[0][lev]        |
                  level_doccc[0][lev]         ) &&
                0 != (level_blocksize[1][lev]    | level_blocksize[2][lev]    |
                      level_subblocksize[1][lev] | level_subblocksize[2][lev] |
                      level_size[1][lev]         | level_size[2][lev]         |
                      level_assoc[1][lev]        | level_assoc[2][lev]        |
                      level_replacement[1][lev]  | level_replacement[2][lev]  |
                      level_fetch[1][lev]        | level_fetch[2][lev]        |
                      level_walloc[1][lev]       | level_walloc[2][lev]       |
                      level_wback[1][lev]        | level_wback[2][lev]        |
                      level_doccc[1][lev]        | level_doccc[2][lev]         ))
            shorthelp ("level %d has i or d together with u cache parameters\n",
                       lev + 1);
    }

    /* check consistency of sizes */
    for (lev = 0;  lev < g_d4opt.maxlevel;  lev++) {
        for (idu = 0;  idu < 3;  idu++) {
            if (level_blocksize[idu][lev] != 0 &&
                    level_blocksize[idu][lev] * level_assoc[idu][lev] > level_size[idu][lev])
                shorthelp ("level %d %ccache size < blocksize * associativity\n",
                           lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
        }
    }

    /* check for no u->id split */
    for (lev = 1;  lev < g_d4opt.maxlevel;  lev++) {
        if (level_blocksize[0][lev - 1] != 0 &&
                level_blocksize[0][lev] == 0) {
            shorthelp ("level %d cache is unified, level %d is not\n", lev, lev + 1);
        }
    }
}


/**
 * @brief Ceiling of log base 2
 *
 * @param[in] x input value
 *
 * @return -1 for clog2(0)
 */
static inline int
clog2 (unsigned int x)
{
    return x ? 31 - __builtin_clz(x) : -1;
}


/*
 * Initialize one cache based on args
 * Die with an error message if there are serious problems.
 */
void
init_1cache (D4Cache *c, int lev, int idu)
{
    c->name = malloc (30);
    if (c->name == NULL) {
        die ("malloc failure initializing l%d%ccache\n", lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));
    }
    sprintf (c->name, "l%d-%ccache", lev + 1, idu == 0 ? 'u' : (idu == 1 ? 'i' : 'd'));

    c->flags |= level_doccc[idu][lev] ? D4F_CCC : 0;
    if (idu == 1) {
        c->flags |= D4F_RO;
    }
    c->lg2blocksize = clog2 (level_blocksize[idu][lev]);
    c->lg2subblocksize = clog2 (level_subblocksize[idu][lev]);
    c->lg2size = clog2 (level_size[idu][lev]);
    c->assoc = level_assoc[idu][lev];

    switch (level_replacement[idu][lev]) {
    default:
        die ("replacement policy '%c' initialization botch\n", level_replacement[idu][lev]);
    case 'l':
        c->replacementf = d4rep_lru;
        c->name_replacement = "LRU";
        break;
    case 'f':
        c->replacementf = d4rep_fifo;
        c->name_replacement = "FIFO";
        break;
    case 'r':
        c->replacementf = d4rep_random;
        c->name_replacement = "random";
        break;
    }

    switch (level_fetch[idu][lev]) {
    default:
        die ("fetch policy '%c' initialization botch\n", level_fetch[idu][lev]);
    case 'd':
        c->prefetchf = d4prefetch_none;
        c->name_prefetch = "demand only";
        break;
    case 'a':
        c->prefetchf = d4prefetch_always;
        c->name_prefetch = "always";
        break;
    case 'm':
        c->prefetchf = d4prefetch_miss;
        c->name_prefetch = "miss";
        break;
    case 't':
        c->prefetchf = d4prefetch_tagged;
        c->name_prefetch = "tagged";
        break;
    case 'l':
        c->prefetchf = d4prefetch_loadforw;
        c->name_prefetch = "load forward";
        break;
    case 's':
        c->prefetchf = d4prefetch_subblock;
        c->name_prefetch = "subblock";
        break;
    }

    switch (level_walloc[idu][lev]) {
    default:
        die ("write allocate policy '%c' initialization botch\n", level_walloc[idu][lev]);
    case 0:
        c->wallocf = d4walloc_impossible;
        c->name_walloc = "impossible";
        break;
    case 'a':
        c->wallocf = d4walloc_always;
        c->name_walloc = "always";
        break;
    case 'n':
        c->wallocf = d4walloc_never;
        c->name_walloc = "never";
        break;
    case 'f':
        c->wallocf = d4walloc_nofetch;
        c->name_walloc = "nofetch";
        break;
    }

    switch (level_wback[idu][lev]) {
    default:
        die ("write back policy '%c' initialization botch\n", level_wback[idu][lev]);
    case 0:
        c->wbackf = d4wback_impossible;
        c->name_wback = "impossible";
        break;
    case 'a':
        c->wbackf = d4wback_always;
        c->name_wback = "always";
        break;
    case 'n':
        c->wbackf = d4wback_never;
        c->name_wback = "never";
        break;
    case 'f':
        c->wbackf = d4wback_nofetch;
        c->name_wback = "nofetch";
        break;
    }

    c->prefetch_distance = level_prefetch_distance[idu][lev] * level_subblocksize[idu][lev];
    c->prefetch_abortpercent = level_prefetch_abortpercent[idu][lev];
}
