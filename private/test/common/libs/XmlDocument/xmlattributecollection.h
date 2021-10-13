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
//  XmlAttributeCollection.h
//
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "precomm.h"
#include "XmlAttribute.h"


//
//  Forward declaration because of XmlAttribute, XmlAtrributelist
//  circular relationship
//

class XmlAttribute;

class XmlAttributeCollection
{
public:
    XmlAttributeCollection(
        IXMLDOMNode* pNode);
    ~XmlAttributeCollection();

    HRESULT GetByName(
        __in TCHAR* name,    
        XmlAttribute* pAttribute);

    HRESULT GetItem(
        XmlAttribute* pAttribute,
        UINT i);

    HRESULT GetLength(
        LONG* pLength);

    HRESULT GetNext(
        XmlAttribute* pAttribute);

    HRESULT Reset();

private:
    IXMLDOMNode* m_pNode;
    IXMLDOMNamedNodeMap* m_pAttributes;

private:
    XmlAttributeCollection(
        const XmlAttributeCollection& attributes);

    XmlAttributeCollection& operator=( 
        const XmlAttributeCollection& attributes);
};

