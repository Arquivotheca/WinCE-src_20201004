//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: parser.cpp
Abstract: Converts an ASP file into the scripting language
--*/

#include "aspmain.h"

// Regular text needs quotes, ie Response.Write("Html!!")

#define WRITE_HTML_BEGIN    " Response.Write(\""    

// In VB the ":" acts like the ";" in C or Jscript, use it to put multiple stamenents on 1 line.
#define WRITE_HTML_END_VB    "\"):    "		
#define WRITE_HTML_END_JS    "\");    "


// In an expression to be evaluated, don't use quotes around Response.Write
// So <%= x %>  becomes Response.Write(x)
#define WRITE_EXP_BEGIN    " Response.Write("    
#define WRITE_EXP_END_VB    "):  "			
#define WRITE_EXP_END_JS    ");  "
#define MAX_VB_STRING		1000	// Max is documented at 1022

#define VB_NEWLINE			"\"&chr(13)&chr(10)&\""
#define JS_NEWLINE			"\\r\\n"
#define VB_DOUBLE_QUOTE_REPLACEMENT   "\"&chr(34)&\""

// These are helper functions only used within this file
BOOL SkipWhiteWriteReturns(PSTR& pszRead, PSTR& pszWrite);
void SkipToEndWriteReturns(PSTR& pszRead, PSTR& pszWrite, PSTR pszEnd);
int CalculateBufferSize(PSTR pszScript, PSTR pszScriptEnd, SCRIPT_LANG lang);


//  This is the "main" parsing engine.  Once all the preproc directives have
//  been handled an all the include files read in, this fcn converts the blob
//  of ASP text into either VBScript or JScript.

BOOL CASPState::ConvertToScript()
{
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;

	PSTR pszRead = m_pszFileData;  	// walks across ASP data, reads
	PSTR pszWrite = NULL; 		// walks across Script data, writes

	PSTR pszSubEnd = NULL;		// substring ending
	PSTR pszFileDataEnd = m_pszFileData + m_cbFileData;
	int iErrLine = -1;
	int nNewBytes;			// # if extra characters to add for scri

	if (-1 == (nNewBytes = CalculateBufferSize(m_pszFileData, pszFileDataEnd, m_scriptLang))) 
	{
		m_aspErr = IDS_E_NO_SCRIPT_TERMINATOR;
		myleave(21);
	}		

	pszWrite = m_pszScriptData = MyRgAllocNZ(CHAR, m_cbFileData + nNewBytes+1);
	if (NULL == m_pszScriptData)
	{
		m_aspErr = IDS_E_NOMEM;
		myleave(20);
	}
	m_pszScriptData[ m_cbFileData + nNewBytes - 1] = '\0';

	// copy newlines from pszRead to pszWrite, skipping whitespace, stopping at next non-WS char
	// NOTE: pszRead and pszWrite are updated 
	SkipWhiteWriteReturns(pszRead, pszWrite);

	// Main loop, copies data from m_pszFileData to m_pszScriptData
	while (pszRead < pszFileDataEnd)
	{
		// Is this a <% %> block?
		if(0 == memcmp(pszRead, BEGIN_COMMAND, CONSTSIZEOF(BEGIN_COMMAND)))
		{
			// in script
			pszRead += 2; 	// skip <%

			// NOTE: This does NOT correctly handle cases of %> embedded in
			// quoted strings, HTML comments etc etc.
			pszSubEnd = MyStrStr(pszRead, END_COMMAND);
			DEBUGCHK(pszSubEnd);

			// This fn updates pszRead, pszWrite
			CopyScriptText(pszRead, pszWrite, pszSubEnd);

			pszRead += 2;   // skip past trailing %>

			// if there are no line breaks skipped, we might have the case where
			// there are multiple staments on one line (<% ... %>  <% ... %> ...)
			// We need the ; or : to make them seperate statements, but keep
			// them on the same line for error reporting.

			// copy newlines from pszRead to pszWrite, skipping whitespace, stopping at next non-WS char
			// NOTE: pszRead and pszWrite are updated 
			if (FALSE == SkipWhiteWriteReturns(pszRead,pszWrite))
				*pszWrite++ = ((m_scriptLang == VBSCRIPT) ? ':' : ';');
		}
		else
		{
			// We're in a plain text block, put into Response.Write("...")
			if(NULL == (pszSubEnd = MyStrStr(pszRead, BEGIN_COMMAND)))
				pszSubEnd = pszFileDataEnd;

			// This fn updates pszRead & pszWrite
			CopyPlainText(pszRead, pszWrite,pszSubEnd);
		}
	} // while (pszRead < pszEndFileData)

	// null terminate the whole thing
	*pszWrite = '\0';

	// IActiveInterfaces are in UNICODE
	m_cbScriptData = pszWrite-m_pszScriptData;

	DEBUGMSG(ZONE_INIT,(L"ASP: Script text is:\r\n%a\r\nEnd script\r\n",m_pszScriptData));
	
	if ( FAILED(SysAllocStringFromSz(m_pszScriptData, m_cbScriptData, 
									 &m_bstrScriptData,m_lCodePage)))
	{
		m_aspErr = IDS_E_NOMEM;
		myleave(22);
	}

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: ConvertToScript failed, err = %d\r\n",err));

	if (FALSE == ret)
		ServerError(NULL,iErrLine);
	
	return ret;
}


//  This function copies HTML text from the script and puts it into a 
//  Response.Write(" ... ") block.  Cases it handles are embedded returns 
//  and embedded quotes, both of which would mess up the scripting engine.

//  NOTE:  VB cannot handle static strings > 1022 bytes, so we break up really
//  long blocks of text into multiple Response.Write("...") calls.

//  We do NOT do this on Jscript because Jscript can handle arbitrary sized strings
//  and because having to break up long strings in VB mutilates performance
//  in IActiveScript run time.  To download a 300 K text file with 1 script command
//  (to make it use the script engine) takes 9 seconds with Jscript and 49 seconds
//  with VBscript.
void CASPState::CopyPlainText(PSTR& pszRead, PSTR& pszWrite, PSTR pszSubEnd)
{
	int nLineFeeds  =0;

	
	while(pszRead < pszSubEnd)
	{
		PSTR pszWriteOrg = pszWrite;
		
		// write out first Response.Write()
		strcpy(pszWrite, WRITE_HTML_BEGIN);
		pszWrite += CONSTSIZEOF(WRITE_HTML_BEGIN);
		
		// copy all chars between pszRead and pszSubEnd2
		for( ;pszRead < pszSubEnd; pszRead++)
		{
			// do embedded quotes for Request.Write("...") 
			if(*pszRead == '"')
			{	
				if (VBSCRIPT == m_scriptLang)		// VB uses ''
				{
					// replace "Foo dblQuotes Foo" ==> "Foo "&chr(34)&" Foo"
					strcpy(pszWrite,VB_DOUBLE_QUOTE_REPLACEMENT);
					pszWrite += CONSTSIZEOF(VB_DOUBLE_QUOTE_REPLACEMENT);
				}
				else
				{
					// replace " with \"
					pszWrite[0] = '\\';
					pszWrite[1] = '\"';
					pszWrite += 2;
				}

			}
			else if ( *pszRead == '\\' && m_scriptLang == JSCRIPT)
			{
				pszWrite[0] = pszWrite[1] = '\\';
				pszWrite += 2;
			}
			else if ( *pszRead == '\n' )
			{
				// do embedded \r's and \n's 
				strcpy(pszWrite, ((VBSCRIPT == m_scriptLang) ? VB_NEWLINE : JS_NEWLINE));
				pszWrite += ((VBSCRIPT == m_scriptLang) ? CONSTSIZEOF(VB_NEWLINE) : CONSTSIZEOF(JS_NEWLINE));
				nLineFeeds++;
			}
			else if ( *pszRead == '\r')
			{
				/* do nothing*/;
			}
			else
			{
				// if not special case, just copy char
				*pszWrite++ = *pszRead;
			}

			if((VBSCRIPT == m_scriptLang) && (pszWrite >= pszWriteOrg+MAX_VB_STRING)) {
				pszRead++;
				break;
			}
		}

		// write out stmt end
		if (VBSCRIPT == m_scriptLang)
		{
			strcpy(pszWrite, WRITE_HTML_END_VB);
			pszWrite += CONSTSIZEOF(WRITE_HTML_END_VB);
		}
		else
		{
			strcpy(pszWrite, WRITE_HTML_END_JS);
			pszWrite += CONSTSIZEOF(WRITE_HTML_END_JS);
		}
	}

	// we put line feeds in script data to make the line ASP says
	// an error occurred on match more closely with the real thing
	for( ;nLineFeeds > 0; nLineFeeds--)
	{
		*pszWrite++ = '\r';
		*pszWrite++ = '\n';
	}
	return;
}


//  This fcn handles copying data between <% and %>, the script commands.
void CASPState::CopyScriptText(PSTR& pszRead, PSTR& pszWrite, PSTR pszSubEnd)
{
	// Preproc Directive was handled, do NOT send it to parser.  However,
	// do copy any \r\n's over

	SkipWhiteWriteReturns(pszRead,pszWrite);
	if ( *pszRead == '@')
	{	
		// updates pszRead & pszWrite
		SkipToEndWriteReturns(pszRead, pszWrite, pszSubEnd);
	}
	// put var in Respose.Write(...)  (no quotes)
	else if ( *pszRead == '=')
	{
		strcpy(pszWrite, WRITE_EXP_BEGIN);
		pszWrite += CONSTSIZEOF(WRITE_EXP_BEGIN);

		pszRead++;		// skip '='
		memcpy(pszWrite, pszRead, pszSubEnd-pszRead);
		pszWrite += (pszSubEnd-pszRead);

		if (VBSCRIPT == m_scriptLang)
		{
			strcpy(pszWrite, WRITE_EXP_END_VB);
			pszWrite += CONSTSIZEOF(WRITE_EXP_END_VB);
		}
		else
		{
			strcpy(pszWrite, WRITE_EXP_END_JS);
			pszWrite += CONSTSIZEOF(WRITE_EXP_END_JS);
		}
	}
	else 	// normal case, just script data
	{
		memcpy(pszWrite, pszRead, pszSubEnd-pszRead);
		pszWrite += (pszSubEnd-pszRead);
	}
	pszRead = pszSubEnd;
}

//  This function skips past white space, except for "\r"  It writes 
//  them to the write pointer.  We need this to keep the error lines in the
//  script file and the script we parse for ASP consistent.

//  returns TRUE if it did write returns, FALSE if none were written

BOOL SkipWhiteWriteReturns(PSTR& pszRead, PSTR& pszWrite)
{
	BOOL fRet = FALSE;
	for( ; isspace(*pszRead); pszRead++)
	{
		if(*pszRead=='\r' || *pszRead== '\n')
		{
			fRet = TRUE;
			*pszWrite++ = *pszRead;
		}
	}
	return fRet;
}


//  Runs from pszRead to pszEnd, writing any new lines along the way.
void SkipToEndWriteReturns(PSTR& pszRead, PSTR& pszWrite, PSTR pszEnd)
{
	for( ;pszRead < pszEnd; pszRead++)
	{
		if(*pszRead=='\r' || *pszRead== '\n')
			*pszWrite++ = *pszRead;
	}
	DEBUGCHK(pszRead==pszEnd);
}

// CalculateBufferSize

// When converting from an ASP Page to Jscript or VBscript, we almost always
// need a bigger buffer than the current buffer we're using.

// CalculateBufferSize returns the number of extra characters that will be needed.

// Returns -1 in the error case, which can only occur if there's an unterminated
// scritp command, ie a <% ... but no closing %>.
int CalculateBufferSize(PSTR pszScript, PSTR pszScriptEnd, SCRIPT_LANG scriptLang)
{
	// For each \r\n in HTML, we replace with language specific newline, and we put
	// a new line at the close of the Response.Write("..") to keep the error line #'s matched up

	int nNewLineAdd = ((scriptLang==VBSCRIPT) ? CONSTSIZEOF(VB_NEWLINE) : CONSTSIZEOF(JS_NEWLINE)) + CONSTSIZEOF("\r\n");
	int nTotalExtra = 0;
	int nDoubleQuote = ((scriptLang==VBSCRIPT) ? CONSTSIZEOF(VB_DOUBLE_QUOTE_REPLACEMENT) : 1);
	
#define EXP_WRITE_ADD 	 (CONSTSIZEOF(WRITE_EXP_BEGIN) + CONSTSIZEOF(WRITE_EXP_END_JS))
#define STRING_WRITE_ADD (CONSTSIZEOF(WRITE_HTML_BEGIN) + CONSTSIZEOF(WRITE_HTML_END_JS))

	PSTR pszTrav = pszScript;
	PSTR pszSubEnd  = NULL;

	svsutil_SkipWhiteSpace(pszTrav);
	while(pszTrav < pszScriptEnd)
	{
		// see if the next text block is a command
		if(0 == memcmp(pszTrav,BEGIN_COMMAND,CONSTSIZEOF(BEGIN_COMMAND)))
		{	
			// 
			// NOTE: This does not correctly handle cases of %> embedded in
			// quoted strings, HTML comments etc etc.
			if (NULL == (pszSubEnd = MyStrStr(pszTrav, END_COMMAND)))
				return -1;		// no closing "%>"
			
			pszTrav += CONSTSIZEOF(BEGIN_COMMAND);	// skip past "<%"
			svsutil_SkipWhiteSpace(pszTrav);

			// in case where there's a <%= variable %>, we have to wrap a 
			// Response.Write around it, which needs extra space in the buffer
			if ( *pszTrav == '=')
			{
				nTotalExtra += EXP_WRITE_ADD;
			}
			//  In the <% and <%@ case, we don't need any extra space.

			pszTrav = pszSubEnd + CONSTSIZEOF(END_COMMAND);
			svsutil_SkipWhiteSpace(pszTrav);
		}
		else
		{
			// We're scanning a non-<% %> block now
			// 
			// NOTE: This does not correctly handle cases of <% embedded in
			// quoted strings, HTML comments etc etc.
			if (NULL == (pszSubEnd = MyStrStr(pszTrav, BEGIN_COMMAND)))
				pszSubEnd = pszScriptEnd;   

			// In VB, we have to break up strings > 1K into multiple Request.Write( ...)
			// calls or the script engine will fail.  		
			if (scriptLang == VBSCRIPT)
				nTotalExtra += STRING_WRITE_ADD * (1 + (pszSubEnd - pszTrav) / MAX_VB_STRING);
			else
				nTotalExtra += STRING_WRITE_ADD;			// for Response.Write("...")

			 
			for( ; pszTrav < pszSubEnd; pszTrav++)
			{
				if (*pszTrav == '"')
					nTotalExtra += nDoubleQuote;		// A " is replaced with a '' or \"

				if (*pszTrav=='\n')
					nTotalExtra += nNewLineAdd;

				// A \ is replaced with \\ in jscript, \ is it's escape char, like C
				if (*pszTrav=='\\' && scriptLang == JSCRIPT)
					nTotalExtra++;
			}
		}
	}
	DEBUGMSG(ZONE_PARSER,(L"ASP: CalculateBufferSize   will add %d bytes to script\r\n",nTotalExtra));
	return nTotalExtra;	
}			
