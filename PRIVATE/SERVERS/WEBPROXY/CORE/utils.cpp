//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "utils.h"

CBuffer::CBuffer(void)
{
	m_pBuffer = m_Buffer;
	m_iSize = DEFAULT_BUFFER_SIZE;
}

CBuffer::~CBuffer(void)
{
	if (m_pBuffer != m_Buffer) {
		delete[] m_pBuffer;
	}
}

PBYTE CBuffer::GetBuffer(int iSize, BOOL fPreserveData)
{	
	//
	// If buffer is not big enough, we have to allocate new buffer
	// which is big enough.
	//
	
	if (iSize > m_iSize) {
		PBYTE pTmp = new BYTE[iSize];
		if (! pTmp) {
			return NULL;
		}

		if (fPreserveData) {
			memcpy(pTmp, m_pBuffer, m_iSize);
		}
	
		// Delete current buffer if it was allocated on the heap
		if (m_pBuffer != m_Buffer) {
			delete[] m_pBuffer;
		}

		// If alloc succeeded then update buffer and size
		m_pBuffer = pTmp;
		m_iSize = iSize;
	}

	return m_pBuffer;
}

int CBuffer::GetSize(void)
{
	return m_iSize;
}


//
//  Functions related to MyInternetCanonicalizationURL
//

#define HEX_ESCAPE	'%'

inline int IsNonCRLFSpace(CHAR c) {
	return ( c == ' ' || c == '\t' || c == '\f' || c == '\v');
}

static const char *HTSkipLeadingWhitespace(const char *str) {
	while (*str == ' ' || *str == '\t') {
		++str;
	}
	return str;
}


// When parsing an URL, if it is something like http://WinCE////Foo/\a.dll/\\john?queryString,
// only allow one '/' or '\' to be consecutive, skip others, up to start of query string.
void HTCopyBufSkipMultipleSlashes(char *szWrite, const char *szRead) {
	BOOL fSkip = FALSE;
	int iStartSkip = 0;

	// Look for "http://" or "https://" at start of string
	if (0 == strncmp(szRead, "http://", 7)) {
		iStartSkip = 7;
	}
	else if (0 == strncmp(szRead, "https://", 8)) {
		iStartSkip = 8;
	}

	if (iStartSkip) {
		strncpy(szWrite, szRead, iStartSkip);
		szWrite += iStartSkip;
		szRead += iStartSkip;
	}
	
	while (*szRead && *szRead != '?') {
		if ((*szRead == '/') || (*szRead == '\\')) {
			if (!fSkip)
				*szWrite++ = *szRead;
		
			fSkip = TRUE; // skip next if it has a '/' or '\\'
		}
		else {
			*szWrite++ = *szRead;
			fSkip = FALSE;
		}
		szRead++;
	}
	strcpy(szWrite,szRead);
}

char from_hex(char c) {
	return c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10;         /* accept small letters just in case */
}


static void HTTrimTrailingWhitespace(char *str)  {
#define CR          '\r'
#define LF          '\n'
	char *ptr;

	ptr = str;

	// First, find end of string

	while (*ptr)
		++ptr;

	for (ptr--; ptr >= str; ptr--) {
		if (IsNonCRLFSpace(*ptr))
			*ptr = 0; /* Zap trailing blanks */
		else
			break;
	}
}

PSTR HTUnEscape(char *pszPath) {
	PSTR pszRead = pszPath;
	PSTR pszWrite = pszPath;

	while (*pszRead)  {
		if (*pszRead == '?')  {
			// leave the query string intact, no conversion.
			while (*pszRead)  {
				*pszWrite++ = *pszRead++;
			}	
			break;
	   	}
		if ((*pszRead == HEX_ESCAPE) && isxdigit(*(pszRead+1)) && isxdigit(*(pszRead+2)))  {
			pszRead++;
			if (*pszRead)
				*pszWrite = from_hex(*pszRead++) * 16;
			if (*pszRead)
				*pszWrite = *pszWrite + from_hex(*pszRead++);
			pszWrite++;
			continue;
		}
		if (*pszRead)
			*pszWrite++ = *pszRead++;
	}
	*pszWrite++ = 0;

	// After conversion, scan for "/.." and remove.
	pszWrite = pszPath;

	while (*pszWrite) {
		if (pszWrite[0] == '?') {
			// leave query string intact.
			break;
		}

		if ((pszWrite[0] == '\\' || pszWrite[0] == '/') &&
			(pszWrite[1] == '.'  && pszWrite[2] == '.') &&
			(pszWrite[3] == '\\' || pszWrite[3] == '/' || pszWrite[3] == 0))  {
			// In this case we remove the previous path, so a request
			// for "/doc/../home.htm" would become "/home.htm" since they
			// are logically equivalent.  	
			// If there was no previous path to remove, as in "/../home.txt",
			// we simply strip the ".." and make URL "/home.txt"


   			PSTR pszPrevPath = (pszWrite == pszPath) ? pszPath : pszWrite - 1;
			for (; pszPrevPath > pszPath; pszPrevPath--)  {
				if (*pszPrevPath == '\\' || *pszPrevPath == '/')
					break;
			}

			PSTR pszCopy = pszWrite;
			pszWrite = pszPrevPath+1;

			if (pszCopy[3]) {
				strcpy(pszWrite,pszCopy+4);
			} 
			else {
				pszWrite[0] = 0;
			}
			pszWrite--;  // need to backtrack one character for "dir1/dir2/../.." case.
		}
		else {
			// typical case.  No conversion.
			pszWrite++;
		}
	}
	return pszPath;
}

BOOL
MyInternetCanonicalizeUrlA(IN LPCSTR lpszUrl,OUT LPSTR lpszBuffer)
{
	const char * pszTrav = HTSkipLeadingWhitespace(lpszUrl);
	HTCopyBufSkipMultipleSlashes(lpszBuffer,pszTrav);
	HTTrimTrailingWhitespace(lpszBuffer);
	HTUnEscape(lpszBuffer);	

	return TRUE;
}


