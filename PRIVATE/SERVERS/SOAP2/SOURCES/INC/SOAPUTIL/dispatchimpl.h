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
//      dispatchimpl.h
//
// Contents:
//
//      IDispatch implementation
//
//----------------------------------------------------------------------------------

#ifndef __DISPATCHIMPL_H_INCLUDED__
#define __DISPATCHIMPL_H_INCLUDED__

#define TYPEINFOIDS(x,y)    const GUID* CDispatchImpl<x>::sm_pguid = &IID_##x; \
                            CTypeInfo CDispatchImpl<x>::sm_typeinfo;\
                            const GUID* CDispatchImpl<x>::sm_plibid = &LIBID_##y;
                            
#define TYPEINFOIDSFORCLASS(c,x,y)    const GUID* CDispatchImpl<c>::sm_pguid = &IID_##x; \
                            CTypeInfo CDispatchImpl<c>::sm_typeinfo;\
                            const GUID* CDispatchImpl<c>::sm_plibid = &LIBID_##y;
                            
#ifndef VARMEMBER
#define VARMEMBER(pv,m)    ((pv)->##m)
#endif
#ifndef VARTYPEMAP
#define VARTYPEMAP(vt)    (vt&~VT_BYREF)
#endif

class CTypeInfo
{
public:
    CTypeInfo();    
    static void ReleaseAll();
    HRESULT LoadTypeInfo( LCID lcid, const GUID *pguid );
    operator ITypeInfo *() { return m_pInfo; }
    ITypeInfo * operator ->() { return m_pInfo; }
    const GUID *sm_plibid;
private:
    ITypeInfo *m_pInfo;
    CTypeInfo *m_pNext;
    static CTypeInfo *sm_pRoot;
};

struct INVOKE_METHOD
        {
            WCHAR *pwszMethod;
            DISPID dispid;
        };
struct INVOKE_ARG
        {
            VARIANT vArg;
            bool fClear;
            bool fMissing;
        };

template <class T>
class __declspec(novtable) CDispatchImpl : public T
{
public:

    CDispatchImpl()
    {
        sm_typeinfo.sm_plibid = sm_plibid;
    }

    ~CDispatchImpl()
    {
    }

    STDMETHOD(GetTypeInfoCount)(UINT* pctinfo)
    {
        if( !pctinfo )
            return E_POINTER;

        *pctinfo = 1;
        return S_OK;
    }

    STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
    {
        HRESULT hRes = E_POINTER;

        if (pptinfo != NULL)
        {        
            if( SUCCEEDED(hRes = sm_typeinfo.LoadTypeInfo( lcid, sm_pguid )) )
            {
                sm_typeinfo->AddRef();
                *pptinfo = sm_typeinfo;
            }
        }

        return hRes;
    }

    STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
        LCID lcid, DISPID* rgdispid)
    {
        
        HRESULT hRes = sm_typeinfo.LoadTypeInfo( lcid, sm_pguid );
        if (SUCCEEDED(hRes))
        {
            hRes = sm_typeinfo->GetIDsOfNames(rgszNames, cNames, rgdispid);
        }
        return hRes;
    }

    STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult,
        EXCEPINFO* pexcepinfo, UINT* puArgErr)
    {
        HRESULT hRes = sm_typeinfo.LoadTypeInfo( lcid, sm_pguid );
        if (SUCCEEDED(hRes))
        {
            hRes = sm_typeinfo->Invoke((IDispatch*)this, dispidMember, wFlags, pdispparams, 
                pvarResult, pexcepinfo, puArgErr);
        }
        return hRes;
    }

protected:

    const GUID * GetIID()
    { 
        return sm_pguid;
    }

static HRESULT FailedError(const HRESULT &hr, EXCEPINFO * pexcepinfo)
{
    // clear out exception info
    IErrorInfo * pei = NULL;
    HRESULT res = hr;

    if(FAILED(hr) && pexcepinfo)
    {
        // Hack: don't create errorinfo for these errors to mimic oleaut behavior.
        if( hr == DISP_E_BADPARAMCOUNT ||
            hr == DISP_E_NONAMEDARGS ||
            hr == DISP_E_MEMBERNOTFOUND)
            return hr;

        // clear out exception info
        pexcepinfo->wCode = 0;
        pexcepinfo->scode = hr;

#ifndef UNDER_CE
		// if error info exists, use it
		GetErrorInfo(0, &pei);
   
		if (pei)
#else
	    // if error info exists, use it
		if (SUCCEEDED(GetErrorInfo(0, &pei)) && pei)
#endif
        {
            // give back to OLE
            SetErrorInfo(0, pei);

            pei->GetHelpContext(&pexcepinfo->dwHelpContext);
            pei->GetSource(&pexcepinfo->bstrSource);
            pei->GetDescription(&pexcepinfo->bstrDescription);
            pei->GetHelpFile(&pexcepinfo->bstrHelpFile);

            // give complete ownership to OLE
            pei->Release();
            res = DISP_E_EXCEPTION;
        }
    }
    return res;
}

private:
    static const GUID* sm_pguid;
    static CTypeInfo sm_typeinfo;
    static const GUID* sm_plibid;
};


HRESULT FindIdsOfNames( OLECHAR **rgNames, UINT cNames, INVOKE_METHOD *rgKnownNames, int cKnownNames, LCID lcid, DISPID *rgdispid );
HRESULT PrepareInvokeArgsAndResult( 
    DISPPARAMS *pdispparams, INVOKE_ARG *rgArgs, VARTYPE *rgTypes, UINT &cArgs,
    VARIANT *&pvarResult, VARTYPE resType );
HRESULT PrepareInvokeArgs( 
    DISPPARAMS *pdispparams, INVOKE_ARG *rgArgs, const VARTYPE *rgTypes, UINT cArgs );
void ClearInvokeArgs( INVOKE_ARG *rgArgs, UINT cArgs );

#ifndef NUMELEM
#define NUMELEM( rg ) (sizeof(rg)/sizeof(*rg))
#endif

#ifdef _DEBUG

struct DebugCheckInvokeMethodOrder
{
    DebugCheckInvokeMethodOrder( WCHAR *pwszTable, INVOKE_METHOD *rgNames, int cNames );
};

#define DEBUG_CHECK_INVOKE_METHOD_ORDER( rgNames ) \
        static DebugCheckInvokeMethodOrder _check_##rgNames( L#rgNames, rgNames, NUMELEM(rgNames) );
#else

#define DEBUG_CHECK_INVOKE_METHOD_ORDER( rgNames )

#endif


#define VT_OPTIONAL 0x0800
#ifdef DEBUG
# define ASSERTONERROR() DebugBreak()
#else    // DEBUG
# define ASSERTONERROR()
#endif    // DEBUG

#ifndef JMP_FAIL
#define JMP_FAIL(hresult, label)    do {if (FAILED(hresult)) { ASSERTONERROR(); goto label;}} while (0)
#endif
#ifndef JMP_ERR
#define JMP_ERR(bFailed, label)        do {if (bFailed) { ASSERTONERROR(); goto label;}} while (0)
#endif
// help for implementing IDispatch::Invoke.  Requires some fixed named variables
#define PROPERTY_INVOKE_READWRITE( _property, _vt, _member, _type ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) )\
    {\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, _vt),\
            pexcepinfo), errExit);\
        if( pvarResult ) \
            hrErr = FailedError( get_##_property( (_type *) &pvarResult->_member ), pexcepinfo );\
    }\
    else if( (wFlags & (DISPATCH_PROPERTYPUTREF | DISPATCH_PROPERTYPUT)) && (pdispparams->cArgs == 1) )\
    {\
        VARTYPE rgTypes[] = {_vt};\
        cArgs = NUMELEM(rgTypes);\
    \
        JMP_FAIL( hrErr = FailedError( \
            PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs ),\
            pexcepinfo ), errExit );\
    \
        hrErr = FailedError( put_##_property( (_type) VARMEMBER(&rgArgs[0].vArg,_member) ), pexcepinfo);\
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }


#define PROPERTY_INVOKE_READWRITE_REF( _property, _vt, _member, _type ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) )\
    {\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, _vt),\
            pexcepinfo), errExit);\
        hrErr = FailedError( get_##_property( (_type *) &pvarResult->_member ), pexcepinfo );\
    }\
    else if( (wFlags & (DISPATCH_PROPERTYPUTREF | DISPATCH_PROPERTYPUT)) && (pdispparams->cArgs == 1) )\
    {\
        VARTYPE rgTypes[] = {_vt};\
        cArgs = NUMELEM(rgTypes);\
    \
        JMP_FAIL( hrErr = FailedError( \
            PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs ),\
            pexcepinfo ), errExit );\
    \
        hrErr = FailedError( putref_##_property( (_type) VARMEMBER(&rgArgs[0].vArg,_member) ), pexcepinfo);\
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }


#define PROPERTY_INVOKE_READ( _property, _vt, _member, _type ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) )\
    {\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, _vt),\
            pexcepinfo), errExit);\
        hrErr = FailedError( get_##_property( (_type *) &pvarResult->_member ), pexcepinfo );\
    }\
    else\
    {\
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }

#define PROPERTY_INVOKE_READWRITE_VARIANT( _property ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) )\
    {\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, VT_EMPTY),\
            pexcepinfo), errExit);\
        JMP_FAIL(hrErr = FailedError( get_##_property( pvarResult ), pexcepinfo ), errExit);\
    }\
    else if( (wFlags & (DISPATCH_PROPERTYPUTREF | DISPATCH_PROPERTYPUT)) && (pdispparams->cArgs == 1) )\
    {\
        VARTYPE rgTypes[] = {VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
    \
        JMP_FAIL( hrErr = FailedError( \
            PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs ),\
            pexcepinfo ), errExit );\
    \
        hrErr = FailedError( put_##_property( rgArgs[0].vArg ), pexcepinfo);\
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }


#define PROPERTY_INVOKE_READ_VARIANT( _property ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) )\
    {\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, VT_EMPTY),\
            pexcepinfo), errExit);\
        JMP_FAIL(hrErr = FailedError( get_##_property( pvarResult ), pexcepinfo ), errExit);\
    }\
    else\
    {\
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }

#define COLLECTION_INVOKE_READ( _collection, _cmember, _ctype, _itype ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) ) \
    { \
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, VT_DISPATCH),\
            pexcepinfo), errExit);\
        hrErr = FailedError( get_##_collection( (_ctype *)&pvarResult->pdispVal ), pexcepinfo ); \
    } \
    else if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 1) ) \
    { \
        VARTYPE rgTypes[] = {VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, rgTypes, cArgs, pvarResult, VT_DISPATCH),\
            pexcepinfo), errExit);\
        hrErr = FailedError( _cmember.get_Item( rgArgs[0].vArg, (_itype *)&pvarResult->pdispVal ), pexcepinfo ); \
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    } 

#define COLLECTION_INVOKE_READWRITE( _collection, _cmember, _ctype, _itype ) \
    if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 0) ) \
    { \
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, NULL, cArgs, pvarResult, VT_DISPATCH),\
            pexcepinfo), errExit);\
        hrErr = FailedError( get_##_collection( (_ctype *)&pvarResult->pdispVal ), pexcepinfo ); \
    } \
    else if( (wFlags & DISPATCH_PROPERTYGET) && (pdispparams->cArgs == 1) ) \
    { \
        VARTYPE rgTypes[] = {VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, rgTypes, cArgs, pvarResult, VT_DISPATCH),\
            pexcepinfo), errExit);\
        hrErr = FailedError( (_cmember).get_Item( rgArgs[0].vArg, (_itype *)&pvarResult->pdispVal ), pexcepinfo ); \
    } \
    else if( (wFlags & DISPATCH_PROPERTYPUT) && (pdispparams->cArgs == 2) ) \
    { \
        VARTYPE rgTypes[] = {VT_VARIANT, VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgs(\
            pdispparams, rgArgs, rgTypes, cArgs),\
            pexcepinfo), errExit);\
        _itype pElem = NULL; \
        JMP_FAIL( hrErr = FailedError( (_cmember).get_Item( rgArgs[0].vArg, \
                &pElem ), pexcepinfo ), errExit );\
        hrErr = FailedError( pElem->put_Value( rgArgs[1].vArg ), pexcepinfo );\
        pElem->Release();\
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    } 

#define ITEM_INVOKE_READWRITE( _itype ) \
    if(pdispparams->cArgs == 1) \
    { \
        VARTYPE rgTypes[] = {VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgsAndResult(\
            pdispparams, rgArgs, rgTypes, cArgs, pvarResult, VT_DISPATCH),\
            pexcepinfo), errExit);\
        hrErr = FailedError( get_Item( rgArgs[0].vArg, (_itype *)&pvarResult->pdispVal ), pexcepinfo ); \
    } \
    else if( (wFlags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) && (pdispparams->cArgs == 2) ) \
    { \
        VARTYPE rgTypes[] = {VT_VARIANT, VT_VARIANT};\
        cArgs = NUMELEM(rgTypes);\
        JMP_FAIL(hrErr = FailedError(PrepareInvokeArgs(\
            pdispparams, rgArgs, rgTypes, cArgs),\
            pexcepinfo), errExit);\
        _itype pElem = NULL; \
        JMP_FAIL( hrErr = FailedError( get_Item( rgArgs[0].vArg, \
                &pElem ), pexcepinfo ), errExit );\
        hrErr = FailedError( pElem->put_Value( rgArgs[1].vArg ), pexcepinfo );\
        pElem->Release();\
    } \
    else \
    { \
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo ); \
    }

#define METHOD_INVOKE( _method ) \
    if( pdispparams->cArgs == 0 )\
    {\
        hrErr = FailedError( _method(), pexcepinfo );\
    }\
    else\
    {\
        hrErr = FailedError( DISP_E_BADPARAMCOUNT, pexcepinfo );\
    }

#endif  // __DISPATCHIMPL_H_INCLUDED__
