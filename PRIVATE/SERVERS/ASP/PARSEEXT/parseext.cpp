//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: parseext.cpp
Abstract: Extended parsing functions that can be compomentalized away

		  This file has functionality to:
          Preprocessing of directives  <%@ ... %>
          Include files.
--*/

#include "aspmain.h"

#define PREPROC_LANG		"LANGUAGE"
#define PREPROC_LCID		"LCID"
#define PREPROC_CDPG		"CODEPAGE"


// for #include stuff
#define COMMENT_BEGIN			"<!--"
#define COMMENT_END				"-->"
#define INC_INCLUDE			"#include"
#define INC_FILE			"file"
#define INC_VIRTUAL			"virtual"

const BOOL c_fUseExtendedParse = TRUE;

//  Looks for <!--  #include file | virtual="file" -->, reads that data into pszFileData

//  NOTE:  No support for more than 1 level of #includes, 16 files max to include.
//  IIS supports an arbitrary number of include files, and allows include files
//  to include other include files, none of which is supported on WinCE ASP.

BOOL CASPState::ParseIncludes()
{		
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	int nNewChars = 0;		// extra space we'll need
	int nFiles = 0;
	int i;
	DWORD dwMaxFileSize = 0;		// largest file we've come across



	// FindIncludeFiles determine amount of memory to add to buffer and gets the
	// file handles loaded up.
	if ( ! FindIncludeFiles(&nFiles,&nNewChars,&dwMaxFileSize))
		myleave(60);


	if (nFiles == 0)		// no inc files, jump to the end
	{
		ret = TRUE;
		myleave(0);
	}

	// ReadIncludeFiles uses file handles to suck include file info into script text
	if ( ! ReadIncludeFiles(nFiles,nNewChars,dwMaxFileSize))
		myleave(69);


	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: ParseIncludes failed, err = %d\r\n",err));
	// No call to ServerError, the called fcns will call it instead

	for (i = 0; i < nFiles; i++)
	{
		MyCloseHandle(m_pIncludeInfo[i].m_hFile);
		m_pIncludeInfo[i].m_hFile = INVALID_HANDLE_VALUE;
	}

	return ret;
}


// Runs through text, maps #include files to physical ones and gets a handle to them.
// This function does not modify the file text itself or read in any files -
// this is handled by ReadIncludeFiles
BOOL CASPState::FindIncludeFiles(int *pnFiles, int *pnNewChars, DWORD *pdwMaxFileSize)
{
	DEBUG_CODE_INIT;
	PSTR pszTrav = NULL;
	BOOL ret = FALSE;
	int nNewChars = 0;		// extra space we'll need for scripting data after inc files are read
	int nFiles = 0;
	char szErr[MAX_PATH + 30];
	WCHAR wszFileName[MAX_PATH+1];
	PSTR pszSubEnd = NULL;	
	PSTR pszSubBegin = NULL;		
	PSTR pszTemp = NULL;

	DWORD dwFileSize;
	DWORD dwMaxFileSize = 0;		// largest file we've come across
	BOOL fFile;						// Using file or virtual directory?


	szErr[0] = '\0';

	for(pszTrav=m_pszFileData; pszTrav=MyStrStr(pszTrav, COMMENT_BEGIN); )		// look for <!--
	{
		pszSubBegin = pszTrav;		// keep original position
		pszTrav += sizeof(COMMENT_BEGIN) - 1;
		svsutil_SkipWhiteSpace(pszTrav);	


		// NOTE: The script engine right now executes code inside HTLM comments,
		// IIS skips code inside HTLM comments.
		// For example we'd execute the statement <!-- <% x = 5 %> -->

		// The script writer can manually comment lines of code out
		// at a time through the script language.

		// continue past regular HTLM comment
		if (0 != _memicmp(pszTrav,INC_INCLUDE,sizeof(INC_INCLUDE) - 1))
			continue;

		// Otherwise this is a <!-- #include ..., continue parsing.

		//  Note: If there are 2 or more #includes on the same line,our error reporting 
		//  (matching the line #'s in the script to line #'s on the page) would 
		//  get really messed up, so we don't allow it in 1st place.

		if (nFiles && 1 == GetLineNumber(m_pIncludeInfo[nFiles-1].m_pszEnd, pszTrav))
		{
			m_aspErr = IDS_E_MULTIPLE_INC_LINE;
			myleave(58);
		}
		
		// is this going to push us over the limit?
		if (nFiles == MAX_INCLUDE_FILES)
		{
			m_aspErr = IDS_E_MAX_INCLUDE;
			myleave(59);
		}

		m_aspErr = IDS_E_BAD_INCLUDE;

		pszTrav += sizeof(INC_INCLUDE) - 1;
		svsutil_SkipWhiteSpace(pszTrav);


		//  The next field tells us if it's a physical or virtual directory.
		if ( 0 == _memicmp(pszTrav,INC_FILE,sizeof(INC_FILE) - 1))
		{
			pszTrav += sizeof(INC_FILE) - 1;
			fFile = TRUE;
		}
		else if (0 == _memicmp(pszTrav,INC_VIRTUAL,sizeof(INC_VIRTUAL) - 1))
		{
			pszTrav += sizeof(INC_VIRTUAL) - 1;
			fFile = FALSE;
		}
		else
			myleave(60);		
		svsutil_SkipWhiteSpace(pszTrav);	

		if ( *pszTrav != '=')
			myleave(63);

		pszTrav++;	// Skip =
		svsutil_SkipWhiteSpace(pszTrav);

		if ( (*pszTrav != '\"') && (*pszTrav != '\''))
			myleave(61);	

		pszTrav++;
		pszSubEnd = pszTrav;
		while (*pszSubEnd) {
			if ((*pszSubEnd == '\"') || (*pszSubEnd == '\''))
				break;
			pszSubEnd++;
		}
		if (! (*pszSubEnd))
			myleave(62);

		if (pszSubEnd - pszTrav > (MAX_PATH-1)) {
			// filesys won't be able to handle this and would overflow our buffers if we try to copy.
			m_aspErr = IDS_E_FILE_OPEN;
			myleave(78);
		}

		// portion off a substring.  We'll reset it later.
		// pszTrav now has the file name, without the trailing quotes
		*pszSubEnd = '\0';

		//  Map the file to physical path.
		if(fFile ? !GetIncludeFilePath(pszTrav, wszFileName) : !GetVirtualIncludeFilePath(pszTrav, wszFileName) )
			myleave(68);

		// On the first include file we find, we must allocate the structure 
		// that helps us keep track of error line #'s.
		if (0 == nFiles )
		{
			int i;
			
			if (NULL == (m_pIncludeInfo = MyRgAllocZ(INCLUDE_INFO, MAX_INCLUDE_FILES)))
			{
				m_aspErr = IDS_E_NOMEM;
				myleave(71);
			}

			// We set the # of lines in each file to 1 initially.  This way should
			// there be an error before we can get actual count (by sucking file in)
			// then we'll use amount of space it takes up in the script.
			for (i = 0; i < MAX_INCLUDE_FILES; i++)
				m_pIncludeInfo[i].m_cLines = 1;	
		}
		if(NULL == (m_pIncludeInfo[nFiles].m_pszFileName = MySzDupA(pszTrav,pszSubEnd - pszTrav)))
		{
			m_aspErr = IDS_E_NOMEM;
			myleave(72);
		}

		if ( NTFAILED(m_pIncludeInfo[nFiles].m_hFile = MyOpenReadFile(wszFileName)) ||
			 NTFAILED((HANDLE)(dwFileSize = GetFileSize(m_pIncludeInfo[nFiles].m_hFile, NULL))))
		{
			WCHAR wszFormat[256]; 
			CHAR szFormat[256];

			MyLoadStringW2A(m_pACB->hInst,IDS_ERROR_CODE,wszFormat,szFormat);
			sprintf(szErr,szFormat,pszTrav, GetLastError());			
			m_aspErr = IDS_E_FILE_OPEN;
			myleave(69);
		}
		
		//  Determine the largest file size, this will be used when it's time
		//  to read them in in ReadIncludeFiles()
		if (dwFileSize > dwMaxFileSize)
			dwMaxFileSize = dwFileSize;

		// Put original char back in place
		*pszSubEnd = '\"';

		// Don't set this to pszTrav right away in case there's an error, 
		// GetLineNumber needs pszTrav to be non-NULL for this to work.	
		pszSubEnd = MyStrStr(pszTrav,COMMENT_END);
		if (NULL == pszSubEnd)
		{
			m_aspErr = IDS_E_COMMENT_UNTERMINATED;
			myleave(66);
		}		
		
		pszTrav  = pszSubEnd + sizeof(COMMENT_END) - 1;

		// make sure there's no <% commands beginning on same line as #include file,
		// if there are this might screw up error reporting.
		if (NULL != (pszTemp = MyStrStr(pszTrav, BEGIN_COMMAND)) &&
			1 == GetLineNumber(pszTrav, pszTemp))
		{
			m_aspErr = IDS_E_MULTIPLE_INC_LINE;
			myleave(70);
		}

		m_pIncludeInfo[nFiles].m_pszStart = pszSubBegin;
		m_pIncludeInfo[nFiles].m_pszEnd   = pszTrav;
		m_pIncludeInfo[nFiles].m_dwFileSize = dwFileSize;

		nFiles++;

		// The amount of extra memory needed is the size of the new file
		// minus the size of the <!-- #include ... --> directive it will replace		
		nNewChars += (dwFileSize - ( pszTrav - pszSubBegin));		
	}

	m_aspErr = IDS_E_DEFAULT;	// set error back to default
	ret = TRUE;
done:
	if (FALSE == ret)
	{	
		ServerError(szErr, GetLineNumber(m_pszFileData, pszTrav));
	}
	
	*pnFiles = nFiles;
	*pnNewChars = nNewChars;
	*pdwMaxFileSize = dwMaxFileSize;


	return ret;
}


//  This function sucks in the include files found in FindInclude files.
BOOL CASPState::ReadIncludeFiles(int nFiles, int nNewChars, DWORD dwMaxFileSize)
{
	DEBUG_CODE_INIT;
	PSTR pszNewFileData = NULL;		// if we include stuff, shove it in here
	PSTR pszRead = m_pszFileData;
	PSTR pszWrite = NULL;
	PSTR pszIncFile = NULL;			// buffer, holds include file
	DWORD dwRead;
	int iErrLine = -1;
	int i;
	int nLines = 0;

	BOOL ret = FALSE;

	if( !(pszWrite = pszNewFileData = MyRgAllocZ(CHAR,m_cbFileData + nNewChars +1)) ||
		!(pszIncFile = MyRgAllocNZ(CHAR,dwMaxFileSize+1)) )
	{
		MyFree(pszNewFileData);
		m_aspErr = IDS_E_NOMEM;
		myleave(68);
	}

	pszNewFileData[m_cbFileData + nNewChars ] = '\0';
	pszIncFile[dwMaxFileSize] 		  = '\0';

	for (i = 0; i < nFiles ; i++)
	{
		// read between end of last inc file (or beginning of file) and the
		// 1st # include
		memcpy(pszWrite, pszRead, m_pIncludeInfo[i].m_pszStart - pszRead);
		pszWrite += m_pIncludeInfo[i].m_pszStart - pszRead;
		
		nLines += GetLineNumber(pszRead, m_pIncludeInfo[i].m_pszStart);
		

		// Read the include file.
		if (! ReadFile(m_pIncludeInfo[i].m_hFile, (LPVOID) pszIncFile, 
					   m_pIncludeInfo[i].m_dwFileSize, &dwRead, 0))
		{
			DEBUGCHK(FALSE);
			myleave(44);
		}

		m_pIncludeInfo[i].m_iStartLine = nLines;

		m_pIncludeInfo[i].m_cLines = GetLineNumber(pszIncFile, 
										pszIncFile + m_pIncludeInfo[i].m_dwFileSize);

		DEBUGMSG(ZONE_PARSER,(L"ASP: Inc file <<%a>> starts line %d, has %d lines\r\n",
							m_pIncludeInfo[i].m_pszFileName, m_pIncludeInfo[i].m_iStartLine,
							m_pIncludeInfo[i].m_cLines));



		//  To get nLines, add the # of lines in the file and subtract 2.  Why 2??
		//  We subtract 1 line because the first line of the inc file is put onto the 
		//  same line as the <! --- >, IE a 3 line include file really only 
		//  spills over into 2 extra lines.
		
		//  We another line from nLines because the newline at the end of this
		//  current line will be counted on the call to GetLineNumber(pszRead, m_pIncludeInfo[i].m_pszStart)
		//  this avoids counting it twice.
	
		nLines += m_pIncludeInfo[i].m_cLines - 2;

		memcpy(pszWrite, pszIncFile, m_pIncludeInfo[i].m_dwFileSize);
		pszWrite += m_pIncludeInfo[i].m_dwFileSize;
		pszRead = m_pIncludeInfo[i].m_pszEnd;
	}

	// now copy the remaining data between last inc file and end of file.
	memcpy(pszWrite, pszRead, m_pszFileData + m_cbFileData - pszRead);
	
	MyFree(m_pszFileData);
	m_pszFileData = pszNewFileData;
	m_cbFileData += nNewChars;
	DEBUGCHK(m_cbFileData > 0);

	ret = TRUE;
done:
	if (FALSE == ret)
		ServerError(NULL,iErrLine);

	MyFree(pszIncFile);
	return ret;
}

// NOTE: assumes out buffer is MAX_PATH wchars
BOOL CASPState::GetVirtualIncludeFilePath(PSTR pszFileName, WCHAR wszNewFileName[])
{
	CHAR szBuf[MAX_PATH+1];
	DWORD dwLen = sizeof(szBuf);

	// Calling function should error out before calling this function if
	// the file name is too long.
	DEBUGCHK(strlen(pszFileName) < sizeof(szBuf));

	// puts the file into a new buffer, which is written in place
	strcpy(szBuf, pszFileName);

	// Use server support to change this buffer into physical path
	if ( ! m_pACB->ServerSupportFunction(m_pACB->ConnID,
			HSE_REQ_MAP_URL_TO_PATH,szBuf, &dwLen, NULL))
	{
		m_aspErr = IDS_E_NO_VIRTUAL_DIR;
		return FALSE;
	}

	// Convert to Wide
	MyA2W(szBuf,wszNewFileName, MAX_PATH+1);
	return TRUE;
}

// NOTE: assumes out buffer is MAX_PATH wchars
BOOL CASPState::GetIncludeFilePath(PSTR pszFileName, WCHAR wszNewFileName[])
{
	WCHAR *wsz = wszNewFileName;
	WCHAR *wszEnd = wsz;

	// We get the relative path (directory he parent ASP is in) by ripping out the very last "/" on the ASP path
	wcscpy(wszNewFileName, m_pACB->wszFileName);

	// Find the last "\" or "/" in the path and use that as base path to copy from.
	for (wsz = wszNewFileName; *wsz; wsz++)
	{
		if (*wsz == '\\' || *wsz == '/')
			wszEnd = wsz+1;	// want to start copying one space past the \ or /
	}
	DEBUGCHK(wszEnd != wsz);
	MyA2W(pszFileName, wszEnd, MAX_PATH-(wszEnd-wszNewFileName));

	return TRUE;
}


//  Scans text, looking for <%@
//  NOTE:  No support for ENABLESESSIONSTATE or TRANSACTION identifiers

//  NOTE:  Technically there can only be one Preproc directive per page, and
//  it must come before any other <% directives.  IIS checks for this, we don't,
//  We use first <%@ directive on page, ignoring any others.
BOOL CASPState::ParseProcDirectives()
{
	PSTR pszTrav = m_pszFileData;		// walks across file
	PSTR pszSubEnd = NULL;			// end of directive

	// Search for <%@.  It's valid for there to be spaces between the <% and @
	while ( pszTrav = MyStrStr(pszTrav,BEGIN_COMMAND))
	{
		pszTrav += sizeof(BEGIN_COMMAND) - 1;
		svsutil_SkipWhiteSpace(pszTrav);
		
		if ( *pszTrav == '@' )
		{
			if (NULL == (pszSubEnd = MyStrStr(pszTrav,END_COMMAND)))
			{
				m_aspErr = IDS_E_NO_SCRIPT_TERMINATOR;
				ServerError(NULL,GetLineNumber(m_pszFileData, pszTrav));
				return FALSE;
			}
		}
		pszTrav++;	// skip past '@'
		svsutil_SkipWhiteSpace(pszTrav);

		//  This fcn made it's own ServerError call, so we don't do it again.
		if ( ! ParseProcDirectiveLine(pszTrav, pszSubEnd))
			return FALSE;	
	}
	return TRUE;
}


//  Handles the <%@ ... %> line
//  There are four cases of var setting + spacing we must worry about,
//  Var=VarName, Var= VarName, Var =VarName, Var = VarName
BOOL CASPState::ParseProcDirectiveLine(PSTR pszTrav, PSTR pszSubEnd)
{
	DEBUG_CODE_INIT;
	PSTR pszNext = NULL;
	PSTR pszEnd = NULL;
	CHAR szErr[256];
	CHAR szVarName[128];
	CHAR szVarValue[128];
	PSTR pszDiv = NULL;
	BOOL ret = FALSE;
	
	szErr[0] = '\0';

	// DebugBreak();
	while (pszTrav < pszSubEnd)
	{
		// Look for "=" sign.  If it's not there or if it's past the trailing %>
		// then there's an error.
		
		if(!(pszNext = pszEnd = strchr(pszTrav, '=')) ||
			(pszNext > pszSubEnd) || ((pszEnd-pszTrav+1) > sizeof(szVarName)))
		{
			m_aspErr = IDS_E_NO_ATTRIB;
			strncpy(szErr,pszTrav,min(sizeof(szErr), pszSubEnd - pszTrav));
			szErr[(min(sizeof(szErr), pszSubEnd - pszTrav)) - 1] = 0;
			myleave(191);
		}

		// get var name
		svsutil_SkipWhiteSpaceRev(pszEnd, pszTrav);
		Nstrcpy(szVarName, pszTrav, pszEnd-pszTrav);

		// skip to var value
		pszNext++;		// skip past '='
		svsutil_SkipWhiteSpace(pszNext);

		if (*pszNext == '\"')  // skip past at most one quote character.
			pszNext++;

		// case where there's no attribute set, ie <%LANGUAGE = %>
		if (pszNext == pszSubEnd)
		{
			m_aspErr = IDS_E_NO_ATTRIB;
			strcpy(szErr,szVarName);
				myleave(191);
		}

		pszEnd = pszNext;
		svsutil_SkipNonWhiteSpace(pszEnd);

		// This handles case where there's no space between last variable and %>
		if (pszEnd > pszSubEnd)
			pszEnd = pszSubEnd;

		if (*(pszEnd-1) == '\"')  // swallow at most one quote character.
			pszEnd--;

		// get var value
		if ((pszEnd <= pszNext) || ((pszEnd-pszNext+1) > sizeof(szVarValue)))
		{
			m_aspErr = IDS_E_NO_ATTRIB;
			strcpy(szErr,szVarName);
			myleave(194);
		}
		
		Nstrcpy(szVarValue, pszNext, pszEnd-pszNext);

		// move the ptr up
		pszTrav = pszEnd;
		
		// Now that we have szVarName and szVarValue, figure out what they are
		if (0 == strcmpi(szVarName, PREPROC_LANG))
		{
			if (0 == strcmpi(szVarValue,"VBSCRIPT"))
			{
				m_scriptLang = VBSCRIPT;
			}
			else if (0 == strcmpi(szVarValue,"JSCRIPT") ||
					(0 == strcmpi(szVarValue,"JAVASCRIPT")))
			{
				m_scriptLang = JSCRIPT;
			}
			else
			{
				m_aspErr = IDS_E_BAD_SCRIPT_LANG;
				strcpy(szErr, szVarValue);
				myleave(193);
			}
		}

		//  Note:  Even though the rest of the world usually refers to LCIDs in
		//  terms of their hex value, IIS reads them in as decimal values.  So do
		//  we.
		
		else if (0 == strcmpi(szVarName, PREPROC_LCID))
		{
			if ( 0 == sscanf(szVarValue, "%d", &m_lcid))
			{
				m_aspErr = IDS_E_NO_ATTRIB;
				strcpy(szErr,szVarName);
				myleave(194);
			}

		// make sure LCID is valid
		    if (FALSE == IsValidLocale(m_lcid, LCID_SUPPORTED))
		    {
		    	m_aspErr = IDS_E_BAD_LCID;
		    	strcpy(szErr,szVarValue);
		    	myleave(199);
		   	}

		}
		else if (0 == strcmpi(szVarName, PREPROC_CDPG))
		{
			CPINFO cp;
			if ( 0 == sscanf(szVarValue, "%d", &m_lCodePage))
			{
				m_aspErr = IDS_E_NO_ATTRIB;
				strcpy(szErr,szVarName);
				myleave(195);
			}

			if (! GetCPInfo(m_lCodePage, &cp))
			{
				m_aspErr = IDS_E_BAD_CODEPAGE;
				sprintf(szErr," %d",m_lCodePage);
				myleave(198);
			}
		}
		else 
		{
			m_aspErr = IDS_E_UNKNOWN_ATTRIB;
			strcpy(szErr,szVarName);
			myleave(196);
		}

		if (*pszTrav == '\"')  // skip past at most one quote character.
			pszTrav++;

		svsutil_SkipWhiteSpace(pszTrav);
	}

	ret = TRUE;
done:
	if (FALSE == ret)
		ServerError(szErr);
		
	return ret;
}


//  This fcn helps us resolve errors line numbers and file names if the
//  script writer used #include files.  

//  iScriptErrLine is the line the error occured on according to IActiveScript

//  ppszFileName points to the include file (if error occured there) or
//  to NULL if the error occured in the main ASP page itself. 

//  piFileName is set to the actual line # the error occured on, in either the
//  script or the include file.
void CASPState::GetIncErrorInfo(int iScriptErrLine, 
								 int *piFileErrLine, PSTR *ppszFileName)
{
	int i;
	int iTotalIncLines = 0;

	
	// If no inc files, then leave everything alone
	if (NULL == m_pIncludeInfo)
	{
		*ppszFileName = NULL;
		*piFileErrLine = iScriptErrLine;
		return; 
	}


	for (i = 0; i < MAX_INCLUDE_FILES; i++)
	{
		//  We don't store # of inc files, rather we stop when 1st name is NULL
		if (m_pIncludeInfo[i].m_pszFileName == NULL)
			break;


		//  In this case error occured in script text before i(th) include file
		if (iScriptErrLine  < m_pIncludeInfo[i].m_iStartLine)
		{
			*ppszFileName = NULL;
			*piFileErrLine = iScriptErrLine - iTotalIncLines;
			return;
		}

		//  In this case error occured in i(th) include file
		if (iScriptErrLine < (m_pIncludeInfo[i].m_iStartLine + m_pIncludeInfo[i].m_cLines))
		{
			*ppszFileName = m_pIncludeInfo[i].m_pszFileName;	   // calling fnc shouldn't free this!
			*piFileErrLine = iScriptErrLine - m_pIncludeInfo[i].m_iStartLine + 1;

			return;
		}

		iTotalIncLines += m_pIncludeInfo[i].m_cLines - 1;
	}

	// If we broke out of the loop without finding a inc file, then the 
	// error occured after the last include file was included.  Subtract
	// off total # of lines include files took up.
	
	*piFileErrLine = iScriptErrLine - iTotalIncLines;

	return;
}

//  Used to get the line number that a statement occured on.

int GetLineNumber(PSTR pszStart, PSTR pszEnd)
{
	int nLines = 1;
	PSTR pszTrav = pszStart;

	while (pszTrav < pszEnd)
	{
		if ( *pszTrav == '\r')
			nLines++;
		pszTrav++;
	}
	return nLines;
}


