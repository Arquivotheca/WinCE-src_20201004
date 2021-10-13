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
// Module Name:    FaultInf.cpp
// 
// Contents:
//
//    Implementation of CFaultInfo class
//    CFaultInfo -- A Helper class for constructing SOAP Fault messages
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "faultinf.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo::FaultMsgFromResourceString( DWORD   dwFaultCodeId,DWORD   dwResourceId,
//                             WCHAR * pwstrdetail, WCHAR * pwstrFaultActor,va_list	*Arguments)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFaultInfo::FaultMsgFromResourceString
    (
        DWORD   dwFaultCodeId,  // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,   // Resource id for the fault string
        WCHAR * pwstrdetail,    // detail part (optional)
        WCHAR * pwstrFaultActor,// faultactor (optional)
       	va_list	*Arguments		// Arguments to be passed to the resource string
    )
{
#ifndef UNDER_CE
    WCHAR       *tempbuf[ONEK+1];
#else
    WCHAR       tempbuf[ONEK+1];
#endif 
    DWORD       cchstr;
    HRESULT     hr; 
    
    if (m_bhasFault)
    {
        // We already have a fault message here
        return S_OK;
    }
    
    // Load the fault description from resource DLL
#ifndef UNDER_CE
    cchstr = GetResourceString(dwResourceId, (WCHAR *)tempbuf, ONEK, Arguments);
#else
    cchstr = GetResourceString(dwResourceId, tempbuf, ONEK, Arguments);
#endif 
    if (!cchstr)
        return S_OK;        // Error not found
        
    ASSERT((dwFaultCodeId == SOAP_IDS_SERVER) ||
            (dwFaultCodeId == SOAP_IDS_CLIENT) ||
            (dwFaultCodeId == SOAP_IDS_MUSTUNDERSTAND) ||
            (dwFaultCodeId == SOAP_IDS_VERSIONMISMATCH) ) ;
            
    // Fill in the CFaultInfo members
#ifndef UNDER_CE
	CHK(m_bstrfaultstring.Assign((BSTR)tempbuf));;
#else
    CHK(m_bstrfaultstring.Assign(tempbuf));;
#endif 

	if (pwstrFaultActor)
	{
	    CHK(m_bstrfaultactor.Assign(pwstrFaultActor));
   	}    
	CHK(m_bstrdetail.Assign(pwstrdetail));
    m_dwFaultCodeId = dwFaultCodeId;
    m_bhasFault = TRUE;
Cleanup:
    return hr;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo::FaultMsgFromResourceHr(DWORD   dwFaultcodeId, DWORD   dwResourceId,
//                          HRESULT hrErr, WCHAR * pwstrFaultActor, va_list	*Arguments)
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFaultInfo::FaultMsgFromResourceHr
    (
        DWORD   dwFaultcodeId,              // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,               // Resource id for the fault string
        HRESULT hrErr,                      // HRESULT to return in detail (optional)
        WCHAR * pwstrFaultActor,            // faultactor (optional)
       	va_list	*Arguments		            // Arguments to be passed to the resource string
    )
{
    WCHAR   *pwchDetail[50];
    
    // first see if we have  a global error information

    
    if (m_bhasFault)
    {
        // We already have a fault message here
        return S_OK;
    }
    
    m_hrError = hrErr; 
    
    if (FAILED(FaultMsgFromGlobalError(dwFaultcodeId)))
    {

        if (dwResourceId == 0)
            dwResourceId = HrToMsgId(hrErr);
        wcscpy((WCHAR *)pwchDetail, (WCHAR *)g_pwstrOpenHresult);
        _itow((int)hrErr, (WCHAR *)pwchDetail+wcslen((WCHAR *)pwchDetail), 16);
        wcscat((WCHAR *)pwchDetail, (WCHAR *)g_pwstrCloseHresult);
        ASSERT(wcslen((WCHAR *)pwchDetail) <= 50);
        return (FaultMsgFromResourceString(
                    dwFaultcodeId, 
                    dwResourceId, 
                    (WCHAR *)pwchDetail,
                    pwstrFaultActor,
                    Arguments));
                
    }
    return(S_OK);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo::getfaultcode()
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
WCHAR * CFaultInfo::getfaultcode()
{
    WCHAR   * ptr;
    if (m_bstrfaultcode.isEmpty() && m_dwFaultCodeId)
    {
        switch (m_dwFaultCodeId)
        {
            case SOAP_IDS_SERVER:
                ptr = (WCHAR *)g_pwstrServer;
                break;
            case SOAP_IDS_MUSTUNDERSTAND:
                ptr = (WCHAR *)g_pwstrCMustUnderstand;
                break;
            case SOAP_IDS_VERSIONMISMATCH:
                ptr = (WCHAR *)g_pwstrVersionMismatch;
                break;
            case SOAP_IDS_CLIENT:
                ptr = (WCHAR*)g_pwstrClient;
                break;                
            default:    
                ptr = (WCHAR *)g_pwstrServer;
                break;
        }                
        m_bstrfaultcode.Assign(ptr);
    }
    
    return (m_bstrfaultcode);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo:::Reset()
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFaultInfo::Reset()
{
    m_bhasFault = FALSE;
    m_bhasISoapError = FALSE;
    m_dwFaultCodeId = 0;
	m_bstrfaultstring.Clear();
	m_bstrfaultcode.Clear();
	m_bstrfaultactor.Clear();
	m_bstrdetail.Clear();

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo::FaultMsgFromGlobalError(DWORD   dwFaultCodeId)
//
//  parameters:
//
//  description:
//      checks our global error info for the thread
//      if exists, takes the parameters from there
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFaultInfo::FaultMsgFromGlobalError
    (
        DWORD   dwFaultCodeId              // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
    )
        
{
    HRESULT hr = E_FAIL;
    CErrorListEntry *pErrorList;
    
    
    ASSERT((dwFaultCodeId == SOAP_IDS_SERVER) ||
            (dwFaultCodeId == SOAP_IDS_CLIENT) ||
            (dwFaultCodeId == SOAP_IDS_MUSTUNDERSTAND) ||
            (dwFaultCodeId == SOAP_IDS_VERSIONMISMATCH) ) ;       

    if (m_bhasFault)
    {
        // We already have a fault message here
        return S_OK;
    }
 
    globalGetError(&pErrorList);

    
    if (pErrorList && pErrorList->Size() > 0)
    {
        m_dwFaultCodeId = dwFaultCodeId;    
        // take the generic faultcodes
        CHK(pErrorList->getFaultCode(&m_bstrfaultcode));
        if (hr==S_FALSE)
        {
            getfaultcode();
        }
        
        CHK(pErrorList->getFaultActor(&m_bstrfaultactor));
        CHK(pErrorList->getFaultDetailBSTR(&m_bstrdetail));
        CHK(pErrorList->getFaultString(&m_bstrfaultstring));
        CHK(pErrorList->getFaultNamespace(&m_bstrNamespace));
        // as we picked it up, reset the list.
        FillErrorInfo();    
        m_bhasFault = TRUE;     
        m_bhasISoapError = pErrorList->isISoapError();
        hr = S_OK;

    }

Cleanup:
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFaultInfo::FillErrorInfo(void)
//
//  parameters:
//
//  description:
//      set's up standard ole automation error object
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaultInfo::FillErrorInfo(void)
{
	ICreateErrorInfo  	*pcerrinfo;
	IErrorInfo		  	*perr; 

	HRESULT hr = CreateErrorInfo(&pcerrinfo);

	if (SUCCEEDED(hr))
	{
	    hr = pcerrinfo->SetDescription(m_bstrdetail);
		if (FAILED(hr))
			goto Cleanup;
		hr = pcerrinfo->SetSource(m_bstrfaultcode);
		if (SUCCEEDED(hr))
		{
			hr = pcerrinfo->QueryInterface(IID_IErrorInfo, (LPVOID FAR*) &perr);
			if (SUCCEEDED(hr))
		    {
		        SetErrorInfo(0, perr);
		        perr->Release();
		    }
		}
	Cleanup:
	    pcerrinfo->Release();
	}

	return;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
