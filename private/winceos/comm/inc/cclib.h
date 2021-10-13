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
/**********************************************************************/
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    ccutil.h

   Declarations for some useful C library functions.

*/
#ifndef _CCLIB_H_
#define _CCLIB_H_

#if defined (__cplusplus)
extern "C"  { 
#endif 

#ifdef UNDER_CE
#include "ndis.h"
#include "dumpmem.h"
#endif // UNDER_CE

//	C Library Functions
//	===================

#ifdef UNDER_CE

#ifdef DONT_COMPILE
// <string.h> functions
#ifdef DEBUG
// These functions are compiler intrinsic
char *strcpy(char *s, char *ct);
unsigned int strlen(char *s);
int strcmp (const char *s1, const char *s2);
#endif // DEBUG
char *strchr(char *s, int c);
#endif // DONT_COMPILE

_CRTIMP char * __cdecl strncpy(char *, const char *, size_t);
_CRTIMP int __cdecl strncmp(const char *, const char *, size_t);

#ifdef UNICODE
#define _tcstol wcstol
#else
#define _tcstol strtol
#endif

PNDIS_BUFFER CopyFlatToNdis(PNDIS_BUFFER DestBuf, uchar *SrcBuf, uint Size,
							uint *StartOffset);
#endif // UNDER_CE


// Help with ARGC/ARGV 

int	CreateArgvArgc(TCHAR *pProgName, TCHAR *argv[20], TCHAR *pCmdLine);



//  PRF and HMAC functions used by Wifi authentication.
#define HMAC_K_PADSIZE              64
VOID
WL_HMAC_SHA1(
    IN      PBYTE       pbData,                     // input mixing data
    IN      DWORD       cbData,
    IN      BYTE        *pbKeyMaterial,              // input key material
    IN      DWORD       cbKeyMaterial,
    IN OUT  BYTE        pbHMAC[],                   // output buffer
    IN      DWORD       cbHMAC                      // output buffer length
    );



#if defined (__cplusplus)
}  
#endif  // CPLUSPLUS
#endif // _CCLIB_H_
