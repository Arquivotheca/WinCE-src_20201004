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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      faultinf.h
//
// Contents:
//
//      Declaration of the CFaultInfo
//      A local class that allows the passing of fault message information to SoapClient and Soapserver
//
//----------------------------------------------------------------------------------

#ifndef __FAULTINF_H_
#define __FAULTINF_H_

#include "memutil.h"
#include "mssoap.h"

/////////////////////////////////////////////////////////////////////////////
// CFaultInfo
class CFaultInfo
{

protected:
    BOOL    m_bhasFault;
    BOOL    m_bhasISoapError; 
    DWORD   m_dwFaultCodeId;
	CAutoBSTR m_bstrfaultcode;
	CAutoBSTR    m_bstrfaultstring;
	CAutoBSTR    m_bstrfaultactor;
	CAutoBSTR    m_bstrdetail;
	CAutoBSTR    m_bstrNamespace; 
    HRESULT     m_hrError;
    
public:
	CFaultInfo(): m_bhasFault(0), m_dwFaultCodeId(0) { } 
	~CFaultInfo()  { } 
	
	inline BOOL HasFaultInfo() { return m_bhasFault; }
    inline WCHAR * getfaultstring() { return m_bstrfaultstring ; }
    inline WCHAR * getfaultactor() { return m_bstrfaultactor ; }
    inline WCHAR * getdetail() { return m_bstrdetail; }
    inline WCHAR * getNamespace() { return m_bstrNamespace; }
    inline BOOL hasISoapError() { return (m_bhasISoapError); }
    WCHAR * getfaultcode();
           
    HRESULT FaultMsgFromResourceString
    (
        DWORD   dwFaultcodeId,              // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,               // Resource id for the fault string
        WCHAR * pwstrdetail,                // detail part (optional)
        WCHAR * pwstrFaultActor,			// faultactor (optional)
       	va_list	*Arguments					// Arguments to be passed to the resource string
    );   	

    HRESULT FaultMsgFromResourceHr
    (
        DWORD   dwFaultcodeId,              // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,               // Resource id for the fault string
        HRESULT hrErr,                      // HRESULT to return in detail (optional)
        WCHAR * pwstrFaultActor,			// faultactor (optional)
       	va_list	*Arguments		            // Arguments to be passed to the resource string
    );   	

    HRESULT FaultMsgFromGlobalError
        (                                   // checks the global error info for faultinformation
        DWORD   dwFaultcodeId               // SOAP_IDS_SERVER/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        );              
        
    void FillErrorInfo(void);

    void Reset();                
};



#endif //__FAULTINF_H_
