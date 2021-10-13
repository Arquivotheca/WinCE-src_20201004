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
#ifndef __STATE_VAR__
#define __STATE_VAR__

#include "string.hxx"

enum DataType;

class StateVar
{
public:
    enum type
    {
        ui1 = 0,
        ui2,
        ui4,
        i1,
        i2,
        i4,
        Int,
        r4,
        r8,
        number,
        fixed_14_4,
        Float,
        Char,
        string,
        date,
        dateTime,
        dateTime_tz,
        time,
        time_tz,
        boolean,
        bin_base64,
        bin_hex,
        uri,
        uuid,
        // add type before this comment
        number_of_types,
        unknown_type
    };

    StateVar(LPCWSTR pwszName, type t, bool bSendEvents)
        : m_strName(pwszName),
          m_type(t),
          m_bSendEvents(bSendEvents)
        {};

    StateVar(LPCWSTR pwszName, LPCWSTR pwszType, bool bSendEvents);

    VARTYPE GetVartype() const;

    LPCWSTR GetName() const
        {return m_strName; }

    bool Decode(LPCWSTR pwszValue, VARIANT* pvarValue) const;
    bool Encode(const VARIANT& varValue, ce::wstring* pstrValue) const;

protected:
    DataType GetDataType() const;
    
    struct type_pair
    {
        wchar_t*    pwszName;
        type        type;
    };

    type                m_type;
    ce::wstring         m_strName;
    bool                m_bSendEvents;
    static type_pair    m_aTypes[number_of_types];
};

#endif // __STATE_VAR__
