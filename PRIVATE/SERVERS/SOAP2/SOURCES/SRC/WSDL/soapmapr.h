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
// File:    soapmapr.h
//
// Contents:
//
//  Header File
//
//        ISoapMapper Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __SOAPMAPR_H_INCLUDED__
#define __SOAPMAPR_H_INCLUDED__

class CWSDLOperation;
class CWSDLService;

// class for handling variants in the keyed array
class CKeyedVariant : public CKeyedEntry<VARIANT*, CVariant>
{
public:
    CKeyedVariant():CKeyedEntry<VARIANT*,CVariant>()
        {};
    CKeyedVariant(DWORD dwCookie):CKeyedEntry<VARIANT*,CVariant>(dwCookie)
        {};
    DWORD GetKey(void);
    HRESULT SetValue(VARIANT* tValue);
    HRESULT SetValue(VARIANT tValue);
    VARIANT* GetValue(void);
};




class CSoapMapper : public ISoapMapper
{

public:
    CSoapMapper();
    ~CSoapMapper();

public:
	HRESULT STDMETHODCALLTYPE get_messageName(BSTR *pbstrmessageName);
	HRESULT STDMETHODCALLTYPE get_elementName(BSTR *pbstrElementName);
	HRESULT STDMETHODCALLTYPE get_partName(BSTR *pbstrPartName);
	HRESULT STDMETHODCALLTYPE get_isInput(smIsInputEnum *psmIsInput);
	HRESULT STDMETHODCALLTYPE get_encoding(BSTR *pbstrEncoding);
	HRESULT STDMETHODCALLTYPE get_elementType(BSTR *pbstrElementType);
	HRESULT STDMETHODCALLTYPE get_xmlNameSpace(BSTR *pbstrxmlNameSpace);
	HRESULT STDMETHODCALLTYPE get_comValue(VARIANT *pVarOut);
	HRESULT STDMETHODCALLTYPE put_comValue(VARIANT varIn);
	HRESULT STDMETHODCALLTYPE get_callIndex(LONG *plcallIndex);
	HRESULT STDMETHODCALLTYPE get_parameterOrder(LONG *plparaOrder);
	HRESULT STDMETHODCALLTYPE get_variantType(long *pvtType);
	HRESULT STDMETHODCALLTYPE Save(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enSaveStyle, BSTR bstrMesssageNS, long lFlags);
	HRESULT STDMETHODCALLTYPE Load(IXMLDOMNode *pNode, BSTR bstrEncoding, enEncodingStyle enStyle, BSTR bstrMesssageNS);


    DECLARE_INTERFACE_MAP;

public:
    HRESULT SaveValue(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle);
    HRESULT Init(IXMLDOMNode *pServiceNode, ISoapTypeMapperFactory *ptypeFactory, bool fInput, BSTR bstrSchemaNS, CWSDLOperation *pParent);
    HRESULT AddWSMLMetaInfo(IXMLDOMNode *pExecutionNode,
                            CWSDLService *pService,
                            IXMLDOMDocument *pWSDLDom,
                            IXMLDOMDocument *pWSMLDom,
                            bool bLoadOnServer);
    void  Shutdown(void);
    TCHAR*    getVarName(void)
    {
        return(m_bstrVarName);
    }
    bool isBasedOnType(void)
    {
        return (m_basedOnType);
    }
    HRESULT setVarName(TCHAR *pchName)
    {
        if (pchName)
        {
            m_bstrVarName.Clear();
            return m_bstrVarName.Assign(pchName);
        }
        return S_OK;
    }

    void    setInput(smIsInputEnum snInput)
    {
        m_enInput = snInput;
    }
    void    setparameterOrder(long lOrder)
    {
        m_lparameterOrder = lOrder;
    }

    TCHAR * getPartName(void)
    {
        return(m_bstrPartName);
    }

    BSTR    getSchemaURI(void)
    {
        return(m_bstrSchemaURI);
    }
    HRESULT setSchemaURI(BSTR bstrURI)
    {
        HRESULT hr = S_OK;
        if (m_bstrSchemaURI.Len()==0)
        {
            CHK (m_bstrSchemaURI.Assign(bstrURI));
        }

    Cleanup:
        return hr;
    }
    VARTYPE getVTType(void)
    {
        return((VARTYPE) m_vtType);
    }

protected:
      HRESULT    PrepareVarOut(VARIANT *pVarOut);
      HRESULT createTypeMapper(void);

protected:
    CAutoRefc<CWSDLOperation>           m_pOwner;
    CAutoRefc<ISoapTypeMapperFactory>  m_ptypeFactory;
private:
    long            m_vtType;           // VARIANT Type rep for the mapper
    CAutoBSTR       m_bstrPartName;        // name of the part in the schema
    CAutoBSTR       m_bstrVarName;                                 // either part name or element name, depending on style in WSDL,
                                                                        // used for payload creation
    CAutoBSTR       m_bstrSchemaElement;    // element or type name in schema
    CAutoBSTR       m_bstrSchemaURI;                                // URI of schema

    smIsInputEnum    m_enInput;
    long            m_lCallIndex;
    long             m_lparameterOrder;
    CAutoRefc<ISoapTypeMapper> m_pTypeMapper;
    CAutoRefc<IXMLDOMNode> m_pSchemaNode;
    bool             m_basedOnType;
    CKeyedDoubleList<CKeyedVariant, VARIANT*>        m_variantList;      // holds the actual values
};




#endif
