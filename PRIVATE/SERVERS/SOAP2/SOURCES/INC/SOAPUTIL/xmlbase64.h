//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
