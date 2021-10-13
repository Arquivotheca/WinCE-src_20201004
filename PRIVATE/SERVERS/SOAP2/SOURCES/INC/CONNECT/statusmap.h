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
// File:
//      StatusMap.h
//
// Contents:
//
//      Declaration of functions mapping HTTP status code into HRESULT
//
//-----------------------------------------------------------------------------

#ifndef __STATUSMAP_H_INCLUDED__
#define __STATUSMAP_H_INCLUDED__

struct HttpMapEntry
{
    DWORD   http;
    HRESULT hr;
};

struct HttpMap
{
    DWORD           elc;
    HttpMapEntry   *elv;
    HRESULT         dflt;
};

extern HttpMap g_HttpStatusCodeMap[];

HRESULT HttpStatusToHresult(DWORD dwStatus);
HRESULT HttpContentTypeToHresult(LPCSTR contentType);
HRESULT HttpContentTypeToHresult(LPCWSTR contentType);

#endif //__STATUSMAP_H_INCLUDED__

