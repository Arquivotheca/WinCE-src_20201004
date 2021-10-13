//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
