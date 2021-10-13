//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

////////////////////////////////////////////////////////////////////////////////
// Global macros

#define countof(x)  (sizeof(x)/sizeof(*(x)))

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
#include <diskio.h>
#include <pehdr.h>
#include <romldr.h>
#include <romimage.h>
#include <filelist.h>
#include "ft.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato            *g_pKato INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO   *g_pShellInfo;

// Add more globals of your own here. There are two macros available for this:
//  GLOBAL  Precede each declaration/definition with this macro.
//  INIT    Use this macro to initialize globals, instead of typing "= ..."
//
// For example, to declare two DWORDs, one uninitialized and the other
// initialized to 0x80000000, you could enter the following code:
//
//  GLOBAL DWORD        g_dwUninit,
//                      g_dwInit INIT(0x80000000);
////////////////////////////////////////////////////////////////////////////////

/*
 * relmerge magic number.  Each file that is an output from relmerge will
 * have this on the top.
 * notice byte ordering.  intel is little endian.
 */
#define RELMERGE_MAGIC 0x45474e4d    /*  "MNGE" for little endian */
#define ALL_IMGFS_FILES TEXT("\\imgfs\\*")
#define IMGFS TEXT("\\imgfs")
#define ALL_PACKAGE_FILES TEXT("\\release\\*_PACKAGE_FILES")
#define RELEASEDIR TEXT("\\release")
#define PAGE_SIZE 4096
#define BINFS TEXT("\\binfs")


typedef struct
{
  DWORD fileLength;             /* length of the file in bytes */
  void * fileData;              /* pointer to the start of the file in
                                   memory.  The file must be contiguios
                                   in the memory space for the other 
                                   functions to work correctly. */
  e32_rom * fileHeader;         /* pointer to the e32_rom structure in the
                                   file.  This forms the main header for the
                                   relmerge output files.*/
  DWORD numberOfSections;       /* number of sections in the file.  Comes 
                                   from the e32_rom header */
  o32_rom * sectionHeaders;     /* pointer to the first section header in 
                                   the file.  Since the section headers are
                                   sequentially ordered in the pe file, this
                                   forms an array.  Use pointer arithmetic to
                                   access subsequent section headers.*/
} sPEROMFileInfo;

/*
 * header at the top of relmerge output files
 */
typedef struct
{
  DWORD relmergeMagic;          /* relmerge magic number */
} sRelmergeHeader;



#endif // __GLOBALS_H__
