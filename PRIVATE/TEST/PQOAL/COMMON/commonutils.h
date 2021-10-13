//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * commonUtils.h
 */

#ifndef __COMMON_UTILS_H
#define __COMMON_UTILS_H

#include "disableWarnings.h"
#include <windows.h>

#include <dbgapi.h>

#include <tchar.h>

#include <katoex.h>
#include <kato.h>

#include <stdlib.h>

/* parsing command line args from tux */
#include "clparse.h"


/*
 * These two things are globals needed to communicate with tux.  They are defined in the
 * tuxDll/tuxStandard.cpp.  Included here because all other files need them
 */
/*
 * needed for kato calls.  It is declared in a tux header.
 */
extern CKato *g_pKato;

/* 
 * needed for command line arg processing in TESTPROC functions.
 * Defined in commonUtils.cpp
 */
extern LPCTSTR g_szDllCmdLine;

/* returns the number of objects in a given data structure */
#define countof(a) (sizeof(a)/sizeof(*(a)))

/* for very verbose, in depth debugging */
//#define PRINT_DEBUG_VVV
#undef PRINT_DEBUG_VVV

#define PRINT_DEBUG_VV

/*
 * output functions
 *
 * The print to kato, which tux redirects as the user directs.
 */
VOID Info(LPCTSTR szFormat, ...);
VOID Error(LPCTSTR szFormat, ...);
VOID IntErr(LPCTSTR szFormat, ...);

/* 
 * always goes to Debug_Output.  Use this to bypass kato (if tux hasn't set
 * it up yet).
 */
VOID Debug(LPCTSTR szFormat, ...);

/*
 * return a random number in a given range
 */
DWORD
getRandomInRange (DWORD lowEnd, DWORD highEnd);

/**** Useful macros *******************************************************/

/* set and clear bits */
#define SET_BIT(val, pos) ((val) |= (0x1 << (pos)))
#define CLR_BIT(val, pos) ((val) &= (~(0x1 << (pos))))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**** Useful functions ****************************************************/

/*
 * Perform a subtraction, accounting for values that might have overflowed.
 *
 * answer = a - b
 *
 * dwSizeOf is the size of the variables that are being subtracted.
 * This allows a person to define a wrap around value.  This value is
 * inclusive. (Use MAX_DWORD, etc)  a and b can't be larger than dwMaxSize.
 *
 * This function returns TRUE on success and FALSE otherwise.
 */
BOOL
AminusB (ULONGLONG & answer, ULONGLONG a, ULONGLONG b, ULONGLONG dwMaxSize);

/* 
 * circular shift dwVal dwShift values to the right.  dwLength gives
 * the length of dwVal in bits.  This allows circular shifts on values
 * that are less than 32 bits.  The value is stored in the lower bits
 * of the DWORD.
 *
 * dwShift must be between (0, 32] and must be less than or equal to
 * dwLength.
 *
 * Any bits not shifted have an undefined value.
 *
 * The function returns the shifted value.
 */
DWORD
circularShiftR (DWORD dwVal, DWORD dwLength, DWORD dwShift);

/* 
 * circular shift dwVal dwShift values to the left.  dwLength gives
 * the length of dwVal in bits.  This allows circular shifts on values
 * that are less than 32 bits.  The value is stored in the lower bits
 * of the DWORD.
 *
 * dwShift must be between (0, 32] and must be less than or equal to
 * dwLength.
 *
 * Any bits not shifted have an undefined value.
 *
 * The function returns the shifted value.
 */
DWORD
circularShiftL (DWORD dwVal, DWORD dwLength, DWORD dwShift);

/* 
 * Return true if val is a power of two.  Return false if it isn't.
 * val = 0 returns false.
 */
BOOL
isPowerOfTwo (DWORD val);

/*
 * val needs to be a power of 2 (non-zero)
 */
DWORD
getNumBitsToRepresentNum (DWORD val);


/**** Thread Priority *****************************************************/

/*
 * priorities are ints, so this is an int 
 * never make this between [0, 255]
 */
#define DONT_CHANGE_PRIORITY (-1)

/*
 * for some tests we want to change the priority.  This function
 * provides an easy way to do this.
 *
 * iNew is the new priority.  This must be a value between [0, 255] or
 * it must be DONT_CHANGE_PRIORITY.
 *
 * if iOld is null, then the old priority isn't saved.  If iOld isn't
 * null, then the old priority is put into iOld.
 *
 * return true on success and false on failure
 */
BOOL
handleThreadPriority (int iNew, int * iOld);

/****** Time Formatting **************************************************/

/*
 * convert a time into a time that is more realistic to print.  This
 * allows us to easily pretty print times.  120000 seconds isn't much
 * help to the normal user without a calculator.  This makes the
 * output nicer.
 *
 * doTime is the value to convert, in seconds.  doFormatted time is
 * the resulting number, and szUnits in the units that should be
 * printed.  The units account for singular and plural.
 *
 */ 
void
realisticTime (double doTime, double & doFormattedTime, _TCHAR * szUnits[]);

/***** String Functions  **************************************************/

/*
 * convert string to ULONGLONG.  Uses sscanf.
 *
 * szStr is the string to convert.  ullVal is the value resulting from
 * the conversion.  return TRUE on success and false otherwise.
 *
 * A little hacking.  For instance "10blah" returns 10 and true.  In
 * general, though, works pretty well.
 *
 * If you pass in a negative number in the string, it will be
 * converted to the corresponding unsigned ulonglong.
 */
BOOL
strToULONGLONG(TCHAR * szStr, ULONGLONG * ullVal);


/***** Constants **********************************************************/

/* defined in winnt.h */
#define MAX_DWORD MAXDWORD

/* defined in stdlib.h */
#define MAX_LARGE_INT _I64_MAX
#define ULONGLONG_MAX _UI64_MAX

/* for certain test we boost the priority to this level */
#define TEST_THREAD_PRIORITY 1


// undefined values are set to this
// easier debugging
#define UNINIT_DW 0xcccccccc
#define BAD_VAL 0xcccccccc

/***** User Defined Error Values ******************************************/

/*
 * bit 29 is used for user defined error codes.
 *
 * Use this to SetLastError so that we are sure that the functions are
 * setting getLastError correctly.  Provides a value that no function
 * under test should ever set.
 */
#define MY_ERROR_JUNK 0x2fffffff

#endif /* __COMMON_UTILS_H */
