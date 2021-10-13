//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * cacheControl.h
 */

/* 
 * Included because it has the definitions of the cache modes.  These
 * are needed for the tux function tables so can't be included here.
 */
#include "tuxCaches.h"


/*
 * This file contains the code used to control the cache behavior for
 * a given virtual address range.  The idea is that one will alloc a
 * huge buffer with VirtualAlloc or AllocPhysMem and will then use the
 * function below to set up the caching on this block of memory.  This
 * involves flipping bits in the TLB.
 *
 * Each proc has a different tlb layout.  The code below works for all
 * four procs that CE supports: x86, ARM, mips, and SHx.  The
 * conditionally compiling allows the code to work for all four procs.
 */



/*
 * Set the cache mode to the value specified in the incoming
 * parameter.  The incoming paramter is set from the CT_* macros from
 * tuxCaches.h.  These are used by other functions, so can't be local
 * only to this file.
 *
 * vals is a pointer to the block of virtual memory over which to
 * change the perms.  dwSizeBytes is the size is bytes of this
 * memory.  This size doesn't have to be a page aligned, but this
 * function works on page boundaries, not bytes.  It effectively
 * rounds the size up to the next page boundary if the size isn't
 * already on a boundary, so other variables declared nearby might be
 * affected by this change.
 *
 * dwParam is the cache mode that you want the cache is.  Only the
 * bits specified by CT_WRITE_MODE_MASK is used.  The possible values
 * are:
 * 
 *  CT_WRITE_THROUGH
 *  CT_WRITE_BACK
 *  CT_DISABLED
 *
 * Note that the mips doesn't support per tlb entries for
 * write-through vs. write-back.  This function considers a request
 * for write-through or write-back to be a cache enabled and
 * effectively does the same thing for this call.
 *
 * return TRUE if we were able to successfully set up the cache and
 * false otherwise.  It prints debug spew as to what it is doing.
 */
BOOL
setCacheMode (DWORD * vals, DWORD dwSizeBytes, DWORD dwParam);


/*
 * Set the cache mode to the value specified in the incoming
 * parameter.  The incoming paramter is set from the CT_* macros from
 * tuxCaches.h.
 *
 * return TRUE on success and FALSE otherwise
 */
BOOL
queryDefaultCacheMode (DWORD * dwMode);
