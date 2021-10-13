//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: aspmain.h
Abstract: Common include file
--*/



#ifndef _ASP_MAIN_H_
#define _ASP_MAIN_H_


#include <windows.h>
#include <httpext.h>
#include <creg.hxx>
#include "svsutil.hxx"
#include <servutil.h>
#include <httpcomn.h>

// Macro used when another class needs to set the CASPState's error code.

#define ASP_ERR(errCode)	{  m_pASPState->m_aspErr = errCode; }

// Needed to make this work on older devices

// Error codes for ASP.
typedef enum 
{
	ASP_E_PARSER = 0,		// don't add extra info
	ASP_E_UNKNOWN,			// unknown error
	ASP_E_NOMEM,			// out of memory
	ASP_E_SENT_HEADERS,		// Can't do certain ops after we sent headers		
	ASP_E_HTTPD,			// httpd failed, can't determine why

	ASP_E_BUFFER_ON,		// Buffering is on, invalid operation given it is
	ASP_E_FILE_OPEN,	
	ASP_E_BAD_INDEX,
	ASP_E_READ_ONLY,
	ASP_E_EXPECTED_STRING,

	ASP_E_NO_SCRIPT_TERMINATOR,
	ASP_E_BAD_INCLUDE,
	ASP_E_MAX_INCLUDE,
	ASP_E_COMMENT_UNTERMINATED,
	ASP_E_NO_VIRTUAL_DIR,	

	ASP_E_BAD_SCRIPT_LANG,
	ASP_E_NO_ATTRIB,
	ASP_E_UNKNOWN_ATTRIB,
	ASP_E_SCRIPT_INIT,
	ASP_E_NOT_IMPL,

	ASP_E_BAD_SUBINDEX,
	ASP_E_BAD_CODEPAGE,
	ASP_E_BAD_LCID,
	ASP_E_MULTIPLE_INC_LINE,
	ASP_E_INVALID_ERROR_CODE	// Used for debugging error codes. 
								// NOTE:  If you add new
								// error codes, put them BEFORE this line.

}  ASP_ERROR;


#define IDS_E_DEFAULT IDS_E_PARSER


//  Which dictionary type a particular object is (corresponds to collection objects)
typedef enum 
{		 
	QUERY_STRING_TYPE,				
	FORM_TYPE,
	REQUEST_COOKIE_TYPE,	
	RESPONSE_COOKIE_TYPE,
	SERVER_VARIABLE_TYPE,
	EMPTY_TYPE						// No data associated with object.
									// This is used for the empty string object.
} DICT_TYPE;

//****************************************************************
//  Generic String manipulation fcns
//****************************************************************


PSTR MySzDupA(PCSTR pszIn, int iLen=0);
PSTR MySzDupWtoA(PCWSTR wszIn, int iInLen=-1, LONG lCodePage= CP_ACP);
PWSTR MySzDupAtoW(PCSTR pszIn, int iInLen=-1);
PWSTR MySzDupW(PCWSTR wszIn, int iLen=0);

char HexToChar(LPSTR szHex);
HRESULT SysAllocStringFromSz(CHAR *sz,DWORD cch,BSTR *pbstrRet,UINT lCodePage);
HRESULT VariantResolveDispatch(VARIANT *pVarOut, VARIANT *pVarIn);


// Functions to help us get URL / DBCS Encoding Stuff.
char *URLEncode(char *szDest, const char *szSrc);
int URLEncodeLen(const char *szSrc);
char *DBCSEncode(char *szDest, const char *szSrc);

int GetLineNumber(PSTR pszStart, PSTR pszEnd);

// Debug zones
#ifdef DEBUG
  #define ZONE_ERROR	DEBUGZONE(0)
  #define ZONE_INIT		DEBUGZONE(1)
  #define ZONE_SCRIPT	DEBUGZONE(2)
  #define ZONE_SERVER	DEBUGZONE(3)
  #define ZONE_REQUEST	DEBUGZONE(4)
  #define ZONE_RESPONSE	DEBUGZONE(5)
  #define ZONE_DICT     DEBUGZONE(6)
  #define ZONE_MEM      DEBUGZONE(7)
  #define ZONE_PARSER   DEBUGZONE(8)
#endif


#define BEGIN_COMMAND     	"<%"
#define END_COMMAND	      	"%>"

// From JScript
// FACILITY_DISPATCH. The error has been reported to the host via
// IActiveScriptSite::OnScriptError.

#define SCRIPT_E_REPORTED   0x80020101L
#define FACILITY_SHOST 3626
#define E_THREAD_INTERRUPT (MAKE_HRESULT(1, FACILITY_SHOST, 1))

#define MAX_INCLUDE_FILES   16
#define VALUE_GROW_SIZE     10



#include "asprc.h"
#include "stdafx.h"
#include "resource.h"
#include <activscp.h>
#include "..\..\http\asp\asp.h"

#include "mfcrepl.h"
#include "scrsite.h"
#include "asp.h"
#include "script.h"
#include "server.h"
#include "response.h"
#include "request.h"
#include "dict.h"
#include "strlist.h"
#include <initguid.h>


// Extrenal constants
extern const char* rgWkday[];
extern const char* rgMonth[];
extern const char cszDateOutputFmt[];
extern const char cszEmpty[];
extern const char cszTextHTML[];
extern DWORD g_dwTlsSlot;
extern CScriptSite *g_pScriptSite;
extern CRITICAL_SECTION g_scriptCS;

// extern CRequestStrList *g_pStrListEmpty;


//  These functions are used to determine which stubs we're using at runtime.
extern const BOOL c_fUseCollections;
extern const BOOL c_fUseExtendedParse;


//  This function is an international safe strstr.  It does a conventional
//  strstr and then makes sure that the byte before the match isn't a leading byte.
//  For instance, if we found the string "<%" in a different language file,
//  it would be possible that the '<' isn't really a seperate character but is
//  trailing character of the byte before it.


#define MyLoadStringW2A(hInst, ErrCode, wsz, sz)   {\
if (0 != LoadString((hInst),(ErrCode),(wsz),ARRAYSIZEOF((wsz)))) \
	MyW2A((wsz),(sz),ARRAYSIZEOF((sz))); \
else \
	(sz)[0] = 0; \
}
	
inline PSTR MyStrStr(PSTR pszStart, PSTR pszSearch)
{
	PSTR psz = pszStart;

	while (psz = strstr(psz,pszSearch))
	{	
		// at beginning of string, must be leading byte.  
		if (psz == pszStart)
			return psz;

		if (! isleadbyte(psz[-1]))
			return psz;

		psz++;
	}
	return NULL;
}


#if defined (OLD_CE_BUILD) && (_WIN32_WCE < 300)
// Write our own strrchr if we're using version 2.0 of CE, it wouldn't exist otherwise
inline char *strrchr( const char *string, int c )  
{ 
	PCSTR pszTrav = string;
	PSTR pszLast = NULL;
	while ( *pszTrav )
	{
		if (*pszTrav == (CHAR) c)
			pszLast = (PSTR) pszTrav;
		pszTrav++;
	}
	return pszLast;
}

#ifdef isalnum
#undef isalnum
#endif  // isalnum

#define isalnum(_c)		( ((_c) >= 'A' && (_c) <= 'Z') || ((_c) >= 'a' && (_c) <= 'z') || ((_c) >= '0' && (_c) <= '9') )

#ifdef isspace
#undef isspace
#endif // isspace

inline int isspace(int c) {
	return ( c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v');
}

#endif  // OLD_CE_BUILD
#endif    // _ASP_MAIN_H_
