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
//  File:       A U T O M A T I O N P R O X Y . C P P
//
//  Contents:   Implementation of the Automation Proxy class
//
//  Notes:
//
//  /09/25
//
//----------------------------------------------------------------------------

#include <windows.h>
#include <upnpdevapi.h>
#include <ncbase.h>
#include <ncdebug.h>
#include "trace.h"

#include "upnphost.h"
#include "AutomationProxy.h"
#include "DataType.hxx"
#include "variant.h"

#include "host_i.c"

/*
 * Enum SST_DATA_TYPE
 *
 * Possible types of state variables
 */

enum SST_DATA_TYPE
{                               // ----------------------
    SDT_STRING = 0,             // VT_BSTR
    SDT_NUMBER,                 // VT_BSTR
    SDT_INT,                    // VT_I4
    SDT_FIXED_14_4,             // VT_CY
    SDT_BOOLEAN,                // VT_BOOL
    SDT_DATETIME_ISO8601,       // VT_DATE
    SDT_DATETIME_ISO8601TZ,     // VT_DATE
    SDT_DATE_ISO8601,           // VT_DATE
    SDT_TIME_ISO8601,           // VT_DATE
    SDT_TIME_ISO8601TZ,         // VT_DATE
    SDT_I1,                     // VT_I1
    SDT_I2,                     // VT_I2
    SDT_I4,                     // VT_I4
    SDT_UI1,                    // VT_UI1
    SDT_UI2,                    // VT_UI2
    SDT_UI4,                    // VT_UI4
    SDT_R4,                     // VT_FLOAT
    SDT_R8,                     // VT_DOUBLE
    SDT_FLOAT,                  // VT_DOUBLE
    SDT_UUID,                   // VT_BSTR
    SDT_BIN_BASE64,             // VT_ARRAY
    SDT_BIN_HEX,                // VT_ARRAY
    SDT_CHAR,                   // VT_UI2 (a wchar)
    SDT_URI,                    // VT_BSTR
    //
    // note(cmr): ADD NEW VALUES IMMEDIATELY BEFORE THIS COMMENT
    //
    SDT_INVALID
};


struct DTNAME
{
    LPCWSTR         m_pszType;
    SST_DATA_TYPE   m_sdt;
};


// Lookup table, sorted by string
// Note: if you add strings here, you also have to add a
//       corresponding entry to SST_DATA_TYPE
//
static CONST DTNAME g_rgdtTypeStrings[] =
{
    {L"bin.base64",     SDT_BIN_BASE64},        // base64 as defined in the MIME IETF spec
    {L"bin.hex",        SDT_BIN_HEX},           // Hexadecimal digits representing octets VT_ARRAY safearray or stream 
    {L"boolean",        SDT_BOOLEAN},           // boolean, "1" or "0" VT_BOOL int 
    {L"char",           SDT_CHAR},              // char, string VT_UI2 wchar 
    {L"date",           SDT_DATE_ISO8601},      // date.iso8601, A date in ISO 8601 format. (no time) VT_DATE long 
    {L"dateTime",       SDT_DATETIME_ISO8601},  // dateTime.iso8601, A date in ISO 8601 format, with optional time and no optional zone. Fractional seconds may be as precise as nanoseconds. VT_DATE long 
    {L"dateTime.tz",    SDT_DATETIME_ISO8601TZ},// dateTime.iso8601tz, A date in ISO 8601 format, with optional time and optional zone. Fractional seconds may be as precise as nanoseconds. VT_DATE long 
    {L"fixed.14.4",     SDT_FIXED_14_4},        // fixed.14.4, Same as "number" but no more than 14 digits to the left of the decimal point, and no more than 4 to the right. VT_CY large_integer 
    {L"float",          SDT_FLOAT},             // float, Same as for "number." VT_R8 double 
    {L"i1",             SDT_I1},                // i1, A number, with optional sign, no fractions, no exponent. VT_I1 char 
    {L"i2",             SDT_I2},                // i2, " VT_I2 short 
    {L"i4",             SDT_I4},                // i4, " VT_I4 long
    {L"int",            SDT_INT},               // int, A number, with optional sign, no fractions, no exponent. VT_I4 long 
    {L"number",         SDT_NUMBER},            // number, A number, with no limit on digits, may potentially have a leading sign, fractional digits, and optionally an exponent. Punctuation as in US English. VT_R8 double 
    {L"r4",             SDT_R4},                // r4, Same as "number." VT_FLOAT float 
    {L"r8",             SDT_R8},                // r8, " VT_DOUBLE double 
    {L"string",         SDT_STRING},            // string, pcdata VT_BSTR BSTR 
    {L"time",           SDT_TIME_ISO8601},      // time.iso8601, A time in ISO 8601 format, with no date and no time zone. VT_DATE long 
    {L"time.tz",        SDT_TIME_ISO8601TZ},    // time.iso8601.tz, A time in ISO 8601 format, with no date but optional time zone. VT_DATE. long 
    {L"ui1",            SDT_UI1},               // ui1, A number, unsigned, no fractions, no exponent. VT_UI1 unsigned char 
    {L"ui2",            SDT_UI2},               // ui2, " VT_UI2 unsigned short 
    {L"ui4",            SDT_UI4},               // ui4, " VT_UI4 unsigned long
    {L"uri",            SDT_URI},               // uri, Universal Resource Identifier VT_BSTR BSTR 
    {L"uuid",           SDT_UUID},              // uuid, Hexadecimal digits representing octets, optional embedded hyphens which should be ignored. VT_BSTR GUID 
    //
    // note(cmr): ADD NEW VALUES IN ALPHABETICAL ORDER
    //
};

// NOTE: the order of elements in this array must correspond to the order of 
// elements in the SST_DATA_TYPE enum.
static CONST VARTYPE g_rgvtTypes[] =
{                               
    VT_BSTR,
    VT_BSTR,
    VT_I4,
    VT_CY,
    VT_BOOL,
    VT_DATE,
    VT_DATE,
    VT_DATE,
    VT_DATE,
    VT_DATE,
    VT_I1,
    VT_I2,
    VT_I4,
    VT_UI1,
    VT_UI2,
    VT_UI4,
    VT_R4,
    VT_R8,
    VT_R8,
    VT_BSTR,
    VT_ARRAY | VT_UI1,
    VT_ARRAY | VT_UI1,
    VT_UI2,
    VT_BSTR,
    //
    // note(cmr): ADD NEW VALUES IMMEDIATELY BEFORE THIS COMMENT. 
    //            If adding new values, see comment above.
    //
    VT_EMPTY
};


PWSTR			COMSzFromWsz(PCWSTR pszSource);
SST_DATA_TYPE	GetTypeFromString(LPCWSTR pszTypeString);
VARTYPE			GetVarTypeFromString(LPCWSTR pszTypeString);



////////////////////////////////////////////////////////////////////////////
// CUPnPAutomationProxy
CUPnPAutomationProxy::CUPnPAutomationProxy()
	: m_rgVariables(m_StateVars),
	  m_rgActions(m_Actions),
	  m_fInitialized(FALSE),
      m_pdispService(NULL),
      m_cVariables(0),
      m_cActions(0),
	  m_strType(NULL),
	  m_bRetval(false),
	  m_bSendEvents(false),
	  m_pCurrentAction(NULL)
{
}


CUPnPAutomationProxy::~CUPnPAutomationProxy()
{
    if (m_pdispService)
        m_pdispService->Release();
}


STDMETHODIMP
CUPnPAutomationProxy::Initialize(
    /*[in]*/   IUnknown    * punkSvcObject,
    /*[in]*/   LPWSTR      pszSvcDescription)
{
    HRESULT hr = S_OK;

    Assert(!m_fInitialized);

    hr = punkSvcObject->QueryInterface(IID_IDispatch,
                                       (void **) &m_pdispService);

    if (SUCCEEDED(hr))
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::Initialize(): "
                 "Successfully obtained IDispatch pointer on service");

        hr = Parse(pszSvcDescription);
    }

    if (SUCCEEDED(hr))
    {
        m_fInitialized = TRUE;
    }

    TraceError("CUPnPAutomationProxy::Initialize(): "
               "Exiting",
               hr);
    return hr;
}


STDMETHODIMP
CUPnPAutomationProxy::QueryStateVariablesByDispIds(
    /*[in]*/   DWORD       cDispIds,
    /*[in]*/   DISPID      * rgDispIds,
    /*[out]*/  DWORD       * pcVariables,
    /*[out]*/  LPWSTR      ** prgszVariableNames,
    /*[out]*/  VARIANT     ** prgvarVariableValues,
    /*[out]*/  LPWSTR      ** prgszVariableDataTypes)
{
    HRESULT hr = S_OK;
    DWORD   cDispIdsToLookUp;
    DISPID  * rgDispIdsToLookUp;
    DISPID  * rgDispIdsToLookUpMem = NULL;
    LPWSTR  * rgszVariableNames = NULL;
    VARIANT * rgvarVariableValues = NULL;
    LPWSTR  * rgszVariableDataTypes = NULL;
    DWORD   cVariables = 0;

    Assert(m_fInitialized);

    if (0 == cDispIds)
    {
        // This means return all variables. Make an array of all our dispids.
        cDispIdsToLookUp = 0;
        rgDispIdsToLookUp = rgDispIdsToLookUpMem = new DISPID[m_cVariables];

        if (rgDispIdsToLookUp)
        {
            for (DWORD i = 0; i < m_cVariables; i++)
            {
                if(m_rgVariables[i].bEvented)
                    rgDispIdsToLookUp[cDispIdsToLookUp++] = m_rgVariables[i].dispid;
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
            TraceError("CUPnPAutomationProxy::"
                       "QueryStateVariablesByDispIds(): "
                       "Could not allocate array of dispids",
                       hr);
        }
    }
    else
    {
        cDispIdsToLookUp = cDispIds;
        rgDispIdsToLookUp = rgDispIds;
    }

    if (SUCCEEDED(hr))
    {
        // Allocate output arrays of size cDispIds.

        rgszVariableNames = (LPWSTR *)CoTaskMemAlloc(
            cDispIdsToLookUp * sizeof(LPWSTR));
        rgvarVariableValues = (VARIANT *)CoTaskMemAlloc(
            cDispIdsToLookUp * sizeof(VARIANT));
        rgszVariableDataTypes = (LPWSTR *)CoTaskMemAlloc(
            cDispIdsToLookUp * sizeof(LPWSTR));

        // Fill in values.

        if (rgszVariableNames && rgvarVariableValues && rgszVariableDataTypes)
        {
            ZeroMemory(rgszVariableNames, cDispIdsToLookUp * sizeof(LPWSTR));
            ZeroMemory(rgvarVariableValues, cDispIdsToLookUp * sizeof(VARIANT));
            ZeroMemory(rgszVariableDataTypes, cDispIdsToLookUp * sizeof(LPWSTR));

            for (DWORD i = 0; SUCCEEDED(hr) && (i < cDispIdsToLookUp); i++)
            {
                UPNP_STATE_VARIABLE    * pVariable = NULL;

                cVariables++;

                pVariable = LookupVariableByDispID(rgDispIdsToLookUp[i]);

                if (pVariable)
                {
                    rgszVariableNames[i] = COMSzFromWsz(pVariable->strName);
                    rgszVariableDataTypes[i] = COMSzFromWsz(pVariable->strDataType);

                    if (rgszVariableNames[i] && rgszVariableDataTypes[i])
                    {
                        UINT uArgErr = 0;
                        DISPPARAMS dispparamsEmpty = {NULL, NULL, 0, 0};

                        hr = m_pdispService->Invoke(rgDispIdsToLookUp[i],
                                                    IID_NULL,
                                                    LOCALE_SYSTEM_DEFAULT,
                                                    DISPATCH_PROPERTYGET,
                                                    &dispparamsEmpty,
                                                    &rgvarVariableValues[i],
                                                    NULL,
                                                    &uArgErr);
                        if (SUCCEEDED(hr))
                        {
                        	hr = StringizeVariant(&rgvarVariableValues[i], pVariable->strDataType);
                        }

                        if (SUCCEEDED(hr))
                        {
                            TraceTag(ttidAutomationProxy,
                                     "CUPnPAutomationProxy::"
                                     "QueryStateVariablesByDispIds(): "
                                     "Successfully obtained value %S for "
                                     "dispid %d",
                                     V_BSTR(&rgvarVariableValues[i]),
                                     rgDispIdsToLookUp[i]);
                        }
                        else
                        {
                            TraceError("CUPnPAutomationProxy::"
                                       "QueryStateVariablesByDispIds(): "
                                       "IDispatch::Invoke failed",
                                       hr);
                        }
                    }
                    else
                    {
                        hr = E_OUTOFMEMORY;
                        TraceError("CUPnPAutomationProxy::"
                                   "QueryStateVariablesByDispIds(): "
                                   "Could not allocate name/data type strings",
                                   hr);
                    }
                }
                else
                {
                    hr = DISP_E_MEMBERNOTFOUND;
                }
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
            TraceError("CUPnPAutomationProxy::"
                       "QueryStateVariablesByDispIds(): "
                       "Could not allocate output arrays",
                       hr);
        }
    }


    // Copy the output arrays to the out parameters.
    if (SUCCEEDED(hr))
    {
        *pcVariables = cVariables;
        *prgszVariableNames = rgszVariableNames;
        *prgvarVariableValues = rgvarVariableValues;
        *prgszVariableDataTypes = rgszVariableDataTypes;
    }
    else
    {
        // Clean up. Assume cVariables accurately describes the number
        // of initialized items in the arrays.

        if (rgszVariableNames)
        {
            for (DWORD i = 0; i < cVariables; i++)
            {
                if (rgszVariableNames[i])
                {
                    CoTaskMemFree(rgszVariableNames[i]);
                    rgszVariableNames[i] = NULL;
                }
            }

            CoTaskMemFree(rgszVariableNames);
            rgszVariableNames = NULL;
        }

        if (rgvarVariableValues)
        {
            for (DWORD i = 0; i < cVariables; i++)
            {
                VariantClear(&rgvarVariableValues[i]);
            }

            CoTaskMemFree(rgvarVariableValues);
            rgvarVariableValues = NULL;
        }

        if (rgszVariableDataTypes)
        {
            for (DWORD i = 0; i < cVariables; i++)
            {
                if (rgszVariableDataTypes[i])
                {
                    CoTaskMemFree(rgszVariableDataTypes[i]);
                    rgszVariableDataTypes[i] = NULL;
                }
            }

            CoTaskMemFree(rgszVariableDataTypes);
            rgszVariableDataTypes = NULL;
        }
    }

    // Clean up custom array of dispIds if we have one.
    if(rgDispIdsToLookUpMem)
    {
        delete [] rgDispIdsToLookUpMem;
        rgDispIdsToLookUpMem = rgDispIdsToLookUp = NULL;
        cDispIdsToLookUp = 0;
    }

    TraceError("CUPnPAutomationProxy::"
               "QueryStateVariablesByDispIds(): "
               "Exiting",
               hr);

    return hr;
}


STDMETHODIMP
CUPnPAutomationProxy::ExecuteRequest(
    /*[in]*/   UPNP_CONTROL_REQUEST    * pucreq,
    /*[out]*/  UPNP_CONTROL_RESPONSE   * pucresp)
{
    HRESULT hr = S_OK;
    
    Assert(m_fInitialized);

    if (lstrcmpW(pucreq->bstrActionName, L"QueryStateVariable") == 0)
    {
        hr = HrQueryStateVariable(pucreq, pucresp);
    }
    else
    {
        hr = HrInvokeAction(pucreq, pucresp);
    }

    TraceError("CUPnPAutomationProxy::ExecuteRequest(): "
               "Exiting",
               hr);

    return hr;
}

STDMETHODIMP
CUPnPAutomationProxy::GetVariableType(
    /*[in]*/   LPWSTR      pszVarName,
    /*[out]*/  BSTR        * pbstrType)
{
    HRESULT                hr = S_OK;
    BSTR                   bstrType = NULL;
    UPNP_STATE_VARIABLE    * pusv = NULL;

    Assert(m_fInitialized);

    pusv = LookupVariableByName(pszVarName);

    if (pusv)
    {
        Assert(pusv->strDataType);
        bstrType = SysAllocString(pusv->strDataType);
    }
    else
    {
        hr = E_INVALIDARG;
    }


    if (SUCCEEDED(hr))
    {
        *pbstrType = bstrType;
    }
    else
    {
        if (bstrType)
        {
            SysFreeString(bstrType);
            bstrType = NULL;
        }
    }

    TraceError("CUPnPAutomationProxy::GetVariableType(): "
               "Exiting",
               hr);

    return hr;
}


STDMETHODIMP
CUPnPAutomationProxy::GetOutputArgumentNamesAndTypes(
    /*[in]*/   LPWSTR      pszActionName,
    /*[out]*/  DWORD       * pcOutArguments,
    /*[out]*/  BSTR        ** prgbstrNames,
    /*[out]*/  BSTR        ** prgbstrTypes)
{
    HRESULT        hr = S_OK;
    DWORD          cOutArguments = 0;
    BSTR           * rgbstrNames = NULL;
    BSTR           * rgbstrTypes = NULL;
    UPNP_ACTION    * pua = NULL;
    
    Assert(m_fInitialized);

    pua = LookupActionByName(pszActionName);

    if (pua)
    {           
        // Allocate arrays for the names and data types. 
        cOutArguments = pua->cOutArgs;

        rgbstrNames = (BSTR *) CoTaskMemAlloc(cOutArguments * sizeof(BSTR));
        rgbstrTypes = (BSTR *) CoTaskMemAlloc(cOutArguments * sizeof(BSTR));

        if (rgbstrNames && rgbstrTypes)
        {
            for (DWORD i = 0; SUCCEEDED(hr) && (i < cOutArguments); i++)
            {
                const UPNP_STATE_VARIABLE    * pusvRelated = NULL;

                rgbstrNames[i] = SysAllocString(pua->rgOutArgs[i].strName);

                pusvRelated = pua->rgOutArgs[i].pusvRelated;

                Assert(pusvRelated);

                rgbstrTypes[i] = SysAllocString(pusvRelated->strDataType);

                if (rgbstrNames[i] && rgbstrTypes[i])
                {
                    TraceTag(ttidAutomationProxy,
                             "CUPnPAutomationProxy::"
                             "GetOutputArgumentNamesAndTypes(): "
                             "Successfully copied output argument %S "
                             "of type %S",
                             rgbstrNames[i],
                             rgbstrTypes[i]);
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::"
                               "GetOutputArgumentNamesAndTypes(): "
                               "Failed to allocate argument name and/or type",
                               hr);
                }
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
            TraceError("CUPnPAutomationProxy::"
                       "GetOutputArgumentNamesAndTypes(): "
                       "Failed to allocate output arrays",
                       hr);
        }
    }
    else
    {
        hr = E_INVALIDARG;
        TraceError("CUPnPAutomationProxy::"
                   "GetOutputArgumentNamesAndTypes(): "
                   "No such action",
                   hr);
    }

    // If successful, copy to the output, otherwise, clean up. 
    if (SUCCEEDED(hr))
    {
        *pcOutArguments = cOutArguments;
        *prgbstrNames = rgbstrNames;
        *prgbstrTypes = rgbstrTypes;
    }
    else
    {
        if (rgbstrNames)
        {
            for (DWORD i = 0; i < cOutArguments; i++)
            {
                if (rgbstrNames[i])
                {
                    SysFreeString(rgbstrNames[i]);
                    rgbstrNames[i] = NULL;
                }
            }
            CoTaskMemFree(rgbstrNames);
            rgbstrNames = NULL;
        }

        if (rgbstrTypes)
        {
            for (DWORD i = 0; i < cOutArguments; i++)
            {
                if (rgbstrTypes[i])
                {
                    SysFreeString(rgbstrTypes[i]);
                    rgbstrTypes[i] = NULL;
                }
            }
            CoTaskMemFree(rgbstrTypes);
            rgbstrTypes = NULL;
        }

        cOutArguments = 0;
    }

    TraceError("CUPnPAutomationProxy::GetOutputArgumentNamesAndTypes(): "
               "Exiting",
               hr);

    return hr;
}



VOID 
CUPnPAutomationProxy::FreeControlResponse(
    UPNP_CONTROL_RESPONSE * pucresp)
{
    if (pucresp->bstrActionName)
    {
        SysFreeString(pucresp->bstrActionName);
        pucresp->bstrActionName = NULL;
    }

    UPNP_CONTROL_RESPONSE_DATA * pucrd = &pucresp->ucrData;

    if (pucresp->fSucceeded)
    {
        if (pucrd->Success.rgvarOutputArgs)
        {
            for (DWORD i = 0; i < pucrd->Success.cOutputArgs; i++)
            {
                VariantClear(&pucrd->Success.rgvarOutputArgs[i]);
            }

            CoTaskMemFree(pucrd->Success.rgvarOutputArgs);
            pucrd->Success.rgvarOutputArgs = NULL;
            pucrd->Success.cOutputArgs = 0;
        }
    }
    else
    {
        if (pucrd->Fault.bstrFaultCode)
        {
            SysFreeString(pucrd->Fault.bstrFaultCode);
            pucrd->Fault.bstrFaultCode = NULL;
        }

        if (pucrd->Fault.bstrFaultString)
        {
            SysFreeString(pucrd->Fault.bstrFaultString);
            pucrd->Fault.bstrFaultString = NULL;
        }

        if (pucrd->Fault.bstrUPnPErrorCode)
        {
            SysFreeString(pucrd->Fault.bstrUPnPErrorCode);
            pucrd->Fault.bstrUPnPErrorCode = NULL;
        }

        if (pucrd->Fault.bstrUPnPErrorString)
        {
            SysFreeString(pucrd->Fault.bstrUPnPErrorString);
            pucrd->Fault.bstrUPnPErrorString = NULL;
        }
    }
}



UPNP_STATE_VARIABLE *
CUPnPAutomationProxy::LookupVariableByDispID(DISPID dispid)
{
    UPNP_STATE_VARIABLE    * pusv = NULL;

    for (DWORD i = 0; i < m_cVariables; i++)
    {
        if (m_rgVariables[i].dispid == dispid)
        {
            pusv = &m_rgVariables[i];
            break;
        }
    }

    if (pusv)
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupVariableByDispID(): "
                 "DISPID %d corresponds to variable %S",
                 pusv->dispid,
                 pusv->strName);
    }
    else
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupVariableByDispID(): "
                 "DISPID %d does not match any variable",
                 dispid);
    }

    return pusv;
}


UPNP_STATE_VARIABLE *
CUPnPAutomationProxy::LookupVariableByName(LPCWSTR pwszName)
{
    UPNP_STATE_VARIABLE    * pusv = NULL;

    for (DWORD i = 0; i < m_cVariables; i++)
    {
        if (lstrcmpW(m_rgVariables[i].strName, pwszName) == 0)
        {
            pusv = &m_rgVariables[i];
            break;
        }
    }

    if (pusv)
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupVariableByName(): "
                 "Found %S in variable table",
                 pusv->strName);
    }
    else
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupVariableByName(): "
                 "%S does not match any variable in variable table",
                 pwszName);
    }

    return pusv;
}


UPNP_ACTION *
CUPnPAutomationProxy::LookupActionByName(LPCWSTR pwszName)
{
    UPNP_ACTION * pua = NULL;

    for (DWORD i = 0; i < m_cActions; i++)
    {
        if (lstrcmpW(m_rgActions[i].strName, pwszName) == 0)
        {
            pua = &m_rgActions[i];
            break;
        }
    }

    if (pua)
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupActionByName(): "
                 "Found %S in action table",
                 pua->strName);
    }
    else
    {
        TraceTag(ttidAutomationProxy,
                 "CUPnPAutomationProxy::LookupActionByName(): "
                 "%S does not match any action in action table",
                 pwszName);
    }

    return pua;
}


HRESULT 
CUPnPAutomationProxy::HrBuildFaultResponse(
    UPNP_CONTROL_RESPONSE_DATA * pucrd,
    LPCWSTR                    pcszFaultCode,
    LPCWSTR                    pcszFaultString,
    LPCWSTR                    pcszUPnPErrorCode,
    LPCWSTR                    pcszUPnPErrorString)
{
    HRESULT hr = S_OK;

    pucrd->Fault.bstrFaultCode = SysAllocString(pcszFaultCode);

    if (pucrd->Fault.bstrFaultCode)
    {
        pucrd->Fault.bstrFaultString = SysAllocString(pcszFaultString);

        if (pucrd->Fault.bstrFaultString)
        {
            pucrd->Fault.bstrUPnPErrorCode = SysAllocString(pcszUPnPErrorCode);

            if (pucrd->Fault.bstrUPnPErrorCode)
            {
                pucrd->Fault.bstrUPnPErrorString = SysAllocString(pcszUPnPErrorString);

                if (pucrd->Fault.bstrUPnPErrorString)
                {
                    TraceTag(ttidAutomationProxy,
                             "CUPnPAutomationProxy::HrBuildFaultResponse(): "
                             "Successfully built fault response: \n"
                             "\tFaultCode: %S\n"
                             "\tFaultString: %S\n"
                             "\tUPnPErrorCode: %S\n"
                             "\tUPnPErrorString: %S",
                             pucrd->Fault.bstrFaultCode,
                             pucrd->Fault.bstrFaultString,
                             pucrd->Fault.bstrUPnPErrorCode,
                             pucrd->Fault.bstrUPnPErrorString);
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::HrBuildFaultResponse(): "
                               "Failed to allocate UPnP error string",
                               hr);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                TraceError("CUPnPAutomationProxy::HrBuildFaultResponse(): "
                           "Failed to allocate UPnP Error code string",
                           hr);
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
            TraceError("CUPnPAutomationProxy::HrBuildFaultResponse(): "
                       "Failed to allocate fault string",
                       hr);
        }
    }
    else
    {
        hr = E_OUTOFMEMORY;
        TraceError("CUPnPAutomationProxy::HrBuildFaultResponse(): "
                   "Failed to allocate fault code string",
                   hr);
    }

    TraceError("CUPnPAutomationProxy::HrBuildFaultResponse(): "
               "Exiting",
               hr);
    return hr;
}


HRESULT 
CUPnPAutomationProxy::HrVariantInitForXMLType(VARIANT * pvar,
                                              LPCWSTR pcszDataTypeString)
{
    pvar->vt = GetVarTypeFromString(pcszDataTypeString);
	pvar->dblVal = 0;

	Assert(pvar->vt != VT_EMPTY);

	return S_OK;
}    
    

HRESULT
CUPnPAutomationProxy::StringizeVariant(VARIANT *pVar, LPCWSTR pwszType)
{
	Assert(pVar != NULL);
	Assert(pwszType != NULL);

	HRESULT			hr = S_OK;
    VARTYPE			vt = VT_ERROR;
	SST_DATA_TYPE	sdt = SDT_INVALID;

    if(!pVar)
		return E_POINTER;

	if(!pwszType)
		return E_POINTER;

	vt = GetVarTypeFromString(pwszType);
	sdt = GetTypeFromString(pwszType);

	// first make sure the variant contains a valid value
	if(FAILED(hr = VariantChangeType(pVar, pVar, 0, vt)))
		return hr;
	
	// encode the value as string
	switch(sdt)
	{
		case SDT_STRING:
		case SDT_NUMBER:
		case SDT_INT:
		case SDT_FIXED_14_4:
		case SDT_I1:
		case SDT_I2:
		case SDT_I4:
		case SDT_UI1:
		case SDT_UI2:
		case SDT_UI4:
		case SDT_R4:
		case SDT_R8:
		case SDT_FLOAT:
		case SDT_UUID:
		case SDT_URI:		// for these types standard VariantChangeType convertion is fine
							hr = VariantChangeType(pVar, pVar, 0, VT_BSTR);
							break;
		
		case SDT_CHAR:		Assert(pVar->vt == VT_UI2);

							pVar->vt = VT_BSTR;
							pVar->bstrVal = SysAllocStringLen(&V_UI2(pVar), 1);
							
							if(pVar->bstrVal == NULL)
								hr = E_OUTOFMEMORY;
							break;

		case SDT_BOOLEAN:	Assert(pVar->vt == VT_BOOL);
							
							// VARIANT_TRUE is -1 so change VT_BOOL variant to a VT_I4 with 1 or 0
							if(V_BOOL(pVar) != VARIANT_FALSE)
								pVar->lVal = 1;
							else
								pVar->lVal = 0;

							pVar->vt = VT_I4;
							
							hr = VariantChangeType(pVar, pVar, 0, VT_BSTR);
							break;

		case SDT_DATE_ISO8601:
		case SDT_DATETIME_ISO8601:
		case SDT_DATETIME_ISO8601TZ:
		case SDT_TIME_ISO8601:
		case SDT_TIME_ISO8601TZ:
							
							{
								DataType dt	= DT_NONE;

								switch(sdt)
								{
									case SDT_DATE_ISO8601:			dt = DT_DATE_ISO8601;
																	break;

									case SDT_DATETIME_ISO8601:		dt = DT_DATETIME_ISO8601;
																	break;

									case SDT_DATETIME_ISO8601TZ:	dt = DT_DATETIME_ISO8601TZ;
																	break;

									case SDT_TIME_ISO8601:			dt = DT_TIME_ISO8601;
																	break;

									case SDT_TIME_ISO8601TZ:		dt = DT_TIME_ISO8601TZ;
																	break;
								}
							
								Assert(dt != DT_NONE);
								Assert(pVar->vt == VT_DATE);
							
								ce::wstring strValue;
							
								hr = UnparseISO8601(&strValue, dt, &V_DATE(pVar));

								pVar->vt = VT_BSTR;
								pVar->bstrVal = SysAllocString(strValue);

								if(pVar->bstrVal == NULL)
									hr = E_OUTOFMEMORY;
							}
							break;

		case SDT_BIN_BASE64:
		case SDT_BIN_HEX:
							{
								Assert(pVar->vt == (VT_ARRAY | VT_UI1));
							
								BYTE		*pData;
								int			cbData;
								long		lBound, uBound;
								ce::wstring strValue;

								SafeArrayGetLBound(pVar->parray, 1, &lBound);
								SafeArrayGetUBound(pVar->parray, 1, &uBound);

								cbData = uBound - lBound + 1;

								SafeArrayAccessData(pVar->parray, (void**)&pData);

								if(SDT_BIN_HEX == sdt)
								{
									UnparseBinHex(&strValue, pData, cbData);
								}
								else
								{
									Assert(SDT_BIN_BASE64 == sdt);

									UnparseBase64(pData, cbData, &strValue);
								}

								SafeArrayUnaccessData(pVar->parray);

								VariantClear(pVar);

								pVar->vt = VT_BSTR;
								pVar->bstrVal = SysAllocString(strValue);

								if(pVar->bstrVal == NULL)
									hr = E_OUTOFMEMORY;
							}
							break;
	}    					

	Assert(pVar->vt == VT_BSTR);

	return hr;	
}


// Decode
HRESULT CUPnPAutomationProxy::Decode(LPCWSTR pwszValue, LPCWSTR pwszType, VARIANT* pvarValue) const
{
	Assert(pvarValue);
	Assert(pwszType);
	Assert(pwszValue);

	if(!pwszValue)
		return E_POINTER;

	if(!pwszType)
		return E_POINTER;

	if(!pvarValue)
		return E_POINTER;

	HRESULT			hr = DISP_E_TYPEMISMATCH;
    VARTYPE			vt = VT_ERROR;
	SST_DATA_TYPE	sdt = SDT_INVALID;
	ce::variant		varTemp;
	int				cch;

	vt = GetVarTypeFromString(pwszType);
	sdt = GetTypeFromString(pwszType);

	cch = wcslen(pwszValue);

	VariantClear(pvarValue);

	switch(sdt)
    {
        case SDT_STRING:
		case SDT_NUMBER:
		case SDT_INT:
		case SDT_FIXED_14_4:
		case SDT_I1:
		case SDT_I2:
		case SDT_I4:
		case SDT_UI1:
		case SDT_UI2:
		case SDT_UI4:
		case SDT_R4:
		case SDT_R8:
		case SDT_FLOAT:
		case SDT_UUID:
		case SDT_URI:		// for these types standard VariantChangeType convertion is fine
                            varTemp = pwszValue;
                            hr = VariantChangeType(pvarValue, &varTemp, 0, vt);
                            break;

        case SDT_CHAR:		Assert(pvarValue->vt == VT_EMPTY);

                            pvarValue->vt = VT_UI2;
                            V_UI2(pvarValue) = *pwszValue;

							hr = S_OK;
                            break;

        case SDT_DATE_ISO8601:
		case SDT_DATETIME_ISO8601:
		case SDT_DATETIME_ISO8601TZ:
		case SDT_TIME_ISO8601:
		case SDT_TIME_ISO8601TZ:
							{
								DataType		dt	= DT_NONE;
								const wchar_t*	pwcNext;

								switch(sdt)
								{
									case SDT_DATE_ISO8601:			dt = DT_DATE_ISO8601;
																	break;

									case SDT_DATETIME_ISO8601:		dt = DT_DATETIME_ISO8601;
																	break;

									case SDT_DATETIME_ISO8601TZ:	dt = DT_DATETIME_ISO8601TZ;
																	break;

									case SDT_TIME_ISO8601:			dt = DT_TIME_ISO8601;
																	break;

									case SDT_TIME_ISO8601TZ:		dt = DT_TIME_ISO8601TZ;
																	break;
								}
							
								Assert(dt != DT_NONE);

								if(SUCCEEDED(hr = ParseISO8601(pwszValue, cch, dt, &V_DATE(pvarValue), &pwcNext)))
									pvarValue->vt = VT_DATE;
							}
							break;

        case SDT_BIN_BASE64:
		case SDT_BIN_HEX:	{
								Assert(pvarValue->vt == VT_EMPTY);
							
								SAFEARRAY		*psa;
								SAFEARRAYBOUND	rgsabound[1];
								BYTE			*pData;
								int				cbData;
								const wchar_t*	pwcNext;
								
								if(SDT_BIN_HEX == sdt)
								{
									cbData = (cch + 1)/2;
								}
								else
								{
									Assert(SDT_BIN_BASE64 == sdt);

									cbData = cch;
								}
								
								rgsabound[0].lLbound = 0;
								rgsabound[0].cElements = cbData;

								psa = SafeArrayCreate(VT_UI1, 1, rgsabound);
								
                                if(psa == NULL)
                                    return E_OUTOFMEMORY;

								SafeArrayAccessData(psa, (void**)&pData);
								
								if(SDT_BIN_HEX == sdt)
								{
									hr = ParseBinHex(pwszValue, cch, pData, &cbData, &pwcNext);
								}
								else
								{
									Assert(SDT_BIN_BASE64 == sdt);

									hr = ParseBase64(pwszValue, cch, pData, &cbData, &pwcNext);
								}
								
								if(SUCCEEDED(hr))
								{
									pvarValue->parray = psa;
									pvarValue->vt = VT_ARRAY | VT_UI1;
									
									SafeArrayUnaccessData(psa);
									
									rgsabound[0].cElements = cbData;
									SafeArrayRedim(psa, rgsabound);
								}
								else
								{
									SafeArrayUnaccessData(psa);
									SafeArrayDestroy(psa);
								}
							}
							break;

		case SDT_BOOLEAN:	Assert(pvarValue->vt == VT_EMPTY);

                            if(0 == wcscmp(pwszValue, L"true") ||
                               0 == wcscmp(pwszValue, L"yes") ||
                               0 == wcscmp(pwszValue, L"1"))
                            {
                                pvarValue->vt = VT_BOOL;
                                pvarValue->boolVal = VARIANT_TRUE;
                                hr = S_OK;
                            }

                            if(0 == wcscmp(pwszValue, L"false") ||
                               0 == wcscmp(pwszValue, L"no") ||
                               0 == wcscmp(pwszValue, L"0"))
                            {
                                pvarValue->vt = VT_BOOL;
                                pvarValue->boolVal = VARIANT_FALSE;
                                hr = S_OK;
                            }
                            break;
    }

	return hr;
}


HRESULT 
CUPnPAutomationProxy::HrInvokeAction(
    UPNP_CONTROL_REQUEST    * pucreq,
    UPNP_CONTROL_RESPONSE   * pucresp)
{
    HRESULT                hr = S_OK;
    UPNP_CONTROL_RESPONSE  ucresp = {0};
    UPNP_ACTION            * pua = NULL;

    pua = LookupActionByName(pucreq->bstrActionName);

    if (pua)
    {
        // Check that we've got the right number of input arguments. 
        if (pua->cInArgs == pucreq->cInputArgs)
        {
            DWORD      cTotalArgs = 0;
            DWORD      cOutArgs = 0;
            VARIANTARG *rgvarg = NULL;
            VARIANTARG *rgvargData = NULL;
            VARIANT    varResult; 
            EXCEPINFO  excepInfo = {0};
            
            VariantInit(&varResult);

            // Build an array of arguments to pass to the service object.

            cTotalArgs = pua->cInArgs + pua->cOutArgs;

            if (pua->bRetVal)
            {
                Assert(cTotalArgs > 0);

                // In UTL, the retval is considered an out parameter. In the
                // automation world, it's considered separate, so reduce the
                // count of parameters by 1 if there is a retval.
                cTotalArgs--; 
            }

            cOutArgs = cTotalArgs - pua->cInArgs;

            if (cTotalArgs > 0)
            {
                rgvarg = new VARIANTARG[cTotalArgs];
                
				if (cOutArgs > 0)
                {
                    rgvargData = new VARIANTARG[cOutArgs];
                }

                if (rgvarg && (!cOutArgs || rgvargData))
                {
                    // Have to copy the arguments in reverse order. Out args
                    // go first.                     
                    
                    for (DWORD i = 0,
                         index = pua->cOutArgs - 1; 
                         SUCCEEDED(hr) && (i < cOutArgs); 
                         i++, index--)
                    {
                        const UPNP_STATE_VARIABLE * pusvRelated = NULL;

                        pusvRelated = pua->rgOutArgs[index].pusvRelated;

                        hr = HrVariantInitForXMLType(&rgvargData[i], pusvRelated->strDataType);

                        if (SUCCEEDED(hr))
                        {
                            rgvarg[i].vt = rgvargData[i].vt | VT_BYREF;
                            rgvarg[i].pdblVal = &(rgvargData[i].dblVal);

                            if (SUCCEEDED(hr))
                            {
                                TraceTag(ttidAutomationProxy,
                                         "CUPnPAutomationProxy::HrInvokeAction(): "
                                         "Successfully initialized output arg %d",
                                         i);
                            }
                            else
                            {
                                TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                                           "Failed to initialize output argument",
                                           hr);
                            }                        
                        }
                        else
                        {
                            TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                                       "Failed to initialize for XML data type",
                                       hr);
                        }
                    }

                    if (SUCCEEDED(hr))
                    {
                        // Now the in arguments. 
                        // i is the index into the array of arguments we'll 
                        // pass to IDispatch::Invoke. It starts at the first
                        // index after the out arguments. j is the index into
                        // the array of input arguments - it starts at the last
                        // and goes down to the first. 

                        for (DWORD i = cOutArgs, j = pucreq->cInputArgs - 1; 
                             i < cTotalArgs; 
                             i++, j--)
                        {
							Assert(pucreq->rgvarInputArgs[j].vt == VT_BSTR);

							VariantInit(&rgvarg[i]);

                            Decode(pucreq->rgvarInputArgs[j].bstrVal, pua->rgInArgs[j].pusvRelated->strDataType, &rgvarg[i]);
                        }
                    }
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                               "Failed to allocate arguments array",
                               hr);
                }
            }
            else
            {
                rgvarg = NULL;
            }

            // Now we have the arguments sorted out. Execute the request. 

            if (SUCCEEDED(hr))
            {
                DISPPARAMS actionParams;
                
                actionParams.rgvarg = rgvarg;
                actionParams.cArgs = cTotalArgs;
                actionParams.rgdispidNamedArgs = NULL;
                actionParams.cNamedArgs = 0;

                hr = m_pdispService->Invoke(pua->dispid,
                                            IID_NULL,
                                            LOCALE_SYSTEM_DEFAULT,
                                            DISPATCH_METHOD,
                                            &actionParams,
                                            &varResult,
                                            &excepInfo, 
                                            NULL);
            }

            // Build a response.

            if (SUCCEEDED(hr))
            {
                UPNP_CONTROL_RESPONSE_DATA * pucrd = NULL;

                TraceTag(ttidAutomationProxy,
                         "CUPnPAutomationProxy::HrInvokeAction(): "
                         "Action %S executed successfully",
                         pua->strName);

                ucresp.bstrActionName = SysAllocString(pua->strName);

                if (ucresp.bstrActionName)
                {
                    ucresp.fSucceeded = TRUE;                
                    pucrd = &ucresp.ucrData;

                    if (pua->cOutArgs > 0)
                    {
                        pucrd->Success.rgvarOutputArgs = (VARIANT *) CoTaskMemAlloc(
                            pua->cOutArgs * sizeof(VARIANT));

                        if (pucrd->Success.rgvarOutputArgs)
                        {
                            DWORD dwStartIndex = 0;

                            pucrd->Success.cOutputArgs = pua->cOutArgs;

                            if (pua->bRetVal)
                            {
                                VariantInit(&pucrd->Success.rgvarOutputArgs[0]);

                                hr = StringizeVariant(&varResult, pua->rgOutArgs[0].pusvRelated->strDataType);
								
								if (SUCCEEDED(hr))
									hr = VariantCopy(&pucrd->Success.rgvarOutputArgs[0], &varResult);

                                if (SUCCEEDED(hr))
                                {
                                    dwStartIndex = 1;
                                    TraceTag(ttidAutomationProxy,
                                             "CUPnPAutomationProxy::"
                                             "HrInvokeAction(): "
                                             "Successfully copied retval");
                                }
                                else
                                {
                                    TraceError("CUPnPAutomationProxy::"
                                               "HrInvokeAction(): "
                                               "Failed to copy retval",
                                               hr);
                                }
                            }

                            if (SUCCEEDED(hr))
                            {
                                for (DWORD i = 0,
                                     j = cOutArgs + dwStartIndex - 1; 
                                     SUCCEEDED(hr) && (i < cOutArgs); 
                                     i++, j--)
                                {
                                    VariantInit(&pucrd->Success.rgvarOutputArgs[j]);
                                    
									hr = StringizeVariant(&rgvargData[i], pua->rgOutArgs[j].pusvRelated->strDataType);
								
									if (SUCCEEDED(hr))
										hr = VariantCopy(&pucrd->Success.rgvarOutputArgs[j], &rgvargData[i]);

                                    if (SUCCEEDED(hr))
                                    {
                                        TraceTag(ttidAutomationProxy,
                                                 "CUPnPAutomationProxy::"
                                                 "HrInvokeAction(): "
                                                 "Successfully copied out arg %d",
                                                 j);
                                    }
                                    else
                                    {
                                        TraceError("CUPnPAutomationProxy::"
                                                   "HrInvokeAction(): "
                                                   "Failed to copy out arg",
                                                   hr);
                                    }
                                }
                            }

                        }
                        else
                        {
                            hr = E_OUTOFMEMORY;
                            TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                                       "Failed to allocate memory for out args",
                                       hr);
                        }

                    }
                    else
                    {
                        pucrd->Success.rgvarOutputArgs = NULL;
                        pucrd->Success.cOutputArgs = 0;
                    }
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                               "Failed to allocate memory for action name",
                               hr);
                }

            }
            else if (DISP_E_EXCEPTION == hr)
            {
                UPNP_CONTROL_RESPONSE_DATA * pucrd = NULL;               

                TraceTag(ttidAutomationProxy,
                         "CUPnPAutomationProxy::HrInvokeAction(): "
                         "Action %S returned an exception",
                         pua->strName);
                
                // Fix up the HRESULT. Even though this is an error in the
                // UPnP sense, we are returning success because from the 
                // processing point of view, the request went through correctly 
                // and just returned a fault response. 
                hr = S_OK;

                ucresp.bstrActionName = SysAllocString(pua->strName);

                if (ucresp.bstrActionName)
                {
                    ucresp.fSucceeded = FALSE;
                    pucrd = &ucresp.ucrData;

                    // If the service object requested deferred fill-in of
                    // the exception info, call its callback function now. 

                    if (excepInfo.pfnDeferredFillIn)
                    {
                        hr = (*(excepInfo.pfnDeferredFillIn))(&excepInfo);

                        if (SUCCEEDED(hr))
                        {
                            TraceTag(ttidAutomationProxy,
                                     "CUPnPAutomationProxy::HrInvokeAction(): "
                                     "Successfully filled in "
                                     "deferred exception info");                                    
                        }
                        else
                        {
                            TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                                       "Failed to fill in "
                                       "deferred exception info",
                                       hr);
                        }
                    }

                    if (SUCCEEDED(hr))
                    {
                        hr = HrBuildFaultResponse(pucrd,
                                                  L"SOAP-ENV:Client",
                                                  L"UPnPError",
                                                  excepInfo.bstrSource,
                                                  excepInfo.bstrDescription);
                    }
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                               "Failed to allocate memory for action name",
                               hr);
                }


            }
            else
            {
                TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
                           "Failed to invoke action",
                           hr);
            }

            // Cleanup. At this point, all the output information should be
            // in the ucresp structure. 
            if (rgvarg)
            {
				for (unsigned i = 0; i < cTotalArgs; i++)
					VariantClear(&rgvarg[i]);
			}

			if (rgvargData)
			{
				for (unsigned i = 0; i < cOutArgs; i++)
                    VariantClear(&rgvargData[i]);
            }

            delete [] rgvarg;
            rgvarg = NULL;

			delete [] rgvargData;
			rgvargData = NULL;

            VariantClear(&varResult);
        }
        else
        {
            // Invalid arguments.
            ucresp.fSucceeded = FALSE;
            hr = HrBuildFaultResponse(&ucresp.ucrData,
                                      L"SOAP-ENV:Client",
                                      L"UPnPError",
                                      L"402",
                                      L"Invalid Args");          
        }
    }
    else
    {
        // Invalid Action name
        ucresp.fSucceeded = FALSE;
        hr = HrBuildFaultResponse(&ucresp.ucrData,
                                  L"SOAP-ENV:Client",
                                  L"UPnPError",
                                  L"401",
                                  L"Invalid Action");
    }

    // If succeeded, copy the response info to the output structure, otherwise
    // free it. 

    if (SUCCEEDED(hr))
    {
        *pucresp = ucresp;
    }
    else
    {
        FreeControlResponse(&ucresp);
    }

    TraceError("CUPnPAutomationProxy::HrInvokeAction(): "
               "Exiting",
               hr);

    return hr;
}

HRESULT 
CUPnPAutomationProxy::HrQueryStateVariable(
    UPNP_CONTROL_REQUEST    * pucreq,
    UPNP_CONTROL_RESPONSE   * pucresp)
{
    HRESULT                hr = S_OK;
    UPNP_CONTROL_RESPONSE  ucresp = {0};
    UPNP_STATE_VARIABLE    * pusv = NULL;
    BSTR                   bstrVarName = NULL;
    
    // QueryStateVariable should have 1 input argument which is the variable
    // name. 

    Assert(pucreq->cInputArgs == 1);
    Assert(pucreq->rgvarInputArgs[0].vt == VT_BSTR);

    bstrVarName = V_BSTR(&pucreq->rgvarInputArgs[0]);
    
    pusv = LookupVariableByName(bstrVarName);

    if (pusv)
    {
        DISPPARAMS dispparamsEmpty = {NULL, NULL, 0, 0};
        VARIANT    varResult;
        EXCEPINFO  excepInfo = {0};

        VariantInit(&varResult);

        // Query the value. 

        hr = m_pdispService->Invoke(pusv->dispid,
                                    IID_NULL,
                                    LOCALE_SYSTEM_DEFAULT,
                                    DISPATCH_PROPERTYGET,
                                    &dispparamsEmpty,
                                    &varResult,
                                    &excepInfo,
                                    NULL);

        // Build a response.

        if (SUCCEEDED(hr))
        {
            UPNP_CONTROL_RESPONSE_DATA * pucrd = NULL;
            
            TraceTag(ttidAutomationProxy,
                     "CUPnPAutomationProxy::HrQueryStateVariable(): "
                     "PROPGET for %S succeeded",
                     bstrVarName);

            ucresp.bstrActionName = SysAllocString(L"QueryStateVariable");

            if (ucresp.bstrActionName)
            {
                ucresp.fSucceeded = TRUE;                
                pucrd = &ucresp.ucrData;

                pucrd->Success.cOutputArgs = 1; 
                pucrd->Success.rgvarOutputArgs = (VARIANT *) CoTaskMemAlloc(
                    sizeof(VARIANT));

                if (pucrd->Success.rgvarOutputArgs)
                {
                    VariantInit(&pucrd->Success.rgvarOutputArgs[0]);

                    hr = StringizeVariant(&varResult, pusv->strDataType);

					if (SUCCEEDED(hr))
						hr = VariantCopy(&pucrd->Success.rgvarOutputArgs[0], &varResult);

                    if (SUCCEEDED(hr))
                    {
                        TraceTag(ttidAutomationProxy,
                                 "CUPnPAutomationProxy::HrQueryStateVariable(): "
                                 "Successfully copied %S to output", V_BSTR(&varResult));
                    }
                    else
                    {
                        TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                                   "Failed to copy result to output",
                                   hr);
                    }
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                    TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                               "Failed to allocate memory for output arg",
                               hr);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                           "Failed to allocate memory for action name",
                           hr);
            }

        }
        else if (DISP_E_EXCEPTION == hr)
        {
            UPNP_CONTROL_RESPONSE_DATA * pucrd = NULL;

            TraceTag(ttidAutomationProxy,
                     "CUPnPAutomationProxy::HrQueryStateVariable(): "
                     "PROPGET for %S returned an exception",
                     bstrVarName);

            // Fix up the HRESULT. Even though this is an error in the
            // UPnP sense, we are returning success because from the 
            // processing point of view, the request went through correctly 
            // and just returned a fault response. 
            hr = S_OK;

            ucresp.bstrActionName = SysAllocString(L"QueryStateVariable");

            if (ucresp.bstrActionName)
            {
                ucresp.fSucceeded = FALSE;
                pucrd = &ucresp.ucrData;

                // If the service object requested deferred fill-in of
                // the exception info, call its callback function now. 

                if (excepInfo.pfnDeferredFillIn)
                {
                    hr = (*(excepInfo.pfnDeferredFillIn))(&excepInfo);

                    if (SUCCEEDED(hr))
                    {
                        TraceTag(ttidAutomationProxy,
                                 "CUPnPAutomationProxy::HrQueryStateVariable(): "
                                 "Successfully filled in "
                                 "deferred exception info");                                    
                    }
                    else
                    {
                        TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                                   "Failed to fill in "
                                   "deferred exception info",
                                   hr);
                    }
                }

                if (SUCCEEDED(hr))
                {
                    hr = HrBuildFaultResponse(pucrd,
                                              L"SOAP-ENV:Client",
                                              L"UPnPError",
                                              excepInfo.bstrSource,
                                              excepInfo.bstrDescription);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                           "Failed to allocate memory for action name",
                           hr);
            }

        }
        else
        {
            TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
                       "PROPGET failed",
                       hr);
        }

        VariantClear(&varResult);
    }
    else
    {
        // Invalid variable name
        ucresp.fSucceeded = FALSE;
        hr = HrBuildFaultResponse(&ucresp.ucrData,
                                  L"SOAP-ENV:Client",
                                  L"UPnPError",
                                  L"404",
                                  L"Invalid Var");
    }

    // If succeeded, copy the response info to the output structure, otherwise
    // free it. 

    if (SUCCEEDED(hr))
    {
        *pucresp = ucresp;
    }
    else
    {
        FreeControlResponse(&ucresp);
    }

    TraceError("CUPnPAutomationProxy::HrQueryStateVariable(): "
               "Exiting",
               hr);

    return hr;
}


// yet another strdup
PWSTR COMSzFromWsz(PCWSTR pszSource)
{
	int cch;
	PWSTR pszDest;
	if (!pszSource)
		return NULL;
	cch = wcslen(pszSource)+1;
	pszDest = (PWSTR )CoTaskMemAlloc(cch*sizeof(WCHAR));
	if (pszDest)
	{
		memcpy(pszDest, pszSource, cch*sizeof(WCHAR));
	}
	return pszDest;
}


/*
 * Function:    GetTypeFromString()
 *
 * Purpose:     Returns the appropriate SDT_DATA_TYPE value for the given
 *              xml-data type string.
 *
 * Arguments:
 *  pszTypeString   [in]    The data type identifier whose SDT_DATA_TYPE value
 *                          is desired.
 *
 * Returns:
 *  If the string is found, it returns the appropriate value of the
 *  SST_DATA_TYPE enumeration.
 *  If the string is not found, it returns SDT_INVALID.
 *
 * Notes:
 *  The source string is compared to known strings in a case-insensitive
 *  comparison.
 *
 */
SST_DATA_TYPE
GetTypeFromString(LPCWSTR pszTypeString)
{
    // there must be an entry in g_rgdtTypeStrings for every real
    // value of SST_DATA_TYPE
    //
    Assert(SDT_INVALID == celems(g_rgdtTypeStrings));

    SST_DATA_TYPE sdtResult;

    sdtResult = SDT_INVALID;

    {
        // now search for the string in the list, using a
        // standard binary search
        //
        INT nLow;
        INT nMid;
        INT nHigh;

        nLow = 0; 
        nHigh = celems(g_rgdtTypeStrings) - 1;

        while (TRUE)
        {
            if (nLow > nHigh)
            {
                // not found
                //
                break;
            }

            nMid = (nLow + nHigh) / 2;

            {
                LPCWSTR pszCurrent;
                int result;

                pszCurrent = g_rgdtTypeStrings[nMid].m_pszType;

                result = _wcsicmp(pszTypeString, pszCurrent);
                if (result < 0)
                {
                    nHigh = nMid - 1;
                }
                else if (result > 0)
                {
                    nLow = nMid + 1;
                }
                else
                {
                    // found
                    //
                    sdtResult = g_rgdtTypeStrings[nMid].m_sdt;
                    break;
                }
            }
        }
    }

    return sdtResult;
}

VARTYPE
GetVarTypeFromString(LPCWSTR pszTypeString)
{
    SST_DATA_TYPE sdt = SDT_INVALID;

    sdt = GetTypeFromString(pszTypeString);

    return g_rgvtTypes[sdt];
}
