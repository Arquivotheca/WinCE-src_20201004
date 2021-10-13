//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
