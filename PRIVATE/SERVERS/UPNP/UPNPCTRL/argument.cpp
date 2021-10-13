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

#include "Argument.h"
#include "StateVar.h"


// ToString
LPCWSTR Argument::ToString() const
{
	if(EncodeValue())
		return m_strValue;
	else
		return NULL;
}


// GetValue
HRESULT Argument::GetValue(VARIANTARG* pvarValue) const
{
	if(m_varValue.vt == VT_EMPTY)
	{
		VariantClear(pvarValue);
		
		pvarValue->vt = VT_ERROR;
		pvarValue->scode = DISP_E_PARAMNOTFOUND;

		return S_OK;
	}
	
	return VariantCopy(pvarValue, const_cast<VARIANT*>(static_cast<const VARIANT*>(&m_varValue)));
}


// GetVartype
VARTYPE Argument::GetVartype() const
{
	if(m_pStateVar)
		return m_pStateVar->GetVartype();
	else
		return 0xFFFF;
}


// BindStateVar
void Argument::BindStateVar(ce::vector<StateVar>::const_iterator it, ce::vector<StateVar>::const_iterator itEnd)
{
	for(; it != itEnd; ++it)
		if(it->GetName() == m_strStateVar)
			m_pStateVar = &*it;
}


// EncodeValue
bool Argument::EncodeValue() const
{
	if(m_pStateVar)
		return m_pStateVar->Encode(m_varValue, &m_strValue);
	
	return false;
}


// DecodeValue
void Argument::DecodeValue(LPCWSTR pwszValue)
{
	Reset();
	
	if(m_pStateVar)
		m_pStateVar->Decode(pwszValue, &m_varValue);
	else
	{
		Assert(m_varValue.vt == VT_EMPTY);

		m_varValue.vt = VT_ERROR;
		m_varValue.scode = DISP_E_TYPEMISMATCH;
	}
}
