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
#pragma once

#include <DxXML.h>

#ifdef __cplusplus
extern "C" {
#endif
// {F12B3CD0-0AEB-4d6d-80D8-61A7E42F9EB7}
DEFINE_GUID(IID_IMetadataProperty, 
0xf12b3cd0, 0xaeb, 0x4d6d, 0x80, 0xd8, 0x61, 0xa7, 0xe4, 0x2f, 0x9e, 0xb7);
// {48D77828-414E-43d2-9000-E3F5B686AC43}
DEFINE_GUID(IID_IPropertyFilter, 
0x48d77828, 0x414e, 0x43d2, 0x90, 0x0, 0xe3, 0xf5, 0xb6, 0x86, 0xac, 0x43);
// {688691F9-E906-4709-9ABB-64CBE9774A18}
DEFINE_GUID(IID_IPropertyEnumerator, 
0x688691f9, 0xe906, 0x4709, 0x9a, 0xbb, 0x64, 0xcb, 0xe9, 0x77, 0x4a, 0x18);
// {8C762EEB-753C-4f8c-82F0-E36BBD7FDCB2}
DEFINE_GUID(IID_IMediaItem, 
0x8c762eeb, 0x753c, 0x4f8c, 0x82, 0xf0, 0xe3, 0x6b, 0xbd, 0x7f, 0xdc, 0xb2);
// {724573BB-12A4-444f-A90E-6308A7403E1F}
DEFINE_GUID(IID_IMediaItemEnumerator, 
0x724573bb, 0x12a4, 0x444f, 0xa9, 0xe, 0x63, 0x8, 0xa7, 0x40, 0x3e, 0x1f);
// {7A317166-1A55-4cc0-AB7D-6AFF5999D225}
DEFINE_GUID(IID_IMediaItemCollection, 
0x7a317166, 0x1a55, 0x4cc0, 0xab, 0x7d, 0x6a, 0xff, 0x59, 0x99, 0xd2, 0x25);
// {1D999D85-385A-4435-B8C6-A5A0317C6CB6}
DEFINE_GUID(IID_IMediaMetadataFactory, 
0x1d999d85, 0x385a, 0x4435, 0xb8, 0xc6, 0xa5, 0xa0, 0x31, 0x7c, 0x6c, 0xb6);

/*
 * Forward interface declarations
 */
struct IMetadataProperty;
struct IPropertyFilter;
struct IPropertyEnumerator;
struct IMediaItem;
struct IMediaItemEnumerator;
struct IMediaItemCollection;
struct IMediaMetadataFactory;


enum PropertyType
{
    Byte = 1,        // One or more unsigned values between 0..255
    Ascii = 2,       // NULL-terminated 8bit ascii string
    UShort = 3,      // Unsigned 16bit Integer
    ULong = 4,       // Unsigned 32bit Integer
    URational = 5,   // Unsigned Rational (One Long Numerator, One Long Denominator)
    Blob = 7,        // One or more bytes of undefined data
    SLong = 9,       // Signed 32 Integer
    SRational = 10,  // Signed Rational
    Unicode = 20,    // NULL-terminated 16bit UTF-16 string
    Guid,            // GUID
    ULongLong,       // 64bit Unsigned Integer
    SLongLong,       // 64bit Signed Integer
    Bool,            // Boolean (32bit)
    Float,           // 32bit IEEE Float
    Double,          // 64bit IEEE Float
    SByte,           // Signed 8bit Integer
    SShort,          // Signed 16bit Integer

    EnsureLong = 0xffffffff,
};

enum PropertyCategory
{
    // Media and Test categories can be specified as string or int in XML
    // (e.g. PropertyCategory="Media" is equivalent to PropertyCategory="0")
    Media = 0,      // The information in this property is from a media file
    Test = 1,       // The information in this property is related to test purposes

    // Categories greater than or equal to UserDef are for user use 
    // (specified as int in xml file, e.g. PropertyCategory="258")
    CategoryUserDef = 256,    
};

enum PropertyStringEncoding
{
    // How is an Ascii or Unicode value encoded?
    String,     // Plain text ("blah")
    Hex,        // Hex characters 
};

enum TestPropertyID
{
    FileName,
    FilePath,
    FileSize,
    UserDef,
};

struct SRATIONAL {
    signed int Numerator;
    signed int Denominator;
};
struct URATIONAL {
    unsigned int Numerator;
    unsigned int Denominator;
};

// Flags for MetadataProperty::Compare (and Filters)
const DWORD COMPARE_MATCH_ID                = 0x01;
const DWORD COMPARE_MATCH_NAME              = 0x02;
const DWORD COMPARE_MATCH_TYPE              = 0x04;
const DWORD COMPARE_MATCH_LOCALEID          = 0x08;
const DWORD COMPARE_MATCH_COUNT             = 0x10;
const DWORD COMPARE_MATCH_VALUE             = 0x20;
const DWORD COMPARE_MATCH_CATEGORY          = 0x40;

// Flags for MediaItem::Compare
const DWORD COMPARE_IGNORE_ID               = 0x100;
const DWORD COMPARE_IGNORE_NAME             = 0x200;
const DWORD COMPARE_ALLOWUNMATCHED_LEFT     = 0x400;
const DWORD COMPARE_ALLOWUNMATCHED_RIGHT    = 0x800;

// Flag for all compares and filters
const DWORD COMPARE_ALLOWCOMPATIBLETYPE     = 0x20000000;
const DWORD COMPARE_IGNORECASE_NAME         = 0x40000000;
const DWORD COMPARE_IGNORECASE_VALUE        = 0x80000000;
const DWORD COMPARE_IGNORECASE              = 
    COMPARE_IGNORECASE_NAME | COMPARE_IGNORECASE_VALUE;

const DWORD COMPARE_VALID_METADATAPROPERTY = 
    COMPARE_MATCH_ID |
    COMPARE_MATCH_NAME |
    COMPARE_MATCH_TYPE |
    COMPARE_MATCH_LOCALEID |
    COMPARE_MATCH_COUNT |
    COMPARE_MATCH_VALUE |
    COMPARE_MATCH_CATEGORY |
    COMPARE_ALLOWCOMPATIBLETYPE |
    COMPARE_IGNORECASE;

const DWORD COMPARE_VALID_MEDIAITEM = 
    COMPARE_IGNORE_ID |
    COMPARE_IGNORE_NAME |
    COMPARE_ALLOWUNMATCHED_LEFT |
    COMPARE_ALLOWUNMATCHED_RIGHT |
    COMPARE_ALLOWCOMPATIBLETYPE |
    COMPARE_IGNORECASE;

const LCID MEDIAMETADATA_NAMELOCALEID = 0x0409;

extern HRESULT WINAPI MediaMetadataCreateFactory(
    OUT IMediaMetadataFactory ** ppFactory);

struct MetadataProperty
{
    unsigned int    ID;                 // Optional ID of this property
    const WCHAR   * Name;               // Optional Name of this property
    unsigned int    Category;           // TestProperty, etc.
    const WCHAR   * Context;            // Optional Context of this property (i.e. frame # or stream name)
    PropertyType    Type;               // Type of this property
    DWORD           LocaleID;           // Optional Locale
    size_t          cbValueBufferSize;  // Total size of value
    unsigned int    ValueCount;         // Number of values
    union
    {
        unsigned char Byte[1];
        char Ascii[1];
        unsigned short int UShort[1];
        unsigned long int ULong[1];
        URATIONAL URational[1];
        unsigned char Blob[1];
        signed long int SLong[1];
        SRATIONAL SRational[1];
        WCHAR Unicode[1];
        GUID Guid[1];
        ULONGLONG ULongLong[1];
        LONGLONG SLongLong[1];
        BOOL Bool[1];
        float Float[1];
        double Double[1];
        signed short int SShort[1];
        signed char SByte[1];
    } Value;
};

#undef INTERFACE
#define INTERFACE IMetadataProperty
DECLARE_INTERFACE_(IMetadataProperty, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IMetadataProperty methods ***/
    STDMETHOD(Compare)(THIS_ IN DWORD Flags, IN IMetadataProperty * pOther, OUT INT * pResult) PURE;
    STDMETHOD(Copy)(THIS_ IN IMetadataProperty * pOther) PURE;

    STDMETHOD(GetMetadataProperty)(THIS_ OUT const MetadataProperty ** ppProperty) PURE;
    STDMETHOD(SetMetadataProperty)(THIS_ IN const MetadataProperty * pProperty, IN const void * pvValue) PURE;
};

#undef INTERFACE
#define INTERFACE IPropertyFilter
DECLARE_INTERFACE_(IPropertyFilter, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IPropertyFilter methods ***/
    STDMETHOD_(BOOL, PropertyMatchesFilter)(THIS_ IN IMetadataProperty * pProperty) PURE;
    STDMETHOD_(BOOL, MediaItemMatchesFilter)(THIS_ IN IMediaItem * pMM) PURE;
    STDMETHOD(AddInclusion)(THIS_ IN DWORD CompareFlags, IN IMetadataProperty * pProperty) PURE;
    STDMETHOD(AddExclusion)(THIS_ IN DWORD CompareFlags, IN IMetadataProperty * pProperty) PURE;
};

#undef INTERFACE
#define INTERFACE IPropertyEnumerator
DECLARE_INTERFACE_(IPropertyEnumerator, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    STDMETHOD(Next)(THIS_ OUT IMetadataProperty ** ppMP) PURE;
    STDMETHOD(Reset)(THIS) PURE;
    STDMETHOD_(UINT, Count)(THIS) PURE;
};

#undef INTERFACE
#define INTERFACE IMediaItem
DECLARE_INTERFACE_(IMediaItem, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    STDMETHOD(GetProperty)(THIS_
        IN  unsigned int PropertyID,
        IN  const WCHAR * PropertyName,
        IN  unsigned int PropertyCategory,
        IN  const void * PropertyValue,
        IN  DWORD Flags,
        OUT IMetadataProperty ** ppProperty,
        OUT IPropertyEnumerator ** ppEnum) PURE;
    
    STDMETHOD(GetProperty)(THIS_
        IN  unsigned int PropertyID,
        IN  const WCHAR * PropertyName,
        IN  unsigned int PropertyCategory,
        IN  DWORD Flags,
        OUT IMetadataProperty ** ppProperty,
        OUT IPropertyEnumerator ** ppEnum) PURE;

    STDMETHOD(GetPropertyEnumerator)(THIS_
        IN  BOOL Filtered,
        OUT IPropertyEnumerator ** ppPEnum) PURE;

    STDMETHOD(AddProperty)(THIS_
        IN IMetadataProperty * pProperty) PURE;
    
    STDMETHOD(DeleteProperty)(THIS_
        IN  unsigned int PropertyID,
        IN  const WCHAR * PropertyName,
        IN  unsigned int PropertyCategory,
        IN  const void * PropertyValue,
        IN  DWORD Flags) PURE;

    STDMETHOD(SetFilters)(THIS_
        IN IPropertyFilter * pFilters, 
        IN unsigned int cFilters) PURE;
    STDMETHOD(AddFilter)(THIS_
        IN IPropertyFilter * pFilter) PURE;
    STDMETHOD(ClearFilters)(THIS) PURE;
    
    STDMETHOD(Compare)(THIS_
        IN DWORD Flags,
        IN IMediaItem * pOther,
        OUT INT * pCompareResult) PURE;
};

#undef INTERFACE
#define INTERFACE IMediaItemEnumerator
DECLARE_INTERFACE_(IMediaItemEnumerator, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IMediaItemEnumerator methods ***/
    STDMETHOD(Next)(THIS_ OUT IMediaItem ** ppMM) PURE;
    STDMETHOD(Reset)(THIS) PURE;
    STDMETHOD_(UINT, Count)(THIS) PURE;
    STDMETHOD(Sort)(THIS_ 
        IN unsigned int PropertyID, 
        IN const WCHAR * PropertyName, 
        IN unsigned int PropertyCategory,
        IN DWORD PropertyFlags,
        IN BOOL Ascending) PURE;
};

#undef INTERFACE
#define INTERFACE IMediaItemCollection
DECLARE_INTERFACE_(IMediaItemCollection, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    STDMETHOD(SetFilters)(THIS_
        IN IPropertyFilter * Filters, 
        IN unsigned int cFilters) PURE;
    STDMETHOD(AddFilter)(THIS_
        IN IPropertyFilter * Filter) PURE;
    STDMETHOD(ClearFilters)(THIS) PURE;

    STDMETHOD(GetMediaItemEnumerator)(THIS_ OUT IMediaItemEnumerator ** ppMMEnumerator) PURE;

    STDMETHOD(AddMediaItem)(THIS_
        IN IMediaItem * pMM) PURE;
    
    STDMETHOD (GetMediaItem)(THIS_
        IN const WCHAR * pPropertyName,
        IN const void * pPropertyValue,
        OUT IMediaItem **ppIMediaItem) PURE;
    
    STDMETHOD (DeleteMediaItem)(THIS_
            IN const WCHAR * pPropertyName,
            IN const void * pPropertyValue) PURE;
};

#undef INTERFACE
#define INTERFACE IMediaMetadataFactory
DECLARE_INTERFACE_(IMediaMetadataFactory, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ IN REFIID riid, OUT LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS)  PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IMediaMetadataFactory methods ***/
    STDMETHOD(LoadMediaMetadata)(THIS_ 
        IN WCHAR * pszFilename, 
        OUT IMediaItemCollection ** ppMediaCollection,
        OUT int * piErrorLine) PURE;

    STDMETHOD(LoadMediaMetadataBuffer)(THIS_ 
        IN char * pBuffer, 
        OUT IMediaItemCollection ** ppMediaCollection,
        OUT int * piErrorLine) PURE;

    STDMETHOD(CreateMetadataProperty)(THIS_
        OUT IMetadataProperty ** ppProperty) PURE;

    STDMETHOD(CreatePropertyFilter)(THIS_
        OUT IPropertyFilter ** ppFilter) PURE;

    STDMETHOD(CreateMediaItem)(THIS_
        OUT IMediaItem ** ppMetadata) PURE;

    STDMETHOD(CreateMediaItemCollection)(THIS_ 
        OUT IMediaItemCollection ** ppCollection) PURE;
    
    STDMETHOD(CreateSimpleFilter)(THIS_
        IN  const MetadataProperty * pmpInclusions,
        IN  const void ** ppvInclusionValues,
        IN  const DWORD * pdwInclusionCompareFlags,
        IN  const int cInclusions,
        IN  const MetadataProperty * pmpExclusions,
        IN  const void ** ppvExclusionValues,
        IN  const DWORD * pdwExclusionCompareFlags,
        IN  const int cExclusions,
        OUT IPropertyFilter ** ppFilter) PURE;
};

#define MAKE_ERROR(x) MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, (x))

#define MM_E_ENUMNEEDED             MAKE_ERROR(0x200)  // 0x80040200
#define MM_E_NOMORE                 MAKE_ERROR(0x201)  // 0x80040201
#define MM_E_PARSEFAILED            MAKE_ERROR(0x202)  // 0x80040202
#define MM_E_UNEXPECTEDELEMENT      MAKE_ERROR(0x203)  // 0x80040203
#define MM_E_UNEXPECTEDATTRIBUTE    MAKE_ERROR(0x204)  // 0x80040204
#define MM_E_MISSINGREQUIRED        MAKE_ERROR(0x205)  // 0x80040205
#define MM_E_OVERFLOW               MAKE_ERROR(0x206)  // 0x80040206
#define MM_E_NOTFOUND               MAKE_ERROR(0x207)  // 0x80040207

#ifdef __cplusplus
};
#endif
