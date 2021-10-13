//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       S U B M I T . C P P
//
//  Contents:
//
//  Notes:
//
//  
//
//----------------------------------------------------------------------------
#include <ssdppch.h>
#pragma hdrstop


#include "ssdpnetwork.h"
#include "ssdpsrv.h"

#include "string.hxx"

extern LONG cInitialized;

extern const CHAR c_szHttpVersion[];
extern const CHAR c_szNt[];
extern const CHAR c_szNts[];
extern char *SsdpHeaderStr[];

const CHAR c_szNotify[]     = "NOTIFY";
const CHAR c_szColon[]      = ":";
const CHAR c_szCrlf[]       = "\r\n";
const CHAR c_szTextXML[]   = "text/xml";


//+---------------------------------------------------------------------------
//
//  Function:   FUpdateEventSourceWithProps
//
//  Purpose:    Given a set of properties that have changed, this function
//              updates the event source's local cache of those property
//              values.
//
//  Arguments:
//      pes     [in]    Pointer to event source
//      cProps  [in]    Number of properties to update
//      rgProps [in]    New values
//
//  Returns:    TRUE if at least one property was updated, FALSE if none were
//
//  
//
//  Notes:
//
BOOL FUpdateEventSourceWithProps(UPNP_EVENT_SOURCE *pes, DWORD cProps,
                                 UPNP_PROPERTY *rgProps)
{
    DWORD       iProp;
    BOOL        fResult = FALSE;
    if (!pes->cProps)
    {
    	// Initialize the property list. Should be the complete set of evented variables
    	Assert(!pes->rgesProps);
	    pes->rgesProps = (UPNP_PROPERTY *) malloc(sizeof(UPNP_PROPERTY) * cProps);

	    // Copy in property information
	    //
	    for (iProp = 0; iProp < cProps; iProp++)
	    {
	        CopyUpnpProperty(&pes->rgesProps[iProp], &rgProps[iProp]);
	        pes->cProps++;
	    }
	    
	    Assert(pes->cProps == cProps);
	    fResult = TRUE;
    	
    	// Set modified flag for all properties for all subscribers
	    for (PLIST_ENTRY p = pes->listSubs.Flink; p != &pes->listSubs; p = p->Flink)
	    {
	        UPNP_SUBSCRIBER *pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
	        
	        Assert(!pSub->rgesModified);
	        pSub->rgesModified = (bool*) malloc(sizeof(bool) * cProps);
	        
	        for (iProp = 0; iProp < cProps; iProp++)
	            pSub->rgesModified[iProp] = true;
	            
	        pSub->cProps = cProps;
	    }
    }
	else
    for (iProp = 0; iProp < cProps; iProp++)
    {
        DWORD           iDstProp;
        UPNP_PROPERTY*  rgesProps = pes->rgesProps;

        for (iDstProp = 0; iDstProp < pes->cProps; iDstProp++)
        {
            if (!_wcsicmp(rgProps[iProp].szName,
                          rgesProps[iDstProp].szName))
            {
                // Free the old value
                free(rgesProps[iDstProp].szValue);

                Assert(rgProps[iProp].szValue);

                // And copy in the new one
                rgesProps[iDstProp].szValue = _wcsdup(rgProps[iProp].szValue);
                
                // Mark the property as modified for all subscribers
	            for (PLIST_ENTRY p = pes->listSubs.Flink; p != &pes->listSubs; p = p->Flink)
	            {
	                UPNP_SUBSCRIBER *pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
        	        
                    pSub->rgesModified[iDstProp] = true;
	            }
                
                fResult = TRUE;
                break;
            }
        }
    }

    return fResult;
}


bool EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr)
{
	wchar_t		aCharacters[] = {L'<', L'>', L'\'', L'"', L'&'};
	wchar_t*	aEntityReferences[] = {L"&lt;", L"&gt;", L"&apos;", L"&quot;", L"&amp;"};
	bool		bReplaced;
	
	pstr->reserve(static_cast<ce::wstring::size_type>(1.1 * wcslen(pwsz)));
	pstr->resize(0);
	
	for(const wchar_t* pwch = pwsz; *pwch; ++pwch)
	{
		bReplaced = false;

		for(int i = 0; i < sizeof(aCharacters)/sizeof(*aCharacters); ++i)
			if(*pwch == aCharacters[i])
			{
				pstr->append(aEntityReferences[i]);
				bReplaced = true;
				break;
			}

		if(!bReplaced)
			pstr->append(pwch, 1);
	}

	return true;
}


const WCHAR c_wszPropertySetBegin[] = L"<U:propertyset xmlns:U=\"urn:schemas-upnp-org:event-1-0\">\r\n";
const WCHAR c_wszPropertySetEnd[]   = L"</U:propertyset>\r\n";

const WCHAR c_wszPropertyBegin[]    = L" <U:property>\r\n";
const WCHAR c_wszPropertyEnd[]      = L" </U:property>\r\n";

const WCHAR c_wszPropBeginFmt[]     = L"  <%s>";
const WCHAR c_wszPropEndFmt[]       = L"</%s>\r\n";

const WCHAR c_wszPropValFmt[]       = L"%s";
const WCHAR c_wszCrlf[]				= L"\r\n";

//+---------------------------------------------------------------------------
//
//  Function:   HrComposeXmlBodyFromEventSource
//
//  Purpose:    Creates an XML message to be sent along with a NOTIFY. The
//              message contains the property changes according to the
//              example schema below.
//
//  Arguments:
//      pes    [in]     Pointer to event source
//      iSeq   [in]    Sequence number of subscriber
//      pszOut [out]    Returns XML body as a null-term string
//
//  Returns:    S_OK if success, E_OUTOFMEMORY if no memory or other
//              INTERNET_* errors
//
//  
//
//  Notes:      This function knows if a subscriber has not been sent an
//              initial update and if so, submits all properties for
//              notification.
//
//              XML schema is as follows:
//
//             <U:propertyset xmlns:U="urn:schemas-upnp-org:event-1-0">
//                 <U:property>
//                     <U:foo>
//                         goodbye
//                     </U:foo>
//                 </U:property>
//                 <U:property>
//                     <U:bar>
//                         27
//                     </U:bar>
//                 </U:property>
//             </U:propertyset>
//
HRESULT HrComposeXmlBodyFromEventSource(UPNP_EVENT_SOURCE *pes,
                                        UPNP_SUBSCRIBER *pSub,
                                        BOOL fAllProps,
                                        LPSTR *pszOut)
{
    HRESULT         hr = S_OK;
    DWORD           iProp;
    DWORD           cchBuffer = 0;
    LPWSTR			szBuffer = NULL;
    DWORD           cProps = 0;
    LPWSTR			szTemp = NULL;

    DWORD           cchMaxFmt;
    DWORD           cchValFmt;
    DWORD           cchBeginFmt;
    DWORD           cchEndFmt;

	ce::wstring		str;

    cchValFmt = wcslen(c_wszPropValFmt);
    cchBeginFmt = wcslen(c_wszPropBeginFmt);
    cchEndFmt = wcslen(c_wszPropEndFmt);

    cchMaxFmt = max(cchValFmt, cchBeginFmt);
    cchMaxFmt = max(cchMaxFmt, cchEndFmt);

    Assert(pszOut);

    // Must re-acquire the list lock and verify that this event source is
    // still in the list. If not, return an error and leave.
    //
    EnterCriticalSection(&g_csListEventSource);
    pes = PesVerifyEventSource(pes);
    if (!pes)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
        goto cleanup;
    }

    // Pass #1:
    // Calculate size of body necessary
    //
    cchBuffer += wcslen(c_wszPropertySetBegin);
    cchBuffer += wcslen(c_wszPropertySetEnd);

    for (iProp = 0; iProp < pes->cProps; iProp++)
    {
        if ((!(pes->rgesProps[iProp].dwFlags & UPF_NON_EVENTED)) &&
            ((pSub->rgesModified[iProp] || fAllProps)))
        {
            cProps++;

            cchBuffer += wcslen(c_wszPropertyBegin);
            cchBuffer += wcslen(c_wszPropertyEnd);

            cchBuffer += wcslen(c_wszPropValFmt);

            cchBuffer += wcslen(c_wszPropBeginFmt);
            cchBuffer += wcslen(c_wszPropEndFmt);

            // For prop begin tag
            cchBuffer += wcslen(pes->rgesProps[iProp].szName);

            EncodeForXML(pes->rgesProps[iProp].szValue, &str);
			cchBuffer += str.length();

            // For prop end tag
            cchBuffer += wcslen(pes->rgesProps[iProp].szName);
        }
    }

    // Final CRLF and null-term.
    cchBuffer += wcslen(c_wszCrlf) + 1;

    // Pass #2:
    // Allocate buffer and add text to it
    //
    szBuffer = new WCHAR[cchBuffer];

    if (!szBuffer)
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        // <U:propertyset xmlns:U="upnp">
        wcscpy(szBuffer, c_wszPropertySetBegin);

        for (iProp = 0; iProp < pes->cProps; iProp++)
        {
            UPNP_PROPERTY * pProp = &pes->rgesProps[iProp];

            if ((!(pes->rgesProps[iProp].dwFlags & UPF_NON_EVENTED)) &&
                ((pSub->rgesModified[iProp] || fAllProps)))
            {
                DWORD   cchName;
                DWORD   cchValue;

                EncodeForXML(pProp->szValue, &str);

				pSub->rgesModified[iProp] = false;    
                cchName = wcslen(pProp->szName);
                cchValue = str.length();

                cchBuffer = max(cchName, cchValue);
                cchBuffer += cchMaxFmt;

                szTemp = new WCHAR[cchBuffer + 1];
                if (szTemp)
                {
                    // <U:property>
                    wcscat(szBuffer, c_wszPropertyBegin);

                    // <foo>
                    _snwprintf(szTemp, cchBuffer, c_wszPropBeginFmt, pProp->szName);
                    wcscat(szBuffer, szTemp);

                    //     goodbye
					_snwprintf(szTemp, cchBuffer, c_wszPropValFmt, static_cast<LPCWSTR>(str));
                    wcscat(szBuffer, szTemp);

                    // </foo>
                    _snwprintf(szTemp, cchBuffer, c_wszPropEndFmt, pProp->szName);
                    wcscat(szBuffer, szTemp);

                    // </U:property>
                    wcscat(szBuffer, c_wszPropertyEnd);

                    delete [] szTemp;
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    goto cleanup;
                }
            }
        }

        // </U:propertyset>
        wcscat(szBuffer, c_wszPropertySetEnd);

		// convert event body to UTF8
		int     cb;
        UINT    cp = CP_UTF8;
        
        cb = WideCharToMultiByte(cp, 0, szBuffer, -1, NULL, 0, 0, 0);

        if(!cb)
            if(cb = WideCharToMultiByte(CP_ACP, 0, szBuffer, -1, NULL, 0, 0, 0))
                cp = CP_ACP;
		
		if(*pszOut = (LPSTR)malloc(cb))
			WideCharToMultiByte(cp, 0, szBuffer, -1, *pszOut, cb, 0, 0);
		else
			hr = E_OUTOFMEMORY;

		delete [] szBuffer;
    }

cleanup:
    LeaveCriticalSection(&g_csListEventSource);
    TraceError("HrComposeXmlBodyFromEventSource", hr);
    return hr;
}


//+---------------------------------------------------------------------------
//
//  Function:   HrSubmitEventToSubscriber
//
//  Purpose:    Submits a raw event to a specific subscriber
//
//  Arguments:
//      dwFlags     [in] Currently unused
//      szHeaders   [in] null-term string of headers for the event,
//                       separated by CRLF
//      szEventBody [in] null-term string containing event body
//      szDestUrl   [in] Destination URL of subscriber
//
//  Returns:    S_OK if success, E_OUTOFMEMORY if no memory or other
//              INTERNET_* errors
//
//  
//
//  Notes:
//
HRESULT HrSubmitEventToSubscriber(DWORD dwFlags,
                                  LPCSTR szSid, DWORD iSeq, LPCSTR szEventBody,
                                  LPCSTR szDestUrl)
{
    HttpRequest request;
	char		pszSeq[10];

	sprintf(pszSeq, "%d", iSeq);
	
	// prepare NOTIFY request
	if(!request.Open("NOTIFY", szDestUrl, "HTTP/1.1"))
		return request.GetHresult();

	// add SID header
	request.AddHeader("NT", "upnp:event");
	request.AddHeader("NTS", "upnp:propchange");
	request.AddHeader("SID", szSid);
	request.AddHeader("SEQ", pszSeq);
	request.AddHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\"");
	request.AddHeader("CONTENT-LENGTH", strlen(szEventBody));

	// write request body
	request.Write(szEventBody);

	TraceTag(ttidEvents, "Sending event to subscriber %s ", szDestUrl);
    TraceTag(ttidEvents, "-------------------------------------------");
    TraceTag(ttidEvents, "SEQ: %d\n%s", iSeq, szEventBody);
    TraceTag(ttidEvents, "-------------------------------------------");

	// send request
	if(!request.Send())
	{
		TraceTag(ttidEvents, "Failed with error: 0x%08x", request.GetHresult());
	    TraceTag(ttidEvents, "-------------------------------------------");
	    
		return request.GetHresult();
    }

	if(request.GetStatus() != HTTP_STATUS_OK)
	{
	    TraceTag(ttidEvents, "Subscriber returned status %d", request.GetStatus());
	    TraceTag(ttidEvents, "-------------------------------------------");
	}
	    
	return S_OK;
}



//+---------------------------------------------------------------------------
//
//  Function:   SubmitUpnpPropertyEvent
//
//  Purpose:    Public API to submit a property change event to all subscribers.
//
//  Arguments:
//      szEventSourceUri [in]   URI identifying the event source
//      dwFlags          [in]   Currently unused
//      cProps           [in]   Number of properties that have changed
//      rgProps          [in]   Properties that have changed
//
//  Returns:    S_OK if success, E_OUTOFMEMORY if no memory or other
//              INTERNET_* errors
//
//  
//
//  Notes:
//
BOOL WINAPI SubmitUpnpPropertyEvent(
    /* [string][in] */ LPCSTR szEventSourceUri,
    /* [in] */ DWORD dwFlags,
    /* [in] */ DWORD cProps,
    /* [in] */ UPNP_PROPERTY __RPC_FAR *rgProps)
{
	DWORD				dwError = NO_ERROR;
    UPNP_EVENT_SOURCE * pes;
    HRESULT             hr = S_OK;
    BOOL                fRet;

    if (InterlockedExchange(&cInitialized, cInitialized) == 0)
    {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    // Validate params
    //
    if (!cProps || !rgProps || !szEventSourceUri || !*szEventSourceUri)
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Get the event source that the URI refers to. If we don't find it,
    // return an error.
    //
    pes = PesFindEventSource(szEventSourceUri);
    if (!pes)
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_INTERNET_ITEM_NOT_FOUND));
        LeaveCriticalSection(&g_csListEventSource);
        SetLastError( ERROR_INTERNET_ITEM_NOT_FOUND);
        return FALSE;
    }

    //EnterCriticalSection(&pes->cs);

    // Take the props passed in and update the event source's cache of these
    // prop values. Mark each value that changed as modified.
    //
    if (fRet = FUpdateEventSourceWithProps(pes, cProps, rgProps))
    {
        SetEvent(g_hEvtEventSource);        
    }
    else
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc:"
                 "UpdateEventSourceWithPropserror %ld.",
                   HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
        SetLastError( ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }
    //LeaveCriticalSection(&pes->cs);
    LeaveCriticalSection(&g_csListEventSource);
    return fRet;
}

