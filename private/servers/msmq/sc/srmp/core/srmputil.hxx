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


Module Name:

    srmputil.hxx

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

