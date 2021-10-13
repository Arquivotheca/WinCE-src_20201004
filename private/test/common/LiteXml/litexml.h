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
#pragma once

#include <string.hxx>
#include <hash_map.hxx>
#include <list.hxx>

namespace litexml
{

typedef WCHAR		XmlChar_t,*LPXSTR;
typedef XmlChar_t	const *LPCXSTR;
typedef ce::wstring	XmlString_t;

#define XTEXT(__str)	L##__str
#define _X(__str)		L##__str

class XmlEscape_t
{
public:
    static XmlString_t Escape(const XmlString_t& text);
    static XmlString_t Unescape(const XmlString_t& xml);

private:
    static VOID Initialize();
private:
    static BOOL fInitialized;
    static ce::hash_map<XmlString_t,XmlString_t> EscapeToChar;
    static ce::hash_map<XmlString_t,XmlString_t> CharToEscape;

};

}	// namespace litexml

#include "lxmlErrors.h"
#include "lxmlDocument.h"
#include "lxmlSearch.h"