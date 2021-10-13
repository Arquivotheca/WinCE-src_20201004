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
// File:    typemapr.cpp
// 
// Contents:
//
//  implementation file 
//
//        ITypeMapper classes  implemenation
//    
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soapmapr.h"
#include "typemapr.h"
#include "utctime.h"
#include <math.h>
#include <limits.h>

#ifdef UNDER_CE
#include "WinCEUtils.h"
#include <windows.h>
#endif


static const WORD isSafe[128] =

    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0,    /* 2x   !"#$%&'()*+,-./  */
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0,    /* 3x  0123456789:;<=>?  */
     0, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 4x  @ABCDEFGHIJKLMNO  */
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,    /* 5X  PQRSTUVWXYZ[\]^_  */
     0, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* 6x  `abcdefghijklmno  */
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0};   /* 7X  pqrstuvwxyz{|}~  DEL */

static const WCHAR hex[] = L"0123456789ABCDEF";


HRESULT defineNamespace(ISoapSerializer* pSoapSerializer, BSTR *pPrefix, const WCHAR * pNamespace1);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:HRESULT defineNamespace( ISoapSerializer* pSoapSerializer,  BSTR *pPrefix, const WCHAR * pNamespace1)
//
//  parameters:
//
//  description:
//      This function can be used the retrieve and or define a namespace.
//      on return the prefix is stored in  pPrefix
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT defineNamespace(
                ISoapSerializer* pSoapSerializer,
                BSTR *pPrefix,
                const WCHAR * pNamespace1)
{
    HRESULT hr = S_OK;

    *pPrefix = NULL;
    
    // is the first namespace defined
    pSoapSerializer->getPrefixForNamespace((BSTR) pNamespace1, pPrefix);
    if (!(*pPrefix))
    {
        CHK(pSoapSerializer->SoapNamespace( (BSTR)g_pwstrEmpty, (BSTR)pNamespace1));
        CHK(pSoapSerializer->getPrefixForNamespace( (BSTR)pNamespace1, pPrefix));
    }

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static inline BOOL _NoEscapeInUri(char b)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline BOOL _NoEscapeInUri(char b)
{
    return (isSafe[b] & 1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static inline BOOL _IsHex(char b) 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline BOOL _IsHex(char b) 
{
    return (isSafe[b] & 2);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static BOOL inline _IsEscaped(WCHAR *pch) 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL inline _IsEscaped(WCHAR *pch)
{
    if (pch[0] > 127 || pch[1] > 127)
        return FALSE;
    return (_IsHex((char) pch[0]) && _IsHex((char) pch[1]) ? TRUE : FALSE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static WORD _HexToWord(char c) 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static WORD _HexToWord(char c)
{
    if(c >= '0' && c <= '9')
        return (WORD) c - '0';
    if(c >= 'A' && c <= 'F')
        return (WORD) c - 'A' + 10;
    if(c >= 'a' && c <= 'f')
        return (WORD) c - 'a' + 10;

    ASSERT(FALSE);  //we have tried to use a non-hex number
    return (WORD) -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static WCHAR _TranslateEscaped(WCHAR * pch)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

static WCHAR _TranslateEscaped(WCHAR * pch)
{
    WCHAR ch;

    ch = (WCHAR) _HexToWord((char)*pch++) * 16; // hi nibble
    ch += _HexToWord((char)*pch); // lo nibble

    return ch;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static HRESULT _EscapeURI(WCHAR * pSource, WCHAR * pTarget)
//
//  parameters:
//
//  description:
//        URI-encodes the source into target, assumes the target buffer is big enough ( 3* len(source))
//     in error case target contents is undefined, but 0-terminated
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static HRESULT _EscapeURI(WCHAR * pSource, WCHAR * pTarget)
{
    while (*pTarget = *pSource)
    {
        if (*pSource > 127)
            {
            *pTarget =0;
            return E_FAIL;
            }
            
            
        char c = (char)*pSource++;
        
        if (_NoEscapeInUri(c))
        {
            pTarget++;
        }
        else
        {
            *pTarget++ = (WCHAR) '%';
            *pTarget++ = hex[(c >> 4) & 15];
            *pTarget++ = hex[c & 15];
        }
    }
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static HRESULT _UnescapeURI(WCHAR * pSource)
//
//  parameters:
//
//  description:
//
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static HRESULT _UnescapeURI(WCHAR * pSource)
{
    HRESULT hr = S_OK;
    WCHAR *pTarget = pSource;

    while (*pTarget = *pSource ++)
    {
        if (*pTarget == '%')
        {
            if (_IsEscaped(pSource))
            {
                *pTarget = _TranslateEscaped(pSource);
                pSource += 2;
            }
            else
                hr = E_FAIL;
        }
        pTarget ++;
    }
    return hr;
}


 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CStringMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStringMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    CAutoBSTR bstrText;
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    
    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));
    
    // now let's put this guy into our comobject
    V_VT(pvar) = VT_BSTR;
    V_BSTR(pvar) = bstrText;

    // the variant took ownership of the bstr
    bstrText.PvReturn();


Cleanup: 
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CStringMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStringMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr;
    CVariant varChanged;

    CHK (ConvertData(*pvar, (VARIANT *)&varChanged, VT_BSTR));
    CHK( pSoapSerializer->writeString(V_BSTR(&varChanged)));    

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CBoolMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBoolMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CVariant cvar;
    CAutoBSTR bstrText;
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));


    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));
    
    CHK (cvar.Assign(bstrText, false));    // now let's put this guy into our comobject
    bstrText.PvReturn();              // the variant took ownership of the bstr
    
    CHK (ConvertData((VARIANT)cvar, pvar, VT_BOOL));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CBoolMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBoolMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    CVariant varChanged;

    CHK (ConvertData(*pvar, (VARIANT *)&varChanged, VT_BOOL));

    if (V_BOOL(&varChanged) == VARIANT_TRUE)
        CHK( pSoapSerializer->writeString((BSTR)g_pwstrSOAPtrue))
    else
        CHK( pSoapSerializer->writeString((BSTR)g_pwstrSOAPfalse));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

   

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDoubleMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDoubleMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CVariant cvar;
    CAutoBSTR bstrText;
     VARTYPE v;
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));

    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));
    
    CHK (cvar.Assign(bstrText, false));    // now let's put this guy into our comobject
    bstrText.PvReturn();              // the variant took ownership of the bstr


    switch (m_enXSDType)
    {
        case enXSDfloat:
            v = VT_R4;
            break;
            
        case enXSDdecimal:            
        case enXSDDouble:
            v = VT_R8;
            break;
                

        default:
            hr = DISP_E_TYPEMISMATCH;
            goto Cleanup;
            break;
    }
    CHK (ConvertData((VARIANT)cvar, pvar, v));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDoubleMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDoubleMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    double dbl;
    CVariant cvar;
    CVariant cvarResult;



    CHK (ConvertData(*pvar, (VARIANT *)&cvar, VT_R8));   // who cares what's in it ...

    // at least we know where the output type is going to be
    dbl = V_R8(&cvar);

    // now in case of exXSDdecimal, we need to persist slightly different...    
    if (m_enXSDType != enXSDdecimal)
    {
        CHK (ConvertData(cvar, (VARIANT *)&cvarResult, VT_BSTR));
        // now, the variant function ToString is used as it seems to be more precise as swprintf
        // as this is depending on the locale, we need to replace "," with "." if it appeared
        CHK( pSoapSerializer->writeString(V_BSTR(&cvarResult)));        
    }    
    


Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDecimalMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      set the constrains for this guy
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDecimalMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = S_OK;

    CHK(CTypeMapper::init(ptypeFactory, pSchema, xsdType));
    CHK(varType(&m_vtType));
    
    m_xsdConstrains = c_XCNone;
    
    switch (xsdType)
    {
        case enXSDpositiveInteger:
            CHK(VarDecFromI4(1,  &m_decminInclusive));                        
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;
        case enXSDnonpositiveInteger:
            CHK(VarDecFromI4(0,  &m_decmaxInclusive));                                    
            m_xsdConstrains |= c_XCmaxInclusive;
            goto intDefault;
        case enXSDnegativeInteger:
            CHK(VarDecFromI4(-1,  &m_decmaxInclusive));                                                
            m_xsdConstrains |= c_XCmaxInclusive;
            goto intDefault;
        case enXSDlong:
            CHK(VarDecFromStr(_T("9223372036854775807"), LCID_TOUSE , 0, &m_decmaxInclusive));
            CHK(VarDecFromStr(_T("-9223372036854775808"), LCID_TOUSE , 0, &m_decminInclusive));            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;
        case enXSDint:
            CHK(VarDecFromStr(_T("2147483647"), LCID_TOUSE , 0, &m_decmaxInclusive));
            CHK(VarDecFromStr(_T("-2147483648"), LCID_TOUSE , 0, &m_decminInclusive));            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;
        case enXSDshort:
            CHK(VarDecFromI4(32767, &m_decmaxInclusive));
            CHK(VarDecFromI4(-32768, &m_decminInclusive));            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;
        case enXSDbyte:
            CHK(VarDecFromI4(127, &m_decmaxInclusive));
            CHK(VarDecFromI4(-128, &m_decminInclusive));            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;
        case enXSDnonNegativeInteger:
            CHK(VarDecFromI4(0, &m_decminInclusive));            
            m_xsdConstrains |= c_XCminInclusive;
            goto intDefault;

        case enXSDunsignedLong:
            CHK(VarDecFromStr(_T("18446744073709551615"), LCID_TOUSE , 0, &m_decmaxInclusive));
            CHK(VarDecFromI4(0, &m_decminInclusive));                                                            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;            
            goto intDefault;
        case enXSDunsignedInt:
            CHK(VarDecFromStr(_T("4294967295"), LCID_TOUSE , 0, &m_decmaxInclusive));
            CHK(VarDecFromI4(0, &m_decminInclusive));                                                            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;            
            goto intDefault;
        case enXSDunsignedShort:
            CHK(VarDecFromI4(65535, &m_decmaxInclusive));                        
            CHK(VarDecFromI4(0, &m_decminInclusive));                                                            
            m_xsdConstrains |= c_XCmaxInclusive;
            m_xsdConstrains |= c_XCminInclusive;            
            goto intDefault;
        case enXSDunsignedByte:
            CHK(VarDecFromI4(255, &m_decmaxInclusive));                                    
            CHK(VarDecFromI4(0, &m_decminInclusive));                                                
            m_xsdConstrains |= c_XCminInclusive;            
            m_xsdConstrains |= c_XCmaxInclusive;
            goto intDefault;
intDefault:
        case enXSDinteger:
            m_xsdConstrains |= c_XCscale;
            m_lscale = 0; 
            break;
        case enXSDdecimal:
            break;

        default:
            // invalid type
            hr = E_FAIL;
    }

Cleanup:    
    return (hr);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDecimalMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDecimalMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CVariant cvar;
    CAutoBSTR bstrText;
    CAutoRefc<IXMLDOMNode> pNode(NULL);
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutorefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));

    CHK(pNodeOfMapper->get_text(&bstrText));
    CHK(cvar.Assign(bstrText, false));    // now let's put this guy into our comobject
    bstrText.PvReturn();              // the variant took ownership of the bstr

    //convert to decimal first to easy compares etc...
    CHK (ConvertData((VARIANT)cvar, pvar, (VARTYPE) VT_DECIMAL));

    CHK(checkConstrains(pvar));

    // if we arrived here, everything is fine
    // convert to real targettype
    if (m_vtType != VT_DECIMAL)
    {
        CHK (ConvertData((VARIANT)cvar, pvar, (VARTYPE) m_vtType));
    }    


Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDecimalMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDecimalMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = E_INVALIDARG;
    CVariant cvar;
    CVariant cdec;
    CHK(ConvertData(*pvar, (VARIANT*) &cdec, VT_DECIMAL));
    CHK(checkConstrains(&cdec));
    CHK (ConvertData(*pvar, (VARIANT *)&cvar, VT_BSTR));

    CHK (pSoapSerializer->writeString(V_BSTR(&cvar)));
 
Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDecimalMapper::checkConstrains(VARIANT *pVar)
//
//  parameters:
//
//  description:
//      verifies that the values fit's the constrains
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDecimalMapper::checkConstrains(VARIANT *pvar)
{
    HRESULT hr = S_OK;
#ifndef UNDER_CE    
    HRESULT hrRes; 
#endif
    // only check constrains when we need them
    if (m_xsdConstrains != c_XCNone)
    {
        if (m_xsdConstrains & c_XCscale)
        {
            CHK_BOOL (V_DECIMAL(pvar).scale == m_lscale, E_INVALIDARG);
        }
        if (m_xsdConstrains & c_XCmaxInclusive)
        {
            CHK_BOOL((VarDecCmp(&m_decmaxInclusive, &V_DECIMAL(pvar)) !=VARCMP_LT), E_INVALIDARG)
        }
        if (m_xsdConstrains & c_XCminInclusive)
        {
            CHK_BOOL((VarDecCmp(&m_decminInclusive, &V_DECIMAL(pvar))!=VARCMP_GT), E_INVALIDARG);            
        }
        // now check constrains....
    }
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTimeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTimeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CVariant cvar;
    CAutoBSTR bstrText;
    CAutoRefc<IXMLDOMNode> pNode(NULL);
    TCHAR *pchDate;
    TCHAR *pchTime;
    TCHAR   *pchTemp;
#ifndef UNDER_CE
    TCHAR   *pchTimeZone;
#endif 
    bool     bTimeZonePlus;
    DATE    dateOut;
    DATE    timeOut;
    DATE    timeDiff;
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);
#ifndef UNDER_CE
    TIME_ZONE_INFORMATION timeZone;
#endif 
    bool                    fLocalConversion = false;

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    timeDiff = 0; 
    
    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));

    CHK_BOOL(::SysStringLen(bstrText) > 0, E_INVALIDARG);
    pchDate = bstrText;

    // if there is a Z at the end, ignore it -> change to 0
    pchTemp = pchDate+_tcslen(pchDate)-1;
    if (*pchTemp == _T('Z'))
    {
        fLocalConversion = true; 
        *pchTemp = 0;        
    }
 
    pchDate = _tcstok(bstrText, _T("T"));
    pchTime = _tcstok(0, _T("T"));

    if (pchTime && (m_enXSDType == enXSDdate || m_enXSDType == enXSDtime))
    {
        // if we are a date or a time, we DO not want the T seperator, as this is only for timeinstant valid
        hr = E_INVALIDARG;
        goto Cleanup;
    }
    if (m_enXSDType == enXSDtime)
        pchTime = pchDate;

    if (m_enXSDType == enXSDtimeInstant)
        CHK_BOOL(pchTime, E_INVALIDARG);
 
    // first check for timezone indication. if exists, strip and store....
    bTimeZonePlus = true;
    pchTemp = 0; 

    if (pchTime)
    {
        pchTemp = wcsrchr(pchTime, _T('-'));
        if (pchTemp)
        {
            bTimeZonePlus = false;
        }
        else
        {
            pchTemp = wcsrchr(pchTime, _T('+'));
        }
    }
    else
    {
        // as a date is stored in ccyy-mm-dd
        // we need to skip past the days and then check if there is anything behind it...
        // the timezone has to look like hh:mm , so look for a : first
        pchTemp = wcsrchr(pchDate, _T(':'));
        while (pchTemp && *pchTemp)
        {
            if (*pchTemp == _T('-') || *pchTemp == _T('+'))
                break;
            pchTemp--;
        }
        if (pchTemp)
        {
            if (*pchTemp == _T('-'))
            {
                bTimeZonePlus = false;
            }
            else if (*pchTemp != _T('+'))
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            }
        }
    }
    if (pchTemp)
    {
        *pchTemp = 0;
        pchTemp++;
        // here is where the timezone should start
        CHK(VarDateFromStr(pchTemp, LCID_TOUSE , 0, &timeDiff));
        fLocalConversion = true;
    }

    if (m_enXSDType == enXSDtime)
    {
        // in this case, the pchDate SHOULD be a time. Verify the timestring
       CHK(verifyTimeString(pchDate));
    }

    CHK(VarDateFromStr(pchDate, LCID_TOUSE , 0, &dateOut));

    if (m_enXSDType == enXSDtimeInstant)
    {
        CHK_BOOL(pchTime, E_INVALIDARG);
        CHK(verifyTimeString(pchTime));
        CHK(VarDateFromStr(pchTime, LCID_TOUSE , 0, &timeOut));        
        dateOut += (dateOut > 0) ? timeOut : timeOut * -1;
    }

    
    if (timeDiff != 0)
    {
        double dSeconds;
        long   lSeconds; 
        
        dSeconds = (timeDiff - floor(timeDiff)) * c_secondsInDay; 
        lSeconds = (long) (floor(dSeconds + 0.5)); 
        if (bTimeZonePlus)
        {
            lSeconds *= -1;
        }
        CHK(adjustTime(&dateOut, lSeconds));
        
    }

    if (fLocalConversion)
    {
        CHK(adjustTimeZone(&dateOut, false));
    }

    if (m_enXSDType == enXSDtime)
    {
        double d  = (dateOut < 0) ? dateOut * -1.0 : dateOut;
        dateOut = d - (floor(d));
    }
    
    V_VT(pvar) = VT_DATE;
    V_DATE(pvar) = dateOut;

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTimeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTimeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = E_FAIL;
    double pTheDate = 0;

#ifndef UNDER_CE
    TCHAR   achBuffer[_MAX_BUFFER_SIZE+1];   
    TCHAR   achNum[_MAX_ATTRIBUTE_LEN+1];
#else //putting on heap since our stack guard page is only 4k
    TCHAR    *achBuffer = new TCHAR[_MAX_BUFFER_SIZE+1];    
    TCHAR    *achNum = new TCHAR[_MAX_ATTRIBUTE_LEN+1];
    CHK_MEM(achBuffer); 
    CHK_MEM(achNum); 
#endif 
   
    SYSTEMTIME systemTime;
    
    CHK_BOOL(V_VT(pvar) & VT_DATE, E_INVALIDARG);
    if (V_VT(pvar) == VT_DATE)
        pTheDate = V_DATE(pvar);
    else 
        pTheDate = *(V_DATEREF(pvar));

    memset(achBuffer, 0, _MAX_BUFFER_SIZE+1);

    if (m_enXSDType != enXSDdate)
    {
        CHK(adjustTimeZone(&pTheDate, true));
    }

    CHK_BOOL (::VariantTimeToSystemTime(pTheDate, &systemTime), E_INVALIDARG);

    if (m_enXSDType != enXSDtime)
    {
        // if we are just xsdTime, do NOT persist the date
        swprintf(achNum, _T("%04d-%02d-%02d"), systemTime.wYear, systemTime.wMonth, systemTime.wDay);
        wcscat(achBuffer, achNum);        
    }
    
    if (m_enXSDType != enXSDdate)
    {
        // if we are just a date, do NOT persist the time
        if (m_enXSDType == enXSDtimeInstant)
        {
            // the T seperator for Time is only needed for timeinstant
            wcscat(achBuffer, _T("T"));        
        }    
        swprintf(achNum, _T("%02d:%02d:%02dZ"), systemTime.wHour, systemTime.wMinute, systemTime.wSecond);                   
        wcscat(achBuffer, achNum);
    }

    CHK( pSoapSerializer->writeString((BSTR)achBuffer));


Cleanup:
    ASSERT(SUCCEEDED(hr));
    
#ifdef UNDER_CE
    if(achBuffer)
        delete [] achBuffer;
    if(achNum)
        delete [] achNum;
#endif 
        
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTimeMapper::verifyTimeString(TCHAR *pchTimeString)
//
//  parameters:
//
//  description:
//      ole only supports hh:mm:ss
//          
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTimeMapper::verifyTimeString(TCHAR *pchTimeString)
{
    HRESULT hr = S_OK;

    while (pchTimeString && *pchTimeString)
    {
        if (*pchTimeString==_T('.'))
        {
            *pchTimeString = 0; 
            goto Cleanup;
        }
        pchTimeString++;
    }

Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT CTimeMapper::adjustTimeZone(DOUBLE *pdblDate)
//
//  parameters:
//
//  description:
//      takes date, calls gettimezoneinfo and uses that to call adjust time
//          
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTimeMapper::adjustTimeZone(DOUBLE *pdblDate, bool fToUTC)
{
    HRESULT hr=S_OK;
    double dblResult;

    if (fToUTC)
    {
        CHK(VariantTimeToUTCTime(*pdblDate, &dblResult));
    }
    else
    {
        CHK(UTCTimeToVariantTime(*pdblDate, &dblResult));
    }
    *pdblDate = dblResult;
    
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNodeList> pChildren(NULL);
    CAutoRefc<IXMLDOMNode> pNode(NULL);
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    
    CHK (pNodeOfMapper->get_childNodes(&pChildren));    
    
    V_VT(pvar) = VT_DISPATCH;
    V_DISPATCH(pvar) = pChildren;    // now let's put this guy into our comobject
    pChildren.PvReturn();             // the variant took ownership of the object

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    CAutoRefc<IXMLDOMNodeList> pNodeList(NULL); 

    CHK_BOOL ((V_VT(pvar) == VT_DISPATCH) ||(V_VT(pvar) == VT_UNKNOWN), E_INVALIDARG);

    CHK ( V_DISPATCH(pvar)->QueryInterface(IID_IXMLDOMNodeList, (void**)&pNodeList));
    
    
    // this might be complex.....
    CHK ( saveList(pSoapSerializer, pNodeList) );


Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::iid(BSTR *pbstrIID)
//
//  parameters:
//
//  description:
//      returning the IID for IXMLDOMNodeList
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE CDOMMapper::iid(BSTR *pbstrIID)
{
    #define GUIDBUFFER  64
    HRESULT hr = S_OK;
    WCHAR acBuffer[GUIDBUFFER +1];

    CHK_BOOL(pbstrIID, E_INVALIDARG);

    *pbstrIID = NULL;
    
#ifndef UNDER_CE
    CHK_BOOL(::StringFromGUID2((REFGUID)IID_IXMLDOMNodeList, (LPOLESTR) &acBuffer, GUIDBUFFER), E_FAIL);
#else
    CHK_BOOL(::StringFromGUID2((REFGUID)IID_IXMLDOMNodeList, (LPOLESTR) acBuffer, GUIDBUFFER), E_FAIL);
#endif 

    *pbstrIID = ::SysAllocString(acBuffer);
    CHK_MEM(*pbstrIID);
    
Cleanup:
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::saveNode(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
//
//  parameters:
//
//  description:
//        recursive method to save nodes and childnodes...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::saveNode(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
{
    HRESULT hr = S_OK;

    CAutoBSTR   bstrXML;
    long         ulLen;

    if (!pNode)
    {
        goto Cleanup;
    }

    hr = pNode->get_xml(&bstrXML);
    if (FAILED(hr) || bstrXML==0) 
    {
        goto Cleanup;
    }
    ulLen = ::SysStringLen(bstrXML);    
    if (ulLen > 0)
    {
        CHK(pISoapSerializer->writeXML(bstrXML));
    }    

Cleanup:
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::processChildren(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::processChildren(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
{
    CAutoRefc<IXMLDOMNodeList> pChildren=0;
    CAutoRefc<IXMLDOMNode> pChild=0;
    HRESULT hr = S_OK;

    // now process all children....
    hr = pNode->get_childNodes(&pChildren);
    if (FAILED(hr)) 
    {
        goto Cleanup;
    }
    hr = saveList(pISoapSerializer, pChildren);

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::saveList(ISoapSerializer *pISoapSerializer, IXMLDOMNodeList *pNodeList)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::saveList(ISoapSerializer *pISoapSerializer, IXMLDOMNodeList *pNodeList)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNode> pChild=0;
    
    if (pNodeList)
    {
        CHK(pNodeList->reset());
        while (pNodeList->nextNode(&pChild)==S_OK && pChild)
        {
            hr = saveNode(pISoapSerializer, pChild);
            if (FAILED(hr)) 
            {
                goto Cleanup;
            }
            pChild.Clear();
        }                
    }

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDOMMapper::saveAttributes(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDOMMapper::saveAttributes(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNamedNodeMap> pIXMLDOMNamedNodeMap = 0;
    CAutoRefc<IXMLDOMNode> pAttrNode=0; 

    hr = pNode->get_attributes(&pIXMLDOMNamedNodeMap);
    if (FAILED(hr) || pIXMLDOMNamedNodeMap == 0) 
    {
        goto Cleanup;
    }

    while (pIXMLDOMNamedNodeMap->nextNode(&pAttrNode)==S_OK && pAttrNode != 0)
    {
        hr = saveNode(pISoapSerializer, pAttrNode);
        if (FAILED(hr)) 
        {
            goto Cleanup;
        }
        pAttrNode.Clear();
    }


Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CURIMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CURIMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CAutoBSTR bstrText;
    CAutoRefc<IXMLDOMNode> pNode(NULL);
    CAutoBSTR bstrFinal;
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    
    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));
    
    CHK (_UnescapeURI(bstrText));

    // now we need to copy the final to get the correct LENGTH of the string
    // in order for BSTR compares
    CHK (bstrFinal.Assign(bstrText));

    V_VT(pvar) = VT_BSTR;
    V_BSTR(pvar) = bstrFinal;    // now let's put this guy into our variant
    bstrFinal.PvReturn();         // the variant took ownership of the bstr

    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CURIMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CURIMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    WCHAR *pResult  = L"";
    BSTR bstr = NULL;
    ULONG ulLen = 0;
    
    CHK_BOOL(V_VT(pvar) == VT_BSTR, E_INVALIDARG);
    bstr = V_BSTR(pvar);

    ulLen = wcslen(bstr); 
    if (ulLen > 0)
    {
        ulLen = ulLen*3+1; 
#ifndef UNDER_CE
       pResult = (WCHAR *) _alloca(ulLen * 2);
#else  
        __try{
            pResult = (WCHAR *) _alloca(ulLen * 2);
        }
        __except(1){
            return E_OUTOFMEMORY;
        }
#endif 
  
        CHK (_EscapeURI (bstr, pResult));
    }
    
    CHK( pSoapSerializer->writeString((BSTR)pResult ));    


Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CBinaryMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      for schemas before 2001 it 
//     checks if the passed in schemanode is 
//          a) a restriction base node
//          b) specifies the supported bin64 encoding
//    in 2001 this should only be called for the correct type
//  returns: 
//      S_OK 
//      E_FAIL if wrong schema
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBinaryMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNode> pNode;
    CAutoBSTR               bstrEncoding;
    
    // we should only initialize once

    // check for needed parameter
    CHK_BOOL(ptypeFactory, E_INVALIDARG);
   
    // start initializing
    CHK(CTypeMapper::init(ptypeFactory, pSchema, xsdType));

    CHK(_XSDFindRevision(pSchema, &m_enRevision));

    if (m_enRevision == enSchema1999 || m_enRevision == enSchema2000)
    {
        XPathState  xp;
        CAutoFormat autoFormat;
        
        CHK (xp.init(pSchema));
        CHK(autoFormat.sprintf(L"xmlns:schema='%s'", _XSDSchemaNS(m_enRevision)));        
        CHK (xp.addNamespace(&autoFormat));

        showNode(pSchema);

        CHK(pSchema->selectSingleNode(_T("descendant-or-self::schema:simpleType/schema:restriction/schema:encoding"), &pNode));

        if (pNode)
        {
            CHK(_WSDLUtilFindAttribute(pNode, _T("value"), &bstrEncoding));

            if (_tcscmp(bstrEncoding, _T("base64")) != 0)
            {
                CHK(E_INVALIDARG);
            }
        }
    }
Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}


   

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CBinaryMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBinaryMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CAutoBSTR    bstrText;
#ifndef UNDER_CE
    BYTE * pbOut(NULL);
#else
    BYTE *pbOut = NULL;
#endif 
    DWORD   dwOut;
    UINT    uiLen;
    BYTE HUGEP *pbBytes; 
    SAFEARRAYBOUND rgsabound[1];    
#ifndef UNDER_CE
    SAFEARRAY       *psa(NULL); 
#else
    SAFEARRAY       *psa = NULL; 
#endif 
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    
    // now we have the right guy, get text
    CHK(pNodeOfMapper->get_text(&bstrText));
    
    uiLen=::SysStringLen(bstrText);

    // only if we have some data, we will create the array, otherwise we will create NULL array
    if (uiLen > 0)
    {
        pbOut = new BYTE[uiLen]; 
        CHK_BOOL (pbOut, E_OUTOFMEMORY);
        
        dwOut = uiLen; 
        
        if (Base64DecodeW(bstrText, uiLen, pbOut, &dwOut)!= ERROR_SUCCESS)
        {
            hr = E_FAIL;
            goto Cleanup;
        }
        // now create a SafeArray back out of the decoded values

        
        rgsabound[0].lLbound = 0;
        rgsabound[0].cElements = dwOut;


        psa = SafeArrayCreate(VT_UI1, 1, rgsabound);
        CHK_BOOL(psa, E_OUTOFMEMORY);                  

        CHK(SafeArrayAccessData(psa, (void HUGEP **) &pbBytes))

        memcpy(pbBytes, pbOut, dwOut); 

        CHK(SafeArrayUnaccessData(psa));
    }    
    
    V_VT(pvar) = VT_ARRAY | VT_UI1;    
    V_ARRAY(pvar) = psa;
  
Cleanup:
    ASSERT(SUCCEEDED(hr));
    delete [] pbOut;
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CBinaryMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBinaryMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    DWORD dwIn;
    DWORD dwOut; 
    CAutoBSTR pbstrValue;

    BYTE HUGEP *pbBytes;     

    long        lUBound;
    long        lLBound; 

    SAFEARRAY *psa;
    VARTYPE   vartype;

    if (V_ISBYREF(pvar))
    {
        psa = *V_ARRAYREF(pvar);
        vartype = V_VT(pvar) & ~VT_BYREF;
    }
    else
    {
        psa = V_ARRAY(pvar);
        vartype = V_VT(pvar);
    }

    CHK_BOOL(vartype == (VT_ARRAY | VT_UI1), E_INVALIDARG);
    
    if (psa)
    {
        // the array can actually be NULL, we will just not output any data in that case
        
        CHK_BOOL((SafeArrayGetDim(psa)==1), E_INVALIDARG);

        CHK(SafeArrayGetUBound(psa, 1, &lUBound));
        CHK(SafeArrayGetLBound(psa, 1, &lLBound));    
        
        dwIn = lUBound - lLBound + 1; 
        
        // Allocate enough memory for full final translation quantum.
        dwOut = ((dwIn + 2) / 3) * 4;
     
         // and enough for CR-LF pairs for every CB_BASE64LINEMAX character line.
         dwOut  += 2 * ((dwOut + 64 - 1) / 64);
     
        
        CHK(pbstrValue.AllocBSTR(dwOut));
        
        CHK(SafeArrayAccessData(psa, (void HUGEP **) &pbBytes))

        
        CHK_BOOL (Base64EncodeW(pbBytes, dwIn, pbstrValue, &dwOut)==ERROR_SUCCESS, E_FAIL);

        CHK(SafeArrayUnaccessData(psa));
        
        
        CHK(pSoapSerializer->writeString(pbstrValue));    
    }

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCDATAMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCDATAMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    CVariant varChanged;

    CHK (ConvertData(*pvar, (VARIANT *)&varChanged, VT_BSTR));

    CHK (pSoapSerializer->writeXML((BSTR)g_pwstrOpenCDATA));
    CHK (pSoapSerializer->writeXML(V_BSTR(&varChanged)));    
    CHK (pSoapSerializer->writeXML((BSTR)g_pwstrCloseCDATA));
Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: WCHAR * CSafearrayMapper::skipNoise(WCHAR * pc)
//
//  parameters:
//
//  description:
//      simple function to skip over blank and tab character
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
WCHAR * CSafearrayMapper::skipNoise(WCHAR * pc)
{
    while (pc  && *pc)
    {
        if ( (*pc == '\t') || (*pc == ' '))
            pc++;
        else
            break;
    }
    return (pc);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSafearrayMapper::ParseArrayDimensions(WCHAR * pcDefDim, long * pDimCount, long ** ppDim)
//
//  parameters:
//
//  description:
//      function parses string of the form "#,#,#,#]"
//      values for # are returned in an array of long (ppDim), count of # in pDimCount
//      any error case returns non-S_OK HRESULT
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CSafearrayMapper::ParseArrayDimensions(WCHAR * pcDefDim, long * pDimCount, long ** ppDim)
{
    HRESULT hr=S_OK;
    CAutoP<long>  pDims(NULL);  // a temporary array to keep track of things
    long  lDim = 0; // use this to count
    
    ASSERT (pcDefDim);
    ASSERT (pDimCount);
    ASSERT (ppDim);

    CHK_BOOL(pcDefDim && *pcDefDim, E_INVALIDARG);
    
    while (1)
    {
        {
            CAutoP<long> pl(NULL);  // need this for temporary copy area

            // we are ready to read a new dimenion, create an array
            // with sufficient room for it
            pl = (long*) new BYTE[sizeof(long) * (lDim+1)];
            CHK_BOOL(pl, E_OUTOFMEMORY);

            // move the current contents into this one if necessary
            if (pDims)
                memcpy(pl, pDims, sizeof(long) * lDim);

            // and take ownership of the new one
            pDims.Clear();
            pDims = pl.PvReturn();
        }

        long ltemp;
        WCHAR *pcTemp;

        pcDefDim = skipNoise(pcDefDim);
        CHK_BOOL(pcDefDim && *pcDefDim, E_INVALIDARG);

        if ( (*pcDefDim == ',') || (*pcDefDim == ']') )
        {
            // this is the case where we have no number, this dimension is going to be unbounded
            ltemp = -1;
            pcTemp = pcDefDim;
        }
        else
        {
            // there better be a number around ... let's figure it out
            ltemp = wcstol(pcDefDim, &pcTemp, 10);
        }
        
        CHK_BOOL((ltemp != LONG_MAX) && (ltemp != LONG_MIN) && (pcTemp!=NULL) && (*pcTemp != 0), E_INVALIDARG);
        pcTemp = skipNoise(pcTemp);

        // throw the value into the array
        pDims[lDim] = ltemp;
        
        if (*pcTemp == ',')
        {
            // we are going to see another dimension ...
            lDim ++;

            pcDefDim = pcTemp+1;
            pcDefDim = skipNoise(pcDefDim);

            CHK_BOOL(pcDefDim && *pcDefDim, E_INVALIDARG);
            // at this point we are ready to keep reading
        }
        else
        {
            if (*pcTemp == ']')
            {
                // in this case we are done
                break;
            }   
            // no clue what got us here, we can handle this - kind of
            CHK (E_INVALIDARG);
        }
    }

    // take ownership of what we returned
    *ppDim = pDims.PvReturn();

    // and remember the number of dimensions
    *pDimCount = lDim +1;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSafearrayMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      set the constrains for this guy
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSafearrayMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNode> pArraySchema(NULL);
    XPathState  xp;

    // we should only initialize once
    ASSERT (m_ptypeFactory == NULL);
    
    // set the object into an empty state
    m_lDimensions = -1;
    m_pabound.Clear();
    m_bstrElementName.Clear();
    m_bstrTypeURI.Clear();
    m_bstrTypeName.Clear();

    m_LvtType  = -1;

    m_soaparraydef = FALSE;

    m_enRevision = enSchemaInvalid; 

    // check for needed parameter
    CHK_BOOL(ptypeFactory, E_INVALIDARG);
   
    // start initializing
    CHK(CTypeMapper::init(ptypeFactory, pSchema, xsdType));

    // hold on to the typefactory, we will need this later
    assign(&m_ptypeFactory, ptypeFactory);

    if (pSchema == NULL)
    {
        // in this case we are assuming we are getting called 
        // from inside the AnyType-Mapper and will do standard 
        // variant type discovery later
        m_enXSDType = enXSDanyType;
        return S_OK;
    }


    // from here on we need a schema
    CHK_BOOL(pSchema, E_INVALIDARG);

    CHK(_XSDFindRevision(pSchema, &m_enRevision));
    
    CHK (xp.init(pSchema));
    // set up our prefixes we can use in xpath
    CHK (xp.addNamespace(g_pwstrXpathEnc)); //give us enc: for encoding
    CHK (xp.addNamespace(g_pwstrXpathDef)); //give us def: wsdl
    
    // we need a schema namespace to look for - actually, we do not care if it is a 
    // 1999 or 2000
    {
        CAutoFormat autoFormat;
        CHK(autoFormat.sprintf(L"xmlns:schema='%s'", _XSDSchemaNS(m_enRevision)));        
        CHK (xp.addNamespace(&autoFormat));         //gives us schema: 
    }
    showNode(pSchema);

    // again 5 scenarios we want to check for:
    // (1) <element><complexType><sequence><element>
    // (2) <complexType><sequence><element>
    // (3) <element><complexType><element>
    // (4) <complexType><complexContent><restriction><attribute>
    // (5) <complexType><complexContent><restriction><sequence><attribute>

    // lets try to figure out if we are in 1,2,3 or 5
    pSchema->selectSingleNode(L".//schema:complexType/schema:sequence/schema:element", &pArraySchema);

    if (!pArraySchema)
        pSchema->selectSingleNode(L".//schema:sequence/schema:element", &pArraySchema);

    if (!pArraySchema)
        pSchema->selectSingleNode(L".//schema:complexType/schema:element", &pArraySchema);

    if (!pArraySchema)
        pSchema->selectSingleNode(L".//schema:complexContent/schema:restriction/schema:sequence/schema:attribute", &pArraySchema);

    if (pArraySchema)
    {
        CAutoBSTR bstrType;

        // we are on an element node now in pArraySchema...get the information we need ...
        CHK( _XPATHFindAttribute(pArraySchema, g_pwstrName, &m_bstrElementName));

        CHK ( _XPATHFindAttribute(pArraySchema, g_pwstrXSDtype, &bstrType) );
        CHK_BOOL(bstrType, E_FAIL);
        {
            WCHAR achPrefix[_MAX_ATTRIBUTE_LEN];
            CAutoBSTR bstrtemp;

            CHK(_WSDLUtilSplitQName(bstrType, achPrefix, &m_bstrTypeName));
            CHK (_XSDFindURIForPrefix(pArraySchema, achPrefix, &m_bstrTypeURI));      

            //this thing will allways have one dimension
            m_pabound = (SAFEARRAYBOUND*)new BYTE[sizeof(SAFEARRAYBOUND)];
            CHK_BOOL(m_pabound, E_OUTOFMEMORY);

            m_pabound[0].cElements = -1;
            m_pabound[0].lLbound = 0;
            m_lDimensions = 1;
            
            //find the maxOccurs values
            CHK ( _XPATHFindAttribute(pArraySchema, g_pwstrMaxOccurs, &bstrtemp) );
            if (bstrtemp)
            {
                long ltemp = -1;
                if ((wcsicmp(bstrtemp, g_pwstrUnbounded)!= 0)&&(wcsicmp(bstrtemp, g_pwstrStar)!= 0))
                {  //  maxOccurs != "unbounded" and  maxOccurs != "*"
                    WCHAR *ptemp = NULL;
                    
                    // figure out maxoccurs:
                    ltemp = wcstol(bstrtemp, &ptemp, 10);
                    CHK_BOOL((ltemp != LONG_MAX) && (ltemp != LONG_MIN) && (ptemp!=NULL) && (*ptemp == 0) && (ltemp>=0), E_INVALIDARG);
                }
                m_pabound[0].cElements = ltemp;
            }
            m_soaparraydef = FALSE;            
            TRACE ( ("cElemts: %d lLBound: %d type: %S %S\n", m_pabound[0].cElements, m_pabound[0].lLbound, m_bstrTypeName, m_bstrTypeURI) );
        }
    }
    else
    {
        CAutoBSTR bstrType;
        
        // we better be in scenario 4 ....
        pSchema->selectSingleNode(L".//schema:complexContent/schema:restriction/schema:attribute", &pArraySchema);

        CHK_BOOL(pArraySchema, E_FAIL);     // in case we could not recognize anything

        showNode(pArraySchema);
        
        // assume we do soapencoding here always
        CHK ( _XPATHFindAttribute(pArraySchema, _T("def:arrayType"), &bstrType) );
        CHK_BOOL(bstrType, E_FAIL);
        
        // check if it is an arraytype
        WCHAR * pDimension = wcsstr(bstrType, L"[");
        CHK_BOOL(pDimension, E_FAIL);

        *pDimension = 0;
        {
            WCHAR achPrefix[_MAX_ATTRIBUTE_LEN];
            CAutoBSTR bstrtempType;
            CAutoP<long> pDim(NULL);
            
            CHK(_WSDLUtilSplitQName(bstrType, achPrefix, &m_bstrTypeName));
            CHK (_XSDFindURIForPrefix(pArraySchema, achPrefix, &m_bstrTypeURI));      

            // now figure out the dimension thing
            pDimension++;   // points to the beginning of the dimension string
            CHK_BOOL(*pDimension, E_INVALIDARG);        // certainly do not expect the end of string here

            CHK(ParseArrayDimensions(pDimension, &m_lDimensions, &pDim));

            m_pabound = (SAFEARRAYBOUND*)new BYTE[m_lDimensions * sizeof(SAFEARRAYBOUND)];
            CHK_BOOL(m_pabound, E_OUTOFMEMORY);

            // now we have to verify the information and move it into the
            // member structures
            for (long lc = 0; lc < m_lDimensions; lc ++)
            {
//                if ( (pDim[lc] == -1) && (lc != 0) )
//                {
//                    CHK (DISP_E_BADINDEX);
//                }
                m_pabound[lc].cElements = pDim[lc];
                m_pabound[lc].lLbound = 0;
                TRACE ( ("dim: %d cElemts: %d lLBound: %d type: %S %S\n", lc,
                                            m_pabound[m_lDimensions-1].cElements, m_pabound[m_lDimensions-1].lLbound, m_bstrTypeName, m_bstrTypeURI) );
            }
            
        }
        m_soaparraydef = TRUE;
    }

Cleanup:
    ASSERT(SUCCEEDED(hr));

    return (hr);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSafearrayMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSafearrayMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IXMLDOMNode> pTemp(NULL);
    CAutoRefc<IXMLDOMNode> pChild(NULL);
    CAutoRefc<ISoapTypeMapper> ptm(NULL);
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);
    
    SAFEARRAY * psa = NULL;
    VARTYPE vartype;
    DOMNodeType dnt;
    long lDimension;
    
    BOOL bDoSoapEncoding = FALSE;
    CAutoP<long> pIndicesHigh(NULL);
    CAutoP<SAFEARRAYBOUND> pabound(NULL);
    CAutoP<long> pIndices(NULL);
    
    XPathState  xp;

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));

    showNode(pNodeOfMapper);

    CHK (xp.init(pNodeOfMapper));
    CHK (xp.addNamespace(g_pwstrXpathEnv));
    CHK (xp.addNamespace(g_pwstrXpathEnc));

    pNodeOfMapper->selectSingleNode(L"./@enc:arrayType", &pTemp);
    if (pTemp)
    {
        bDoSoapEncoding = TRUE;
        pTemp.Clear();
    }
    

    if (bDoSoapEncoding)
    {
        CAutoRefc<IXMLDOMNode> pTemp(NULL);

        CAutoBSTR   bstrType;
        CAutoBSTR   bstrURI;
        WCHAR achPrefix[_MAX_ATTRIBUTE_LEN];
        CAutoBSTR   bstrNakedType;
        WCHAR * pcDimension;
        CAutoP<long>pDim;

        CHK (pNodeOfMapper->selectSingleNode(L"./@enc:arrayType", &pTemp));
        CHK_BOOL(pTemp, E_FAIL);
        showNode(pTemp);
        CHK(pTemp->get_text(&bstrType));

        CHK(_WSDLUtilSplitQName( bstrType, achPrefix, &bstrNakedType));

        CHK (_XPATHGetNamespaceURIForPrefix(pNodeOfMapper, achPrefix, &bstrURI));
        
        pcDimension = wcschr(bstrNakedType,'[');
        CHK_BOOL (pcDimension, E_FAIL);

         *pcDimension = 0;
        hr = m_ptypeFactory->getTypeMapperbyName(bstrNakedType, bstrURI, &ptm); // we require the type on the schema
        ERROR_ONHR2(hr, SOAP_IDS_MAPPERARRAYCREATEELEMENT, WSDL_IDS_MAPPER, bstrNakedType, bstrURI);
      

        // this points now to the beginning of the dimensions
        pcDimension++;
        CHK (ParseArrayDimensions(pcDimension, &lDimension, &pIndicesHigh));

        // for anytype arrays we are not initialized, dimensions can be -1
        if (m_lDimensions != -1)
        {
            SETHR_BOOL(lDimension == m_lDimensions, E_INVALIDARG);
            ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
        }
        
        pabound = (SAFEARRAYBOUND*) new BYTE[lDimension * sizeof(SAFEARRAYBOUND)];
        pIndices = (long*) new BYTE[lDimension * sizeof(long)];

        for (long lc = 0; lc < lDimension; lc ++)
        {
            pabound[lc].lLbound = 0;
            pIndices[lc] = 0;

            // we can not be undefined in any dimension beside 0
            if ( (lc != 0) && (pIndicesHigh[lc] < 0))
            {
                ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
            }
            
            if (m_lDimensions != -1)
            {
                if (lc == 0)
                {
                    if ((pIndicesHigh[lc] == -1) && (m_pabound[lc].cElements== -1))
                    {
                        // variable in the dimension
                        pabound[lc].cElements = 1;        // one is the minimum
                        continue;
                    }
                    if (m_pabound[lc].cElements != -1)
                    {
                        SETHR_BOOL(pIndicesHigh[lc] <= (long)m_pabound[lc].cElements, DISP_E_BADINDEX);
                        ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
                    }
                    pabound[lc].cElements = max(pIndicesHigh[lc], (long)m_pabound[lc].cElements);
                    pIndicesHigh[lc] = pabound[lc].cElements;
                    continue;
                }
                if (m_pabound[lc].cElements != -1)
                {
                    // make sure we do not want more elements then the schema allows
                    SETHR_BOOL(pIndicesHigh[lc] <= (long)m_pabound[lc].cElements, DISP_E_BADINDEX);
                    ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
                }
            }
            
            pabound[lc].cElements = pIndicesHigh[lc];
        }
    }
    else
    {
        ASSERT(m_enXSDType!= enXSDUndefined);
        if ((!m_bstrTypeName) && (!m_bstrTypeURI))
        {
            // since we got no clue what we are supposed to map, this is our only hope:
            CHK (m_bstrTypeName.Assign((BSTR)g_pwstrAnyType));
            CHK (m_bstrTypeURI.Assign((BSTR)_XSDSchemaNS(m_enRevision)));
        }
        CHK(m_ptypeFactory->getTypeMapperbyName(m_bstrTypeName, m_bstrTypeURI, &ptm)); // we require the type on the schema
        lDimension = 1;

        // for anytype arrays we are not initialized, dimensions can be -1
        if (m_lDimensions != -1)
        {
            // we better be one-dimensional at this point in time
            SETHR_BOOL(lDimension == m_lDimensions, E_INVALIDARG);
            ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
        }
        
        // set up the rest:
        pIndicesHigh = (long*)new BYTE[sizeof(long)];
        pabound = (SAFEARRAYBOUND*) new BYTE[sizeof(SAFEARRAYBOUND)];
        pIndices = (long*)new BYTE[sizeof(long)];

        CHK_BOOL(pIndicesHigh && pIndices && pabound, E_OUTOFMEMORY);

        pabound[0].lLbound  = 0; 
        if (m_pabound[0].cElements == -1)
           {
               // unbound, will grow later
               pabound[0].cElements = 1;
           }
        else
        {
            pabound[0].cElements = max(1,m_pabound[0].cElements);
        }            
        pIndicesHigh[0] = m_pabound[0].cElements;
        pIndices[0] = 0;
        
    }


    // Compute the destination vartype in the safearray...
    if ((m_bstrTypeName) && (m_bstrTypeURI))
    {
        // during Init we got some type information, this
        // is actually what we want in the created safearray
        CAutoRefc<ISoapTypeMapper> ptm_temp(NULL);
        CHK(m_ptypeFactory->getTypeMapperbyName(m_bstrTypeName, m_bstrTypeURI, &ptm_temp));
        CHK (ptm_temp->varType((long*)&vartype));
    }
    else
    {
        CHK (ptm->varType((long*)&vartype));
    }
    if (vartype == VT_DECIMAL)
    {
        // we will create decimals as a array of variants
        vartype = VT_VARIANT;
    }



    pNodeOfMapper->get_firstChild(&pChild);
    while (pChild)
    {
        CHK (pChild->get_nodeType(&dnt));
        if (dnt == NODE_ELEMENT)
            break;  // we are on an element node
        
        // read over other nodes someone might have put into the message
        CHK (pChild->get_nextSibling(&pTemp));
        pChild.Clear();
        pChild = pTemp.PvReturn();
    }    
    
    // at this point we have all the information collected we can
    // we might run into the issue of a NULL array, assuming it looks something like
    // <array> </array>
    // if this is true we just will create the variant, and pass in a NULL array
    if (pChild)
    {
#ifndef UNDER_CE
        if (vartype == VT_DISPATCH || vartype == VT_UNKNOWN)
        {
            IID iid;
            IID * pIID = 0;
            CAutoBSTR bstrIID;
            CAutoRefc<ISoapTypeMapper2> ptm2; 

            if (ptm->QueryInterface(IID_ISoapTypeMapper2, (void**)&ptm2)== S_OK)
            {
                CHK(ptm2->iid(&bstrIID));
            }

            // acquire IID here
            if (!bstrIID.isEmpty())
            {
                hr = CLSIDFromString(bstrIID, &iid);

                ERROR_ONHR1(hr, WSML_IDS_TYPEMAPPERCLSID, WSDL_IDS_MAPPER, bstrIID);   
                pIID = &iid;
            }
            else
            {
                pIID = (IID *) &IID_IDispatch;
            }
            //psa = SafeArrayCreateEx(vartype, lDimension, pabound, (PVOID) pIID);
            ASSERT(FALSE);
            CHK_BOOL(psa, E_OUTOFMEMORY);      
        }
        else
        {
#endif 
            psa = SafeArrayCreate(vartype, lDimension, pabound);
            CHK_BOOL(psa, E_OUTOFMEMORY);      
#ifndef UNDER_CE
        }
#endif 
    }
    
    pTemp.Clear();
  
    while (pChild)
    {
        CVariant  cvar;  
        CAutoBSTR bstrText;
        
        showNode(pChild);


        CHK(ptm->read(pChild, bstrEncoding, enStyle, 0, &cvar));

        // we might need to change the type
        if (vartype != VT_VARIANT)
        {
            CHK( VariantChangeTypeEx(&cvar, &cvar, LCID_TOUSE, VARIANT_ALPHABOOL, vartype));
        }

        switch (vartype)
        {
            case VT_DISPATCH:
                CHK( SafeArrayPutElement(psa, pIndices, V_DISPATCH(&cvar)) );    
                break;

            case VT_BSTR:
                CHK( SafeArrayPutElement(psa, pIndices, V_BSTR(&cvar)) );    
                break;

            case VT_VARIANT:
                CHK( SafeArrayPutElement(psa, pIndices, &cvar) );    
                break;

            #define SWITCHER(A,B) case A: CHK( SafeArrayPutElement(psa, pIndices, &(B(&cvar))) ); break;
            
            SWITCHER(VT_CY, V_CY);
            SWITCHER(VT_DATE, V_DATE);
            SWITCHER(VT_BOOL, V_BOOL);
            SWITCHER(VT_I4, V_I4);
            SWITCHER(VT_I2, V_I2);
            SWITCHER(VT_I1, V_I1);
            SWITCHER(VT_UI1, V_UI1);
            SWITCHER(VT_UI2, V_UI2);
            SWITCHER(VT_UI8, V_UI8);
            SWITCHER(VT_INT, V_INT);
            SWITCHER(VT_UINT, V_UINT);
            SWITCHER(VT_R4, V_R4);
            SWITCHER(VT_R8, V_R8);
                
            default:
                CHK (hr = E_INVALIDARG);
                break;

            #undef SWITCHER
        }


        do
        {
            // get the next node
            CHK (pChild->get_nextSibling(&pTemp));
            pChild.Clear();
            pChild = pTemp.PvReturn();
            if (pChild)            
            {
                CHK (pChild->get_nodeType(&dnt));
            }
        }    
        while (pChild && (dnt != NODE_ELEMENT));



        // we advanced to the next node (or not), 
        // now we need to change the pIndices structure
        int iCurrentDim = lDimension-1;

        while (iCurrentDim >= 0)
        {
            pIndices[iCurrentDim]++;
            if (pIndices[iCurrentDim] < pIndicesHigh[iCurrentDim])
            {
                // we adjusted the needed index, we are ready to loop
                if (!pChild)
                {   
                    // there is no-other element to read, but we thought we have another one
                    ERROR_ONHR0(DISP_E_BADINDEX, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
                }
                break;      // get out of the while-loop and ready to read the next element
            }

            if (iCurrentDim == 0)
            {
                // the first dimension could be dynamic
                if ((pIndicesHigh[0] == -1) && (m_pabound[0].cElements== -1))
                {
                    if (pChild)
                    {
                        // we are going to grow the savearray and keep going
                        pabound[0].lLbound = 0;
                        pabound[0].cElements++;
                        CHK (SafeArrayRedim(psa,pabound));

                        //and we are ready to go and read
                        break;
                    }
                }

                // we will exceeded the number of elements to read ...
                // unless there is nothing more to read
                if (pChild)
                {
                    ERROR_ONHR0(E_INVALIDARG, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
                }

                // when we get here, we are actually in good shape - 
                // this will probably lead to a success case.
                
            }
            
            // the current index in this dimension exceeds the allowed max-value
            // reset to the lowest possible value
            pIndices[iCurrentDim] = 0;

            // and keep going
            iCurrentDim --;
        }


    }

    V_VT(pvar) = VT_ARRAY | vartype;
    V_ARRAY(pvar) = psa;
  
    // control over the array if given up ....
    psa = NULL;
    
Cleanup:
    if (FAILED(hr) && hr != E_INVALIDARG)
    {
         globalAddError(WSML_IDS_TYPEMAPPERPUTSOAPVALUE, WSDL_IDS_MAPPER, hr, m_bstrElementName);                                               
    }
    if (psa)
        ::SafeArrayDestroy(psa);
   
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSafearrayMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSafearrayMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr = S_OK;
    SAFEARRAY * psa = NULL;
    UINT uiDim;  // array dimension
    CAutoRefc<ISoapTypeMapper> ptm;
    CAutoBSTR bstrXSDPrefix;  // these will take the XSD and XSI encoding if needed
    CAutoBSTR bstrXSIPrefix;
    VARTYPE   vartype;
    CAutoFormat af;
    CAutoP<long> pIndices(NULL);
    CAutoP<long> pIndicesLow(NULL);
    CAutoP<long> pIndicesHigh(NULL);
    WCHAR achTemp[_MAX_ATTRIBUTE_LEN];
#ifdef UNDER_CE
     UINT iDimension = 1;
#endif 
    
    // check for initial parameter
    CHK_BOOL(pSoapSerializer, E_INVALIDARG);
    CHK_BOOL(pvar, E_INVALIDARG);

    CHK_BOOL (V_VT(pvar) & VT_ARRAY, E_INVALIDARG);

    if (V_ISBYREF(pvar))
    {
        psa = *V_ARRAYREF(pvar);
        vartype = V_VT(pvar) & ~VT_BYREF;
    }
    else
    {
        psa = V_ARRAY(pvar);
        vartype = V_VT(pvar);
    }

    // make sure we got an array
    CHK_BOOL (vartype & VT_ARRAY, E_INVALIDARG);
    vartype &= ~VT_ARRAY;      // vartype now contains the vartype of the 
                                // safearray contents
                                    
    if ((!m_bstrTypeName) && (!m_bstrTypeURI))
    {
        // since we got no clue what we are supposed to map, this is our only hope:
        CHK (m_bstrTypeName.Assign((BSTR)g_pwstrAnyType));

        if (m_enRevision == enSchemaInvalid)
        {
            // we are an anytype, and we don't know our schema yet....
            if (FAILED(_XSDFindRevision(pSoapSerializer, &m_enRevision)))
            {
                m_enRevision = enSchemaLast;
            }
        }
        
        CHK (m_bstrTypeURI.Assign((BSTR)_XSDSchemaNS(m_enRevision)));
    }
    
    if (psa == NULL)
    {
        // we got the case of a NULL array parameter, we handle this
        // as a one-dimensional array with 0 elements
        uiDim = 1;
    }
    else
    {
        hr = m_ptypeFactory->getTypeMapperbyName(m_bstrTypeName, m_bstrTypeURI, &ptm); // we require the type on the schema
        ERROR_ONHR2(hr, SOAP_IDS_MAPPERARRAYCREATEELEMENT, WSDL_IDS_MAPPER, m_bstrTypeName, m_bstrTypeURI);

        // get the needed information from the array
        uiDim = SafeArrayGetDim(psa);       // Dimension of the array
        CHK_BOOL(uiDim >= 1, E_INVALIDARG); // need at least one dimension
        
        // the following can happen if we are trying to map an array of anytype ...
        if (m_lDimensions != -1)
        {
            // then make sure we got what we expected
#ifndef UNDER_CE
            CHK_BOOL(uiDim == m_lDimensions, E_INVALIDARG); // dimensions have to match
#else
            CHK_BOOL(uiDim == (ULONG)m_lDimensions, E_INVALIDARG); // dimensions have to match
#endif 
        }
    }
    
    
    *achTemp = 0;

    pIndices = (long*)new BYTE[sizeof(long)*uiDim];
    pIndicesLow = (long*)new BYTE[sizeof(long)*uiDim];
    pIndicesHigh = (long*)new BYTE[sizeof(long)*uiDim];
    CHK_BOOL(pIndices && pIndicesLow && pIndicesHigh, E_OUTOFMEMORY);
    
#ifndef UNDER_CE
    for (UINT iDimension = 1; iDimension <= uiDim; iDimension ++)
#else
    for (iDimension = 1; iDimension <= uiDim; iDimension ++)
#endif 
    {
        LONG lLBound, lUBound;  // the array boundaries
        long lElementCount;

        if (psa)
        {
            CHK (SafeArrayGetLBound(psa, iDimension, &lLBound));
            CHK (SafeArrayGetUBound(psa, iDimension, &lUBound));
        }
        else
        {
            lLBound = 0;
            lUBound = -1;
        }
        
        lElementCount = lUBound-lLBound+1;
        CHK_BOOL(lElementCount >= 0, E_INVALIDARG);

        // in the case we got initialized....
        if (m_lDimensions != -1)
        {
            // ...check that the element number requirements are justified
            if (m_pabound[iDimension-1].cElements != -1)
            {
                if (m_soaparraydef)
                {
                    // Element count has to match exactly
#ifndef UNDER_CE
                    SETHR_BOOL (lElementCount == m_pabound[iDimension-1].cElements, E_INVALIDARG);
#else
                    SETHR_BOOL ((ULONG)lElementCount == m_pabound[iDimension-1].cElements, E_INVALIDARG);
#endif 
                }
                else
                {
                    // Element count can't exceed the upper limit
                    SETHR_BOOL ( ((ULONG)lElementCount) <= m_pabound[iDimension-1].cElements, E_INVALIDARG);
                }
                
                ERROR_ONHR0(hr, SOAP_IDS_MAPPERARRAYDIMENSIONS, WSDL_IDS_MAPPER);
            }
        }
        CHK_BOOL(wcslen(achTemp) < _MAX_ATTRIBUTE_LEN-20, E_INVALIDARG);
        if (*achTemp)
            swprintf(achTemp, L"%s,%d", achTemp, lElementCount);
        else
            swprintf(achTemp, L"%d", lElementCount);

        pIndices[iDimension - 1] = lLBound;
        pIndicesLow[iDimension - 1] = lLBound;
        pIndicesHigh[iDimension - 1] = lUBound+1;
    }

    CHK (defineNamespace(pSoapSerializer, &bstrXSDPrefix, m_bstrTypeURI));
    af.sprintf(L"%s:%s[%s]", bstrXSDPrefix, m_bstrTypeName, achTemp);
    CHK(pSoapSerializer->SoapAttribute((BSTR)g_pwstrarrayType, (BSTR)g_pwstrEncStyleNS, &af));

    // to help on some interop issues: 
    // generate xsi:type="SOAP-ENC:Array"

    // figure out the XSI namepace
    CHK(_XSDAddSchemaInstanceNS(pSoapSerializer, m_enRevision, &bstrXSIPrefix));   
    
    {   
        CAutoBSTR bstrSoapEncPrefix;
        WCHAR acBuffer[_MAX_ATTRIBUTE_LEN+1];
                
        // we need the SoapEncoding Namespace for this
        CHK (_XSDAddNS(pSoapSerializer, &bstrSoapEncPrefix, (WCHAR*)g_pwstrEncStyleNS));
        wcscpy(acBuffer, bstrSoapEncPrefix);
        wcscat(acBuffer, L":");
        wcscat(acBuffer, g_pwstrArray);
        
        // add the XSI:type attribute
          CHK(pSoapSerializer->SoapAttribute((BSTR) g_pwstrXSDtype, NULL, (BSTR)acBuffer, bstrXSIPrefix));
    }

    if ( (uiDim == 1) && (pIndices[0] == 0) && (pIndicesHigh[0] == 0) )
    {
        // in this case we had either a NULL array passed in,
        // or we are handling an array without any elements ... for simplicity:
        goto Cleanup;
    }
    
    int iCurrentDim;
    do
    {
        CVariant v;

        if (m_bstrElementName)
        {
            CHK( pSoapSerializer->startElement(m_bstrElementName, NULL, NULL, NULL));
        }
        else
        {

            if ( (m_bstrTypeURI == NULL) || (_XSDIsPublicSchema(m_bstrTypeURI)) )
            {
                CHK( pSoapSerializer->startElement(m_bstrTypeName, (BSTR)g_pwstrEncStyleNS, NULL, NULL));
            }
            else
            {
                CHK( pSoapSerializer->startElement(m_bstrTypeName, m_bstrTypeURI, NULL, NULL));
            }
        }

        V_VT(&v) = vartype;
        switch (vartype)
        {

            #define SWITCHER(A,B) case A: CHK(SafeArrayGetElement(psa, pIndices, &(B(&v)) )); break;
            
            SWITCHER(VT_BSTR, V_BSTR);
            SWITCHER(VT_CY, V_CY);
            SWITCHER(VT_DATE, V_DATE);
            SWITCHER(VT_BOOL, V_BOOL);
            SWITCHER(VT_DECIMAL, V_DECIMAL);
            SWITCHER(VT_I4, V_I4);
            SWITCHER(VT_I2, V_I2);
            SWITCHER(VT_I1, V_I1);
            SWITCHER(VT_UI1, V_UI1);
            SWITCHER(VT_UI2, V_UI2);
            SWITCHER(VT_UI8, V_UI8);
            SWITCHER(VT_INT, V_INT);
            SWITCHER(VT_UINT, V_UINT);
            SWITCHER(VT_R4, V_R4);
            SWITCHER(VT_R8, V_R8);
            SWITCHER(VT_DISPATCH, V_DISPATCH);

            case VT_VARIANT: 
                CHK(SafeArrayGetElement(psa, pIndices, &v) ); 
                break;
                    
            default:
                CHK (hr = E_INVALIDARG);
                break;

            #undef SWITCHER
            
        }
    
        hr = ptm->write(pSoapSerializer, bstrEncoding, enStyle, 0, &v);
        if (FAILED(pSoapSerializer->endElement()))
            hr = E_FAIL;

        CHK(hr);


        // this element is written now, we have to get the next element in the safearray

        iCurrentDim = uiDim-1;

        while (iCurrentDim >= 0)
        {
            pIndices[iCurrentDim]++;
            if (pIndices[iCurrentDim] < pIndicesHigh[iCurrentDim])
            {
                // we adjusted the needed index, we are ready to loop
                break;      // get out of the while-loop
            }

            // the current index in this dimension exceeds the allowed max-value
            // reset to the lowest possible value
            pIndices[iCurrentDim] = pIndicesLow[iCurrentDim];

            iCurrentDim --;
        }
    }
    while (iCurrentDim >= 0);
    
    
Cleanup:
    ASSERT(hr==S_OK);
    if (FAILED(hr))
    {
         globalAddError(WSML_IDS_TYPEMAPPERPUTVALUE, WSDL_IDS_MAPPER, hr, m_bstrElementName ? m_bstrElementName : m_bstrTypeName);                                               
    }
    
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSafearrayMapper::varType(long*pvtType)
//
//  parameters:
//
//  description:
// 
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSafearrayMapper::varType(long*pvtType)
{
    HRESULT hr = S_OK;
    VARTYPE vartype = VT_EMPTY;
    
    if (m_LvtType != -1)
    {
        *pvtType = m_LvtType;
        goto Cleanup;
    }
    

    CHK_BOOL (pvtType, E_INVALIDARG);

    // Compute the destination vartype in the safearray...
    if ((m_bstrTypeName) && (m_bstrTypeURI) && (m_ptypeFactory))
    {
        // during Init we got some type information, this
        // is actually what we want in the created safearray
        CAutoRefc<ISoapTypeMapper> ptm_temp(NULL);

        CHK(m_ptypeFactory->getTypeMapperbyName(m_bstrTypeName, m_bstrTypeURI, &ptm_temp));
        CHK (ptm_temp->varType((long*)&vartype));
    }

    *pvtType = VT_ARRAY | vartype;

    m_LvtType =  *pvtType;

Cleanup:
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAnyTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAnyTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = S_OK;
    
    ASSERT (m_ptypeFactory == NULL);
  
    CHK_BOOL(ptypeFactory, E_INVALIDARG);
    
    // start initializing
    CHK(CTypeMapper::init(ptypeFactory, pSchema, xsdType));
    assign(&m_ptypeFactory, ptypeFactory);

    CHK(_XSDFindRevision(pSchema, &m_enRevision));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAnyTypeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags,VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAnyTypeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags,VARIANT * pvar)
{
    HRESULT hr = S_OK;
    CAutoRefc<ISoapTypeMapper> ptm(NULL);
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);
    CAutoBSTR bstrFullType;


    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));
    
    showNode(pNodeOfMapper);

    {
        XPathState  xp;
        CAutoRefc<IXMLDOMNode> pTemp(NULL);
        
        CHK (xp.init(pNodeOfMapper));
        CHK (xp.addNamespace(g_pwstrXpathEnc));

        // check for soap-encoded array
        CHK (pNodeOfMapper->selectSingleNode(L"./@enc:arrayType", &pTemp));
        if (pTemp)
        {
            // no additional checks here, let's just assume it is an array,
            // the array mapper will do the rest of the analysis ... 
            CHK(m_ptypeFactory->getMapper(enXSDarray, 0, &ptm));    
    }
    }

    if (!ptm)
    {
        CAutoRefc<IXMLDOMNode> pTemp(NULL);
        
        // we did not find an array mapper above, do the normal eval
        
        _XSDFindNodeInSchemaNS(g_pwstrXpathXSIanyType, pNodeOfMapper, &pTemp);
        if (pTemp)
        {
            CHK(pTemp->get_text(&bstrFullType));
        }   
        else
        {
            // not an array definition and not an xsi:type ...
            
            // we take a look at the name and assume it has usefull information
            // something like ns:type
            CHK(pNodeOfMapper->get_nodeName(&bstrFullType));
        }
    }
    
    if ( (bstrFullType) && (!ptm) )
    {
        // at this point this should contain a string of the form prefix:type
        CAutoBSTR bstrURI;
        CAutoBSTR bstrType; 
        WCHAR achPrefix[_MAX_ATTRIBUTE_LEN];

        _WSDLUtilSplitQName( bstrFullType, achPrefix, &bstrType);


        if (!wcscmp(bstrType, g_pwstrBinaryType))
        {
            CHK(m_ptypeFactory->getMapper(enXSDbinary, 0, &ptm));
        }
        else if (!wcscmp(bstrType, g_pwstrSBase64ECType))
        {
            CHK(m_ptypeFactory->getMapper(enXSDbinary, 0, &ptm));
        }
        else
        {
            CHK (_XPATHGetNamespaceURIForPrefix(pNodeOfMapper, achPrefix, &bstrURI));
            if ( (!wcscmp(bstrURI, g_pwstrTKData)) && (!wcscmp(bstrType, g_pwstrEmptyType)) )
            {
                hr = m_ptypeFactory->getMapper(enTKempty, 0, &ptm);
            }
            else
            {
                if (wcsicmp(bstrURI, g_pwstrEncStyleNS))
                {
                    // this is not the soap-encoding ...
                    hr = m_ptypeFactory->getTypeMapperbyName(bstrType, bstrURI, &ptm);
                }
                else
                {
                    // this is soapencoding ... use the last- schema namespace
                    hr = m_ptypeFactory->getTypeMapperbyName(bstrType, (BSTR)_XSDSchemaNS(enSchemaLast), &ptm);
                }
            }
        }
                
       ERROR_ONHR2(hr, SOAP_IDS_MAPPERANYTYPECREATEELEMENT, WSDL_IDS_MAPPER, bstrType, bstrURI);
    }

    if (!ptm)
    {
        ERROR_ONHR0(E_INVALIDARG, SOAP_IDS_MAPPERINVALIDANYTYPE, WSDL_IDS_MAPPER);
    }
    CHK(ptm->read(pNodeOfMapper, bstrEncoding, enStyle, 0, pvar));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAnyTypeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags,VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a Variant into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAnyTypeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags,VARIANT * pvar)
{
    HRESULT hr=S_OK;
    VARTYPE vtype;
    enXSDType enxsd;
    CAutoRefc<ISoapTypeMapper> ptm(NULL);

    // might be a ref
    if ( V_VT(pvar) == (VT_VARIANT | VT_BYREF) )
        pvar = V_VARIANTREF(pvar);

    vtype = V_VT(pvar) & (~VT_BYREF);   // get the variant-type without the byref - attribute
    if (vtype & VT_ARRAY)
        {
            if ( (vtype & ~VT_ARRAY) == VT_UI1)
                enxsd =  enXSDbinary;
            else
                enxsd = enXSDarray;
        }
    else
    {
        if (vtype == VT_EMPTY)
            enxsd = enTKempty;
        else
            enxsd = vtVariantTypeToXsdVariant(vtype);           // what xsd tyoe matches this variant
        CHK_BOOL(enxsd != enXSDUndefined, E_INVALIDARG);    // and can we do something usefull with it
    }

    {
        // now we are going to create the mapper
        // ... for the array mapper we will create a schema to initialize the mapper correctly
        CAutoRefc<IXMLDOMNode> pNode(NULL);
        CAutoRefc<IXMLDOMDocument2> pDOM(NULL);
        CAutoFormat af;
        VARIANT_BOOL vb;          

#ifndef UNDER_CE
        CHK  (CoCreateInstance(CLSID_FreeThreadedDOMDocument30, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void **)&pDOM));
#else
        CHK  (CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void **)&pDOM));
        CHK  (pDOM->put_resolveExternals(false));    
#endif
     
        if ( (enxsd == enXSDarray) && (m_enRevision != enSchemaInvalid))
        {
            SAFEARRAY * psa = NULL;
            UINT uiDim;
            CAutoFormat adim; 
            // simple fragment, enough for the array init to
            // recognize the active schema and the array type
            const WCHAR * pcType = xsdVariantTypeToXsdString(m_enRevision, vtype & ~VT_ARRAY);
            if (pcType == NULL)
                pcType = g_pwstrAnyType;

            // now compute the dimensions
            if (V_ISBYREF(pvar))
            {
                psa = *V_ARRAYREF(pvar);
            }
            else
            {
                psa = V_ARRAY(pvar);
            }
                    
            if (psa == NULL)
            {
                // we got the case of a NULL array parameter, we handle this
                // as a one-dimensional array with 0 elements
                uiDim = 1;
            }
            else
            {
                // get the needed information from the array
                uiDim = SafeArrayGetDim(psa);       // Dimension of the array
                CHK_BOOL(uiDim >= 1, E_INVALIDARG); // need at least one dimension
            }
        
            for (UINT iDimension = 1; iDimension <= uiDim; iDimension ++)
            {
                LONG lLBound, lUBound;  // the array boundaries
                long lElementCount;
                WCHAR acBuffer[32];

                if (psa)
                {
                    CHK (SafeArrayGetLBound(psa, iDimension, &lLBound));
                    CHK (SafeArrayGetUBound(psa, iDimension, &lUBound));
                }
                else
                {
                    lLBound = 0;
                    lUBound = -1;
                }
        
                lElementCount = lUBound-lLBound+1;
                CHK_BOOL(lElementCount >= 0, E_INVALIDARG);

                swprintf(acBuffer, L"%10d", lElementCount);
                if ( (&adim) && (wcslen(&adim) > 0))
                {
                    CAutoFormat acopy;

                    acopy.copy(&adim);
                    adim.sprintf(L"%s,%s", &acopy, acBuffer);
                }
                else
                {
                    adim.sprintf(L"%s", acBuffer);
                }
            }

            af.sprintf(L"<complexContent xmlns='%s' xmlns:senc='http://schemas.xmlsoap.org/soap/encoding/' xmlns:wsdl='http://schemas.xmlsoap.org/wsdl/'><restriction base='senc:Array'><attribute ref='senc:arrayType' wsdl:arrayType='%s[%s]'/></restriction></complexContent>",
                      _XSDSchemaNS(m_enRevision), pcType, &adim);
        }
        else if ((enxsd == enXSDbinary) && (m_enRevision == enSchema2000))
        {
            af.sprintf(L"<s:simpleType xmlns:s='%s'> <s:restriction base= '%s'><s:encoding value= '%s'></s:encoding></s:restriction></s:simpleType>",_XSDSchemaNS(m_enRevision), L"binary", L"base64");
        }
        else
        {
            // simple fragment, enough for init to recognize the active schema
            af.sprintf(L"<s:complextype xmlns:s='%s'></s:complextype>", _XSDSchemaNS(m_enRevision));
        }
        
        CHK (pDOM->loadXML(&af, &vb));
        CHK (pDOM->QueryInterface(IID_IXMLDOMNode, (void **)&pNode));
        
        CHK(m_ptypeFactory->getMapper(enxsd, pNode, &ptm)); // we require the type on the schema
    }

    
    // at this point in time we have a mapper for the variant, now we output the XSI for the type and 
    // then hand of the real work to the mapper;
    if ( (enxsd != enXSDarray) && (enxsd != enTKempty) )
    {   
        // if we are not in the array case, we have to define some namespaces 
        // and generate the XSI type
        CAutoBSTR bstrXSIPrefix;
        CAutoBSTR bstrXSDPrefix;
        WCHAR acBuffer[_MAX_ATTRIBUTE_LEN+1];
                
        // figure out the XSI namepace
        CHK(_XSDAddSchemaInstanceNS(pSoapSerializer, m_enRevision, &bstrXSIPrefix));        
        // figure out the XSD namepace
        CHK(_XSDAddSchemaNS(pSoapSerializer, m_enRevision, &bstrXSDPrefix));        

        wcscpy(acBuffer, bstrXSDPrefix);
        wcscat(acBuffer, L":");
        wcscat(acBuffer, xsdTypeToXsdString(m_enRevision, enxsd));
            
        // add the XSI attribute
          CHK(pSoapSerializer->SoapAttribute((BSTR) g_pwstrXSDtype, NULL, (BSTR)acBuffer, bstrXSIPrefix));
    }

    
    // now please write the variant
    CHK(ptm->write(pSoapSerializer, bstrEncoding, enStyle, 0, pvar));
    
    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEmptyTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEmptyTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = S_OK;
    
    // start initializing
    CHK(CTypeMapper::init(ptypeFactory, pSchema, xsdType));

    CHK(_XSDFindRevision(pSchema, &m_enRevision));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEmptyTypeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      reading a string out of a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEmptyTypeMapper::read(IXMLDOMNode *_pNodeOfMapper, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr;
    CAutoRefc<IXMLDOMNode> pChild(NULL);
    CAutoRefc<IXMLDOMNode> pNodeOfMapper(_pNodeOfMapper);
    DOMNodeType dnt;

    // make sure we got a node
    CHK_BOOL(pNodeOfMapper, E_INVALIDARG);
    // and create an additional refcount, since we are handling it in a CAutoRefc
    pNodeOfMapper->AddRef();

    // follow the Href if necessary
    CHK (FollowHref(&pNodeOfMapper));

    CHK_BOOL(pvar, E_INVALIDARG);
    ::VariantInit(pvar);

    // verify that we really got an empty
    pNodeOfMapper->get_firstChild(&pChild);
    while (pChild)
    {
        CAutoRefc<IXMLDOMNode> pTemp(NULL);

        CHK (pChild->get_nodeType(&dnt));
        if (dnt == NODE_ELEMENT)
            break;  // we are on an element node
        
        // read over other nodes someone might have put into the message
        CHK (pChild->get_nextSibling(&pTemp));
        pChild.Clear();
        pChild = pTemp.PvReturn();
    }    

    CHK_BOOL(pChild == NULL, E_INVALIDARG);
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEmptyTypeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
//
//  parameters:
//
//  description:
//      mapping a string into a soap message
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEmptyTypeMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar)
{
    HRESULT hr=S_OK;
    CAutoBSTR bstrXSIPrefix;
    CAutoBSTR bstrTKPrefix;
       
    WCHAR acBuffer[_MAX_ATTRIBUTE_LEN+1];

    CHK_BOOL(pvar, E_INVALIDARG);
    CHK_BOOL(V_VT(pvar) == VT_EMPTY, E_INVALIDARG);
                    
    // figure out the XSI namepace
    CHK(_XSDAddSchemaInstanceNS(pSoapSerializer, m_enRevision, &bstrXSIPrefix));        
    
    // figure out the toolkit data namespace
    CHK(_XSDAddTKDataNS(pSoapSerializer, m_enRevision, &bstrTKPrefix));        

    wcscpy(acBuffer, bstrTKPrefix);
    wcscat(acBuffer, L":");
    wcscat(acBuffer, g_pwstrEmptyType);
            
    // add the XSI attribute
    CHK(pSoapSerializer->SoapAttribute((BSTR) g_pwstrXSDtype, NULL, (BSTR)acBuffer, bstrXSIPrefix));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


