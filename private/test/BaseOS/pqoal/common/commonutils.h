//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*
 * commonUtils.h
 */

#pragma once

#include "disableWarnings.h"
#include <windows.h>

#include <tchar.h>

#include <katoex.h>
#include <kato.h>

#include <stdlib.h>

#include "cmd.h"
#include "guardedMemory.h"

/* parsing command line args from tux */
#include "clparse.h"

#include "intsafe.h"

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
VOID InfoA(char * szFormat, ...);
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

/*
 * return a random double between 0 and 1
 */
double
getRandDoubleZeroToOne (VOID);


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
 * perform substraction on two dwords.  Return the result, handling overflow
 * by rolling over the top.
 */
DWORD
AminusB (DWORD a, DWORD b);

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

/*
 * break if the user has specified -b as part of the command line
 * to the test
 */
void breakIfUserDesires ();

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
strToULONGLONG(const TCHAR *const szStr, ULONGLONG * ullVal);

/*
 * converts a string in the "time format" into a ULONGLONG containing the
 * number of seconds represented by the string.
 *
 * szTime is the string with the time value.  *pullTimeS, on success,
 * contains the string value in seconds.  One error the function
 * returns FALSE and *pullTimeS is undefined.
 *
 * The time values must be positive.  They are formed from a numerical
 * part followed by a unit identifier.  The unit identifier can be
 * either:
 *
 * s = seconds
 * m = minutes
 * h = hours
 * d = days
 *
 * These are case sensitive.  No space can reside between the number
 * and the units designator.  Nothing past the unit designator is
 * checked.
 *
 * If no unit designator is passed then the number is assumed to be
 * given in seconds.
 *
 * Valid input:
 *
 * "12"   =>   12 s
 * "12s"   =>   12 s
 * "12h"  =>   12 * 60 * 60 s
 * "12m"  =>   12 * 60 s
 * "12minutes" => 12 * 60 s
 * "12mom"  => 12 * 60 s   (only first character is used)
 * "0s"     => 0 s
 *
 * Invalid input (should cause error):
 *
 * "h"
 * "h12"
 * ""
 * "12b"
 * "12 m"
 */
BOOL
strToSeconds (const TCHAR *const szTime, ULONGLONG * pullTimeS);

/* Converts a string to a double */
BOOL
strToDouble (const TCHAR *const szStr, double * doN);

/*
 * use this to work around issues in clparse.h.
 *
 * clparse.h does not understand the negative numbers used for the drift values.
 * This version correctly parses these values.
 */
BOOL GetArgument(LPCTSTR pszCmdLine, IN LPCTSTR pszArgName, __inout_ecount_opt(*pdwLen) LPTSTR pszArgVal, PDWORD pdwLen );



/***** Constants **********************************************************/

/* defined in winnt.h */
#define DWORD_MAX MAXDWORD

/* defined in stdlib.h */
#define MAX_LARGE_INT MAXLONGLONG
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

/****** Identifying a type of device */

BOOL
getDeviceType (__out_ecount(dwLen) TCHAR * szDeviceName,
           DWORD dwLen);

/*
 * This returns the device id for a given dev board.
 *
 * The term device id is used for many different things.  In this case
 * the function returns the platform string out of the device id
 * structure.  This is commonly referred to as the device id since it
 * uniquely identifies this instance of the device (dev board or piece
 * of hardware).
 *
 * szDevId is an allocated string of char for the device id.  Device
 * ids are traditionally ansi so this isn't a tchar.  dwLen is the
 * length of szDevId is characters (bytes).
 *
 * returns true on success and false otherwise.
 */
BOOL
getDeviceId (__out_ecount(dwLen) char * szDevId, DWORD dwLen);


/*
 * This function returns the Platform Name for a given dev board.
 */
BOOL GetPlatformName(__out_ecount(dwLen) TCHAR * szBootMeName, DWORD dwLen);


/* COMMONUTILS.H */
