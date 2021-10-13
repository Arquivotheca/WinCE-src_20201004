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
//  XmlElementList.h
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "precomm.h"
#include "XmlElement.h"

//
//  Forward declaration because XmlElement, XmlElementList
//  have circular relationship
//

class XmlElement;

class XmlElementList
{
public:
    XmlElementList(
        IXMLDOMSelection* pNodes);
    ~XmlElementList();

    HRESULT GetItem(
        XmlElement** ppElement,
        UINT i);

    HRESULT GetLength(
        LONG* pLength);

    HRESULT GetNext(
        XmlElement** ppElement);

    HRESULT Reset();

private:
    IXMLDOMSelection* m_pNodes;

    XmlElementList(
        const XmlElementList& elements);

    XmlElementList& operator=(
        const XmlElementList& elements);
};


