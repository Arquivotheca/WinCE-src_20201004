//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    misc.c

Abstract:

    This file contains miscellaneous routines.

Revision History:

--*/

#include "fatfs.h"


int Log2(unsigned int n)
{
    int l;

    if ((n & n-1) != 0)
        return -1;              // n is not a power of 2
    for (l = -1 ; n ; ++l)
        n >>= 1;
    return l;
}


void InitList(PDLINK pdl)
{
    pdl->next = pdl->prev = pdl;
}


int ListIsEmpty(PDLINK pdl)
{
    return pdl->next == pdl;
}


void AddItem(PDLINK pdlIns, PDLINK pdlNew)
{
    pdlNew->prev = pdlIns;
    (pdlNew->next = pdlIns->next)->prev = pdlNew;
    pdlIns->next = pdlNew;
}


void RemoveItem(PDLINK pdl)
{
    pdl->prev->next = pdl->next;
    pdl->next->prev = pdl->prev;
}


/*  pchtoi - convert decimal string to integer
 *
 *  ENTRY
 *      pch - pointer to CHAR (not WCHAR or TCHAR) array
 *      cch - number of characters required (0 if any number allowed)
 *
 *  EXIT
 *      value of number, -1 if error (numbers < 0 are not supported)
 */

int pchtoi(PCHAR pch, int cch)
{
    int i = 0;

    while (TRUE) {
        if (*pch < '0' || *pch > '9')
            break;
        i = i * 10 + (*pch++ - '0');
        if (cch) {
            if (--cch == 0)
                break;
        }
    }
    if (cch)
        i = -1;
    return i;
}


/*  itopch - convert integer to decimal string
 *
 *  ENTRY
 *      i - integer to convert (numbers < 0 or > 9999 are not supported)
 *      pch - pointer to CHAR (not WCHAR or TCHAR) array
 *
 *  EXIT
 *      number of characters in string; note the string is NOT null-terminated
 */

int itopch(int i, PCHAR pch)
{
    int c, q;
    CONST int *piDiv;
    static CONST int iDiv[] = {10000,1000,100,10,1};

    c = 0;      // total number of digits output
    if (i >= 0 && i < iDiv[0]) {
        piDiv = &iDiv[1];
        do {
            if (c != 0 || *piDiv == 1 || i >= *piDiv) {
                q = i / (*piDiv);
                *pch++ = '0' + q;
                i -= q * (*piDiv);
                c++;
            }
        } while (*piDiv++ != 1);
    }
    return c;
}


/*  SetBitArray - set bit in bit array
 *
 *  ENTRY
 *      pdwBitArray - pointer to bit array
 *      i - 0-based position of bit to set (i is range-checked)
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      Bit arrays start with two special DWORDs that are not part of
 *      of the actual array of bits.  The first DWORD is initialized to
 *      the number of bits that can be held, and the second DWORD keeps track
 *      of the total number of SET bits.
 *
 *      You can either allocate and initialize the bit array yourself, or
 *      call CreateBitArray;  the latter is a macro in FATFS.H that creates a
 *      temporary array that's automatically freed upon leaving the current
 *      scope.
 */

void SetBitArray(PDWORD pdwBitArray, int i)
{
    DWORD mask;
    PDWORD pdw;

    if (i >= 0 && (DWORD)i < pdwBitArray[0]) {

        mask = 1 << (i % 32);
        pdw = pdwBitArray + i/32+2;

        if ((*pdw & mask) == 0)
            pdwBitArray[1]++;

        *pdw |= mask;
    }
}


#ifdef LATER    // no one needs this function yet -JTP

/*  ClearBitArray - clear bit in bit array
 *
 *  ENTRY
 *      pdwBitArray - pointer to bit array
 *      i - 0-based position of bit to clear (i is range-checked)
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      Bit arrays start with two special DWORDs that are not part of
 *      of the actual array of bits.  The first DWORD is initialized to
 *      the number of bits that can be held, and the second DWORD keeps track
 *      of the total number of SET bits.
 *
 *      You can either allocate and initialize the bit array yourself, or
 *      call CreateBitArray;  the latter is a macro in FATFS.H that creates a
 *      temporary array that's automatically freed upon leaving the current
 *      scope.
 */

void ClearBitArray(PDWORD pdwBitArray, int i)
{
    DWORD mask;
    PDWORD pdw;

    if (i >= 0 && (DWORD)i < pdwBitArray[0]) {

        mask = 1 << (i % 32);
        pdw = pdwBitArray + i/32+2;

        if ((*pdw & mask) != 0) {
            ASSERT(pdwBitArray[1] != 0);
            pdwBitArray[1]--;
        }

        *pdw &= ~mask;
    }
}

#endif


/*  TestBitArray - test bit in bit array
 *
 *  ENTRY
 *      pdwBitArray - pointer to bit array
 *      i - 0-based position of bit to test (i is range-checked)
 *
 *  EXIT
 *      TRUE if bit is set, FALSE if bit is not set
 *
 *  NOTES
 *      Bit arrays start with two special DWORDs that are not part of
 *      of the actual array of bits.  The first DWORD is initialized to
 *      the number of bits that can be held, and the second DWORD keeps track
 *      of the total number of SET bits.
 *
 *      You can either allocate and initialize the bit array yourself, or
 *      call CreateBitArray;  the latter is a macro in FATFS.H that creates a
 *      temporary array that's automatically freed upon leaving the current
 *      scope.
 */

BOOL TestBitArray(PDWORD pdwBitArray, int i)
{
    ASSERT(i >= 0 && (DWORD)i < pdwBitArray[0]);

    return i >= 0 && (DWORD)i < pdwBitArray[0]?
        (pdwBitArray[i/32+2] & (1 << (i % 32))) != 0 : FALSE;
}


/*  DOSTimeToFileTime - convert DOS format time to NT FILETIME
 *
 *  FAT files use a bit-packed date & time format versus NT's 64bit integer
 *  time format.
 *
 *  DOS Date format: YYYY YYYM MMMD DDDD (Year since 1980, Month, Day of month)
 *  DOS Time format: HHHH HMMM MMMS SSSS (Hour, Minute, Seconds/2 (0-29))
 *
 *  ENTRY
 *      dosDate - date in DOS bitpacked format
 *      dosTime - time in DOS bitpacked format
 *      tenMSec - milliseconds / 10
 *      pft - pointer to FILETIME structure to fill in
 *
 *  EXIT
 *      TRUE if successful, FALSE if error
 */

BOOL DOSTimeToFileTime(WORD dosDate, WORD dosTime, BYTE tenMSec, PFILETIME pft)
{
    FILETIME ft;
    SYSTEMTIME st;
    WORD wMSec = tenMSec * 10;

    st.wYear = (dosDate >> 9) + 1980;
    st.wMonth = (dosDate >> 5) & 0xf;
    st.wDayOfWeek = 0;
    st.wDay = dosDate & 0x1f;
    st.wHour = dosTime >> 11;
    st.wMinute = (dosTime >> 5) & 0x3f;
    st.wSecond = (dosTime & 0x1f) << 1;

    DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!DOSTimeToFileTime: local file system time: y=%d,m=%d,d=%d,h=%d,m=%d,s=%d\n"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond));
    // The ms field can range from 0 - 1999 ms inclusive.  
    // Use if statements to avoid expensive division operation.
    if (wMSec >= 2000) {
        DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!DOSTimeToFileTime failed: invalid milliseconds value: %d.\r\n"), wMSec));
        return FALSE;
    } else if (wMSec >= 1000) {
        st.wSecond++;
        st.wMilliseconds = wMSec - 1000;            
    } else {
        st.wMilliseconds = wMSec;
    }

    if (SystemTimeToFileTime(&st, &ft)) {

        DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!DOSTimeToFileTime: local file time: %08x,%08x\n"), ft.dwLowDateTime, ft.dwHighDateTime));
        if (LocalFileTimeToFileTime(&ft, pft)) {

            DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!DOSTimeToFileTime: file time: %08x,%08x\n"), pft->dwLowDateTime, pft->dwHighDateTime));
            return TRUE;
        }
    }
    return FALSE;
}


/*  FileTimeToDOSTime - convert NT FILETIME to DOS format time
 *
 *  FAT files use a bit-packed date & time format versus NT's 64bit integer
 *  time format.
 *
 *  DOS Date format: YYYY YYYM MMMD DDDD (Year since 1980, Month, Day of month)
 *  DOS Time format: HHHH HMMM MMMS SSSS (Hour, Minute, Seconds/2 (0-29))
 *
 *  ENTRY
 *      dosDate - date in DOS bitpacked format
 *      dosTime - time in DOS bitpacked format
 *      tenMSec - milliseconds / 10
 *      pft - pointer to FILETIME structure to fill in
 *
 *  EXIT
 *      TRUE if successful, FALSE if error
 */

BOOL FileTimeToDOSTime(PFILETIME pft, PWORD pdosDate, PWORD pdosTime, PBYTE ptenMSec)
{
    FILETIME ft;
    SYSTEMTIME st;

    ASSERT(pdosDate);

    DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!FileTimeToDOSTime: file time: %08x,%08x\n"), pft->dwLowDateTime, pft->dwHighDateTime));
    FileTimeToLocalFileTime(pft, &ft);

    DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!FileTimeToDOSTime: local file time: %08x,%08x\n"), ft.dwLowDateTime, ft.dwHighDateTime));
    FileTimeToSystemTime(&ft, &st);

    DEBUGMSG(ZONE_STREAMS && ZONE_LOGIO,(DBGTEXT("FATFS!FileTimeToDOSTime: local file system time: y=%d,m=%d,d=%d,h=%d,m=%d,s=%d\n"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond));
    *pdosDate = ((st.wYear-1980) << 9) | (st.wMonth << 5) | (st.wDay);

    if (pdosTime)
        *pdosTime = (st.wHour << 11) | (st.wMinute << 5) | (st.wSecond >> 1);
    if (ptenMSec)
        *ptenMSec = (st.wMilliseconds / 10);

    return TRUE;
}


#ifdef OLD_IOCTLS       // defunct code

/*  SetSizePointer - update a SIZEPTR structure as appropriate
 *
 *  For consistency, certain APIs (like IOCTLs) pass us DWORD/PVOID pairs,
 *  where the DWORD represents the number of elements in an array, and the
 *  PVOID is the address of the array.  Furthermore, if the caller doesn't
 *  know how many elements are in the array yet, then the convention is for
 *  the caller to pass us a NULL PVOID, and we'll set the DWORD to the
 *  number of elements.  Then the caller can allocate the appropriate amount
 *  of memory and call us again with a non-NULL PVOID.
 *
 *  ENTRY
 *      psp - pointer to a DWORD/PVOID pair
 *      cb - size of each element in bytes
 *      c - count of elements
 *      pSrc - pointer to caller's buffer, NULL if none
 *      hproc - handle to caller's process, NULL if current process
 *
 *  EXIT
 *      None;  either the caller's PVOID buffer is filled in, or the DWORD
 *      size is filled in.  The calling function should have a try/except in
 *      place.
 */

void SetSizePointer(PSIZEPTR psp, DWORD cb, DWORD c, PVOID pSrc, HANDLE hProc)
{
    if (psp->p == NULL) {

        psp->c = c;             // just return number of elements to caller

    } else {

        PVOID pDst = psp->p;

        if (hProc)
            pDst = MapPtrToProcess(pDst, hProc);

        if (c > psp->c)         // if there are more elements than caller wants,
            c = psp->c;         // reduce it

        memcpy(pDst, pSrc, cb * c);
    }
}

#endif  // OLD_IOCTLS
