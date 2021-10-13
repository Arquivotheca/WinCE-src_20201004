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
//  File:       X M L I F . C P P
//
//  Contents:   The C interface to the XML parser.
//
//----------------------------------------------------------------------------



#include "xmlmain.h"

//////////
// XMLGetFirstChild
//      Obtain the child node of specified element.
//
// PARAMETERS
//      hElem:      Handle to the current element.
//
// RETURNS
//      HELEMENT    Handle to first child, or NULL if no children were found.
//
HELEMENT    XMLGetFirstChild(HELEMENT hElem)        
{ 
    return ((CElem*)hElem)->m_pFirstChild; 

} 

//////////
// XMLGetNextSibling
//      Obtain the sibling node of specified element.
//
// PARAMETERS
//      hElem:      Handle to the current element.
//
// RETURNS
//      HELEMENT    Handle to next sibling, or NULL if no sibling was found.
//
HELEMENT    XMLGetNextSibling(HELEMENT hElem)        
{ 
    return ((CElem*)hElem)->m_pNextSibling; 
}

//////////
// XMLGetFirstAttribute
//      Obtain the first attribute of specified element.
//
// PARAMETERS
//      hElem:       Handle to the current element.
//
// RETURNS
//      HATTRIBUTE   Handle to first attribute, or NULL if no attributes were found.
//
HATTRIBUTE  XMLGetFirstAttribute(HELEMENT hElem)     
{ 
    return ((CElem*)hElem)->m_pFirstAttrib; 
}

//////////
// XMLGetNextAttribute
//      Obtains the next attribute after specified attribute.
//
// PARAMETERS
//      hAttrib:        Handle to the current attribute.
//
// RETURNS
//      HATTRIBUTE      Handle to the next attribute, or NULL if no attributes were found.
//
HATTRIBUTE  XMLGetNextAttribute(HATTRIBUTE hAttrib)  
{ 
    return ((CAttrib*)hAttrib)->m_pNextAttrib; 
}

//////////
// XMLGetElementName
//      Obtain the name of the specified element.
//
// PARAMETERS
//      hElem:   Handle to the current element.
//
// RETURNS
//      PCSTR    Pointer to the name string.
//
PCSTR       XMLGetElementName(HELEMENT hElem)        
{ 
    return ((CElem*)hElem)->m_pszName; 
}

//////////
// XMLGetElementNameSpace
//      Obtain the namespace prefix of the specified element.
//
// PARAMETERS
//      hElem:   Handle to the current element.
//
// RETURNS
//      PCSTR    Pointer to the prefix string.
//
PCSTR       XMLGetElementNameSpace(HELEMENT hElem)   
{ 
    return ((CElem*)hElem)->m_pszPrefix; 
}

//////////
// XMLGetElementValue
//      Obtain the  value string (contents) of the specified element.
//
// PARAMETERS
//      hElem:   Handle to the current element.
//
// RETURNS
//      PCSTR    Pointer to the value string.
//
PCSTR       XMLGetElementValue(HELEMENT hElem)       
{ 
    return ((CElem*)hElem)->m_pszValue; 
}

//////////
// XMLGetAttributeName
//      Obtain the name of the specified attribute.
//
// PARAMETERS
//      hAttrib:   Handle to the current attribute.
//
// RETURNS
//      PCSTR      Pointer to the attribute name string.
//
PCSTR       XMLGetAttributeName(HATTRIBUTE hAttrib)  
{ 
    return ((CAttrib*)hAttrib)->m_pszName; 
}

//////////
// XMLGetAttributeNameSpace
//      Obtain the namespace prefix of the specified attribute.
//
// PARAMETERS
//      hAttrib:   Handle to the current attribute.
//
// RETURNS
//      PCSTR      Pointer to the namespace prefix string.
//
PCSTR       XMLGetAttributeNameSpace(HATTRIBUTE hAttrib)  
{ 
    return ((CAttrib*)hAttrib)->m_pszPrefix; 
}

//////////
// XMLGetAttributeValue
//      Obtain the value of the specified attribute.
//
// PARAMETERS
//      hAttrib:   Handle to the current attribute.
//
// RETURNS
//      PCSTR      Pointer to the value string.
//
PCSTR       XMLGetAttributeValue(HATTRIBUTE hAttrib) 
{ 
    return ((CAttrib*)hAttrib)->m_pszValue; 
}


//////////
// XMLFree
//      Frees the parse tree, including attribute and element strings.
//
// PARAMETERS
//      hRoot:  Handle to the root element to free.
//
// RETURNS
//      Nothing.
//
VOID XMLFree(HELEMENT hRoot)
{
    delete ((CElem*)hRoot);
    
}


//////////
// XMLParse
//      Parses an XML document. This function processes the XML string 
//      and produces a tree of nodes.  After this, the other XMLif functions 
//      (above) can be used to traverse the tree.
//
// PARAMETERS
//      pszInput:   NULL-terminated XML string.
//      phRoot:     On OUTPUT, returns handle to the root element.
//
// RETURNS
//      DWORD       0 if successful.
//                  1 if a parsing error occurred.
//
DWORD XMLParse(PCSTR pszInput, HELEMENT* phRoot)
{
    PSTR pszCurr = (PSTR)pszInput;
    pszOrigBuf = pszCurr;
    CElem* pRoot = ParseElem(pszCurr);
    if(pRoot)
    {
        *phRoot = (HELEMENT)pRoot;
        return 0;
    }
    return 1;
    
}

