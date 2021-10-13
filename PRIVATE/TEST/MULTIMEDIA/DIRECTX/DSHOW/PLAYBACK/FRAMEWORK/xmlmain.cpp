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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       X M L M A I N . C P P
//
//  Contents:   XML parser.
//
//----------------------------------------------------------------------------

#include "xmlmain.h"

#include <stdlib.h> //Needed for atoi.

PCSTR pszOrigBuf;

// XML escape sequences for special characters.
#define NUM_ESCAPES 5
PSTR  rgEscapeStr[NUM_ESCAPES] = { "gt", "lt", "amp", "apos", "quot"};
BYTE  rgEscapeVal[NUM_ESCAPES] = { '>',  '<',  '&',   '\'',    '"'  };

const char cszCDATA[] = "<![ CDATA[ ";
#define CONSTSIZEOF(x)  (sizeof(x)-1)


//////////
// MySzDupN
//      Allocates a new or reallocates an existing pszPrev, then
//      copies iLen bytes from pszIn, converting ampersand escapes if needed.
//
// PARAMETERS
//      pszIn:           String to be duplicated.
//      iLen:            Length of pszIn.
//      fConvertEscapes: Set to TRUE if you want to map special characters;
//                       Set to FALSE if you do not want mapping.
//      pszPrev:         If not NULL, pszIn is appended to pszPrev,
//                       If NULL, a new string is allocated.
//
// RETURNS
//      PSTR            A copy of pszIn or (pszPrev + pszIn).
//
PSTR MySzDupN(PCSTR pszIn, 
              int iLen, 
              BOOL fConvertEscapes = FALSE, 
              PCSTR pszPrev = NULL) 
{ 
    PSTR pszRet, pszOut;
    ASSERT(pszIn && iLen);
    
	if (iLen <= 0)
		return NULL;

    // If pszPrev is not NULL, extend the string.
    if(pszPrev)
    {
        int iPrevLen = strlen(pszPrev);
        pszRet = (PSTR)LocalReAlloc((PVOID)pszPrev, 
                                    iPrevLen + iLen + 1,  
                                    LMEM_MOVEABLE|LMEM_ZEROINIT);
        pszOut = pszRet + iPrevLen;
    }
    else
        pszRet = pszOut = MySzAllocA(iLen);
    
    // Check if out of memory.
    if(!pszRet)
        return NULL;
    
    // If escapes are not being converted, copy the string and return.
    if(!fConvertEscapes)
    {
        memcpy(pszOut, pszIn, iLen); 
        pszOut[iLen] = 0;
        return pszRet; 
    }
    
    // Otherwise, scan for and convert special characters.
    DWORD chReplace;
    PSTR pszEnd;
    int i;
    int j;
    int k;
    
    // Convert &amp, lt, gt, apos, quot, &#nn, &#xnn.
    for(i = 0, j = 0; i < iLen && j < iLen; )
    {
        // If & is closer than 3 chars from the end, it is not a legal escape--ignore.
        if(pszIn[i] != '&' || (iLen - i) < 3)
        {
            pszOut[j++] = pszIn[i++];
            continue;
        }
        
        // Found the '&' start of an escape. Now find the end.
        pszEnd = (PSTR)strchr(pszIn + i + 1, ';');
        if(!pszEnd)
            goto error;
        
        // Find  the replacement character.
        chReplace = 0;
        if(pszIn[i + 1] == '#')
        {
            // Found the & start of a numeric escape.
            if(pszIn[i + 2] == 'x')
            {
                if (sscanf(pszIn + i + 3, "%x", &chReplace) != 1)
                    goto error;
            }
            else
                chReplace = atoi(pszIn + i + 2);
        }
        else
        {
            // Find the matching named escape.
            for(k = 0; k < NUM_ESCAPES; k++)
            {
                if(0 == _strnicmp(pszIn + i + 1, 
                                  rgEscapeStr[k], 
                                  (pszEnd - (pszIn + i + 1))))
                {
                    chReplace = rgEscapeVal[k];
                    break;
                }
            }
        }
        
        // Was it a good replacement?
        if(chReplace)
        {
            DEBUGMSG(ZONE_XMLPARSE, (TEXT("Replacing Escape sequence(%.*s) with (%c)\r\n"), ((pszEnd+1)-(pszIn+i)), pszIn+i, chReplace));
            pszOut[j++]=(CHAR)chReplace;
            i = (pszEnd - pszIn) + 1;
            continue;
        }
        
error:
        // A badly formed or unrecognized escape was found, so pass it through unchanged.
        DEBUGMSG(ZONE_XMLERROR, (TEXT("Illegal Escape sequence(%.4s)\r\n"), pszIn+i));
        ASSERT(FALSE);
        pszOut[j++] = pszIn[i++];
    }
    pszOut[iLen] = 0;
    return pszRet;
} // end 


//////////
// SkipWS
//      Skips whitespace and modifies the input pointer in-place. 
//      If there is no whitespace, the pointer is left unchanged.
//
// PARAMETERS
//      pszCurr:    On INPUT, pointer to the current character.  
//                  On OUTPUT, points to the next non-whitespace character
//
// RETURNS
//      nothing
//
VOID SkipWS(PSTR& pszCurr)
{
    while(*pszCurr == ' '  || 
          *pszCurr == '\t' || 
          *pszCurr == '\r' || 
          *pszCurr == '\n')
    {
        pszCurr++;
    }
}  // end MySzDupN


//////////
// IsTokenChar
//      Determines whether the character passed is a token character.  
//
// PARAMETERS
//      ch:     character to test.
//
// RETURNS
//      BOOL    TRUE if ch is a token character.
//              FALSE if ch is not a token character.
//
BOOL IsTokenChar(CHAR ch)
{
    // Allow digit, dot, dash, underscore, colon, and "letter" as token characters.
    // Only ASCII letters are allowed.
    return ( (ch >= '0' && ch <= '9') ||
             (ch >= 'A' && ch <= 'Z') || 
             (ch >= 'a' && ch <= 'z') ||
             (ch == '.' || ch == '-' || ch == '_' || ch == ':') );
} // end IsTokenChar


//////////
// GetToken
//      Obtains a token or quoted attribute value.
//
// PARAMETERS
//      pszCurr:  On input, pointer to the location to start parsing from.
//                On output, pointer to the remaining unparsed data.
//      pszToken: On output, pointer to the parsed token or attribute value 
//                within the input stream (any quotes are stripped).
//
// RETURNS
//      int       The length of the parsed token.
//
int GetToken(/*[in-out]*/PSTR& pszCurr, /*[out]*/PSTR& pszToken)
{
    SkipWS(pszCurr);
    
    // Search for quoted strings and return it in pszToken.
    if(*pszCurr == '"' || *pszCurr == '\'')
    {
        CHAR chMatch = *pszCurr++;
        pszToken = pszCurr;
        
        while(*pszCurr && (*pszCurr != chMatch))
        {
            pszCurr++;
        }

        if(*pszCurr && (pszCurr > pszToken))
        {
            int iRet = (pszCurr - pszToken);
            pszCurr++;
            SkipWS(pszCurr);
            DEBUGMSG(ZONE_XMLPARSE, (TEXT("GetToken returned (%.*s) at %d\r\n"), iRet, pszToken, (pszCurr-pszOrigBuf)));
            return iRet;
        }
    }  // Otherwise, return the token string in pszToken.
    else if(IsTokenChar(*pszCurr))
    {
        pszToken = pszCurr;
        while(IsTokenChar(*pszCurr))
        {
            pszCurr++;
        }
        
        if(*pszCurr && (pszCurr > pszToken))
        {
            int iRet = (pszCurr - pszToken);
            SkipWS(pszCurr);
            DEBUGMSG(ZONE_XMLPARSE, (TEXT("GetToken returned (%.*s) at %d\r\n"), iRet, pszToken, (pszCurr-pszOrigBuf)));
            return iRet;
        }
    }
    // Failed to get a token.
    if(*pszCurr != '>' && *pszCurr != '/') 
    { 
        DEBUGMSG(ZONE_XMLERROR, (TEXT("GetToken failed at %d (%c)\r\n"), (pszCurr-pszOrigBuf), *pszCurr)); 
    }

    return 0;
} // end GetToken


//////////
// GetElemValue
//      Obtains the body of an XML element.
//
// PARAMETERS
//      pszCurr:  On input, pointer to the location to start parsing from.
//                On output, pointer to the remaining unparsed data.
//      pszToken: On output, pointer to the parsed value within the input stream.
//
// RETURNS
//      int       The length of the parsed value fragment.
//                If the end of value is reached, -1 is returned.
//
// NOTE: Due to CDATA, it may take several iterative calls to GetElemValue to 
// assemble a complete element value.  The caller should call this function 
// repeatedly, assembling the returned value fragments, until GetElem Value 
// returns -1.
//
int GetElemValue(/*[in-out]*/PSTR& pszCurr, /*[out]*/PSTR& pszValue)
{
    // Check if we're done or at a CDATA.
    if(*pszCurr == '<')
    {
        // If not CDATA, then we're at the end of the value. Return -1.
        if(strncmp(pszCurr, cszCDATA, CONSTSIZEOF(cszCDATA)) != 0)
        {
            DEBUGMSG(ZONE_XMLPARSE, (TEXT("GetElemValue endvalue at %d\r\n"), (pszCurr-pszOrigBuf)));
            return -1;
        }
        
        // We have a CDATA. Find its end.
        pszValue = pszCurr + CONSTSIZEOF(cszCDATA);
        PSTR pszTemp = strstr(pszValue, "]]>");
        if(!pszTemp)
        { 
            DEBUGMSG(ZONE_XMLERROR, (TEXT("GetElemValue-CDATA failed at %d (%c)\r\n"), (pszCurr-pszOrigBuf), *pszCurr)); 
            return -1;
        }
        int iRet = pszTemp - pszValue;
        pszCurr = pszTemp + 3;
        DEBUGMSG(ZONE_XMLPARSE, (TEXT("GetElemValue-CDATA returned (%.*s) at %d\r\n"), iRet, pszValue, (pszCurr-pszOrigBuf)));
        return iRet;
    }
    
    // Non-CDATA case.
    pszValue = pszCurr;
    while(*pszCurr && (*pszCurr != '<'))
        pszCurr++;
    if(*pszCurr && (pszCurr > pszValue))
    {
        DEBUGMSG(ZONE_XMLPARSE, (TEXT("GetElemValue returned (%.*s) at %d\r\n"), (pszCurr - pszValue), pszValue, (pszCurr-pszOrigBuf)));
        return (pszCurr - pszValue);
    }
    if(*pszCurr != '<') 
    { 
        DEBUGMSG(ZONE_XMLERROR, (TEXT("GetElemValue failed at %d (%c)\r\n"), (pszCurr-pszOrigBuf), *pszCurr)); 
        return -1;
    }
    return 0;
} // end GetElemValue


//////////
// GetNameSpace
//       Separates XML attribute/element names into a namespace qualifier 
//       and the actual name.
//
// PARAMETERS
//      pszName:    On input, points the XML attribute/element.  May or may not have a namespace qualifier.
//                  On output, points to the actual name.
//      iLenName:   On input, is the length of pszName.  
//                  On output, is changed to the new length of pszName.
//      pszPrefix:  On output, points to the namespace qualifier, if one exists.
//      iLenPrefix: On output, points to the new length of pszPrefix.
//
// RETURNS
//      BOOL        TRUE if there is a namespace qualifer.
//                  FALSE if there is not a namespace qualifier.
//
BOOL GetNameSpace(LPSTR &pszName, int &iLenName, LPSTR &pszPrefix, int &iLenPrefix)
{
    int i;
    // Check for a namespace prefix.
    for(i = 0; i < iLenName && pszName[i] != ':'; i++);
    
    // If there is a prefix, split it out.
    if (i < iLenName)
    {
        iLenPrefix = i;
        pszPrefix = pszName;
        pszName = pszName + i + 1;
        iLenName = iLenName - i - 1;
        return TRUE;
    }
    
    pszPrefix = NULL;
    iLenPrefix = 0;
    return FALSE;
} // end GetNameSpace


//////////
// ParseAttrib
//      Parse for an attribute declaration in a string.
//
// PARAMETERS
//      pszCurr:    The string to parse.
//
// RETURNS
//      CAttrib*    A newly allocated CAttrib.
//                  NULL if no attribute was found.
//
CAttrib* ParseAttrib(PSTR& pszCurr)
{
    PSTR pszName = 0;
    PSTR pszPrefixCopy = 0;
    PSTR pszValue = 0;
    int  iLenName = 0;
    int  iLenValue = 0;
    int  iLenPrefix = 0;
    
    // GetToken eliminates leading & trailing whitespace.
    if(!(iLenName = GetToken(pszCurr, pszName)))
        return NULL;
    
    // Get the name and namespace.
    GetNameSpace(pszName, iLenName, pszPrefixCopy, iLenPrefix);
    
    // If there is a namespace qualifier, copy it.
    if (pszPrefixCopy)
        pszPrefixCopy = MySzDupN(pszPrefixCopy, iLenPrefix);
    
    // Verify that the next character is "=".
    if(*pszCurr == '=')
    {
        pszCurr++;
        if(iLenValue = GetToken(pszCurr, pszValue))
        {
            // Recurse to get siblings, and return with a complete linked list of attributes.
            DEBUGMSG(ZONE_XMLPARSE, 
                    (TEXT("Succeeded ParseAttrib: Name=%.*s Value=(%.*s) at %d\r\n"), 
                    iLenName, pszName, iLenValue, pszValue, (pszCurr-pszOrigBuf)));

            return new CAttrib(MySzDupN(pszName, iLenName), 
                                pszPrefixCopy, 
                                MySzDupN(pszValue, iLenValue, TRUE), 
                                ParseAttrib(pszCurr));
        }
    }
    
    MyFree(pszPrefixCopy);
    
    DEBUGMSG(ZONE_XMLERROR, (TEXT("Failed ParseAttrib: Name=%.*s Value=(%.*s) at %d (%c)\r\n"), iLenName, pszName, iLenValue, pszValue, (pszCurr-pszOrigBuf), *pszCurr));
    return NULL;
} // end ParseAttrib


//////////
// ParseElem
//      Parse for an element in a string.  This function recursively 
//      calls itself to process nested elements.
//
// PARAMETERS
//      pszCurr:    The string to parse.
//
// RETURNS
//      CElem*      A newly allocated CElem.
//                  Returns NULL if the parse failed.
//
CElem* ParseElem(PSTR& pszCurr)
{
    BOOL  fTerminal = FALSE;
    PSTR  pszOrig = pszCurr;
    PSTR  pszName = 0;
    PSTR  pszPrefixCopy = 0;
    int   iLenName = 0;
    int   iLenPrefix = 0;
    CAttrib* pAttribs = NULL;
    PSTR pszValueCopy = 0;
    CElem* pFirstChild = 0;
    
restart:
    SkipWS(pszCurr);
    if(*pszCurr == 0)   // We are at the end of input.
        return NULL;
    if(*pszCurr != '<')
        goto error;
    pszCurr++;
    SkipWS(pszCurr);
    if(*pszCurr == '/')
    {
        // Found the end of some prior element. Reset pointers and regress.
        pszCurr = pszOrig;
        return NULL;
    }
    // Check for and eliminate comments.
    if(*pszCurr == '!')
    {
        if(*(pszCurr + 1) == '-' && *(pszCurr + 2) == '-')
        {
            // Found a comment. Skip it.
            for(int i = 2; pszCurr[i + 1]; i++)
            {
                if(pszCurr[i]     == '-'   && 
                   pszCurr[i + 1] == '-'   && 
                   pszCurr[i + 2] == '>')
                {
                    DEBUGMSG(ZONE_XMLPARSE, (TEXT("Eliminate comment(%.*s)\r\n"), i+4, pszCurr-1));
                    pszCurr += (i + 3);
                    // Eliminate the comment and restart parsing for the element.
                    goto restart;
                }
            }
        }
        DEBUGMSG(ZONE_XMLERROR, (TEXT("ERROR Parsing (%.6s)\r\n"), pszCurr-1));
        goto error;
    }
    // Check for and eliminate processing instructions
    if(*pszCurr == '?')
    {
        for(int i = 1; pszCurr[i]; i++)
        {
            if(pszCurr[i] == '?' && pszCurr[i + 1] == '>')
            {
                DEBUGMSG(ZONE_XMLPARSE, (TEXT("Eliminate processing instruction(%.*s)\r\n"), i+3, pszCurr-1));
                pszCurr += (i + 2);
                // Eliminate processing instruction and restart parsing for element
                goto restart;
            }
        }
        DEBUGMSG(ZONE_XMLERROR, (TEXT("ERROR Parsing (%.6s)\r\n"), pszCurr-1));
        goto error;
    }
    // GetToken eliminates leading and trailing whitespace.
    if(!(iLenName = GetToken(pszCurr, pszName)))
        goto error;
    
    // Determine if there is a namespace.
    GetNameSpace(pszName, iLenName, pszPrefixCopy, iLenPrefix);
    
    if (pszPrefixCopy)
        pszPrefixCopy = MySzDupN(pszPrefixCopy, iLenPrefix);
    
    // Parse the attributes. This will eliminate leading and trailing whitespace.
    pAttribs = ParseAttrib(pszCurr);
    
    if(*pszCurr == '/')
    {
        fTerminal = TRUE;
        pszCurr++;
    }
    
    // Check for the the closing '>'.
    if(*pszCurr != '>')
        goto error;
    pszCurr++;
    
    // If this is a terminal element (<ELEMENT/>), then there are no children or element values.
    if(fTerminal)
    {
        // Recurse for siblings.
        DEBUGMSG(ZONE_XMLPARSE, (TEXT("Succeeded ParseElem(empty): Name=%.*s at %d\r\n"), iLenName, pszName, (pszCurr-pszOrigBuf)));
        return new CElem(MySzDupN(pszName, iLenName), pszPrefixCopy, NULL, pAttribs, NULL, ParseElem(pszCurr));
    }
    
    // We have a nonterminal element. Iterate through (data | element). 
    for(;;)
    {
        PSTR  pszNewValue;
        int   iLenNewValue;
        
        // If we find a value, appended it to the previous value segment, if any.
        while(-1 != (iLenNewValue = GetElemValue(pszCurr, pszNewValue)))
        {
            if(iLenNewValue)
            {
                DEBUGMSG(ZONE_XMLPARSE, (TEXT("Get Elem value segment(%.*s)\r\n"), iLenNewValue, pszNewValue));
                pszValueCopy = MySzDupN(pszNewValue, iLenNewValue, TRUE, pszValueCopy);
            }
            else 
            {
                DEBUGMSG(1, (TEXT("Get Elem empty value segment\r\n"))); 
            }
        }
        
        // Search for another child.
        CElem* pNewChild = ParseElem(pszCurr);
        if(pNewChild)
        {
            // Append found child to existing chain of children.
            CElem** ppLastChild;
            for(ppLastChild = &pFirstChild; 
                *ppLastChild; 
                ppLastChild = &((*ppLastChild)->m_pNextSibling))
                ;
            ASSERT(ppLastChild && !(*ppLastChild));
            *ppLastChild = pNewChild;
            DEBUGMSG(ZONE_XMLPARSE, (TEXT("Get child elem(%x, %s) First child=%x\r\n"), pNewChild, pNewChild->m_pszName, pFirstChild));
        }
        // Continue while still finding value segmenets or children.
        if(iLenNewValue == (-1) && !pNewChild)
            break;
    }
    
    // Obtain closing token.
    int iLenName2;
    int iLenPrefix2;
    PSTR pszName2;
    PSTR pszPrefix2;
    
    pszPrefix2 = NULL;
    SkipWS(pszCurr);
    if(*pszCurr != '<')
        goto error;
    pszCurr++;
    SkipWS(pszCurr);
    if(*pszCurr != '/')
        goto error;
    pszCurr++;
    
    // Obtain name of closing token.
    if(!(iLenName2 = GetToken(pszCurr, pszName2)))
        goto error;
    
    GetNameSpace(pszName2, iLenName2, pszPrefix2, iLenPrefix2);
    
    
    // Check closing token.
    if((iLenName2 != iLenName) 
        || (0 != _strnicmp(pszName, pszName2, iLenName))
		|| (pszPrefix2 && !pszPrefixCopy)
        || (pszPrefix2 && pszPrefixCopy && (iLenPrefix != iLenPrefix2 || 0 != _strnicmp(pszPrefixCopy, pszPrefix2, iLenPrefix))))
    {
        DEBUGMSG(ZONE_XMLERROR, (TEXT("element %.*s not closed at %.*s %d \r\n"), iLenName, pszName, iLenName2, pszName2, (pszCurr-pszOrigBuf)));
        goto error;
    }
    
    // Check for and eliminate the final '>'.
    if(*pszCurr != '>')
        goto error;
    pszCurr++;
    
    // Recurse for siblings, if any.
    DEBUGMSG(ZONE_XMLPARSE, (TEXT("Succeeded ParseElem: Name=%.*s Value=(%s) at %d\r\n"), iLenName, pszName, pszValueCopy, (pszCurr-pszOrigBuf)));
    return new CElem(MySzDupN(pszName, iLenName), pszPrefixCopy, pszValueCopy, pAttribs, pFirstChild, ParseElem(pszCurr));
    
error:
    MyFree(pszPrefixCopy);
    MyFree(pszValueCopy);
    
    DEBUGMSG(ZONE_XMLERROR, (TEXT("Failed ParseElem: Name=%.*s Value=(%s) at %d (%c)\r\n"), iLenName, pszName, pszValueCopy, (pszCurr-pszOrigBuf), *pszCurr));
    return NULL;
} // end ParseElem




