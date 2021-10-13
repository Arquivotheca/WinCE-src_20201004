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
// File:    wsdloper.h
// 
// Contents:
//
//  Header File 
//
//        IWSDLOperation Interface describtion
//        
//        CWSDLMessagePart -> helper class used in an operation
//            to hold the input/output parts of the operation
//    
//-----------------------------------------------------------------------------
#ifndef __WSDLOPER_H_INCLUDED__
#define __WSDLOPER_H_INCLUDED__

#include "wsdlserv.h"

class CWSDLOperation;
class CEnumSoapMappers;
class CSoapMapper;

//////////////////////////////////////////////////////////////////////////////////////////
//
//    one message part for input and one for output
//
//////////////////////////////////////////////////////////////////////////////////////////
class CWSDLMessagePart
{
public:
    CWSDLMessagePart();
    ~CWSDLMessagePart();

public:
    HRESULT Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory, TCHAR *pchPortType, TCHAR *pchMessageType, TCHAR *pchOperationName, CWSDLOperation *pParent );
    bool needsWrapper(void)
    {
        return(m_fWrapperNeeded);
    }
    TCHAR *GetNameSpace(void) 
    {
        return(m_bstrNameSpace);
        
    }
    TCHAR *GetMessageName(void)
    {
        return(m_bstrMessageName);
    }
    
    TCHAR *GetWrapperName(void)
    {
        return(m_fWrapperNeeded ? ((WCHAR*)m_bstrParametersWrapper) : m_bstrMessageName);
        
    }
    
    TCHAR *GetEncodingStyle(void)
    {
        return(m_bstrEncodingStyle);
    }
    bool IsBody(void)
    {
        return(m_fBody);
    }
    void setBody(bool flag)
    {
        m_fBody = flag;
    }
    bool IsLiteral(void)
    {
        return(m_fIsLiteral);
    }

    HRESULT AddOrdered(ISoapMapper * pMapper);
        

    CEnumSoapMappers* getMappers(void)
    {
        return (m_pEnumMappersToSave);
    }

protected:
    bool            m_fIsLiteral;    // if not literal, has to be encoded 
    bool            m_fBody; 
    bool            m_fWrapperNeeded;
    CAutoBSTR       m_bstrEncodingStyle; 
    CAutoBSTR       m_bstrNameSpace;
    CAutoBSTR       m_bstrMessageName;
    CAutoBSTR       m_bstrParts;
    CAutoBSTR       m_bstrParametersWrapper;
    CAutoRefc<CEnumSoapMappers>    m_pEnumMappersToSave;
};


//////////////////////////////////////////////////////////////////////////////////////////
//
//    Operation: holds two message parts and the construct enum for the mappers
//
//////////////////////////////////////////////////////////////////////////////////////////
class CWSDLOperation: public IWSDLOperation
{

public:
    CWSDLOperation();
    ~CWSDLOperation();

public:

    HRESULT STDMETHODCALLTYPE get_name(BSTR *pbstrOperationName);
    const WCHAR * STDMETHODCALLTYPE getNameRef();
    HRESULT STDMETHODCALLTYPE get_soapAction(BSTR *pbstrSoapAction);

    HRESULT STDMETHODCALLTYPE get_objectProgID(BSTR *pbstrobjectProgID);
    HRESULT STDMETHODCALLTYPE get_objectMethod(BSTR *pbstrobjectMethod);
    HRESULT STDMETHODCALLTYPE get_hasHeader( VARIANT_BOOL *pbHeader);
    HRESULT STDMETHODCALLTYPE get_style(BSTR *pbstrStyle);
    HRESULT STDMETHODCALLTYPE get_preferredEncoding(BSTR * pbstrpreferredEncoding); 
    HRESULT STDMETHODCALLTYPE get_documentation(BSTR *bstrServiceDocumentation);    

    HRESULT STDMETHODCALLTYPE GetOperationParts(IEnumSoapMappers **ppIEnumSoapMappers);
    HRESULT STDMETHODCALLTYPE ExecuteOperation(ISoapReader *pISoapReader, ISoapSerializer *pISoapSerializer);

    HRESULT STDMETHODCALLTYPE Save(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput);
    HRESULT STDMETHODCALLTYPE SaveHeaders(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput);
    HRESULT STDMETHODCALLTYPE Load(ISoapReader *pISoapReader, VARIANT_BOOL vbInput);
    
    DECLARE_INTERFACE_MAP;


    
public:
    HRESULT Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory, 
                              TCHAR *pchPortType, BOOL fDocumentMode, 
                              BSTR bstrpreferredEncoding, BOOL bCreateHrefs);
    HRESULT    AddMapper(CSoapMapper *pSoapMapper, CWSDLMessagePart *pMessage, bool fInput, long lOrder);
    HRESULT    AddWSMLMetaInfo(IXMLDOMNode *pServiceNode, 
                            CWSDLService *pWSDLService, 
                            IXMLDOMDocument *pWSDLDom, 
                            IXMLDOMDocument *pWSMLDom, 
                            bool bLoadOnServer);
    HRESULT Save(ISoapSerializer *pISoapSerializer, bool fInput);

    void    Shutdown(void);
    HRESULT verifyPartName(smIsInputEnum snInput, TCHAR *pchPartName);

    // helpers that the soapmapper uses to forward to...
    HRESULT get_messageName(BSTR *pbstrMessageName, bool fInput);
    TCHAR*  getEncoding(bool fInput);
    BOOL    compareSoapAction(BSTR bstrSoapAction);
    BOOL    isDocumentMode(void)
    {
        return(m_fDocumentMode);
    }

    TCHAR * getMessageNameSpace(bool fInput);
    
    
protected:
    HRESULT setupInvokeError(IDispatch *pDispatch, EXCEPINFO *pxcepInfo, unsigned int uArgErr, HRESULT hrErrorCode);
    HRESULT prepareOrderString(void);
    HRESULT findBodyNode(IXMLDOMNode *pRootNode, IXMLDOMNode **ppNode);


private:
    CAutoBSTR                   m_bstrName;
    CAutoBSTR                   m_bstrSoapAction;
    CAutoBSTR                   m_bstrMethodName;
    
    CAutoBSTR                   m_bstrpreferedEncoding;
    CAutoBSTR                   m_bstrparameterOrder;
    CAutoBSTR                   m_bstrDocumentation;
    DISPID                        m_dispID; 
    BOOL                        m_bCachable;
    BOOL                        m_fDocumentMode;
    BOOL                        m_fCreateHrefs;
    CWSDLMessagePart           *m_pWSDLInputMessage;
    CWSDLMessagePart           *m_pWSDLOutputMessage;
    CAutoRefc<CEnumSoapMappers> m_pEnumSoapMappers;
    CAutoRefc<CDispatchHolder>  m_pDispatchHolder;    
    CAutoRefc<CDispatchHolder>  m_pHeaderHandler;    
    TCHAR                     **m_ppchparameterOrder;
    long                        m_cbparaOrder; 
};



#endif
