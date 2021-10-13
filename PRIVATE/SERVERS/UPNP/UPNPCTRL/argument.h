//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __ARGUMENT__
#define __ARGUMENT__

#include "string.hxx"
#include "variant.h"
#include "vector.hxx"

class StateVar;

// Argument
class Argument
{
public:
    Argument(LPCWSTR pwszName, LPCWSTR pwszStateVariable, bool bRetval)
        : m_strName(pwszName),
          m_strStateVar(pwszStateVariable),
          m_bRetval(bRetval),
          m_pStateVar(NULL)
    {}

    // SetValue
    void SetValue(const VARIANTARG& varValue)
        {m_varValue = varValue; }

    void SetValue(LPCWSTR pwszValue)
        {DecodeValue(pwszValue); }

    // GetValue
    HRESULT GetValue(VARIANTARG* pvarValue) const;

    // GetName
    LPCWSTR GetName() const
        {return m_strName; }

    // IsRetval
    bool IsRetval() const
        {return m_bRetval; }

    // Reset
    void Reset()
        {m_varValue.Clear(); }

    // BindStateVar
    void BindStateVar(ce::vector<StateVar>::const_iterator itBegin, ce::vector<StateVar>::const_iterator itEnd);
    void BindStateVar(const StateVar* pStateVar)
        {m_pStateVar = pStateVar; }

    // GetVartype
    VARTYPE GetVartype() const;
    
    // ToString
    LPCWSTR ToString() const;
    
protected:
    bool EncodeValue() const;
    void DecodeValue(LPCWSTR pwszValue);    

protected:
    ce::wstring         m_strName;
    ce::wstring         m_strStateVar;
    bool                m_bRetval;
    ce::variant         m_varValue;
    mutable ce::wstring m_strValue;
    const StateVar*     m_pStateVar;
};

#endif // __ARGUMENT__
