//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop

#include "variant.h"
#include "DataType.hxx"

#include "StateVar.h"

StateVar::type_pair StateVar::m_aTypes[StateVar::number_of_types] = 
    {
        L"ui1",          ui1,
        L"ui2",          ui2,        
        L"ui4",          ui4,        
        L"i1",           i1,         
        L"i2",           i2,         
        L"i4",           i4,         
        L"int",          Int,        
        L"r4",           r4,         
        L"r8",           r8,         
        L"number",       number,     
        L"fixed.14.4",   fixed_14_4, 
        L"float",        Float,      
        L"char",         Char,       
        L"string",       string,     
        L"date",         date,       
        L"dateTime",     dateTime,   
        L"dateTime.tz",  dateTime_tz,
        L"time",         time,       
        L"time.tz",      time_tz,    
        L"boolean",      boolean,    
        L"bin.base64",   bin_base64, 
        L"bin.hex",      bin_hex,    
        L"uri",          uri,        
        L"uuid",         uuid
    };


// StateVar
StateVar::StateVar(LPCWSTR pwszName, LPCWSTR pwszType, bool bSendEvents)
    : m_strName(pwszName),
      m_type(unknown_type),
      m_bSendEvents(bSendEvents)
{
    // make sure types array is initialized for all types
    Assert(m_aTypes[number_of_types - 1].type == number_of_types - 1);

    for(int i = 0; i < number_of_types; ++i)
        if(0 == _wcsicmp(pwszType, m_aTypes[i].pwszName))
        {
            m_type = m_aTypes[i].type;
            break;
        }
}


// GetVartype
VARTYPE StateVar::GetVartype() const
{
    // var types in order of StateVar::type
    static VARTYPE vartypes[] = {
        VT_UI1,             // ui1,    
        VT_UI2,             // ui2,        
        VT_UI4,             // ui4,        
        VT_I1,              // i1,         
        VT_I2,              // i2,         
        VT_I4,              // i4,         
        VT_I4,              // Int,        
        VT_R4,              // r4,         
        VT_R8,              // r8,         
        VT_BSTR,            // number,     
        VT_CY,              // fixed_14_4, 
        VT_R8,              // Float,      
        VT_UI2,             // Char,       
        VT_BSTR,            // string,     
        VT_DATE,            // date,       
        VT_DATE,            // dateTime,   
        VT_DATE,            // dateTime_tz,
        VT_DATE,            // time,       
        VT_DATE,            // time_tz,    
        VT_BOOL,            // boolean,    
        VT_ARRAY | VT_UI1,  // bin_base64  
        VT_ARRAY | VT_UI1,  // bin_hex,    
        VT_BSTR,            // uri,        
        VT_BSTR};           // uuid

    if(m_type < sizeof(vartypes)/sizeof(*vartypes))
        return vartypes[m_type];
    else
        return 0xFFFF;
}


// GetDataType
DataType StateVar::GetDataType() const
{
    // data types in order of StateVar::type
    static DataType datatypes[] = {
        DT_NONE,				// ui1,    
        DT_NONE,				// ui2,        
        DT_NONE,				// ui4,        
        DT_NONE,				// i1,         
        DT_NONE,				// i2,         
        DT_NONE,				// i4,         
        DT_NONE,				// Int,        
        DT_NONE,				// r4,         
        DT_NONE,				// r8,         
        DT_NONE,				// number,     
        DT_NONE,				// fixed_14_4, 
        DT_NONE,				// Float,      
        DT_NONE,				// Char,       
        DT_NONE,				// string,     
        DT_DATE_ISO8601,		// date,       
        DT_DATETIME_ISO8601,    // dateTime,   
        DT_DATETIME_ISO8601TZ,  // dateTime_tz,
        DT_TIME_ISO8601,        // time,       
        DT_TIME_ISO8601TZ,      // time_tz,    
        DT_NONE,				// boolean,    
        DT_NONE,				// bin_base64  
        DT_NONE,				// bin_hex,    
        DT_NONE,				// uri,        
        DT_NONE};				// uuid

    if(m_type < sizeof(datatypes)/sizeof(*datatypes))
        return datatypes[m_type];
    else
        return DT_NONE;
}


// Decode
bool StateVar::Decode(LPCWSTR pwszValue, VARIANT* pvarValue) const
{
	Assert(pvarValue);

	ce::variant		varTemp;
	const wchar_t*	pwcNext;
	int				cch;

	cch = wcslen(pwszValue);

	VariantClear(pvarValue);

	switch(m_type)
    {
        case StateVar::ui1:
        case StateVar::ui2:
        case StateVar::ui4: 
        case StateVar::i1:
        case StateVar::i2:
        case StateVar::i4:
        case StateVar::Int:
        case StateVar::r4:
        case StateVar::r8:
        case StateVar::number:
        case StateVar::fixed_14_4:
        case StateVar::Float:
        case StateVar::string:
        case StateVar::uri:
        case StateVar::uuid:
                            varTemp = pwszValue;
                            if(SUCCEEDED(VariantChangeType(pvarValue, &varTemp, 0, GetVartype())))
                                return true;
                            break;

        case StateVar::Char:
                            Assert(pvarValue->vt == VT_EMPTY);

                            pvarValue->vt = VT_UI2;
                            V_UI2(pvarValue) = *pwszValue;
                            return true;

        case StateVar::date:
        case StateVar::dateTime:
        case StateVar::dateTime_tz:
        case StateVar::time:
        case StateVar::time_tz:
							
							if(SUCCEEDED(ParseISO8601(pwszValue, cch, GetDataType(), &V_DATE(pvarValue), &pwcNext)))
							{
								pvarValue->vt = VT_DATE;
								return true;
							}
							break;


        case StateVar::bin_hex:
        case StateVar::bin_base64:
                            {
								Assert(pvarValue->vt == VT_EMPTY);
							
								SAFEARRAY		*psa;
								SAFEARRAYBOUND	rgsabound[1];
								BYTE			*pData;
								int				cbData;
								HRESULT			hr;
								
								if(StateVar::bin_hex == m_type)
								{
									cbData = (cch + 1)/2;
								}
								else
								{
									Assert(StateVar::bin_base64 == m_type);

									cbData = cch;
								}
								
								rgsabound[0].lLbound = 0;
								rgsabound[0].cElements = cbData;

								psa = SafeArrayCreate(VT_UI1, 1, rgsabound);
								
                                if(psa == NULL)
                                {
                                    pvarValue->vt = VT_ERROR;
                                    pvarValue->scode = E_OUTOFMEMORY;

                                    return false;
                                }

								SafeArrayAccessData(psa, (void**)&pData);
								
								if(StateVar::bin_hex == m_type)
								{
									hr = ParseBinHex(pwszValue, cch, pData, &cbData, &pwcNext);
								}
								else
								{
									Assert(StateVar::bin_base64 == m_type);

									hr = ParseBase64(pwszValue, cch, pData, &cbData, &pwcNext);
								}
								
								if(SUCCEEDED(hr))
								{
									pvarValue->parray = psa;
									pvarValue->vt = VT_ARRAY | VT_UI1;
									
									SafeArrayUnaccessData(psa);
									
									rgsabound[0].cElements = cbData;
									SafeArrayRedim(psa, rgsabound);
									
									return true;
								}
								else
								{
									SafeArrayUnaccessData(psa);
									SafeArrayDestroy(psa);
								}
							}
							break;

		case StateVar::boolean:
                            Assert(pvarValue->vt == VT_EMPTY);

                            if(0 == wcscmp(pwszValue, L"true") ||
                               0 == wcscmp(pwszValue, L"yes") ||
                               0 == wcscmp(pwszValue, L"1"))
                            {
                                pvarValue->vt = VT_BOOL;
                                pvarValue->boolVal = VARIANT_TRUE;
                                return true;
                            }

                            if(0 == wcscmp(pwszValue, L"false") ||
                               0 == wcscmp(pwszValue, L"no") ||
                               0 == wcscmp(pwszValue, L"0"))
                            {
                                pvarValue->vt = VT_BOOL;
                                pvarValue->boolVal = VARIANT_FALSE;
                                return true;
                            }
                            break;
        
        default:            Assert(pvarValue->vt == VT_EMPTY);
                            
                            pvarValue->vt = VT_ERROR;
                            pvarValue->scode = DISP_E_BADVARTYPE;
                            return false;
    }

	Assert(pvarValue->vt == VT_EMPTY);

    pvarValue->vt = VT_ERROR;
    pvarValue->scode = DISP_E_TYPEMISMATCH;
	
	return false;
}


// Encode
bool StateVar::Encode(const VARIANT& varValue, ce::wstring* pstrValue) const
{
	Assert(pstrValue);
	
	ce::variant varTemp;

	pstrValue->resize(0);

	if(FAILED(varTemp.ChangeType(GetVartype(), &varValue)))
		return true;
	
	switch(m_type)
    {
        case StateVar::ui1:
        case StateVar::ui2:
        case StateVar::ui4: 
        case StateVar::i1:
        case StateVar::i2:
        case StateVar::i4:
        case StateVar::Int:
        case StateVar::r4:
        case StateVar::r8:
        case StateVar::number:
        case StateVar::fixed_14_4:
        case StateVar::Float:
        case StateVar::string:
        case StateVar::uri:
        case StateVar::uuid:
                            if(SUCCEEDED(varTemp.ChangeType(VT_BSTR)))
                            {
                                *pstrValue = V_BSTR(&varTemp);
                                break;
                            }
                            break;

        case StateVar::Char:
                            Assert(varTemp.vt == VT_UI2);

                            pstrValue->assign(V_UI2(&varTemp));
                            break;

        case StateVar::date:
        case StateVar::dateTime:
        case StateVar::dateTime_tz:
        case StateVar::time:
        case StateVar::time_tz:
							Assert(varTemp.vt == VT_DATE);
							
							UnparseISO8601(pstrValue, GetDataType(), &V_DATE(&varTemp));
							break;

        case StateVar::bin_base64:
        case StateVar::bin_hex:
                            {
								Assert(varTemp.vt == (VT_ARRAY | VT_UI1));
							
								BYTE	*pData;
								int		cbData;
								long	lBound, uBound;

								SafeArrayGetLBound(varTemp.parray, 1, &lBound);
								SafeArrayGetUBound(varTemp.parray, 1, &uBound);

								cbData = uBound - lBound + 1;

								SafeArrayAccessData(varTemp.parray, (void**)&pData);

								if(StateVar::bin_hex == m_type)
								{
									UnparseBinHex(pstrValue, pData, cbData);
								}
								else
								{
									Assert(StateVar::bin_base64 == m_type);

									UnparseBase64(pData, cbData, pstrValue);
								}

								SafeArrayUnaccessData(varTemp.parray);
							}
							break;

        case StateVar::boolean:
                            Assert(varTemp.vt == VT_BOOL);

                            if(V_BOOL(&varTemp) != VARIANT_FALSE)
                                *pstrValue = L"1";
                            else
                                *pstrValue = L"0";
                            break;
    }

	return true;
}

