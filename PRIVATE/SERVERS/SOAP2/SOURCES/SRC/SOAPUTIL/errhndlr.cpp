//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      errhndlr.cpp
//
// Contents:
//
//      Implementation of Error handling routines for mssoap.dll.
//
//----------------------------------------------------------------------------------


#include "headers.h"

typedef struct _HrMsgId
{
    HRESULT  _hr;
    DWORD    _MsgId;
} HRTOMSGID;


#ifdef DESKTOP_BUILD
#undef UNDER_CE
#endif

#ifdef UNDER_CE
//forward declare the RC load string (pulled in from mssoapr.dll)
extern "C"  int SoapRC_LoadString(UINT uID, LPTSTR lpBuffer, int nBufferMax);
#endif 

// Add the corresponding HRESULT and Resource string id pairs here
HRTOMSGID g_hrMsdIdPairs[] =
{
    { E_OUTOFMEMORY,                SOAP_IDS_OUTOFMEMORY            },
    { E_FAIL,                       SOAP_IDS_UNSPECIFIED_ERR        },
    { E_INVALIDARG,                 SOAP_IDS_INVALID_PARAM          },
    { OLE_E_BLANK,                  SOAP_CLIENT_NOT_INITED          },
    { CLIENT_UNKNOWN_PROPERTY,      SOAP_UNKNOWN_PROPERTY           },

    { DISP_E_TYPEMISMATCH,          SOAP_IDS_INVALID_PARAM          },
    { HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED),   SOAP_CLIENT_ALREADY_INITED          },

    { CONN_E_AMBIGUOUS,             SOAP_CONN_AMBIGUOUS             },

    { CONN_E_BAD_REQUEST,           SOAP_CONN_BAD_REQUEST           },
    { CONN_E_ACCESS_DENIED,         SOAP_CONN_ACCESS_DENIED         },
    { CONN_E_FORBIDDEN,             SOAP_CONN_FORBIDDEN             },
    { CONN_E_NOT_FOUND,             SOAP_CONN_NOT_FOUND             },
    { CONN_E_BAD_METHOD,            SOAP_CONN_BAD_METHOD            },
    { CONN_E_REQ_TIMEOUT,           SOAP_CONN_REQ_TIMEOUT           },
    { CONN_E_CONFLICT,              SOAP_CONN_CONFLICT              },
    { CONN_E_GONE,                  SOAP_CONN_GONE                  },
    { CONN_E_TOO_LARGE,             SOAP_CONN_TOO_LARGE             },
    { CONN_E_ADDRESS,               SOAP_CONN_ADDRESS               },

    { CONN_E_SERVER_ERROR,          SOAP_CONN_SERVER_ERROR          },
    { CONN_E_SRV_NOT_SUPPORTED,     SOAP_CONN_SRV_NOT_SUPPORTED     },
    { CONN_E_BAD_GATEWAY,           SOAP_CONN_BAD_GATEWAY           },
    { CONN_E_NOT_AVAILABLE,         SOAP_CONN_NOT_AVAILABLE         },
    { CONN_E_SRV_TIMEOUT,           SOAP_CONN_SRV_TIMEOUT           },
    { CONN_E_VER_NOT_SUPPORTED,     SOAP_CONN_VER_NOT_SUPPORTED     },

    { CONN_E_BAD_CONTENT,           SOAP_CONN_BAD_CONTENT           },

    { CONN_E_CONNECTION_ERROR,      SOAP_CONN_CONNECTION_ERROR      },
    { CONN_E_BAD_CERTIFICATE_NAME,  SOAP_CONN_BAD_CERTIFICATE_NAME  },

    { CONN_E_HTTP_UNSPECIFIED,      SOAP_CONN_HTTP_UNSPECIFIED      },
    { CONN_E_HTTP_SENDRECV,         SOAP_CONN_HTTP_SENDRECV         },
    { CONN_E_HTTP_OUTOFMEMORY,      SOAP_CONN_HTTP_OUTOFMEMORY      },
    { CONN_E_HTTP_BAD_RESPONSE,     SOAP_CONN_HTTP_BAD_RESPONSE     },
    { CONN_E_HTTP_BAD_URL,          SOAP_CONN_HTTP_BAD_URL          },
    { CONN_E_HTTP_DNS_FAILURE,      SOAP_CONN_HTTP_DNS_FAILURE      },
    { CONN_E_HTTP_CONNECT_FAILED,   SOAP_CONN_HTTP_CONNECT_FAILED   },
    { CONN_E_HTTP_SEND_FAILED,      SOAP_CONN_HTTP_SEND_FAILED      },
    { CONN_E_HTTP_RECV_FAILED,      SOAP_CONN_HTTP_RECV_FAILED      },
    { CONN_E_HTTP_HOST_NOT_FOUND,   SOAP_CONN_HTTP_HOST_NOT_FOUND   },
    { CONN_E_HTTP_OVERLOADED,       SOAP_CONN_HTTP_OVERLOADED       },
    { CONN_E_HTTP_FORCED_ABORT,     SOAP_CONN_HTTP_FORCED_ABORT     },
    { CONN_E_HTTP_NO_RESPONSE,      SOAP_CONN_HTTP_NO_RESPONSE      },
    { CONN_E_HTTP_BAD_CHUNK,        SOAP_CONN_HTTP_BAD_CHUNK        },
    { CONN_E_HTTP_PARSE_RESPONSE,   SOAP_CONN_HTTP_PARSE_RESPONSE   },
        
    { WSDL_MUSTUNDERSTAND,   SOAP_IDS_MUSTUNDERSTAND_ERR   },        

};



/////////////////////////////////////////////////////////////////////////////
// HrToMsgId - Returns the MsgId that most closely associates with the 
//      HRESULT code given

DWORD HrToMsgId(HRESULT hr)
{
    int i;
    for (i = 0 ; i < countof(g_hrMsdIdPairs) ; i++)
    {
        if (g_hrMsdIdPairs[i]._hr == hr)
            return g_hrMsdIdPairs[i]._MsgId;
    }
    return SOAP_IDS_UNSPECIFIED_ERR;
}


DWORD GetResourceString
    (
    DWORD	dwMessageId,	// requested message identifier
    LPWSTR	lpBuffer,		// pointer to message buffer
    DWORD	nSize,			// maximum size of message buffer
    ...                         // variable args
    )
{
    DWORD   cch = 0;

    va_list args;
    va_start(args, nSize);
    cch = GetResourceStringHelper(dwMessageId, lpBuffer, nSize, &args);
    va_end(args);
    return cch;
}


/////////////////////////////////////////////////////////////////////////////
//  GetResourceString - Loads and formats a resource string from the
//          localized resource dll.
//  Returns: No of characters in the returned string


DWORD GetResourceStringHelper
(
    DWORD	dwMessageId,	// requested message identifier
    LPWSTR	lpBuffer,		// pointer to message buffer
    DWORD	nSize,			// maximum size of message buffer
    va_list	*Arguments		// Arguments to be passed to the message
)
{
    DWORD   cch = 0;


    // IDs with values < SOAP_LOCALIZABLE_STRING_BEGIN are in 
    // mssoap.dll, and the ones above are in mssoapr.dll
 
#ifndef UNDER_CE
    cch = FormatMsg(FORMAT_MESSAGE_FROM_HMODULE,
                        dwMessageId < SOAP_LOCALIZABLE_STRING_BEGIN ?
                            g_hInstance : g_hInstance_language,
                        dwMessageId, 0,
                        lpBuffer, nSize, Arguments);
#else

    WCHAR    *pwchMsg;
    int       cChars;
  
    // Get message   
    __try {
       pwchMsg = (WCHAR*) _alloca (nSize*2 + 1);  
    } 
    __except(1)
    {
        //an error occured with _alloca (out of mem)
        return 0;   
    }
    if(dwMessageId < SOAP_LOCALIZABLE_STRING_BEGIN)
        cChars = LoadStringW((HINSTANCE)g_hInstance, dwMessageId, pwchMsg, nSize);
    else
        cChars = SoapRC_LoadString(dwMessageId, pwchMsg, nSize);

    cch = FormatMessage(FORMAT_MESSAGE_FROM_STRING,
                    pwchMsg,
                    dwMessageId,
                    0,
                    lpBuffer,
                    nSize,
                    Arguments);
#endif
    return cch;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int CwchMultiByteToWideChar
//
//  parameters:
//
//  description:
//        Convert DBCS to UNICODE Trim string, if it's too long
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
int CwchMultiByteToWideChar
    (
    UINT	cp,
    char	*pch,
    int		cch,
    WCHAR	*pwch,
    int		cwch
    )
    {
    ASSERT (0 != cwch);
    // Calculate length, if necessary
    if (-1 == cch)
    	cch = (int)strlen (pch);
    // Convert in place, if possible
    if (cch <= cwch)
    	return MultiByteToWideChar(cp, 0, pch, cch, pwch, cwch);

    // Do it hard way...
#ifdef CE_NO_EXCEPTIONS
        WCHAR *pwchTemp;
    __try{
         pwchTemp = (WCHAR*) _alloca (cch*sizeof(WCHAR));
     }
    __except(1){
        return 0;
     }
#else
#ifndef UNDER_CE
    WCHAR    *pwchTemp = (WCHAR*) _alloca (cch*sizeof(WCHAR));
#else
    WCHAR *pwchTemp;
    try {
       pwchTemp = (WCHAR*) _alloca (cch*sizeof(WCHAR));
    }
    catch(...){
        return 0;
    }  
#endif
#endif

    int		cwchTemp = MultiByteToWideChar(cp, 0, pch, cch, pwchTemp, cch);

    cwch = MIN (cwch, cwchTemp);
    memcpy (pwch, pwchTemp, cwch*sizeof(WCHAR));

    return cwch;
    }


// isdigit() on W2K in certain conditions return true for the 
// far east period punctuation causing havoc processing localized error messages.
// Replacing isdigit by our own version.  No need to worry about DBCS lead byte
// since the preceding character is either FIsAsciiDigit or '%'.
#define FIsAsciiDigit(ch) ((((BYTE) (ch)) >= '0') && (((BYTE) (ch)) <= '9'))


//-----------------------------------------------------------------------------
// W95FormatMsg
//
// WIN95 and WIN98 do not support FormatMessageW, so we have to use FormatMessageA
// and perform conversions ASCII<->UNICODE on the fly
//
//
//-----------------------------------------------------------------------------
#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
DWORD WINAPI W95FormatMsg
    (
    DWORD	dwFlags,		// source and processing options
    LPCVOID	lpSource,		// pointer to message source
    DWORD	dwMessageId,	// requested message identifier
    DWORD	dwLanguageId,	// language identifier for requested message
    LPWSTR	lpBuffer,		// pointer to message buffer
    DWORD	nSize,			// maximum size of message buffer
    va_list	*Arguments		// address of array of message inserts
    )
    {
#ifdef _WIN64
    //
    // This should only get called on Win9x, which is not 64 bit
    //
    ASSERT(FALSE);
    return 0;
#else

    WCHAR	rgwchFmt[20];
    WCHAR	*pwchTmp;
    char	*pchMsg;
    char	*pchTmp;
    char	*pchSave;
    int		cChars;
    DWORD	rdwArgs[99+2];	// Arguments
    DWORD	cArgs;
    DWORD	nArg;
    int		cStars;
    UINT	cp;

    nSize --;	// For terminating zero
    rgwchFmt[0] = '%';

    // Use system ANSI codepage.
    cp = CP_ACP;

    // If necessary, allocate buffer for message
    if (dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER)
    	{
    	nSize = ONEK;
    	pwchTmp = (WCHAR *) LocalAlloc (LMEM_FIXED, nSize * sizeof(WCHAR));
    	* (WCHAR**) lpBuffer = pwchTmp;
    	lpBuffer = pwchTmp;
    	if (!lpBuffer)
    	    return 0;
    	}

    // Allocate temporary buffer
    pchMsg = (char*) _alloca (nSize*2 + 1);	// Can be 2-bytes sequences

    // Get message
    if (dwFlags & FORMAT_MESSAGE_FROM_HMODULE)
    {
    	cChars = LoadStringA((HINSTANCE)lpSource, dwMessageId, pchMsg, nSize);
    }
    else
    {
    	cChars = FormatMessageA(dwFlags | FORMAT_MESSAGE_IGNORE_INSERTS,
    						lpSource,
    						dwMessageId,
    						dwLanguageId,
    						pchMsg,
    						nSize,
    						NULL);
    	pchMsg[cChars] = 0;
    }

    // If there is no need to format it, return immediatelly
    if ((0 == cChars) || (Arguments == NULL) || (dwFlags & FORMAT_MESSAGE_IGNORE_INSERTS))
    	{
    	cChars = CwchMultiByteToWideChar(cp, pchMsg, -1, lpBuffer, nSize);
    	ASSERT (cChars <= (int) nSize);
    	if (cChars >= (int) nSize)
    		cChars = nSize-1;
    	lpBuffer[cChars] = 0;
    	return cChars;
    	}

    // Ok, we have to format it.
    for (cArgs = 0, pwchTmp = lpBuffer, pchTmp = pchSave = pchMsg; nSize && * pchTmp; pchTmp ++)
    	{
    	if ('%' == *pchTmp)
    		{
    		// Write the previous portion of the string
    		if (pchTmp != pchSave)
    			{
    			*pchTmp = 0;
    			cChars = CwchMultiByteToWideChar(cp, pchSave, -1, pwchTmp, nSize);
    			pwchTmp += cChars;
    			nSize -= cChars;
    			if (0 == nSize)
    				break;
    			}
    		pchTmp ++;
    		// Check for special sequences
    		if ('n' == * pchTmp)
    			{
    			* pwchTmp ++ = L'\n';
    			nSize --;
    			}
    		else if ('0' == * pchTmp)
    			{
    			* pwchTmp ++ = 0;
    			nSize --;
    			}
    		// Is it argument? If so, handle it
    		else if (FIsAsciiDigit (* pchTmp))
    			{
    			// First, get arg #
    			nArg = (* pchTmp) - '0';
    			pchTmp ++;
    			if (FIsAsciiDigit (* pchTmp))
    				{
    				nArg = nArg * 10 + (* pchTmp) - '0';
    				pchTmp ++;
    				}
    			cStars = 0;
    			// Get format specifier, if any
    			if ('!' == * pchTmp)
    				{
    				pchSave = ++ pchTmp;
    				while ('!' != * pchTmp)
    					{
    					if ('*' == * pchTmp)
    						cStars ++;
    					pchTmp ++;
    					}
    				* pchTmp = 0;

    				MultiByteToWideChar(cp, 0, pchSave, -1, rgwchFmt+1, sizeof(rgwchFmt)/sizeof(WCHAR));
    				}
    			else
    				{
    				rgwchFmt[1] = L's';
    				rgwchFmt[2] = 0;
    				pchTmp --;
    				}
    			// Copy arguments to local array
    			while (cArgs < nArg + cStars)
    				{
    				rdwArgs [cArgs] = (dwFlags & FORMAT_MESSAGE_ARGUMENT_ARRAY) ?
    									((DWORD*) Arguments)[cArgs] :
    									va_arg (* Arguments, DWORD);
    				cArgs ++;
    				}
    			// Now print that argument. Pass 3 parameters, as there can be 2 stars
    			cChars = _snwprintf (pwchTmp, nSize, rgwchFmt, rdwArgs[nArg-1], rdwArgs[nArg], rdwArgs[nArg+1]);
    			if (0 > cChars)
    				cChars = nSize;
    			pwchTmp += cChars;
    			nSize -= cChars;
    			}
    		else
    			pchTmp --;
    		pchSave = pchTmp + 1;
    		}
    	}
    // Write last portion of the string
    if (nSize && pchTmp != pchSave)
    	{
    	cChars = CwchMultiByteToWideChar(cp, pchSave, -1, pwchTmp, nSize);
    	pwchTmp += cChars;
    	nSize -= cChars;
    	}
    * pwchTmp = 0;
    return pwchTmp - lpBuffer;
#endif
    }
#endif

#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
//-----------------------------------------------------------------------------
// FormatMsg -- Version of FormatMessage that works on all platforms
//-----------------------------------------------------------------------------
DWORD WINAPI FormatMsg
    (
    DWORD    dwFlags,        // source and processing options
    LPCVOID    lpSource,        // pointer to message source
    DWORD    dwMessageId,    // requested message identifier
    DWORD    dwLanguageId,    // language identifier for requested message
    LPWSTR    lpBuffer,        // pointer to message buffer
    DWORD    nSize,            // maximum size of message buffer
    va_list    *Arguments        // address of array of message inserts
    )
{
    WCHAR    *pwchMsg;
    int        cChars;
    if (g_fIsWin9x)
    {
        return W95FormatMsg(dwFlags,
                    lpSource,
                    dwMessageId,
                    dwLanguageId,
                    lpBuffer,
                    nSize,
                    Arguments);
    }

    // Otherwise we use the os version of FormatMessage 
    // Allocate temporary buffer

    // Get message
    if (dwFlags & FORMAT_MESSAGE_FROM_HMODULE)
    {
        __try {
           pwchMsg = (WCHAR*) _alloca (nSize*2 + 1);  
        } 
        __except(1)
        {
            //an error occured with _alloca (out of mem)
            return 0;   
        }
        cChars = LoadStringW((HINSTANCE)lpSource, dwMessageId, pwchMsg, nSize);
		dwFlags = (dwFlags & ~FORMAT_MESSAGE_FROM_HMODULE) | FORMAT_MESSAGE_FROM_STRING;
    }
    else
    {
        pwchMsg = (WCHAR *)lpSource;
    }

    return FormatMessage(dwFlags,
                    pwchMsg,
                    dwMessageId,
                    dwLanguageId,
                    lpBuffer,
                    nSize,
                    Arguments);
}
#endif 




//-----------------------------------------------------------------------------
// ForwardToBackslash -- Converts all forward slashes to back slashes in a string
//-----------------------------------------------------------------------------
void ForwardToBackslash(char *pstr)
{

    if(pstr)
    {
        while (*pstr)
        {
            if (*pstr == '/')
                *pstr = '\\';
            pstr++;
        }
    }    

}


void ForwardToBackslashW(WCHAR *pwstr)
{
    if(pwstr)
    {
        while (*pwstr)
        {
            if (*pwstr == '/')
                *pwstr = '\\';
            pwstr++;
        }
    }    
}


