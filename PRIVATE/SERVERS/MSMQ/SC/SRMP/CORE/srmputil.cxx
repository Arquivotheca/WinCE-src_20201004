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

    SrmpUtil.cxx

Abstract:

    Generic helper functions for SRMP
 
--*/


#include <windows.h>
#include <msxml2.h>
#include <svsutil.hxx>

#include "sc.hxx"
#include "scsrmp.hxx"
#include "srmpdefs.hxx"


//
//  Definitions
//
#define GUID_FORMAT     L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define GUID_STR_LENGTH (8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12)

//
//  Utility functions
//
BOOL UTF8ToBSTR(PSTR psz, BSTR *ppBstr) {
	DWORD cp = CP_UTF8;

	int iLen = MultiByteToWideChar(cp, 0, psz, -1, NULL, NULL);
	if (0 == iLen) {
		cp = CP_ACP;
		iLen = MultiByteToWideChar(cp, 0, psz, -1, NULL, NULL);

		if (0 == iLen) {
			scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"Srmp: Unable to convert UTF8 text to UNICODE.  GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}
	}

	if (NULL == ((*ppBstr) = SysAllocStringByteLen(NULL,(iLen+1)*sizeof(WCHAR)))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"Srmp: SysAllocStringByteLen fails.  GLE=0x%08x\r\n",GetLastError()));
		return FALSE;
	}

	MultiByteToWideChar(cp, 0, psz, -1, *ppBstr, iLen);
	return TRUE;
}


// Can be run on HTTP or MIME header types, finds szHeader in szHeaderList.
// Returns to the next character after end of header string
PSTR FindHeader(PCSTR szHeaderList, PCSTR szHeader) {
	PSTR pszTrav = (PSTR) szHeaderList;
	DWORD cbHeader = strlen(szHeader);

	do {
		// reached double CRLF
		if (0 == _strnicmp(pszTrav,cszCRLF2,ccCRLF2))
			return NULL;

		if (0 == _strnicmp(pszTrav,szHeader,cbHeader))
			return pszTrav + cbHeader;

		if (NULL == (pszTrav = strstr(pszTrav,cszCRLF)))
			return NULL;

		pszTrav += ccCRLF;
	} while (1);

	return NULL;
}


PSTR MyStrStrI(const char * str1, const char * str2) {
	char *cp = (char *) str1;
	char *s1, *s2;
	while (*cp) {
		s1 = cp;
		s2 = (char *) str2;
		while ( *s1 && *s2 && (tolower(*s1) == tolower(*s2)) )
			s1++, s2++;
		if (!*s2)
			return(cp);
		cp++;
	}
	return(NULL);
}


PSTR RemoveLeadingSpace(LPCSTR p, LPCSTR pEnd) {
    for(; ((pEnd > p) && iswspace(*p)); ++p) {
        ;
    }
    return (PSTR) p;
}

PSTR RemoveTralingSpace(LPCSTR p, LPCSTR pEnd) {
    for(; ((pEnd >= p) && iswspace(*pEnd)); --pEnd)  {
        ;
    }
    return (PSTR) pEnd;
}


BOOL StringToGuid(LPCWSTR p, GUID* pGuid) {
	UINT w2, w3, d[8];
	if(swscanf(p,GUID_FORMAT,&pGuid->Data1,
	        &w2, &w3,                       //  Data2, Data3
	        &d[0], &d[1], &d[2], &d[3],     //  Data4[0..3]
	        &d[4], &d[5], &d[6], &d[7]      //  Data4[4..7]
	        ) != 11)
	    return FALSE;

	pGuid->Data2 = static_cast<WORD>(w2);
	pGuid->Data3 = static_cast<WORD>(w3);
	for(int i = 0; i < 8; i++) 	{
	    pGuid->Data4[i] = static_cast<BYTE>(d[i]);
	}
	return TRUE;
}
