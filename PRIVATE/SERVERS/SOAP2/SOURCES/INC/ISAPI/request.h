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
// Module Name:    request.h
// 
// Contents:
//
//    ISAPI Extension that services SOAP packages.
//
//-----------------------------------------------------------------------------

#ifndef REQUEST_H_INCLUDED
#define REQUEST_H_INCLUDED

struct _THREADINFO;

class CRequest
{

public:
    CRequest(EXTENSION_CONTROL_BLOCK * pECB, 
        struct _THREADINFO * pThreadInfo) : 
        m_pIStreamIn(pECB), m_pIStreamOut(pECB), m_pECB(pECB), 
        m_pThreadInfo(pThreadInfo), m_pCahcedObj(NULL),
        m_pszWSDLFilePath(NULL), m_pszWSMLFilePath(NULL)
    {  m_pIStreamIn.AddRef();		// Keep refcount always 1
       m_pIStreamOut.AddRef(); };
    ~CRequest() {  } ;

    HRESULT Validate();
    HRESULT ProcessParams();
    HRESULT ProcessPathInfo();

    HRESULT ExecuteRequest();
    HRESULT ProcessGet(BOOL isHead);

    BSTR GetHeaderParam(char *pstrHdr);
    void  WriteFaultMessage(HRESULT hr, BSTR bstrActor);
    void FlushContents();
    void SetErrorCode(ERROR_CODE errcode);

private:

    EXTENSION_CONTROL_BLOCK * m_pECB;
    struct _THREADINFO      * m_pThreadInfo;

    CThdCachedObj *         m_pCahcedObj;
    CAutoRg<char>           m_pszWSDLFilePath;
    CAutoRg<char>           m_pszWSMLFilePath;
    CInputStream            m_pIStreamIn;     
    COutputStream           m_pIStreamOut;    
    
    HRESULT InitObjectForCache(CThdCachedObj ** ppCachedObj);
};

#endif  //REQUEST_H_INCLUDED

