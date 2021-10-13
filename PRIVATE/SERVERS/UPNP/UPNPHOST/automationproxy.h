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
//  File:       A U T O M A T I O N P R O X Y . H 
//
//  Contents:   Header file for the Automation Proxy class. 
//
//  Notes:      
//
//  /09/25
//
//----------------------------------------------------------------------------

#ifndef __AUTOMATIONPROXY_H
#define __AUTOMATIONPROXY_H
#pragma once

#include "string.hxx"
#include "vector.hxx"
#include "sax.h"
#include "combook.h"
#include "host.h"

#define ttidAutomationProxy 1

// UPNP_STATE_VARIABLE
typedef struct tagUPNP_STATE_VARIABLE
{
    tagUPNP_STATE_VARIABLE(LPCWSTR pwszName, LPCWSTR pwszType, bool bEvented)
		: strName(pwszName),
		  strDataType(pwszType),
		  bEvented(bEvented)
	{};
	
	ce::wstring strName;
    ce::wstring	strDataType;
    DISPID		dispid;
    bool        bEvented;
} UPNP_STATE_VARIABLE, StateVar;


// UPNP_ARGUMENT
typedef struct tagUPNP_ARGUMENT
{
    tagUPNP_ARGUMENT(LPCWSTR pwszName, LPCWSTR pwszStateVar, bool bRetval)
        : strName(pwszName),
          strStateVar(pwszStateVar),
          bRetval(bRetval),
          pusvRelated(NULL)
	{};

	void BindStateVar(ce::vector<StateVar>::const_iterator itBegin, ce::vector<StateVar>::const_iterator itEnd);

	ce::wstring					strName;
	ce::wstring					strStateVar;
	bool						bRetval;
    const UPNP_STATE_VARIABLE	*pusvRelated;
} UPNP_ARGUMENT, Argument;


// UPNP_ACTION
typedef struct tagUPNP_ACTION
{
    tagUPNP_ACTION(LPCWSTR pwszName, LPCWSTR)
		: strName(pwszName),
		  dispid(0),
		  cInArgs(0),
		  cOutArgs(0),
		  bRetVal(false)
	{};

	void AddInArgument(const Argument& arg)
        {rgInArgs.push_back(arg); }

    void AddOutArgument(const Argument& arg)
        {rgOutArgs.push_back(arg); }

	void BindArgumentsToStateVars(ce::vector<StateVar>::const_iterator itBegin, ce::vector<StateVar>::const_iterator itEnd);

	ce::wstring					strName;
    DISPID						dispid;
    DWORD						cInArgs;
    ce::vector<UPNP_ARGUMENT>	rgInArgs;
    DWORD						cOutArgs;
    ce::vector<UPNP_ARGUMENT>   rgOutArgs;
    bool						bRetVal;
} UPNP_ACTION, Action;


// CUPnPAutomationProxy
class CUPnPAutomationProxy :
    public IUPnPAutomationProxy,
    public IUPnPServiceDescriptionInfo,
	ce::SAXContentHandler
{
public:

    BEGIN_INTERFACE_TABLE(CUPnPAutomationProxy)
        IMPLEMENTS_INTERFACE(IUPnPAutomationProxy)
        IMPLEMENTS_INTERFACE(IUPnPServiceDescriptionInfo)
    END_INTERFACE_TABLE()

	// implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(CUPnPAutomationProxy)

	// implement static CreateInstance(IUnknown *, REFIID, void**)
    IMPLEMENT_CREATE_INSTANCE(CUPnPAutomationProxy)


    CUPnPAutomationProxy();
    ~CUPnPAutomationProxy();


// IUPnPAutomationProxy
public:
    STDMETHOD(Initialize)(
        /*[in]*/   IUnknown    * punkSvcObject,
        /*[in]*/   LPWSTR      pszSvcDescription);


    STDMETHOD(QueryStateVariablesByDispIds)(
        /*[in]*/   DWORD       cDispIds,
        /*[in]*/   DISPID      * rgDispIds,
        /*[out]*/  DWORD       * pcVariables,
        /*[out]*/  LPWSTR      ** prgszVariableNames,
        /*[out]*/  VARIANT     ** prgvarVariableValues,
        /*[out]*/  LPWSTR      ** prgszVariableDataTypes);


    STDMETHOD(ExecuteRequest)(
        /*[in]*/   UPNP_CONTROL_REQUEST    * pucreq,
        /*[out]*/  UPNP_CONTROL_RESPONSE   * pucresp);

// IUPnPServiceDescriptionInfo
public:

    STDMETHOD(GetVariableType)(
        /*[in]*/   LPWSTR      pszVarName,
        /*[out]*/  BSTR        * pbstrType);


    STDMETHOD(GetOutputArgumentNamesAndTypes)(
        /*[in]*/   LPWSTR      pszActionName,
        /*[out]*/  DWORD       * pcOutArguments,
        /*[out]*/  BSTR        ** prgbstrNames,
        /*[out]*/  BSTR        ** prgbstrTypes);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);

// Helper functions
private:
    VOID FreeControlResponse(UPNP_CONTROL_RESPONSE * pucresp);
	HRESULT Parse(LPWSTR pwszSvcDescription);
    UPNP_STATE_VARIABLE *LookupVariableByDispID(DISPID dispid);
    UPNP_STATE_VARIABLE *LookupVariableByName(LPCWSTR pwszName);
    HRESULT StringizeVariant(VARIANT *pVar, LPCWSTR pwszType);
	HRESULT Decode(LPCWSTR pwszValue, LPCWSTR pwszType, VARIANT* pvarValue) const;
    UPNP_ACTION *LookupActionByName(LPCWSTR pwszName);
    HRESULT HrBuildFaultResponse(UPNP_CONTROL_RESPONSE_DATA * pucrd,
                                 LPCWSTR                    pcszFaultCode,
                                 LPCWSTR                    pcszFaultString,
                                 LPCWSTR                    pcszUPnPErrorCode,
                                 LPCWSTR                    pcszUPnPErrorString);
    HRESULT HrVariantInitForXMLType(VARIANT * pvar,
                                    LPCWSTR pcszDataTypeString);

    HRESULT HrInvokeAction(UPNP_CONTROL_REQUEST    * pucreq,
                           UPNP_CONTROL_RESPONSE   * pucresp);

    HRESULT HrQueryStateVariable(UPNP_CONTROL_REQUEST    * pucreq,
                                 UPNP_CONTROL_RESPONSE   * pucresp);

// State
private:
    BOOL					m_fInitialized;
    IDispatch				*m_pdispService;
    DWORD					m_cVariables;
    ce::vector<StateVar>	m_StateVars;	// name used by parsing code copied from ServiceImpl
	ce::vector<StateVar>&	m_rgVariables;	// old name used by CUPnPAutomationProxy code
    DWORD					m_cActions;
    ce::vector<Action>      m_Actions;		// name used by parsing code copied from ServiceImpl
	ce::vector<Action>&     m_rgActions;	// old name used by CUPnPAutomationProxy code

	// members used during parsing
    LPCWSTR					m_strType;		// not used, added to make parsing code from ServiceImpl happy
	ce::wstring				m_strActionName;
    ce::wstring				m_strArgumentDirection;
    ce::wstring				m_strArgumentName;
    ce::wstring				m_strStateVarName;
    ce::wstring				m_strStateVarType;
    bool					m_bRetval;
    bool					m_bSendEvents;
    UPNP_ACTION*			m_pCurrentAction;
};

#endif //!__AUTOMATIONPROXY_H

