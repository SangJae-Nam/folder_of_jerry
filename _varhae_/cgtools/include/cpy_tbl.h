/*****************************************************************************/
/* cpy_tbl.h  v5.2.8                                                         */
/* Copyright (c) 2003 Texas Instruments Incorporated                         */
/*                                                                           */
/* Specification of copy table data structures which can be automatically    */
/* generated by the linker (using the table() operator in the LCF).          */
/*****************************************************************************/
#ifndef _CPY_TBL
#define _CPY_TBL

#ifdef __cplusplus
extern "C" namespace std {
#endif /* __cplusplus */

/*****************************************************************************/
/* Copy Record Data Structure                                                */
/*****************************************************************************/
typedef struct copy_record
{
   unsigned int		 src_pgid;
   unsigned int		 dst_pgid;
   unsigned long	 src_addr;
   unsigned long	 dst_addr;
   unsigned long	 size;
} COPY_RECORD;

/*****************************************************************************/
/* Copy Table Data Structure                                                 */
/*****************************************************************************/
typedef struct copy_table 
{
   unsigned int		 rec_size;
   unsigned int		 num_recs;
   COPY_RECORD		 recs[1];
} COPY_TABLE;

/*****************************************************************************/
/* Prototypes for near and far general purpose copy routines.                */
/*****************************************************************************/
extern void copy_in(COPY_TABLE *tp);
extern void far_copy_in(far COPY_TABLE *tp);

#ifdef __cplusplus
} /* extern "C" namespace std */
#endif /* __cplusplus */

/*****************************************************************************/
/* Prototypes for utilities used by copy_in() to move code/data between      */
/* program and data memory (see cpy_utils.asm for source).                   */
/*****************************************************************************/
extern void ddcopy(unsigned long src, unsigned long dst, unsigned long size);
extern void dpcopy(unsigned long src, unsigned long dst, unsigned long size);
extern void pdcopy(unsigned long src, unsigned long dst, unsigned long size);
extern void ppcopy(unsigned long src, unsigned long dst, unsigned long size);
#endif /* !_CPY_TBL */

#if defined(__cplusplus) && !defined(_CPP_STYLE_HEADER)
using std::COPY_RECORD;
using std::COPY_TABLE;
using std::copy_in;
using std::far_copy_in;
#endif /* _CPP_STYLE_HEADER */
