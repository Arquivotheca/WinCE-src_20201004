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
//      server.h
//
// Contents:
//
//      Declaration of the CSoapServer class
//
//-----------------------------------------------------------------------------

#ifndef __SERVER_H_
#define __SERVER_H_

#include "faultinf.h"

/////////////////////////////////////////////////////////////////////////////
// CServerFaultInfo
// A local class that allows the passing of fault message information to Soapserver

typedef enum 
{
    enSerializerInit = 0,
    enSerializerEnvelope = 1,
    enSerializerBody = 2,
} enSerializerState; 

class CServerFaultInfo : public CFaultInfo
{

private:

    CAutoRefc<IWSDLPort>    m_pIWSDLPort;

public:
    CServerFaultInfo() { } 
    ~CServerFaultInfo()  { } 
    
    HRESULT writeDetailTree(ISoapSerializer *pSerializer);
    HRESULT saveFaultDetail(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList);
    HRESULT saveErrorInfo(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList);    
    void    getReturnCodeAsString(TCHAR *pchBuffer, CErrorDescription *pError);
    
    void SetCurrentPort(IWSDLPort *pPort)
    {
        m_pIWSDLPort.Clear();
        m_pIWSDLPort = pPort;
        m_pIWSDLPort->AddRef();
    }
        
    HRESULT FaultMsgFromRes
    (
        DWORD   dwFaultcodeId,  // SOAP_IDS_SERVER(default)/SOAP_IDS_CLIENT/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,   // Resource id for the fault string
        WCHAR * pwstrdetail,    // detail part (optional)
       	va_list	*Arguments		// Arguments to be passed to the resource string
    );   	

    HRESULT FaultMsgFromResHr
    (
        DWORD   dwFaultcodeId,  // SOAP_IDS_SERVER(default)/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,   // Resource id for the fault string
        HRESULT hrErr,          // HRESULT to return in detail (optional)
       	va_list	*Arguments		// Arguments to be passed to the resource string
    );   	

};



/////////////////////////////////////////////////////////////////////////////
// CSoapServer

class CSoapServer: 
    public ISupportErrorInfo,
    public CDispatchImpl<ISOAPServer>
{

private:

    CAutoRefc<IWSDLReader>      m_pIWSDLReader;
    enSerializerState           m_serializerState; 

public:
    CSoapServer()
    {
    }
    ~CSoapServer()
    {
    }

public:

    //
    // ISupportsErrorInfo
    //
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);
	
    //
    // ISOAPServer
    //
    STDMETHOD(Init)(BSTR pUrlWSDLFile, BSTR bstrWSMLFileSpec);
    STDMETHOD(SoapInvoke)(VARIANT varInput, IUnknown *pStreamOut, BSTR bstrSoapAction);

    STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId);
    STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
					  VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);

    // Internal method used by other MSSOAP components to add fault 
    // information to SOAPServer.
    STDMETHODIMP GetFaultInfoObject(CFaultInfo *pcFaultInfo);	

private:
    STDMETHODIMP ConstructGenericSoapErrorMessage(CServerFaultInfo   *pSoapFaultInfo,
                ISoapSerializer *pISoapSerializer, 
                DWORD   dwFaultcodeId,  // (Optional)faultcode
                DWORD   MsgId,          // Message ID 
                HRESULT hrRes,          // HRESULT code
                WCHAR * pwstrDetail,    // (optional) detail element
                va_list*Arguments);     // Arguments for the string resource

    STDMETHODIMP ConstructSoapResponse(CServerFaultInfo   *pSoapFaultInfo, ISoapSerializer *pISoapSerializer, IWSDLOperation *pWSDLOperation,
                        IUnknown *pStreamOut); 

    DECLARE_INTERFACE_MAP;
};


#endif //__SERVER_H_
