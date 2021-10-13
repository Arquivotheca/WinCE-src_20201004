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
// File:    wsdlserv.h
//
// Contents:
//
//  Header File
//
//        IWSDLService Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __WSDLSERV_H_INCLUDED__
#define __WSDLSERV_H_INCLUDED__

class CWSDLPort;
class CEnumWSDLPorts;
class CWSDLService;

class CDispatchHolder : public IUnknown
{
friend class CWSDLService;
public:
    CDispatchHolder()
    {
        m_bCachable=false;
    }

    HRESULT Init(void);

    HRESULT GetDispatchPointer(IDispatch **ppDispatch);
    HRESULT GetHeaderPointer(IHeaderHandler **ppDispatch);
    HRESULT GetProgID(BSTR *pbstrobjectProgID);

    TCHAR * getProgID(void)
    {
        return m_bstrProgID;;
    }



    DECLARE_INTERFACE_MAP;

private:
    BOOL                    m_bCachable;
    GIP(IDispatch)           m_gipDispatch;
    CAutoBSTR               m_bstrProgID;
    CAutoBSTR               m_bstrID;
    CLSID                   m_clsid;
};





class CWSDLService : public IWSDLService
{

public:
    CWSDLService();
    ~CWSDLService();

public:
    HRESULT STDMETHODCALLTYPE get_name(BSTR *bstrName);
    HRESULT STDMETHODCALLTYPE get_documentation(BSTR *bstrServiceDocumentation);
    HRESULT STDMETHODCALLTYPE GetSoapPorts(IEnumWSDLPorts **ppIWSDLPorts);

    DECLARE_INTERFACE_MAP;

public:
    HRESULT Init(IXMLDOMNode *pServiceNode, ISoapTypeMapperFactory *ptypeFactory);
    HRESULT    AddWSMLMetaInfo(IXMLDOMNode *pServiceNode, IXMLDOMDocument *pWSDLDom, IXMLDOMDocument *pWSMLDom, bool bLoadOnServer);
    CDispatchHolder * GetDispatchHolder(TCHAR *pchObjectID);
    CDispatchHolder * GetHeaderHandler(TCHAR *pchHeaderHandlerID);

    HRESULT registerCustomMappers(IXMLDOMNode *pServiceNode);
    BOOL    doCustomMappersExist(void)
    {
        return m_bCustomMappersCreated;
    }

    TCHAR * getName(void)
    {
        return(m_bstrName);
    }

protected:

private:
    CAutoBSTR       m_bstrName;
    CAutoBSTR         m_bstrDocumentation;
    CAutoBSTR        m_bstrHeaderHandler;
    CEnumWSDLPorts     *m_pPortsList;
    CDispatchHolder    **m_pDispatchHolders;
    long            m_cbDispatchHolders;
    bool            m_bCustomMappersCreated;
    CAutoRefc<ISoapTypeMapperFactory> m_pTypeFactory;

};

#endif
