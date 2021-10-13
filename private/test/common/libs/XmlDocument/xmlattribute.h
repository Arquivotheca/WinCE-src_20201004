//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  XmlAttribute.h  Attribute is a name value pair
//
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "precomm.h"
#include "XmlAttributeCollection.h"

//
//  Forward declaration because XmlElement, XmlElementList
//  have circular relationship
//
class XmlElementList;

class XmlAttribute
{

public:
    XmlAttribute();
    ~XmlAttribute();

    HRESULT GetName( 
        __out_ecount(size) TCHAR *name,
        UINT size);
        
    HRESULT GetValue(
        __out_ecount(size) TCHAR *value,
        UINT size);

    HRESULT Set(
        __in TCHAR* name, 
        __in TCHAR* value);

private:
    TCHAR* m_name;
    TCHAR* m_value;

    XmlAttribute(
        const XmlAttribute& attribute);

    XmlAttribute& operator =( 
        const XmlAttribute& attribute);
};

