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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       DxXML.h
//
//  Contents:   Header file for interface to the XML parser
//
//----------------------------------------------------------------------------
#pragma once

#include <windows.h>

#define ZONE_XMLPARSE   0
#define ZONE_XMLERROR   1

// Define debug messages for CE/Not CE.
#ifndef UNDER_CE
#ifdef TESTXML
#define DEBUGMSG(x, y)  if(x) printf y
#define ASSERT(x)   { if(!(x)) puts("Assertion failed!"); }
#else
#define DEBUGMSG(x, y)  
#define ASSERT(x)   
#endif
#endif

#define MySzAllocA(n)       ((PSTR)LocalAlloc(LPTR, (1+(n))))
#define MyFree(p)			{ if(p) LocalFree((PVOID)p); }
#define XMLPARSE_INPLACE    1

class CXMLElement;

// XML attributes class.
class CXMLAttribute
{
public:    
    CXMLAttribute( PCSTR pszName, 
				PCSTR pszPrefix, 
				PCSTR pszValue, 
				CXMLAttribute* pNextAttrib,
                LONG lLineNumber );    
    virtual ~CXMLAttribute();

public:
	PCSTR GetName() { return m_pszName; }
	PCSTR GetNameSpace() { return m_pszPrefix; }
	PCSTR GetValue() { return m_pszValue; }
    int GetLineNumber() { return m_cLineNumber; }

	// Obtain the next/prev attribute; obtains a handle to the attribute. 
	CXMLAttribute *GetNextAttribute() { return m_pNextAttrib; }
	CXMLAttribute *GetPrevAttribute() { return m_pPrevAttrib; }
	CXMLElement *GetParentElement() { return m_pParent; }
	
	void SetNextAttribute( CXMLAttribute *pNextAttrib ) { m_pNextAttrib = pNextAttrib; }
	void SetPrevAttribute( CXMLAttribute *pPrevAttrib ) { m_pPrevAttrib = pPrevAttrib; }
	void SetParentElement( CXMLElement *pParent ) { m_pParent = pParent; }
private:
    PCSTR    m_pszName;         // Name of the attribute.
    PCSTR    m_pszValue;        // Actual value of the attribute.
    PCSTR    m_pszPrefix;       // Namespace qualifier.
    LONG     m_cLineNumber;
    CXMLAttribute* m_pNextAttrib;  // Link to the next attribute.
	CXMLAttribute* m_pPrevAttrib;	// Link to the previous attribute.	
	CXMLElement* m_pParent;			// Link to the parent element.	
};


// XML element class, nested XML element class.
class CXMLElement
{
public: 
    CXMLElement( PCSTR pszName, 
			  PCSTR pszPrefix, 
			  PCSTR pszValue, 
			  CXMLAttribute* pFirstAttrib, 
			  CXMLElement* pFirstChild, 
			  CXMLElement* pNextSibling,
              LONG lLineNumber );    
    virtual ~CXMLElement();

public:
	PCSTR GetName() { return m_pszName; }
	PCSTR GetNameSpace() { return m_pszPrefix; }
	PCSTR GetValue() { return m_pszValue; }
    int GetLineNumber() { return m_cLineNumber; }

	// Obtain the first child element; returns a handle to the element.
	CXMLElement *GetFirstChildElement() { return m_pFirstChild; }
	// Obtain the parent element; returns a handle to the element.
	CXMLElement *GetParentElement() { return m_pParent; }

	// Obtain the first attribute; obtains a handle to the attribute. 
	CXMLAttribute *GetFirstAttribute() { return m_pFirstAttrib; }

	// Obtain a sibling element; returns a handle to the element. 
	CXMLElement *GetNextSiblingElement() { return m_pNextSibling; }
	CXMLElement *GetPrevSiblingElement() { return m_pPrevSibling; }	

	void SetNextSiblingElement( CXMLElement *pNextSib ) { m_pNextSibling = pNextSib; }
	void SetPrevSiblingElement( CXMLElement *pPrevSib ) { m_pPrevSibling = pPrevSib; }
	void SetParentElement( CXMLElement *pParent ) { m_pParent = pParent; }

private:
    PCSTR    m_pszName;             // Name of the element.
    PCSTR    m_pszPrefix;           // Namespace qualifier.
    PCSTR    m_pszValue;            // Actual value of the element.
    LONG     m_cLineNumber;
    CXMLAttribute* m_pFirstAttrib;        // Link to the first attribute.
    CXMLElement*   m_pFirstChild;         // Link to the first child of this element.
	CXMLElement*   m_pParent;             // Link to the parent of this element.
    CXMLElement*   m_pNextSibling;        // Link to the next sibling of this element.
	CXMLElement*   m_pPrevSibling;        // Link to the Prev sibling of this element.
};

//---------- XML Interface functions ---------------------

// Parses an XML document; returns a handle to the root element.
BOOL
XMLParse( PSTR pszInput, CXMLElement *&pRoot, int * piErrorLine = NULL );

// Frees the parse tree; hRoot is the handle returned by XMLParse.
VOID
XMLFree( CXMLElement *pElement );           

HRESULT
XMLParseFile( PWSTR pwszFilename, CXMLElement *&pRoot, int * piErrorLine = NULL );

