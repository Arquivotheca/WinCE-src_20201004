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

    scsrmp.hxx

Abstract:

    SRMP Helper functions

--*/



#if ! defined (__scrmutil_HXX__)
#define __scrmutil_HXX__	1

#define GUID_FORMAT_A	"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"

BOOL UTF8ToBSTR(PSTR psz, BSTR *ppBstr);
PSTR FindHeader(PCSTR szHeaderList, PCSTR szHeader);
PSTR MyStrStrI(const char * str1, const char * str2);
PSTR RemoveLeadingSpace(LPCSTR p, LPCSTR pEnd);
PSTR RemoveTralingSpace(LPCSTR p, LPCSTR pEnd);
BOOL StringToGuid(LPCWSTR p, GUID* pGuid);

#endif // __scrmutil_HXX__

