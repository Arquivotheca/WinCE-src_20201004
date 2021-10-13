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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
   debug.c
   
Abstract:  
   Routines for debugging EDBG services. 
   
Functions:

   KITLOutputDebugString
   inet_ntoa
   inet_addr
   
--*/
#include <windows.h>
#include <stdarg.h>
#include "kernel.h"
#include "kitl.h"
#include "kitlp.h"

static void pOutputByte(char c);
static void pOutputNumHex(unsigned long n,long depth);
static void pOutputNumDecimal(unsigned long n);
static void vOutputFormatString(const char *sz, va_list vl);
static void OutputString(const char *s);

static char *szSprintf;

// Critical section to protect debug messages from intermixing
extern CRITICAL_SECTION KITLODScs;

static WCHAR UniBuf[256];

/*
 *   KITLOutputDebugString
 *
 *   Routine to format and send debug messages for EDBG subsystem
 *
 *   Supported formatting characters:
 *
 *               Format string | type
 *               u | unsigned
 *               d | int
 *               c | char
 *               s | string
 *               x | 4-bit hex number
 *               B | 8-bit hex number
 *               H | 16-bit hex number
 *               X | 32-bit hex number
 *
 */
void
KITLOutputDebugString(const char *sz, ...)
{
    va_list         vl;
    // Watch for reentrancy - we may be calling in to do a debug message from within
    // EnterCriticalSection() for example.
    BOOL fUseSysCalls = (KITLGlobalState & KITL_ST_MULTITHREADED) && !InSysCall();
    if (fUseSysCalls) {
        //OEMWriteDebugByte('S');
        EnterCriticalSection(&KITLODScs);
    }
    else {
        //OEMWriteDebugByte('N');
    }

    va_start(vl, sz);

    AcquireKitlSpinLock ();
    vOutputFormatString(sz, vl);
    ReleaseKitlSpinLock ();
        
    va_end(vl);
    if (fUseSysCalls)
        LeaveCriticalSection(&KITLODScs);    
}

/*****************************************************************************
*
*
*   @func   void    |   FormatString | Simple formatted output string routine
*
*   @rdesc  Returns length of formatted string
*
*   @parm   unsigned char * |   pBuf |
*               Pointer to string to return formatted output.  User must ensure
*               that buffer is large enough.
*
*   @parm   const unsigned char * |   sz,... |
*               Format String:
*
*               @flag Format string | type
*               @flag u | unsigned
*               @flag d | int
*               @flag c | char
*               @flag s | string
*               @flag x | 4-bit hex number
*               @flag B | 8-bit hex number
*               @flag H | 16-bit hex number
*               @flag X | 32-bit hex number
*
*   @comm
*           Same as OutputFormatString, but output to buffer instead of serial port.
*/
static unsigned int
FormatString(
    char *pBuf, const char *sz, ...
)
{
    unsigned int    c;
    va_list         vl;
    
    va_start(vl, sz);
    
    szSprintf = pBuf;
    vOutputFormatString(sz, vl);
    pOutputByte(0);
    c = szSprintf - pBuf;
    szSprintf = 0;
    va_end(vl);
    return (c);
}

static void
vOutputFormatString(
    const char *sz, va_list vl
    )
{
    unsigned int    c;
    while (*sz) {
        c = *sz++;
        switch (c) { 
            case (UCHAR)'%':
                c = *sz++;
                switch (c) { 
                    case 'x':
                        pOutputNumHex(va_arg(vl, unsigned long), 0);
                        break;
                    case 'B':
                        pOutputNumHex(va_arg(vl, unsigned long), 2);
                        break;
                    case 'H':
                        pOutputNumHex(va_arg(vl, unsigned long), 4);
                        break;
                    case 'X':
                        pOutputNumHex(va_arg(vl, unsigned long), 8);
                        break;
                    case 'd': {
                        long    l;
                
                        l = va_arg(vl, long);
                        if (l < 0) { 
                            pOutputByte('-');
                            l = - l;
                        }
                        pOutputNumDecimal((unsigned long)l);
                    }
                    break;
                    case 'u':
                        pOutputNumDecimal(va_arg(vl, unsigned long));
                        break;
                    case 's':
                        OutputString(va_arg(vl, char *));
                        break;
                    case 'S':
                        OEMWriteDebugString (va_arg(vl, LPWSTR));
                        break;
                    case '%':
                        pOutputByte('%');
                        break;
                    case 'c':
                        c = va_arg(vl, UCHAR);
                        pOutputByte((UCHAR)c);
                        break;
                
                    default:
                        pOutputByte(' ');
                        break;
                }
                break;
            case '\n':
                pOutputByte('\r');
                // fall through
            default:
                pOutputByte((UCHAR)c);
        }
    }
}

    
/*****************************************************************************
*
*
*   @func   void    |   pOutputByte | Sends a byte out of the monitor port.
*
*   @rdesc  none
*
*   @parm   unsigned int |   c |
*               Byte to send.
*
*/
static void
pOutputByte(
    char c
)
{
    if (szSprintf)
        *szSprintf++ = c;
    else
        OEMWriteDebugByte(c);
}


/*****************************************************************************
*
*
*   @func   void    |   pOutputNumHex | Print the hex representation of a number through the monitor port.
*
*   @rdesc  none
*
*   @parm   unsigned long |   n |
*               The number to print.
*
*   @parm   long | depth |
*               Minimum number of digits to print.
*
*/
static void
pOutputNumHex(
    unsigned long   n, 
    long            depth
)
{
    if (depth) {
        depth--;
    }
    
    if ((n & ~0xf) || depth) {
        pOutputNumHex(n >> 4, depth);
        n &= 0xf;
    }
    
    if (n < 10) {
        pOutputByte((UCHAR)(n + '0'));
    } else { 
        pOutputByte((UCHAR)(n - 10 + 'A'));
    }
}


/*****************************************************************************
*
*
*   @func   void    |   pOutputNumDecimal | Print the decimal representation of a number through the monitor port.
*
*   @rdesc  none
*
*   @parm   unsigned long |   n |
*               The number to print.
*
*/
static void
pOutputNumDecimal(
    unsigned long n
)
{
    if (n >= 10) {
        pOutputNumDecimal(n / 10);
        n %= 10;
    }
    pOutputByte((UCHAR)(n + '0'));
}

/*****************************************************************************
*
*
*   @func   void    |   OutputString | Sends an unformatted string to the monitor port.
*
*   @rdesc  none
*
*   @parm   const UCHAR * |   s |
*               points to the string to be printed.
*
*   @comm
*           backslash n is converted to backslash r backslash n
*/
static void
OutputString(
    const char *s
)
{
    while (*s) {        
        if (*s == '\n') {
            OEMWriteDebugByte('\r');
        }
        OEMWriteDebugByte(*s++);
    }
}



// This routine will take a binary IP address as represent here and return a dotted decimal version of it
char *inet_ntoa( DWORD dwIP ) {

    static char szDottedD[16];

    FormatString( szDottedD, "%u.%u.%u.%u",
        (BYTE)dwIP, (BYTE)(dwIP >> 8), (BYTE)(dwIP >> 16), (BYTE)(dwIP >> 24) );

    return szDottedD;

} // inet_ntoa()

// This routine will take a dotted decimal IP address as represent here and return a binary version of it
DWORD inet_addr(char *pszDottedD ) {

    DWORD dwIP = 0;
    DWORD cBytes;
    char *pszLastNum;
    int atoi (const char *s);
    
    // Replace the dots with NULL terminators
    pszLastNum = pszDottedD;
    for( cBytes = 0; cBytes < 4; cBytes++ ) {
        while(*pszDottedD != '.' && *pszDottedD != '\0')
            pszDottedD++;
        if (pszDottedD == '\0' && cBytes != 3)
            return 0;
        *pszDottedD = '\0';
        dwIP |= (atoi(pszLastNum) & 0xFF) << (8*cBytes);
        pszLastNum = ++pszDottedD;
    }

    return dwIP;

} // inet_ntoa()

