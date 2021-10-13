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
//      client.h
//
// Contents:
//
//      Declarations for the SoapClient
//
//----------------------------------------------------------------------------------

#ifndef __CLIENT_H_
#define __CLIENT_H_


/////////////////////////////////////////////////////////////////////////////
// CClientFaultInfo
// A local class that allows the passing of fault message information to Soapserver

#ifdef __cplusplus
extern "C" {
#endif
extern const IID GUID_NULL;
#ifdef __cplusplus
}
#endif


class CClientFaultInfo : public CFaultInfo
{
protected:
    CComPointer<IErrorInfo> m_pErrorInfo;

public:
    CClientFaultInfo() {}
    ~CClientFaultInfo() {}

    void SetFields(BSTR bstrfaultcode, BSTR bstrfaultstring,
                   BSTR bstrfaultactor, BSTR bstrdetail);

    void SetErrorInfo(IErrorInfo *pErrorInfo);
    void PublishErrorInfo(void);

    void Reset(void)
    {
        SetErrorInfo(0);
        CFaultInfo::Reset();
        globalResetErrors();
    }

};

inline void CClientFaultInfo::SetErrorInfo(IErrorInfo *pErrorInfo)
{
    m_pErrorInfo = pErrorInfo;
}

class CAbstrNames
{
public:
    CAbstrNames()
    : m_abstrNames(NULL),
      m_lSize(0),
      m_lFilled(0)
    {}


    ~CAbstrNames()
    {
	    Clear();
    }

    HRESULT SetSize(long size);
	long GetSize(void) const;
	HRESULT AddName(WCHAR * name);
	void Sort(void);
	HRESULT FindName(WCHAR * name, long *lResult) const;
	WCHAR *getName(long lOffset) const;
	void Clear(void);

private:
	WCHAR **m_abstrNames;
	long m_lSize;
	long m_lFilled;
};



////////////////////////////////////////////////////////////////////////////
//
//  class CClientThreadStore: holds to Thread dependant client information
//
//
//////////////////////////////////////////////////////////////////////////////
class CClientThreadStore : public IUnknown
{
public:
    void setSerializer(ISoapSerializer * pSerializer)
    {
        m_pISoapSer.Clear();
        assign(&m_pISoapSer, pSerializer);
    }

    HRESULT RetrieveConnector(ISoapConnector **pConnector);
    HRESULT ReturnConnector(ISoapConnector *pConnector);

    ISoapSerializer * getSerializer(void)
    {
        return(m_pISoapSer);
    }

    CClientFaultInfo * getFaultInfo(void)
    {
        return(&m_SoapFaultInfo);
    }

	DECLARE_INTERFACE_MAP;

protected:
    CSet< CComPointer<ISoapConnector>, CComPointer<ISoapConnector> > m_ConnectorSet;
    CAutoRefc<ISoapSerializer>          m_pISoapSer;
	CClientFaultInfo                    m_SoapFaultInfo;
};


//////////////////////////////////////////////////////////////////////////////


#define CThreadInfo       CKeyedObj<CClientThreadStore *, CClientThreadStore *>

class CPropertyEntry : public CDoubleListEntry
{
private:
    CAutoBSTR m_Name;
    CVariant  m_Value;

public:
    BSTR GetName()                   { return m_Name;   };
    HRESULT SetName(BSTR pName)      { m_Name.Assign(pName, FALSE); return S_OK; };

    VARIANT &GetValue()              { return m_Value;  };
    HRESULT SetValue(VARIANT &value) { return m_Value.Assign(&value); };
};


/////////////////////////////////////////////////////////////////////////////
// CSoapClient
class CSoapClient :
    public ISupportErrorInfo,
    public CDispatchImpl<ISOAPClient>
{

private:
	CTypeInfoMap    *m_pTypeInfoMap;
    CAbstrNames     m_abstrNames;
#ifndef UNDER_CE
    bool            m_bUseServerHTTPRequest;
#endif 
    BOOL            m_bflInitSuccess;
    CLSID           m_CLSIDConnector;
    BSTR            m_bstrCLSIDConnector;
    CAutoBSTR       m_bstrpreferredEncoding;

    CTypedDoubleList<CPropertyEntry> m_PropertyBag;
    CCritSect                        m_PropertyCS;

    // Not sure if we need to keep all of these references.
    // If unnecessary, we may simply remove them later.
    CAutoRefc<IWSDLReader>           m_pIWSDLReader;
    CAutoRefc<IWSDLService>          m_pIWSDLService;
    CAutoRefc<ISoapConnectorFactory> m_pISoapConnFact;
    CAutoRefc<IWSDLPort>             m_pIWSDLPort;   // Port interface describing service
    CAutoRefc<IEnumWSDLOperations>   m_pIEnumWSDLOps;  // Operations description
    CAutoRefc<IHeaderHandler>        m_pHeaderHandler;
  	CAutoRefc<ITypeInfo>             m_pSoapClientTypeInfo;    // ITypeInfo ptr

    // all variables above are initialized in the .MSSOAPINIT call. The only thread 
    // dependant data is kept in thread aware storage...
    CKeyedDoubleList<CThreadInfo, CClientThreadStore *>		m_threadInfo;

public:
	CSoapClient():
	    m_pTypeInfoMap(0),
	    m_bflInitSuccess(FALSE),
	    m_bstrCLSIDConnector(0)
	{
        m_CLSIDConnector = CLSID_NULL;
#ifndef UNDER_CE
        m_bUseServerHTTPRequest = false;
#endif 
        m_PropertyCS.Initialize();
	}

	~CSoapClient()
	{
	    ::SysFreeString(m_bstrCLSIDConnector);
	    Reset();
	    m_PropertyCS.Delete();
	}


// ISOAPClient
public:
    STDMETHOD(mssoapinit) (/* [in] */ BSTR bstrWSDLFile,
					/* [in] */ BSTR bstrServiceName,
					/* [in] */ BSTR bstrPort,
					/* [in] */ BSTR bstrWSMLFile);

    STDMETHOD(get_faultcode)(
				/* [out, retval] */ BSTR * pbstrFaultcode);
    STDMETHOD(get_faultstring)(
				/* [out, retval] */ BSTR * pbstrFaultstring);
    STDMETHOD(get_faultactor)(
				/* [out, retval] */ BSTR * pbstrActor);
    STDMETHOD(get_detail)(
				/* [out, retval] */ BSTR * pbstrDetail);
    STDMETHOD(get_ConnectorProperty)(/* [in] */ BSTR PropertyName,
                    /* [out, retval] */ VARIANT *pPropertyValue);
    STDMETHOD(put_ConnectorProperty)(/* [in] */ BSTR PropertyName,
                        /* [in] */ VARIANT PropertyValue);
    STDMETHOD(get_ClientProperty)(/* [in] */ BSTR PropertyName,
                    /* [out, retval] */ VARIANT *pPropertyValue);
    STDMETHOD(put_ClientProperty)(/* [in] */ BSTR PropertyName,
                        /* [in] */ VARIANT PropertyValue);

    STDMETHOD(putref_HeaderHandler)(/* [in] */ IDispatch *pHeaderHandler);


    STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId);
	STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
					  VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);

// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

private:
	HRESULT moveResult(ISoapMapper *pMapper, VARIANT *pVariant);
	HRESULT GetTI1(LCID lcid);
	HRESULT LoadNameCache(ITypeInfo* pTypeInfo);
	HRESULT EnsureTI(LCID lcid);
	HRESULT InvokeUnknownMethod(DISPID dispID, DISPPARAMS * pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);
    HRESULT ConstructSoapRequestMessage(
                    DISPPARAMS * pdispparams,
                    ISoapSerializer *pISoapSer,
                    IWSDLOperation * pIWSDLOp,
                    ISequentialStream * pIStream);
    HRESULT ProcessSoapResponseMessage(
                    DISPPARAMS * pdispparams,
					VARIANT * pvarResult,
					EXCEPINFO * pexcepinfo,
					ISoapReader * pISoapReader,
                    IWSDLOperation * pIWSDLOp);
    HRESULT VariantCopyToByRef(VARIANT *pvarDest, VARIANT *pvarSrc, LCID lcid);
    HRESULT CheckForFault(
					ISoapReader * pISoapReader,
                    BOOL *pbHasFault);

    HRESULT VariantGetParam(
                    DISPPARAMS * pdispparams,
                    UINT position,
                    VARIANT * pvarResult);
    HRESULT ArrayGetParam(
                    DISPPARAMS * pdispparams,
                    UINT position,
                    VARIANT * pvarResult);
    HRESULT RetrieveThreadStore(CClientThreadStore **ppThreadStore, BOOL fCreateConnector);
    HRESULT RetrieveFaultInfo(CClientFaultInfo **ppFaultInfo);
    HRESULT RetrieveConnector(ISoapConnector **pConnector);
    HRESULT ReturnConnector(ISoapConnector *pConnector);
    HRESULT StoreProperty(BSTR bstrName, VARIANT &value);
    HRESULT SetProperties(ISoapConnector *pConnector);
    void ClearErrors(void);
    void Reset(void);

	DECLARE_INTERFACE_MAP;
};

#endif //__CLIENT_H_
