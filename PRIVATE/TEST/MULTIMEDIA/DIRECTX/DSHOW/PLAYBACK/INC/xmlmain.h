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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       X M L M A I N . H
//
//  Contents:   Head file for the XML parser structures and classes.
//
//----------------------------------------------------------------------------

// Include necessary headers
#include <windows.h>
#include "xmlif.h"              // Interface into the XML parser.
#include <stdio.h>

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
#define MyFree(p)       { if(p) LocalFree((PVOID)p); }

// XML attributes class.
class CAttrib
{
public:
    PCSTR    m_pszName;         // Name of the attribute.
    PCSTR    m_pszValue;        // Actual value of the attribute.
    PCSTR    m_pszPrefix;       // Namespace qualifier.
    CAttrib* m_pNextAttrib;     // Link to the next attribute.
    
    CAttrib(PCSTR pszName, PCSTR pszPrefix, PCSTR pszValue, CAttrib* pAttrib)
    {
        m_pszName = pszName;
        m_pszPrefix = pszPrefix;
        m_pszValue = pszValue;
        m_pNextAttrib = pAttrib;
    }
    
    ~CAttrib()
    {
        MyFree(m_pszName);
        MyFree(m_pszPrefix);
        MyFree(m_pszValue);
        delete m_pNextAttrib;
    }
};


// XML element class, nested XML element class.
class CElem
{
public:
    PCSTR    m_pszName;             // Name of the element.
    PCSTR    m_pszPrefix;           // Namespace qualifier.
    PCSTR    m_pszValue;            // Actual value of the element.
    CAttrib* m_pFirstAttrib;        // Link to the first attribute.
    CElem*   m_pFirstChild;         // Link to the first child of this element.
    CElem*   m_pNextSibling;        // Link to the next sibling of this element.
    
    CElem(PCSTR pszName, PCSTR pszPrefix, PCSTR pszValue, CAttrib* pAttrib, CElem* pChild, CElem* pSibling)
    {
        m_pszName = pszName;
        m_pszPrefix = pszPrefix;
        m_pszValue = pszValue;
        m_pFirstAttrib = pAttrib;
        m_pFirstChild = pChild;
        m_pNextSibling = pSibling;
    }
    
    ~CElem()
    {
        MyFree(m_pszName);
        MyFree(m_pszPrefix);
        MyFree(m_pszValue);
        delete m_pFirstAttrib;
        delete m_pFirstChild;
        delete m_pNextSibling;
    }
};

// Parse of the individual bits of an Element .
CElem* ParseElem(PSTR& pszCurr);

extern PCSTR pszOrigBuf;


