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
////////////////////////////////////////////////////////////////////////////////
//
//  XmlIterator.h:
//    Provides iteration of XML element node lists under a specified path.  Collects
//    'text' values from an arbitrary number of descendant elements and packages them
//    up for a single callback to the consumer.
//
//  Revision History:
//      Jonathan Leonard (a-joleo) : 12/15/2009 - Created.
//                       (a-joleo) :   2/3/2009 - Added support for capturing attrs.
//                       (a-joleo) :   3/7/2010 - Added support for capturing all subnodes.
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <list>
#include <map>

class IXmlIteratorCallback
{
public:
    typedef std::map<CComBSTR, CComBSTR> XmlNodeMap;
    virtual void Callback(const XmlNodeMap& nodes, const XmlNodeMap& attrs) = 0;
};

class XmlIterator
{
public:
    typedef std::list<CAdapt<CComBSTR>> SubNodeList;

    // Iterates the XML document and returns only the subnodes at the paths
    // indicated by subNodePaths under the pathToIterate.
    static void Iterate(IXmlIteratorCallback* callback, LPCWSTR xmlFilename,
        LPCWSTR pathToIterate, const SubNodeList& subNodePaths, bool bCaptureAttrs = false);

    // Iterates the XML document and returns all subnodes under pathToIterate
    static void Iterate(IXmlIteratorCallback* callback, LPCWSTR xmlFilename,
        LPCWSTR pathToIterate, bool bCaptureAttrs = false);
};