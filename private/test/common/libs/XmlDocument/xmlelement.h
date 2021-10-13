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
//  XmlElement.h  
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "precomm.h"

#include "XmlElementList.h"
#include "XmlAttributeCollection.h"

//
//  Forward declaration because XmlElement, XmlElementList
//  have circular relationship
//

class XmlElementList;

class XmlElement
{
public:
    XmlElement(
        IXMLDOMNode* pNode);
    ~XmlElement();

    XmlElement(
        const XmlElement& element);

    const XmlElement& operator=(
        const XmlElement& rhs);

    HRESULT GetAttributes(
        XmlAttributeCollection** ppAttributes);
        
    HRESULT GetAttributeValue(
        __in PTSTR name,
        __out_ecount(size) PTSTR value,
        UINT size); 
        
    HRESULT GetChildren(
        XmlElementList** ppNodes);

    HRESULT GetElementsByTagName(
        __in PTSTR name,
        XmlElementList** ppElementList);
    
    HRESULT GetSingleElementByTagName(
        __in PTSTR name,
        XmlElement** ppElement);

    HRESULT GetTagName(
        __out_ecount(size) PTSTR name,
        UINT size);

    HRESULT GetText(
        __out_ecount(size) PTSTR text,
        UINT size);
    
    HRESULT GetXml(
        __out_ecount(size) PTSTR text,
        UINT size);

    HRESULT GetXmlSize(
        size_t* pSize);

private:
    IXMLDOMNode* m_pNode;

};

