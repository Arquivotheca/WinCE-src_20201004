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
 *              NK Kernel printf code
 *
 *
 * Module Name:
 *
 *              dbgprintf.c
 *
 * Abstract:
 *
 *              This file implements a simplified version of swprintf function,
 *              used for NKDbgPrintfW and NKvDbgPrintfW
 *
 * NOTE: 
 *              This file is only used for user mode coredll, and included by 
 *              Kernel directly. It never exist in kernel mode coredll as the
 *              kernel mode version (kcoredll) calls to kernel directly.
 *
 */

#ifndef KERN_CORE
#include <windows.h>
#define NKwcslen        wcslen
#endif

#define out(c) if (--cchLimit) *lpOut++=(c); else goto errorout

//-------------------------- Prototype declarations ---------------------------

LPCWSTR SP_GetFmtValue(LPCWSTR lpch, int  *lpw, va_list *plpParms);
int SP_PutNumber(LPWSTR, ULONG, int, int, int);
int SP_PutNumber64(LPWSTR lpb, ULONGLONG i64, int limit, int radix, int mycase);
void SP_Reverse(LPWSTR lp1, LPWSTR lp2);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
NKwvsprintfW(
    __out_ecount (maxchars) LPWSTR lpOut,
    LPCWSTR lpFmt,
    va_list lpParms,
    int maxchars
    ) 
{
    int left, width, prec, size, sign, radix, upper, cch, cchLimit;
    WCHAR prefix, fillch;
    LPWSTR lpT;
    LPCSTR lpC;
    union {
        long l;
        unsigned long ul;
        __int64 i64;
        WCHAR sz[sizeof(long)];
    } val;

    cchLimit = maxchars;
    while (*lpFmt) {
        if (*lpFmt==(WCHAR)'%') {
            /* read the flags.  These can be in any order */
            left=0;
            prefix=0;
            while (*++lpFmt) {
                if (*lpFmt==(WCHAR)'-')
                    left++;
                else if (*lpFmt==(WCHAR)'#')
                    prefix++;
                else
                    break;
            }
            /* find fill character */
            if (*lpFmt==(WCHAR)'0') {
                fillch=(WCHAR)'0';
                lpFmt++;
            } else
                fillch=(WCHAR)' ';
            /* read the width specification */
            lpFmt=SP_GetFmtValue(lpFmt,&cch, &lpParms);
            width=cch;
            /* read the precision */
            if (*lpFmt==(WCHAR)'.') {
                lpFmt=SP_GetFmtValue(++lpFmt,&cch, &lpParms);
                prec=cch;
            } else
                prec=-1;
            /* get the operand size */
            size=1;
            if (*lpFmt=='l') {
                lpFmt++;
            } else if (*lpFmt=='h') {
                size=0;
                lpFmt++;
            } else if (((*lpFmt == 'I')||(*lpFmt == 'i'))
                    && (*(lpFmt+1) == '6')
                    && (*(lpFmt+2) == '4')) {
                lpFmt+=3;
                size = 2;
            }
            upper=0;
            sign=0;
            radix=10;
            switch (*lpFmt) {
                case 0:
                    goto errorout;
                case (WCHAR)'i':
                case (WCHAR)'d':
                    sign++;
                    __fallthrough;
                case (WCHAR)'u':
                    /* turn off prefix if decimal */
                    prefix=0;
donumeric:
                    /* special cases to act like MSC v5.10 */
                    if (left || prec>=0)
                        fillch=(WCHAR)' ';
                    if (size == 1)
                        val.l=va_arg(lpParms, long);
                    else if (size == 2)
                        val.i64 = va_arg(lpParms, __int64);
                    else if (sign)
                        val.l=va_arg(lpParms, short);
                    else
                        val.ul=va_arg(lpParms, unsigned);

                    if (sign) {
                        if ((size < 2) && (val.l < 0L)) {
                            val.l = -val.l;
                        } else if ((2 == size) && (val.i64 < (__int64) 0)) {
                            val.i64 = -val.i64;
                        } else {
                            sign = 0;
                        }
                    }
                    
                    lpT=lpOut;
                    /* blast the number backwards into the user buffer */
                    if (size == 2)
                        cch=SP_PutNumber64(lpOut,val.i64,cchLimit,radix,upper);
                    else
                        cch=SP_PutNumber(lpOut,val.l,cchLimit,radix,upper);
                    if (!(cchLimit-=cch))
                        goto errorout;
                    lpOut+=cch;
                    width-=cch;
                    prec-=cch;
                    if (prec>0)
                    width-=prec;
                    /* fill to the field precision */
                    while (prec-->0)
                        out((WCHAR)'0');
                    if (width>0 && !left) {
                        /* if we're filling with spaces, put sign first */
                        if (fillch!=(WCHAR)'0') {
                            if (sign) {
                                sign=0;
                                out((WCHAR)'-');
                            width--;
                            }
                            if (prefix) {
                                out(prefix);
                                out((WCHAR)'0');
                                prefix=0;
                            }
                        }
                        if (sign)
                            width--;
                        /* fill to the field width */
                        while (width-->0)
                            out(fillch);
                        /* still have a sign? */
                        if (sign)
                            out((WCHAR)'-');
                        if (prefix) {
                            out(prefix);
                            out((WCHAR)'0');
                        }
                        /* now reverse the string in place */
                        SP_Reverse(lpT,lpOut-1);
                } else {
                        /* add the sign character */
                        if (sign) {
                            out((WCHAR)'-');
                            width--;
                        }
                        if (prefix) {
                            out(prefix);
                            out((WCHAR)'0');
                        }
                        /* reverse the string in place */
                    SP_Reverse(lpT,lpOut-1);
                        /* pad to the right of the string in case left aligned */
                    while (width-->0)
                            out(fillch);
                }
                    break;
                case (WCHAR) 'p' :
                    // Fall into case below, since NT is going to 64 bit
                    // they are starting to use p to indicate a pointer
                    // value.  They only seem to support lower case 'p'
                case (WCHAR)'X':
                    upper++;
                    __fallthrough;
                case (WCHAR)'x':
                    radix=16;
                    if (prefix)
                    if (upper)
                            prefix=(WCHAR)'X';
                        else
                            prefix=(WCHAR)'x';
                    goto donumeric;
                case (WCHAR)'c':
                    val.sz[0] = va_arg(lpParms, WCHAR);
                    val.sz[1]=0;
                    lpT=val.sz;
                    cch = 1;  // Length is one character. 
                             /* stack aligned to larger size */
                    goto putstring;
                case 'a':   // ascii string!
                case 'S':
PrtAscii:
                    lpC=va_arg(lpParms, LPCHAR);
                    if (!lpC)
                        lpC = "(NULL)";
                    cch=strlen(lpC);
                    if (prec>=0 && cch>prec)
                        cch=prec;
                    width -= cch;
                    if (left) {
                        while (cch--)
                            out((WCHAR)*lpC++);
                        while (width-->0)
                            out(fillch);
                    } else {
                        while (width-->0)
                            out(fillch);
                        while (cch--)
                            out((WCHAR)*lpC++);
                    }
                    break;
                case 's':
                    if (!size)
                        goto PrtAscii;
                    lpT=va_arg(lpParms,LPWSTR);
                    if (!lpT)
                        lpT = L"(NULL)";
                    cch=NKwcslen(lpT);
putstring:
                    if (prec>=0 && cch>prec)
                        cch=prec;
                    width -= cch;
                    if (left) {
                        while (cch--)
                            out(*lpT++);
                        while (width-->0)
                            out(fillch);
                    } else {
                        while (width-->0)
                            out(fillch);
                        while (cch--)
                            out(*lpT++);
                    }
                    break;
                default:
                    out(*lpFmt);    /* Output the invalid char and continue */
                    break;
            }
        } else /* character not a '%', just do it */
            out(*lpFmt);
        /* advance to next format string character */
        lpFmt++;
    }
errorout:
    *lpOut=0;
    return maxchars-cchLimit;
}



//------------------------------------------------------------------------------
//  GetFmtValue
//  reads a width or precision value from the format string
//------------------------------------------------------------------------------
LPCWSTR 
SP_GetFmtValue(
    LPCWSTR lpch,
    int *lpw,
    va_list *plpParms
    ) 
{
    int i=0;
    if (*lpch == TEXT('*')) {
        *lpw = va_arg(*plpParms, int);
        lpch++;
    } else {
        while (*lpch>=(WCHAR)'0' && *lpch<=(WCHAR)'9') {
            i = i*10 + (*lpch-(WCHAR)'0');
            lpch++;
        }
        *lpw=i;
    }
    /* return the address of the first non-digit character */
    return lpch;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SP_Reverse(
    LPWSTR lpFirst,
    LPWSTR lpLast
    ) 
{
    int swaps;
    WCHAR tmp;
    swaps = ((((DWORD)lpLast - (DWORD)lpFirst)/sizeof(WCHAR)) + 1)/2;
    while (swaps--) {
        tmp = *lpFirst;
        *lpFirst++ = *lpLast;
        *lpLast-- = tmp;
    }
}

const WCHAR lowerval[] = TEXT("0123456789abcdef");
const WCHAR upperval[] = TEXT("0123456789ABCDEF");



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SP_PutNumber(
    LPWSTR lpb,
    ULONG n,
    int limit,
    int radix,
    int mycase
    ) 
{
    int used = 0;
    while (limit--) {
        *lpb++ = (mycase ? upperval[(n % radix)] : lowerval[(n % radix)]);
        used++;
        if (!(n /= radix))
            break;
    }
    return used;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SP_PutNumber64(
    LPWSTR lpb,
    ULONGLONG i64,
    int limit,
    int radix,
    int mycase
    ) 
{
    int used = 0;
    while (limit--) {
        *lpb++ = (mycase ? upperval[(i64 % radix)] : lowerval[(i64 % radix)]);
        used++;
        if (!(i64 /= radix))
            break;
    }
    return used;
}

