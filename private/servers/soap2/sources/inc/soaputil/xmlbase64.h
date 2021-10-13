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
//+----------------------------------------------------------------------------
//
// 
// File:    xmlbase64.h
// 
// Contents:
//
//    Base64 encode/decode function prototypes.
//
//-----------------------------------------------------------------------------
#ifndef _XMLBASE64_H
#define _XMLBASE64_H

DWORD Base64DecodeA(const char * pchIn, DWORD cchIn, BYTE * pbOut, DWORD * pcbOut);
DWORD Base64EncodeA(const BYTE * pbIn, DWORD cbIn, char * pchOut, DWORD * pcchOut);
DWORD Base64EncodeW(BYTE const *pbIn, DWORD cbIn, WCHAR *wszOut, DWORD *pcchOut);
DWORD Base64DecodeW(const WCHAR * wszIn, DWORD cch, BYTE *pbOut, DWORD *pcbOut);

#endif	//_XMLBASE64_H
