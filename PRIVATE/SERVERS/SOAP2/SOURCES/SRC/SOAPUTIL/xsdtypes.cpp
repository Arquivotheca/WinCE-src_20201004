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
//      xsdtypes.cpp
//
// Contents:
//
//      Implementation of fixed size hash table that hashes on strings.
//
//----------------------------------------------------------------------------------

#include "headers.h"

///////////////////////////////////////////////////////////////////////////////
// Following table is used by WSDL Generator to convert VT datatypes
//  into xsd datatype strings
//  xmldom type is handled in the code

bool _compareRevisions(schemaRevisionNr enTarget, schemaRevisionNr enQuery);

typedef struct _XSDTypeEntries
{
    schemaRevisionNr m_enRevision;
    enXSDType        m_enXSDType;
    TCHAR *          m_xsdAsString;
    XSDConstrains    m_xsdContrains;
    VARTYPE          m_vtType;
} XSDTypeEntries;

typedef struct _VTToXSDType
{
    schemaRevisionNr m_enRevision;
    VARTYPE          m_vtType;
    WCHAR *          m_pwstrXSD;
    enXSDType        m_xsdVariant;     // allows to map from vtType to TypeMapper. used in 'anyType'-Mapper
} VTToXSDType;



const VTToXSDType g_VTToXSDMapping[] = {
    { enSchemaAll,  VT_BSTR,    L"string",          enXSDstring},
    { enSchemaAll,  VT_BOOL,    L"boolean",         enXSDboolean},
    { enSchema2000, VT_DATE,    L"timeInstant",     enXSDtimeInstant},
    { enSchema2001, VT_DATE,    L"dateTime",        enXSDtimeInstant},
    { enSchemaAll,  VT_DECIMAL, L"decimal",         enXSDdecimal},
    { enSchemaAll,  VT_I2,      L"short",           enXSDshort},
    { enSchemaAll,  VT_I4,      L"int",             enXSDint},
    { enSchemaAll,  VT_UI1,     L"unsignedByte",    enXSDunsignedByte},
    { enSchemaAll,  VT_INT,     L"int",             enXSDdecimal},
    { enSchemaAll,  VT_UINT,    L"unsignedInt",     enXSDdecimal},
    { enSchemaAll,  VT_R4,      L"float",           enXSDfloat},
    { enSchemaAll,  VT_R8,      L"double",          enXSDDouble},
    { enSchemaAll,  VT_VARIANT, L"anyType",         enXSDUndefined},
};



const XSDTypeEntries g_xsdMapping[] =
{
    { enSchemaAll,  enXSDDOM,                 L"xmldom",             c_XCNone,      VT_DISPATCH},
    { enSchemaAll,  enXSDboolean,             L"boolean",            c_XCpattern | c_XCwhiteSpace, VT_BOOL},
    { enSchemaAll,  enXSDstring,              L"string",             XC_String,     VT_BSTR},
    { enSchema2000, enXSDuriReference,        L"uriReference",       XC_String,     VT_BSTR},
    { enSchema2001, enXSDuriReference,        L"anyURI",             XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDid,                  L"ID",                 XC_ID,         VT_BSTR},
    { enSchemaAll,  enXSDidRef,               L"IDREF",              XC_ID,         VT_BSTR},
    { enSchemaAll,  enXSDentity,              L"ENTITY",             XC_ID,         VT_BSTR},
    { enSchemaAll,  enXSDQName,               L"QName",              XC_ID,         VT_BSTR},
    { enSchema2000, enXSDcdata,               L"CDATA",              XC_String,     VT_BSTR},
    { enSchema2001, enXSDcdata,               L"normalizedString",   XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDtoken,               L"token",              XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDlanguage,            L"language",           XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDidRefs,              L"IDREFS",             XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDentities,            L"ENTITIES",           XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDnmtoken,             L"NMTOKEN",            XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDnmtokens,            L"NMTOKENS",           XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDname,                L"Name",               XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDncname,              L"NCName",             XC_String,     VT_BSTR},
    { enSchemaAll,  enXSDnotation,            L"NOTATION",           XC_ID,         VT_BSTR},


    { enSchemaAll,  enXSDfloat,               L"float",              XC_Number,     VT_R4},
    { enSchemaAll,  enXSDDouble,              L"double",             XC_Number,     VT_R8},

    { enSchemaAll,  enXSDdecimal,             L"decimal",            XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDinteger,             L"integer",            XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDnonpositiveInteger,  L"nonPositiveInteger", XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDnegativeInteger,     L"negativeInteger",    XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDnonNegativeInteger,  L"nonNegativeInteger", XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDpositiveInteger,     L"positiveInteger",    XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDlong,                L"long",               XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDunsignedLong,        L"unsignedLong",       XC_Decimal,    VT_DECIMAL},
    { enSchemaAll,  enXSDunsignedInt,         L"unsignedInt",        XC_Decimal,    VT_DECIMAL},

    { enSchemaAll,  enXSDint,                 L"int",                XC_Decimal,    VT_I4},

    { enSchemaAll,  enXSDshort,               L"short",              XC_Decimal,    VT_I2},
    { enSchemaAll,  enXSDbyte,                L"byte",               XC_Decimal,    VT_I2},
    { enSchemaAll,  enXSDunsignedShort,       L"unsignedShort",      XC_Decimal,    VT_I4},
    { enSchemaAll,  enXSDunsignedByte,        L"unsignedByte",       XC_Decimal,    VT_UI1},

    { enSchemaAll,  enXSDdate,                L"date",               XC_Time,       VT_DATE},
    { enSchema2000, enXSDtimeInstant,         L"timeInstant",        XC_Time,       VT_DATE},
    { enSchema2001, enXSDtimeInstant,         L"dateTime",           XC_Time,       VT_DATE},
    { enSchemaAll,  enXSDtime,                L"time",               XC_Time,       VT_DATE},

    { enSchema2000, enXSDtimeDuration,        L"timeDuration",       XC_DateBase,   VT_BSTR},
    { enSchema2001, enXSDtimeDuration,        L"duration",           XC_DateBase,   VT_BSTR},
    { enSchema2000, enXSDrecurringDuration,   L"recurringDuration",  XC_DateBase | c_XCduration | c_XCperiod, VT_BSTR},
    { enSchema2000, enXSDtimePeriod,          L"timePeriod",         XC_Time,       VT_BSTR},
    { enSchema2000, enXSDmonth,               L"month",              XC_Time,       VT_BSTR},
    { enSchema2001, enXSDmonth,               L"gMonth",             XC_Time,       VT_BSTR},
    { enSchema2001, enXSDmonth,               L"gYearMonth",         XC_Time,       VT_BSTR},
    { enSchema2000, enXSDyear,                L"year",               XC_Time,       VT_BSTR},
    { enSchema2001, enXSDyear,                L"gYear",              XC_Time,       VT_BSTR},
    { enSchema2000, enXSDrecurringDate,       L"recurringDate",      XC_Time,       VT_BSTR},
    { enSchema2001, enXSDrecurringDate,       L"gMonthDay",          XC_Time,       VT_BSTR},
    { enSchema2000, enXSDrecurringDay,        L"recurringDay",       XC_Time,       VT_BSTR},
    { enSchema2001, enXSDrecurringDay,        L"gDay",               XC_Time,       VT_BSTR},
    { enSchema2000, enXSDcentury,             L"century",            XC_Time,       VT_BSTR},
    { enSchemaAll,  enXSDanyType,             L"anyType",            c_XCNone,      VT_VARIANT},


    { enSchemaAll,  enXSDarray,               L"",                   XC_Array,      VT_SAFEARRAY},
    { enSchema2000, enXSDbinary,              L"binary",             XC_Binary,     VT_ARRAY | VT_UI1},
    { enSchema2001, enXSDbinary,              L"base64Binary",       XC_Binary,     VT_ARRAY | VT_UI1},
};

static const WCHAR  acEmpty[] =L"";


// three little helpers that do a quick lookup
// from the variant string rep in the WSML to the VTYPTE
// and the other way round




HRESULT xsdVerifyenXSDType(const schemaRevisionNr enRevision, const TCHAR *pchVarType, enXSDType *pXSDType)
{
    for (int i = countof(g_xsdMapping) - 1; i >= 0; i--)
    {
        if (wcscmp(pchVarType, g_xsdMapping[i].m_xsdAsString) == 0)
        {
            if (_compareRevisions(g_xsdMapping[i].m_enRevision, enRevision))
            {
                *pXSDType = g_xsdMapping[i].m_enXSDType;
                return (S_OK);
            }
        }
    }
    return (E_FAIL);
}


HRESULT xsdVariantType(enXSDType enType, long *pvtType)
{
    for (int i = countof(g_xsdMapping) - 1; i >= 0; i--)
    {
        if (enType== g_xsdMapping[i].m_enXSDType)
        {
            *pvtType = (long) g_xsdMapping[i].m_vtType;
            return S_OK;
        }
    }
    *pvtType = VT_NULL;
    return S_OK;
}



TCHAR * xsdVariantTypeToXsdString(const schemaRevisionNr enRevision, VARTYPE vtType)
{
    // Return the first one found (VT_BSTR should map to string)
    for (int i = 0; i < countof(g_VTToXSDMapping); i++)
    {
        if (vtType == g_VTToXSDMapping[i].m_vtType)
        {
            if (_compareRevisions(g_VTToXSDMapping[i].m_enRevision, enRevision))
                return(g_VTToXSDMapping[i].m_pwstrXSD);
        }
    }
    return(0);
}



enXSDType vtVariantTypeToXsdVariant(VARTYPE vtType)
{
    // Return the first one found (VT_BSTR should map to string)
    for (int i = 0; i < countof(g_VTToXSDMapping); i++)
    {
        if (vtType == g_VTToXSDMapping[i].m_vtType)
        {
            return(g_VTToXSDMapping[i].m_xsdVariant);
        }
    }
    return(enXSDUndefined);
}



WCHAR * xsdTypeToXsdString(const schemaRevisionNr enRevision, enXSDType enType)
{
    // Return the first one found (VT_BSTR should map to string)
    for (int i = 0; i < countof(g_xsdMapping); i++)
    {
        if (enType == g_xsdMapping[i].m_enXSDType)
        {
            if (_compareRevisions(g_xsdMapping[i].m_enRevision, enRevision))
            {
                return(g_xsdMapping[i].m_xsdAsString);
            }
        }
    }
    return((WCHAR *) &(acEmpty[0]));
}


bool _compareRevisions(schemaRevisionNr enTarget, schemaRevisionNr enQuery)
{
    bool fRet = false;

    switch (enTarget)
    {
        case enSchemaAll:
            fRet = true;
            break;
        case enSchema2001:
            fRet = enTarget == enQuery;
            break;
        case enSchema2000:
            fRet = (enTarget == enQuery || enQuery == enSchema1999);
            break;
        default:
            // if that happens table was changed without change of this method
            ASSERT(true);
    }
    return (fRet);
}
