//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      xsdtypes.h
//
// Contents:
//
//      This module defines the xsd to variant mapping table
//
//----------------------------------------------------------------------------------

#ifndef __XSDTYPES_H_INCLUDED__
#define __XSDTYPES_H_INCLUDED__

typedef unsigned long XSDConstrains;

typedef enum
{
    enSchemaInvalid = 0, 
    enSchema2001 = 1,        
    enSchema2000 = 2,
    enSchema1999 = 3,
    enSchemaAll   = 20,
    enSchemaLast = enSchema2001,
    
} schemaRevisionNr;


const XSDConstrains c_XCNone = 0x0000;
const XSDConstrains c_XClength=0x0001;
const XSDConstrains c_XCminLength=0x0002;
const XSDConstrains c_XCmaxLength=0x0004;
const XSDConstrains c_XCpattern=0x0008;
const XSDConstrains c_XCenumeration=0x0010;
const XSDConstrains c_XCwhiteSpace=0x0020;
const XSDConstrains c_XCmaxInclusive=0x0040;
const XSDConstrains c_XCmaxExclusive=0x0080;
const XSDConstrains c_XCminExclusive=0x0100;
const XSDConstrains c_XCminInclusive=0x020;
const XSDConstrains c_XCprecision=0x0400;
const XSDConstrains c_XCscale=0x0800;
const XSDConstrains c_XCencoding=0x1000;
const XSDConstrains c_XCduration=0x2000;
const XSDConstrains c_XCperiod=0x4000;

#define XC_String  (c_XClength | c_XCminLength | c_XCmaxLength | c_XCpattern | c_XCenumeration | c_XCwhiteSpace)
#define XC_Number  (c_XCpattern | c_XCwhiteSpace | c_XCenumeration | c_XCmaxInclusive | c_XCminInclusive | c_XCmaxExclusive | c_XCminExclusive)
#define XC_DateBase  XC_Number
#define XC_Binary  (c_XCencoding | XC_String)
#define XC_ID  (XC_Number | c_XClength | c_XCminLength | c_XCmaxLength)
#define XC_Decimal  ( XC_Number | c_XCprecision | c_XCscale)
#define XC_Time  (XC_DateBase | c_XCduration | c_XCperiod)
#define XC_Array  (c_XCNone)


HRESULT xsdVerifyenXSDType(const schemaRevisionNr enRevision, const TCHAR *pchVarType, enXSDType *plXSDType);
WCHAR * xsdTypeToXsdString(const schemaRevisionNr enRevision, enXSDType enType);
WCHAR * xsdVariantTypeToXsdString(const schemaRevisionNr enRevision, VARTYPE vtType);


enXSDType vtVariantTypeToXsdVariant(VARTYPE vtType);
HRESULT xsdVariantType(enXSDType enType, long *pvtType);

#endif  // __XSDTYPES_H_INCLUDED__
