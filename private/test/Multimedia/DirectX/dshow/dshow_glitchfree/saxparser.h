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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once

#include "Scanner.h"

//
// SAXParser
//
class SAXParser
{
public:
	SAXParser();
	virtual ~SAXParser();

	// Derived class must implement the following pure virtual methods
	virtual bool StartDocument() = 0;
	virtual bool StartElement(const char *szElement, int cchElement) = 0;
	virtual bool Attribute(const char *szAttribute, int cchAttribute) = 0;
	virtual bool AttributeValue(const char *szAttributeValue, int cchAttributeValue) = 0;
	virtual bool Characters(const char *szCharacters, int cchCharacters) = 0;
	virtual bool EndElement(const char *szElement, int cchElement) = 0;
	virtual bool EndDocument() = 0;

	// Parse
	bool Parse(const char *szFilename);

protected:
	bool Document();
	bool Element();
	bool Name();
	bool Attributes();
	bool ElementCont();
	bool EndElement();
	bool Content();
	bool Elements();
	bool Characters();
	bool Attribute();
	bool AttributeName();
	bool AttributeValue();
	bool SkipComment();

protected:
	CScanner  m_scanner;
	char     *m_pszName;
	int       m_cchName;
	char     *m_pszAttributeName;
	int       m_cchAttributeName;
	char     *m_pszAttributeValue;
	int       m_cchAttributeValue;
	char     *m_pszCharacters;
	int       m_cchCharacters;
};
