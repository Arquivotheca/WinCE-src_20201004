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

    parser.c

Abstract:

    This module encode and decode ssdp notify and search messages.



    Ting Cai (tingcai) creation-date-07-25-99

--*/

/* Note:

  Use midl memory routines for SSDP_REQUEST

*/

#include <ssdppch.h>
#pragma hdrstop

#include "ipexport.h"
#include "ssdpparserp.h"

#define HTTP_VERSION_STR "HTTP/1.1"

#define END_HEADERS_SIZE 3
const char * szEndOfHeaders = "\n\r\n";

#define HEADER_END_SIZE 4
const char * szHeaderEnd = "\r\n\r\n"; 

BOOL IsStrDigits(LPSTR pszStr);
VOID strtrim(CHAR ** pszStr);

static const CHAR c_szMaxAge[] = "max-age";
static const DWORD c_cchMaxAge = celems(c_szMaxAge) - 1;
static const CHAR c_szExtHeader[] = "EXT:\r\n";

extern CHAR g_pszExtensionURI[500];
extern CHAR g_pszHeaderPrefix[10];
extern CHAR g_pszNlsHeader[50];

// Keep in sync with SSDP_METHOD in ssdp.idl
CHAR *SsdpMethodStr[] =
{
    "NOTIFY",
    "M-SEARCH",
    "SUBSCRIBE",
    "UNSUBSCRIBE",
    "INVALID",
};


// Keep in sync with SSDP_HEADER in ssdp.idl
CHAR *SsdpHeaderStr[] =
{
    "Host",
    "NT",
    "NTS",
    "ST",
    "Man",
    "MX",
    "Location",
    "AL",
    "USN",
    "Cache-Control",
    "Callback",
    "Timeout",
    "Scope",
    "SID",
    "SEQ",
    "Content-Length",
    "Content-Type",
    "Server",
    "Ext",
    "Opt",
    g_pszNlsHeader
};


BOOL ComposeSsdp(char* pszFirstLine1, char* pszFirstLine2, char* pszFirstLine3, SSDP_REQUEST *Source, SSDP_HEADER* pIncludeHeaders, int nHeaders, CHAR **pszBytes)
{
    INT iLength = 0;
    INT iNumOfBytes = 0;
    CHAR * szBytes;
    INT i;
    SSDP_HEADER Header;

    ////////////////////////////////////
    // Step 1. calculate size of SSDP message
    ////////////////////////////////////
    
    // calculate size of first line
    assert(pszFirstLine1 && pszFirstLine2 && pszFirstLine3);
    
    iLength += strlen(pszFirstLine1) + strlen(SP) +
               strlen(pszFirstLine2) + strlen(SP) +
               strlen(pszFirstLine3) + strlen(CRLF);
    
    // calculate size of headers
    for (i = 0, Header = pIncludeHeaders[0]; i < nHeaders; Header = pIncludeHeaders[++i])
    {
        assert(Header < NUM_OF_HEADERS);
        
        // for NLS header include prefix + dash
        if(SSDP_NLS == Header && g_pszHeaderPrefix[0])
            iLength += strlen(g_pszHeaderPrefix) + strlen(DASH);
            
        // header name + colon + crlf
        iLength += strlen(SsdpHeaderStr[Header]) + strlen(COLON) + strlen(CRLF);
        
        // value for headers other than EXT
        if(SSDP_EXT != Header)
        {
            assert(Source->Headers[Header] != NULL);
            iLength += strlen(Source->Headers[Header]);
        }
    }

    // terminating CRLF
    iLength += strlen(CRLF);

    ////////////////////////////////////
    // Step 2. compose SSDP message
    ////////////////////////////////////
    
    // allocate memory for the message
    if(NULL == (szBytes = (CHAR *) malloc(sizeof(CHAR) * iLength + 1)))
    {
        TraceTag(ttidSsdpParser, "Failed to allocate memory for the ssdp message.");
        return FALSE;
    }

    // write first line
    iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s%s%s%s%s%s", pszFirstLine1, SP, pszFirstLine2, SP, pszFirstLine3, CRLF);

    // write headers
    for (i = 0, Header = pIncludeHeaders[0]; i < nHeaders; Header = pIncludeHeaders[++i])
    {
        assert(Header < NUM_OF_HEADERS);
        
        // prefix + dash for NLS header
        if(SSDP_NLS == Header && g_pszHeaderPrefix[0])
            iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s%s", g_pszHeaderPrefix, DASH);
        
        // header name + colon
        iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s%s", SsdpHeaderStr[Header], COLON);
        
        // value for headers other than EXT
        if(SSDP_EXT != Header)
        {
            assert(Source->Headers[Header] != NULL);
            iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s", Source->Headers[Header]);
        }
            
        // crlf
        iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s", CRLF);
    }

    // write terminating CRLF
    iNumOfBytes += sprintf(szBytes + iNumOfBytes, "%s",CRLF);
    
    Assert(iNumOfBytes <= iLength);

    *pszBytes = szBytes;

    return TRUE;
}


// SSDP response
BOOL ComposeSsdpResponse(SSDP_REQUEST *Source, SSDP_HEADER* pIncludeHeaders, int nHeaders, CHAR **pszBytes)
{
    return ComposeSsdp(HTTP_VERSION_STR, "200", "OK", Source, pIncludeHeaders, nHeaders, pszBytes);
}

// SSDP request (NOTIFY or M-SEARCH)
BOOL ComposeSsdpRequest(SSDP_REQUEST *Source, SSDP_HEADER* pIncludeHeaders, int nHeaders, CHAR **pszBytes)
{
    return ComposeSsdp(SsdpMethodStr[Source->Method], Source->RequestUri, HTTP_VERSION_STR, Source, pIncludeHeaders, nHeaders, pszBytes);
}


// Pre-Conditions: 
// Result->Headers[CONTENT_LENGTH] contains only digits.
// pContent points to the first char "\r\n\r\n".
// Return Value: 
// returns FALSE if there not enough content. 

BOOL ParseContent(const char *pContent, SSDP_REQUEST *Result)
{
    if(!Result->Headers[CONTENT_LENGTH])
    {
        return TRUE;
    }

    long lContentLength = atol(Result->Headers[CONTENT_LENGTH]);
    if (lContentLength == 0)
    {
        // If it can't be conver to a number or it is 0.
        TraceTag(ttidSsdpParser, "Content-Length is 0.");
        return TRUE;
    }
    else
    {
        // better, but may break WinME: if ((long)strlen(pContent) != lContentLength)
        if (((long) strlen(pContent) + 1) < lContentLength)
        {
            TraceTag(ttidSsdpParser, "Not enough bytes for content as specified in "
                     "Content-Length: %d vs %d", strlen(pContent) + 1,
                     lContentLength);
            return FALSE;
        }

        Result->Content = (CHAR*) SsdpAlloc(lContentLength + 1);
        if (Result->Content == NULL)
        {
            // To-Do: ? 
            TraceTag(ttidSsdpParser, "Failed to allocate memory for Content");
            return FALSE;
        }
        else
        {
            strncpy(Result->Content,pContent, lContentLength);
            // NTRAID#NTBUG-166137-2000/08/18-danielwe: Add the NULL terminator
            Result->Content[lContentLength] = 0;
            return TRUE;
        }
    }
}

BOOL VerifySsdpMethod(CHAR *szMethod, SSDP_REQUEST *Result)
{
    INT i;

    for (i = 0; i < NUM_OF_METHODS; i++)
    {
        if (_stricmp(SsdpMethodStr[i],szMethod) == 0)
        {
            Result->Method = (SSDP_METHOD)i;
            break;
        }
    }

    if (i == NUM_OF_METHODS)
    {
        TraceTag(ttidSsdpParser, "Parser could not find method . "
                 "Received %s", szMethod);
        Result->status = HTTP_STATUS_BAD_METHOD;
        return FALSE;
    }
    return TRUE;
}    

CHAR * ParseRequestLine(CHAR * szMessage, SSDP_REQUEST *Result)
{
    CHAR *token;

    //  Get the HTTP method
    token = strtok(szMessage," \t\n");
    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Parser could not locate the seperator, "
                 "space, tab or eol");
        return NULL;
    }

	if (!VerifySsdpMethod(token, Result))
		return NULL;
	
    // Get the Request-URI
    token = strtok(NULL," ");
    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Parser could not find the url in the "
                 "message.");
        return NULL;
    }

    // Ingore the name space parsing for now, get the string after the last '/'.

    Result->RequestUri = (CHAR*) SsdpAlloc(strlen(token) + 1);

    if (Result->RequestUri == NULL)
    {
        TraceTag(ttidSsdpParser, "Parser could not allocate memory for url.");
        return NULL;
    }

    // Record the service.
    strcpy(Result->RequestUri, token);

    // Get the version number
    token = strtok(NULL,"  \t\r");

    // To-Do: Record the version number when necessary.

    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Failed to get the version in the request "
                 "header.");
        FreeSsdpRequest(Result);
        return NULL;
    }

    if (_stricmp(token, "HTTP/1.1") != 0)
    {
        TraceTag(ttidSsdpParser, "The version specified in the request "
                 "message is not HTTP/1.1");
        FreeSsdpRequest(Result);
        return NULL;
    }

    return (token + strlen(token) + 1);

}


// VerifyRequestLine
BOOL VerifyRequestLine(SSDP_REQUEST *Result)
{
    if (0 != _stricmp(Result->RequestUri, "*"))
    {
        TraceTag(ttidSsdpParser, "Requested resource is not * in SSDP request");
        
        return FALSE;
    }
    
    return TRUE;
}


BOOL VerifySsdpHeaders(SSDP_REQUEST *Result)
{
    // M_SEARCH
    if (Result->Method == SSDP_M_SEARCH)
    {
        // MAN
        if(Result->Headers[SSDP_MAN] == NULL || 0 != _stricmp("\"ssdp:discover\"", Result->Headers[SSDP_MAN]))
        {
            TraceTag(ttidSsdpParser, "MAN header must be \"ssdp:discover\" for M-SEARCH");
            return FALSE;
        }
        
        // MX header
        if(Result->Headers[SSDP_MX] == NULL || Result->Headers[SSDP_MX][0] == '\x0')
        {
            TraceTag(ttidSsdpParser, "MX header missing or empty for M-SEARCH");
            return FALSE;
        }
        
        if(Result->Headers[SSDP_MX] == NULL || IsStrDigits(Result->Headers[SSDP_MX]) == FALSE)
        {
            TraceTag(ttidSsdpParser, "MX header should be all digits for M-SEARCH");
            return FALSE;
        }

        // ST header
        if(Result->Headers[SSDP_ST] == NULL || Result->Headers[SSDP_ST][0] == '\x0')
        {
            TraceTag(ttidSsdpParser, "ST header missing or empty for M-SEARCH");
            return FALSE;
        }
    }

    // NOTIFY
    if (Result->Method == SSDP_NOTIFY)
    {
        // NT header
        if (Result->Headers[SSDP_NT] == NULL || Result->Headers[SSDP_NT][0] == '\x0')
        {
            TraceTag(ttidSsdpParser, "NT header missing or empty for a NOTIFY message."); 
            return FALSE;
        }
        
        // USN header
        if (Result->Headers[SSDP_USN] == NULL || Result->Headers[SSDP_USN][0] == '\x0')
        {
            TraceTag(ttidSsdpParser, "USN header missing or empty for ssdp:alive NOTIFY message.");
            return FALSE;
        }

        // ssdp:alive
        if (Result->Headers[SSDP_NTS] != NULL && 0 == _stricmp(Result->Headers[SSDP_NTS], "ssdp:alive"))
        {
            // CACHE-CONTROL header
            if (Result->Headers[SSDP_CACHECONTROL] == NULL || Result->Headers[SSDP_CACHECONTROL][0] == '\x0')
            {
                TraceTag(ttidSsdpParser, "CACHE-CONTROL header missing or empty for ssdp:alive NOTIFY message.");
                return FALSE;
            }
            
            // LOCATION header
            if (Result->Headers[SSDP_LOCATION] == NULL || Result->Headers[SSDP_LOCATION][0] == '\x0')
            {
                TraceTag(ttidSsdpParser, "LOCATION header missing or empty for ssdp:alive NOTIFY message.");
                return FALSE;
            }
            
            // SERVER header
            if (Result->Headers[SSDP_SERVER] == NULL || Result->Headers[SSDP_SERVER][0] == '\x0')
            {
                TraceTag(ttidSsdpParser, "SERVER header missing or empty for ssdp:alive NOTIFY message.");
                return FALSE;
            }
        }
        else
            // ssdp:byebye
            if (Result->Headers[SSDP_NTS] == NULL || 0 != _stricmp(Result->Headers[SSDP_NTS], "ssdp:byebye"))
            {
                TraceTag(ttidSsdpParser, "NTS header must be one of ssdp:alive, ssdp:byebye for a SSDP NOTIFY message.");
                return FALSE;
            }
    }
    
    return TRUE; 
}


// VerifySsdpResponseHeaders
BOOL VerifySsdpResponseHeaders(SSDP_REQUEST *Result)
{
    // CACHE-CONTROL header
    if (Result->Headers[SSDP_CACHECONTROL] == NULL || Result->Headers[SSDP_CACHECONTROL][0] == '\x0')
    {
        TraceTag(ttidSsdpParser, "CACHE-CONTROL header missing or empty for ssdp response.");
        return FALSE;
    }
    
    // EXT header
    if (Result->Headers[SSDP_EXT] == NULL)
    {
        TraceTag(ttidSsdpParser, "EXT header missing in SSDP response.");
        return FALSE;
    }
    
    // LOCATION header
    if (Result->Headers[SSDP_LOCATION] == NULL || Result->Headers[SSDP_LOCATION][0] == '\x0')
    {
        TraceTag(ttidSsdpParser, "LOCATION header missing or empty for ssdp response.");
        return FALSE;
    }
    
    // SERVER header
    if (Result->Headers[SSDP_SERVER] == NULL || Result->Headers[SSDP_SERVER][0] == '\x0')
    {
        TraceTag(ttidSsdpParser, "SERVER header missing or empty for ssdp response.");
        return FALSE;
    }
    
    // ST header
    if(Result->Headers[SSDP_ST] == NULL || Result->Headers[SSDP_ST][0] == '\x0')
    {
        TraceTag(ttidSsdpParser, "ST header missing or empty for ssdp response");
        return FALSE;
    }
    
    // USN header
    if (Result->Headers[SSDP_USN] == NULL || Result->Headers[SSDP_USN][0] == '\x0')
    {
        TraceTag(ttidSsdpParser, "USN header missing or empty for ssdp response.");
        return FALSE;
    }
    
    return TRUE;
}


BOOL HasContentBody(PSSDP_REQUEST Result)
{
    return (Result->Headers[CONTENT_LENGTH] != NULL); 
}

BOOL ParseSsdpRequest(CHAR * szMessage, PSSDP_REQUEST Result)
{
    CHAR *szHeaders;
    CHAR pszHeaderPrefix[100] = {0};
        
    szHeaders = ParseRequestLine(szMessage, Result); 

    if (szHeaders == NULL)
    {
        return FALSE; 
    }
    else
    {
        if(VerifyRequestLine(Result) == FALSE)
            return FALSE;
    }
    
    {
        // make a copy of headers and parse them to get value of OPT header
        CHAR* pszHeadersCopy;
        
        pszHeadersCopy = (CHAR*)malloc(strlen(szHeaders) + 1);
        
        if(pszHeadersCopy)
        {
            SSDP_REQUEST Temp;
            
            memset(&Temp, 0, sizeof(Temp));
            strcpy(pszHeadersCopy, szHeaders);
            
            if(ParseHeaders(pszHeadersCopy, &Temp))
            {
                if(Temp.Headers[SSDP_OPT] && strlen(Temp.Headers[SSDP_OPT]) < sizeof(pszHeaderPrefix) - 2)
                {
                    strcpy(pszHeaderPrefix, Temp.Headers[SSDP_OPT]);
                    strcat(pszHeaderPrefix, DASH);
                }
            }
            
            FreeSsdpRequest(&Temp);
            free(pszHeadersCopy);
        }
    }
    
    char *pContent = ParseHeaders(szHeaders, Result, pszHeaderPrefix);
    if ( pContent == NULL)
    {
        return FALSE;
    }
    else
    {
        if (VerifySsdpHeaders(Result) == FALSE)
        {
            return FALSE; 
        }

        // Headers are OK. 
        
        if (Result->Headers[CONTENT_LENGTH] != NULL)
        {
            // To-Do: Maybe we can share the this routine with those 
            // in ProcessTcpReceiveData(); 
            // In that case, we need catch the return value of ParseContent
            // and probably return a more meaningful error code. 
            ParseContent(pContent, Result);
        }
        return TRUE;
    }
}


// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
// Returns a pointer to the first CHAR after the status line
// Returns NULL if fail to parse status line

BOOL ParseSsdpResponse(CHAR *szMessage, SSDP_REQUEST *Result)
{
    CHAR *token;
    CHAR *szHeaders;
    CHAR pszHeaderPrefix[100] = {0};

    //  get the version
    token = strtok(szMessage," \t\n");
    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Response: Parser could not locate the "
                 "seperator, space, tab or eol");
        return FALSE;
    }

    if (_stricmp(token, "HTTP/1.1") != 0)
    {
        TraceTag(ttidSsdpParser, "The version specified in the response "
                 "message is not HTTP/1.1");
        return FALSE;
    }

    // get the response code
    token = strtok(NULL," ");
    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Parser could not find response code in the "
                 "message.");
        return FALSE;
    }

    if (_stricmp(token, "200") != 0)
    {
        TraceTag(ttidSsdpParser, "The response code in the response message "
                 "is not HTTP/1.1");
        return FALSE;
    }


    // get the response message, no need for now.
    token = strtok(NULL,"  \t\r");

    if (token == NULL)
    {
        TraceTag(ttidSsdpParser, "Response: Parser could not locate the "
                 "seperator, space, tab or eol");
        return FALSE;
    }

    szHeaders = token + strlen(token) + 1;
    
    {
        // make a copy of headers and parse them to get value of OPT header
        CHAR* pszHeadersCopy;
        
        pszHeadersCopy = (CHAR*)malloc(strlen(szHeaders) + 1);
        
        if(pszHeadersCopy)
        {
            SSDP_REQUEST Temp;
            
            memset(&Temp, 0, sizeof(Temp));
            strcpy(pszHeadersCopy, szHeaders);
            
            if(ParseHeaders(pszHeadersCopy, &Temp))
            {
                if(Temp.Headers[SSDP_OPT] && strlen(Temp.Headers[SSDP_OPT]) < sizeof(pszHeaderPrefix) - 2)
                {
                    strcpy(pszHeaderPrefix, Temp.Headers[SSDP_OPT]);
                    strcat(pszHeaderPrefix, DASH);
                }
            }
            
            FreeSsdpRequest(&Temp);
            free(pszHeadersCopy);
        }
    }
    
    char *pContent = ParseHeaders(szHeaders, Result, pszHeaderPrefix);

    if (pContent == NULL || Result->Headers[SSDP_USN] == NULL)
    {
        return FALSE;
    }
    else
    {
        if (VerifySsdpResponseHeaders(Result) == FALSE)
        {
            return FALSE; 
        }
        
        if (Result->Headers[CONTENT_LENGTH] != NULL)
        {
            ParseContent(pContent, Result);
        }
        return TRUE;
    }
}

char * ParseHeaders(CHAR *szMessage, SSDP_REQUEST *Result, CHAR* pszHeaderPrefix/* = NULL*/)
{
    CHAR *token;
    INT i;
    INT nHeaderPrefix = 0;

    if(pszHeaderPrefix && *pszHeaderPrefix)
    {
        nHeaderPrefix = strlen(pszHeaderPrefix);
        
        ASSERT(pszHeaderPrefix[nHeaderPrefix - 1] == '-');
    }
    
    // Get the next header
    token = strtok(szMessage, "\r\n");

    while (token != NULL)
    {
        CHAR * pHeaderSep; // points to the ':' that seperates the header and its content.
        CHAR * pBeyondTokenEnd;

        pBeyondTokenEnd = token + strlen(token) + 1;

        pHeaderSep = strchr( token, ':' );
        if (pHeaderSep == NULL)
        {
            TraceTag(ttidSsdpParser, "Token %s does not have a ':', ignored.",
                     token);
        }
        else
        {
            *pHeaderSep = '\0';

            strtrim(&token);

            for (i = 0; i < NUM_OF_HEADERS; i++)
            {
                CHAR* pszHeader = token;
                
                // if we have a recognized extension skip the headr prefix
                if(0 == _strnicmp(pszHeaderPrefix, pszHeader, nHeaderPrefix))
                    pszHeader += nHeaderPrefix;
                
                if (_stricmp(SsdpHeaderStr[i], pszHeader) == 0)
                {
                    CHAR *szValue;

                    szValue = pHeaderSep + 1;
                    strtrim(&szValue);

                    if (SSDP_CACHECONTROL == i)
                    {
                        // Further parse the cache-control header
                        //
                        szValue = strstr(szValue, c_szMaxAge);
                        if (szValue)
                        {
                            CHAR *  szTemp = szValue + c_cchMaxAge;

                            strtrim(&szTemp);
                            if (*szTemp != '=')
                            {
                                TraceTag(ttidSsdpParser, "Invalid max-age directive"
                                         " in cache-control header.");
                                break;
                            }
                            else
                            {
                                szTemp++;
                                strtrim(&szTemp);
                                while (isdigit(*szTemp))
                                {
                                    szTemp++;
                                }

                                // Nul term the string so the cache-control
                                // header should now be "max-age=398733" and
                                // nothing more or less
                                *szTemp = 0;
                            }
                        }
                        else
                        {
                            TraceTag(ttidSsdpParser, "Cache-control header"
                                     "did not include max-age directive.");
                            break;
                        }
                    }

                    if(SSDP_OPT == i)
                    {
                        // check if we recognize this extension
                        if(0 != _strnicmp(szValue, g_pszExtensionURI, strlen(g_pszExtensionURI)))
                            break;
                            
                        if(!(szValue = strstr(szValue, "ns")))
                            break;
                            
                        szValue += 2; // skip ns
                        
                        while(*szValue && (*szValue == ' ' || *szValue == '=')) // skip white spaces and =
                            szValue++;
                                    
                        if(*szValue == '\x0')
                            break;
                    }
                    
                    Result->Headers[i] = (CHAR *) SsdpAlloc(
                        sizeof(CHAR) * (strlen(szValue) + 1));
                    if (Result->Headers[i] == NULL)
                    {
                        TraceTag(ttidSsdpParser, "Failed to allocate memory "
                                 "for szValue %s",szValue);
    					FreeSsdpRequest(Result);
                        Result->status = HTTP_STATUS_SERVER_ERROR;
                        return NULL;
                    }
                    strcpy(Result->Headers[i], szValue);
                    break;
                }
            }

            if (i == NUM_OF_HEADERS && pszHeaderPrefix != NULL) // don't display trace if pszHeaderPrefix because it is pre-pass to get extension prefix
            {
                // Ignore not recognized header
                TraceTag(ttidSsdpParser, "Token %s does not match any SSDP "
                         "headers",token);
            }
        }
        // Get the next header
        if (!strncmp(pBeyondTokenEnd, szEndOfHeaders, END_HEADERS_SIZE))
        {
            return (pBeyondTokenEnd + END_HEADERS_SIZE);
        }
        else
        {
            token = strtok(NULL, "\r\n");
        }
    }

    // We should always have a "\r\n\r\n" in any legal message.
    TraceTag(ttidSsdpParser, "Received message does not contain \\r\\n\\r\\n. Ignored. ");
    FreeSsdpRequest(Result);
	Result->status = HTTP_STATUS_BAD_REQUEST;	
    return NULL;
}

VOID InitializeSsdpRequest(SSDP_REQUEST *pRequest)
{
	memset(pRequest, 0, sizeof(SSDP_REQUEST));
    pRequest->Method = SSDP_INVALID;

}


VOID FreeSsdpRequest(SSDP_REQUEST *pSsdpRequest)
{
    INT i = 0;

    if (pSsdpRequest->Content != NULL)
    {
        SsdpFree(pSsdpRequest->Content);
        pSsdpRequest->Content = NULL;
    }

    if (pSsdpRequest->ContentType != NULL)
    {
        SsdpFree(pSsdpRequest->ContentType);
        pSsdpRequest->ContentType = NULL;
    }

    if (pSsdpRequest->RequestUri != NULL)
    {
        SsdpFree(pSsdpRequest->RequestUri);
        pSsdpRequest->RequestUri = NULL;
    }

    for (i = 0; i < NUM_OF_HEADERS; i++)
    {
        if (pSsdpRequest->Headers[i] != NULL)
        {
            SsdpFree(pSsdpRequest->Headers[i]);
            pSsdpRequest->Headers[i] = NULL;
        }
    }
}
// Get rid of leading or trailing white space or tab.

VOID PrintSsdpRequest(const SSDP_REQUEST *pssdpRequest)
{
    INT i;

    for (i = 0; i < NUM_OF_HEADERS; i++)
    {
        if (pssdpRequest->Headers[i] == NULL)
        {
            TraceTag(ttidSsdpParser, "%s = (NULL) ",SsdpHeaderStr[i],
                     pssdpRequest->Headers[i]);
        }
        else
        {
            TraceTag(ttidSsdpParser, "%s = (%s) ",SsdpHeaderStr[i],
                     pssdpRequest->Headers[i]);
        }
    }
}

// Assume szValue does not have beginning or trailing spaces.

INT GetMaxAgeFromCacheControl(const CHAR *szValue)
{
    CHAR * pEqual;
    _int64 Temp;

    if (szValue == NULL)
    {
        return -1;
    }

    pEqual = strchr(szValue, '=');
    if (pEqual == NULL)
    {
        return -1;
    }

    Temp = _atoi64(pEqual+1);

    if (Temp > UINT_MAX / 1000)
    {

        TraceTag(ttidSsdpAnnounce, "Life time exceeded the UINT limit. "
                 "Set to limit");

        Temp = UINT_MAX;
    }

    return (UINT)Temp;
}

BOOL ConvertToByebyeNotify(PSSDP_REQUEST pSsdpRequest) 
{
    CHAR * szTemp = "ssdp:byebye"; 

    free(pSsdpRequest->Headers[SSDP_NTS]); 
    pSsdpRequest->Headers[SSDP_NTS] = NULL; 

    pSsdpRequest->Headers[SSDP_NTS] = _strdup(szTemp); 

    if (pSsdpRequest->Headers[SSDP_NTS] == NULL)
    {
        return FALSE; 
    }
    else 
    {
        return TRUE; 
    }
}

BOOL ConvertToAliveNotify(PSSDP_REQUEST pSsdpRequest) 
{
    CHAR * szTemp = "ssdp:alive"; 

    Assert(pSsdpRequest->Headers[SSDP_ST] != NULL); 
    Assert(pSsdpRequest->Headers[SSDP_NTS] == NULL); 
    pSsdpRequest->Headers[SSDP_NTS] = _strdup(szTemp); 

    if (pSsdpRequest->Headers[SSDP_NTS] == NULL)
    {
        return FALSE; 
    }
    else 
    {
        pSsdpRequest->Headers[SSDP_NT] = pSsdpRequest->Headers[SSDP_ST]; 
        pSsdpRequest->Headers[SSDP_ST] = NULL; 
        return TRUE; 
    }
}

BOOL CompareSsdpRequest(const PSSDP_REQUEST pRequestA, const PSSDP_REQUEST pRequestB)
{
    INT i;

    for (i = 0; i < NUM_OF_HEADERS; i++)
    {
        if ((pRequestA->Headers[i] == NULL && 
             pRequestB->Headers[i]!= NULL) ||
            (pRequestA->Headers[i] != NULL && 
             pRequestB->Headers[i]== NULL))
        {
            return FALSE; 
        } 
        else if (pRequestA->Headers[i] != NULL && 
                 pRequestB->Headers[i] != NULL)
        {
            if (strcmp(pRequestA->Headers[i], pRequestB->Headers[i]) != 0) 
            {
                TraceTag(ttidSsdpParser, "Different headers index %d",i); 
                return FALSE; 
            }
        }
    }

    Assert(pRequestA->Content == NULL && pRequestB->Content == NULL); 

    Assert(pRequestA->ContentType == NULL && pRequestB->ContentType == NULL); 

    // We ignore Request URI, as they should always be * for alive and byebye. 

    return TRUE;

}

// Deep copy

BOOL CopySsdpRequest(PSSDP_REQUEST Destination, const PSSDP_REQUEST Source)
{
    INT i;
    *Destination = *Source;

    for (i = 0; i < NUM_OF_HEADERS; i++)
    {
        if (Source->Headers[i] != NULL)
        {
            Destination->Headers[i] = (CHAR *) SsdpAlloc(
                strlen(Source->Headers[i]) + 1);
            if (Destination->Headers[i] == NULL)
            {
                goto cleanup;
            }
            else
            {
                strcpy(Destination->Headers[i], Source->Headers[i]);
            }
        }
    }

    if (Source->Content != NULL)
    {
        Destination->Content = (CHAR *) SsdpAlloc(
            strlen(Source->Content) + 1);
        if (Destination->Content == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(Destination->Content, Source->Content);
        }
    }

    if (Source->ContentType != NULL)
    {
        Destination->ContentType = (CHAR *) SsdpAlloc(
            strlen(Source->ContentType) + 1);
        if (Destination->ContentType == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(Destination->ContentType, Source->ContentType);
        }
    }

    if (Source->RequestUri != NULL)
    {
        Destination->RequestUri = (CHAR *) SsdpAlloc(
            strlen(Source->RequestUri) + 1);
        if (Destination->RequestUri == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(Destination->RequestUri, Source->RequestUri);
        }
    }

    return TRUE;

cleanup:

    FreeSsdpRequest(Destination);
    return FALSE;
}

BOOL IsStrDigits(LPSTR pszStr)
{
    int i = 0; 
    while (pszStr[i] != '\0')
    {
        if (isdigit(pszStr[i++]) == 0)
        {
            return FALSE; 
        }
    }

    return TRUE; 
}
VOID strtrim(CHAR ** pszStr)
{

    CHAR *end;
    CHAR *begin;

    // Empty string. Nothing to do.
    //
    if (!(**pszStr))
    {
        return;
    }

    begin = *pszStr;
    end = begin + strlen(*pszStr) - 1;

    while (*begin == ' ' || *begin == '\t')
    {
        begin++;
    }

    *pszStr = begin;

    while (*end == ' ' || *end == '\t')
    {
        end--;
    }

    *(end+1) = '\0';
}

CHAR* IsHeadersComplete(const CHAR *szHeaders)
{
    return strstr(szHeaders, szHeaderEnd); 
}


// IsLinkLocal
__inline int
IsLinkLocal(const IPv6Addr *Addr)
{
    return ((Addr->s6_bytes[0] == 0xfe) &&
            ((Addr->s6_bytes[1] & 0xc0) == 0x80));
}

// FixURLAddressScopeId
BOOL FixURLAddressScopeId(LPCSTR pszURL, DWORD ScopeId, LPSTR pszBuffer, DWORD* pdwSize)
{
    Assert(pszURL != NULL);
    Assert(pdwSize != NULL);
    Assert(pszBuffer != NULL || *pdwSize == 0);
    
    char pszHost[INTERNET_MAX_HOST_NAME_LENGTH];
	char pszUrlPath[INTERNET_MAX_PATH_LENGTH];
	char pszUserName[INTERNET_MAX_USER_NAME_LENGTH];
    char pszPassword[INTERNET_MAX_PASSWORD_LENGTH];
    
	URL_COMPONENTSA urlComp = {0};
    
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
    
    // server
    urlComp.lpszHostName = pszHost;
	urlComp.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
    
    // URL Path
    urlComp.lpszUrlPath = pszUrlPath;
	urlComp.dwUrlPathLength = INTERNET_MAX_PATH_LENGTH;

    // user
    urlComp.lpszUserName = pszUserName;
    urlComp.dwUserNameLength = INTERNET_MAX_USER_NAME_LENGTH;

    // password
    urlComp.lpszPassword = pszPassword;
    urlComp.dwPasswordLength = INTERNET_MAX_PASSWORD_LENGTH;
    
	// break down URL
	if(!InternetCrackUrlA(pszURL, 0, ICU_DECODE, &urlComp))
	{
	    TraceTag(ttidSsdpParser, "Error %d cracking URL %s.", GetLastError(), pszURL);
	    return FALSE;
	}
	    
    int             nSize;
    SOCKADDR_IN6    sockaddr;
    wchar_t         pwszHost[INTERNET_MAX_HOST_NAME_LENGTH];
    
    if(-1 == mbstowcs(pwszHost, pszHost, INTERNET_MAX_HOST_NAME_LENGTH))
    {
        TraceTag(ttidSsdpParser, "Error converting host %s to unicode.", pszHost);
        return FALSE;
    }
    
    // get IPv6 address from the URL
    if(ERROR_SUCCESS == WSAStringToAddress(pwszHost, AF_INET6, NULL, (LPSOCKADDR)&sockaddr, &(nSize = sizeof(sockaddr))))
		if(IsLinkLocal((IPv6Addr*)&sockaddr.sin6_addr))
		{
			DWORD dw;
			
			// set scope id for IPv6 address
			sockaddr.sin6_scope_id = ScopeId;
    		
			// format address with scope id
			if(ERROR_SUCCESS != WSAAddressToString((LPSOCKADDR)&sockaddr, sizeof(sockaddr), NULL, pwszHost + 1, &(dw = INTERNET_MAX_HOST_NAME_LENGTH - 2)))
			{
			    TraceTag(ttidSsdpParser, "Error %d converting address to string.", GetLastError());
	            return FALSE;
			}
			
			pwszHost[0] = L'[';
			wcscat(pwszHost, L"]");
			
			urlComp.dwHostNameLength = wcstombs(pszHost, pwszHost, INTERNET_MAX_HOST_NAME_LENGTH);
			
			Assert(urlComp.dwHostNameLength > 0);
    		
			// recreate the location URL, now including scope id
			return InternetCreateUrlA(&urlComp, 0, pszBuffer, pdwSize);
		}
		
    return TRUE;
}
