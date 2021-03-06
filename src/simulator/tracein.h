/*
 * Declarations for trace input handling for Dinero IV's command interface.
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

#ifndef TRACEIN_H
#define TRACEIN_H

/**
 * abstract trace input
 */
typedef struct TraceIn {
    const char *infile_path;               /** input file path */
    FILE *infile_fp;                       /** input file handler */
    uint64_t trace_count;                  /** # of reading trace */
    void (*read_func) (struct TraceIn *trace_ctx,
                       D4MemRef *memref);  /** pointer to read line trace handling
                                               function*/
} TraceIn;

/**
 * initialize TraceIn structure
 * @param[out] tin pointer to uninitial TraceIn structure
 * @param[in] input file path
 * @return 0 -> success  1 -> error occur
 */
int tracein_init(TraceIn *tin, const char* infile);

/**
 * read one trace record. The funtion exit immediate if there are any
 * unexpected error
 * @param[out] tin pointer to initialized TraceIn structure
 * @param[in] input file path
 */
void tracein_read(TraceIn *tin, D4MemRef *memref);

/**
 * Read in ASCII from standard input
 * Expect 3 significant fields per line:
 *    +--------------+----------+-------+
 *    |  access type |  address |  size |
 *    +--------------+----------+-------+
 * The rest of the data input line is ignored so it may be used for comments.
 *
 * Accesstype
 *	  r  read
 *	  w  write
 *	  i  instruction fetch
 *	  m  miscellaneous (like a read but won't generate prefetch)
 *	  c  copyback (no invalidate implied)
 *	  v  invalidate (no copyback implied)
 * Address
 *    'A' hex address format
 *
 * Size
 *    'A' hex address format
 *
 * @param[in,out] trace_ctx pointer to trace context
 * @param[out] memref pointer to unset D4MemRef instance
 */
void tracein_xdin (TraceIn *trace_ctx, D4MemRef *r);

/**
 * Read in ASCII from standard input
 * Expect 2 significant fields per line:
 *    +---------+----------+
 *    |  label  |  address |
 *    +---------+----------+
 * The rest of the data input line is ignored so it may be used for comments.
 * This version is a bit more forgiving than the FAST_BUT_DANGEROUS_INPUT
 * version in Dinero III.
 *
 * Accesstype
 *	  r  read
 *	  w  write
 *	  i  instruction fetch
 *	  m  miscellaneous (like a read but won't generate prefetch)
 *	  c  copyback (no invalidate implied)
 *	  v  invalidate (no copyback implied)
 * Address
 *    'A' hex address format
 *
 * Size
 *    'A' hex address format
 *
 *
 * @warning If more than one tuple is put on a line, all but the first
 *          tuple will be ignored.
 *
 * @param[in,out] trace_ctx pointer to trace context
 * @param[out] memref pointer to unset D4MemRef instance
 */
void tracein_din (TraceIn *trace_ctx, D4MemRef *r);

/**
 * 32-bit pixie trace format consists of 32-bit words,
 *
 *   31   28 27   24 23            0
 *  +-------+-------+---------------+
 *  | count |  type |    address    |
 *  +-------+-------+---------------+
 *
 * count
 *     1. basic blocks
 *          tells how many sequential instructions to fetch before doing
 *          something else.
 *     2. loads/store
 *          tells how many ifetches * to do after the load or store, before
 *          doing something else.
 * address
 *     1. basic blocks : word address (4-byte) address
 *     2. load/store : byte address
 *
 * @param[in,out] trace_ctx pointer to trace context
 * @param[out] memref pointer to unset D4MemRef instance
 */
void tracein_pixie32 (TraceIn *trace_ctx, D4MemRef *r);

/*
 * 64-bit pixie trace format consists of 64-bit words,
 *
 *   63   56 55   48 47            0
 *  +-------+-------+---------------+
 *  | count |  type |    address    |
 *  +-------+-------+---------------+
 *
 * count
 *     1. basic blocks
 *          tells how many sequential instructions to fetch before doing
 *          something else.
 *     2. loads/store
 *          tells how many ifetches * to do after the load or store, before
 *          doing something else.
 * address
 *     1. basic blocks : word offset(4-byte) from the beginning of the DSO
 *                       (i.e., (address - dso start)/4).
 *     2. load/store : byte address
 *
 * We currently assume the "n32" execution mode, and therefore
 * truncate all addresses to their low 32 bits (sizeof(int)==4).
 *
 * We currently only support nonshared executables, although the
 * format supports full use of DSOs.  Also, to be perfectly legitimate,
 * we should map data addresses to correspond to what they would have been
 * in the unpixified program (they may well have been displaced
 * in the pixified version); we don't do that either.
 *
 * We could automatically distinquish between 32-bit and 64-bit formats,
 * but we don't.
 *
 * @param[in,out] trace_ctx pointer to trace context
 * @param[out] memref pointer to unset D4MemRef instance
 */
void tracein_pixie64 (TraceIn *trace_ctx, D4MemRef *r);

/**
 * This format is pretty similar to the traditional Dinero "din" format,
 * but it supplies the size as well as address and access type, and
 * requires no conversion from ascii strings on input.
 * Each record is 8 bytes:
 *      struct {
 *         uint32_t address;  //little endian
 *         uint16_t size;     //little endian g an address,
 *         uint8_t  type;
 *         uint8_t  padding;
 *      }
 *
 * @param[in,out] trace_ctx pointer to trace context
 * @param[out] memref pointer to unset D4MemRef instance
 */
void tracein_binary (TraceIn *trace_ctx, D4MemRef *r);

/** the accesstype returned by next_trace_item when the trace is exhausted */
#define D4TRACE_END	D4NUMACCESSTYPES
#endif
