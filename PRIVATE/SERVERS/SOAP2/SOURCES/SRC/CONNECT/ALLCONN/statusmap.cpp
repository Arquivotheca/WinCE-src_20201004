//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      StatusMap.cpp
//
// Contents:
//
//      Implements code mapping HTTP status into HRESULT
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

static HttpMapEntry g_HttpStatusMapEntries200 [] =
{
    { HTTP_STATUS_OK,                   S_OK                        },
    { HTTP_STATUS_CREATED,              S_OK                        },
    { HTTP_STATUS_ACCEPTED,             S_OK                        },
    { HTTP_STATUS_PARTIAL,              S_OK                        },
    { HTTP_STATUS_NO_CONTENT,           S_OK                        },
    { HTTP_STATUS_RESET_CONTENT,        S_OK                        },
    { HTTP_STATUS_PARTIAL_CONTENT,      S_OK                        },
};

static HttpMapEntry g_HttpStatusMapEntries300 [] =
{
    { HTTP_STATUS_AMBIGUOUS,            CONN_E_AMBIGUOUS            },
    { HTTP_STATUS_MOVED,                CONN_E_NOT_FOUND            },
    { HTTP_STATUS_REDIRECT,             CONN_E_NOT_FOUND            },
    { HTTP_STATUS_REDIRECT_METHOD,      CONN_E_NOT_FOUND            },
    { HTTP_STATUS_REDIRECT_KEEP_VERB,   CONN_E_NOT_FOUND            },
};

static HttpMapEntry g_HttpStatusMapEntries400 [] =
{
    { HTTP_STATUS_BAD_REQUEST,          CONN_E_BAD_REQUEST          },
    { HTTP_STATUS_DENIED,               CONN_E_ACCESS_DENIED        },
    { HTTP_STATUS_FORBIDDEN,            CONN_E_FORBIDDEN            },
    { HTTP_STATUS_NOT_FOUND,            CONN_E_NOT_FOUND            },
    { HTTP_STATUS_BAD_METHOD,           CONN_E_BAD_METHOD           },
    { HTTP_STATUS_REQUEST_TIMEOUT,      CONN_E_REQ_TIMEOUT          },
    { HTTP_STATUS_CONFLICT,             CONN_E_CONFLICT             },
    { HTTP_STATUS_GONE,                 CONN_E_GONE                 },
    { HTTP_STATUS_REQUEST_TOO_LARGE,    CONN_E_TOO_LARGE            },
    { HTTP_STATUS_URI_TOO_LONG,         CONN_E_ADDRESS              },
};

static HttpMapEntry g_HttpStatusMapEntries500 [] =
{
    { HTTP_STATUS_SERVER_ERROR,         CONN_E_SERVER_ERROR         },
    { HTTP_STATUS_NOT_SUPPORTED,        CONN_E_SRV_NOT_SUPPORTED    },
    { HTTP_STATUS_BAD_GATEWAY,          CONN_E_BAD_GATEWAY          },
    { HTTP_STATUS_SERVICE_UNAVAIL,      CONN_E_NOT_AVAILABLE        },
    { HTTP_STATUS_GATEWAY_TIMEOUT,      CONN_E_SRV_TIMEOUT          },
    { HTTP_STATUS_VERSION_NOT_SUP,      CONN_E_VER_NOT_SUPPORTED    },
};


HttpMap g_HttpStatusCodeMap [] =
{
    { 0,                                    0,                          E_FAIL              },     // 000
    { 0,                                    0,                          E_FAIL              },     // 100
    { countof(g_HttpStatusMapEntries200),   g_HttpStatusMapEntries200,  S_OK                },     // 200
    { countof(g_HttpStatusMapEntries300),   g_HttpStatusMapEntries300,  CONN_E_AMBIGUOUS    },     // 300
    { countof(g_HttpStatusMapEntries400),   g_HttpStatusMapEntries400,  CONN_E_BAD_REQUEST  },     // 400
    { countof(g_HttpStatusMapEntries500),   g_HttpStatusMapEntries500,  CONN_E_SERVER_ERROR },     // 500
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static HRESULT HttpStatusToHresultClass(HttpMap *map, DWORD dwStatus)
//
//  parameters:
//          
//  description:
//          Converts HTTP status code to HRESULT in given HTTP status code class
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
static HRESULT HttpStatusToHresultClass(HttpMap *map, DWORD dwStatus)
{
    ASSERT(map != 0);

    HRESULT hr = map->dflt;

    if (map->elv != 0)
    {
        int i = 0;
        int j = map->elc - 1;

        while (i <= j)
        {
            int k = (i + j) >> 1;
            HttpMapEntry *pEntry = map->elv + k;

            if (pEntry->http == dwStatus)
            {
                hr = pEntry->hr;
                break;
            }
            else if (pEntry->http < dwStatus)
            {
                i = k + 1;
            }
            else
            {
                j = k - 1;
            }
        }
    }

    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT HttpStatusToHresult(DWORD dwStatus)
//
//  parameters:
//          
//  description:
//          Converts HTTP status code to HRESULT
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT HttpStatusToHresult(DWORD dwStatus)
{
    DWORD   dwClass = dwStatus / 100;
    HRESULT hr      = E_FAIL;

    if (dwClass < 6)
    {
        hr = HttpStatusToHresultClass(g_HttpStatusCodeMap + dwClass, dwStatus);
    }

    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT HttpContentTypeToHresult(LPCSTR contentType)
//
//  parameters:
//          
//  description:
//          Decides whether content type is text/xml. If it is not text/xml, returns error
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT HttpContentTypeToHresult(LPCSTR contentType)
{
    // only compare the START of the contentType, as there might be charset info and other stuff
    // behind it that we don't care about
    if (contentType == 0 || strnicmp(contentType, s_TextXmlA, strlen(s_TextXmlA)) == 0)
    {
        return S_OK;
    }
    else
    {
        return CONN_E_BAD_CONTENT;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT HttpContentTypeToHresult(LPCWSTR contentType)
//
//  parameters:
//          
//  description:
//          Decides whether content type is text/xml. If it is not text/xml, returns error
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT HttpContentTypeToHresult(LPCWSTR contentType)
{
    if (contentType == 0 || _wcsnicmp(contentType, s_TextXmlW, wcslen(s_TextXmlW)) == 0)
    {
        return S_OK;
    }
    else
    {
        return CONN_E_BAD_CONTENT;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

