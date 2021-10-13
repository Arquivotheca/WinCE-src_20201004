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

/*++


    Module Name:

        AttrBlock.h

    Abstract:

        Attributed Media Block data block definitions.

    Revision History:

        07-Mar-2002    created


  Caution - don't mix Subclassed classes with non subclassed types in the same subblock


--*/

#ifndef __ATTRBLOCK_H__
#define __ATTRBLOCK_H__

#include "licbase.h"        // to get KIDLEN
// ---------------------------------------------
// forward declarations
class CAttrSubBlock_List;
class CAttrSubBlock;

// ---------------------------------------------
// enums

enum EnAttrSubBlock_Class
{
    SubBlock_Uninitialized  = -1,   // haven't gotten to it yet

	//**************************************************
    //***** Do NOT change these numbers            *****
	//***** Existing content uses these numbers    *****
	//***** You will break playback of these files *****
	//**************************************************

#if 0
    SubBlock_XDSClass0      = 0,    // reseve a bunch of blocks for XDS packets
    SubBlock_XDSClass1      = 1,
    SubBlock_XDSClass2      = 2,
    SubBlock_XDSClass3      = 3,
    SubBlock_XDSClass4      = 4,
    SubBlock_XDSClass5      = 5,
    SubBlock_XDSClass6      = 6,
    SubBlock_XDSClass7      = 7,
    SubBlock_XDSClass8      = 8,
    SubBlock_XDSClass9      = 9,
    SubBlock_XDSClassA      = 10,
    SubBlock_XDSClassB      = 11,
    SubBlock_XDSClassC      = 12,
    SubBlock_XDSClassD      = 13,
    SubBlock_XDSClassE      = 14,
    SubBlock_XDSClassF      = 15,   // end class, probably never sent
#endif

    SubBlock_EncDecAttrs   = 20,

#if 0
    SubBlock_PackedRating   = 100,
    SubBlock_EncryptMethod  = 101,
    SubBlock_DRM_KID        = 102,

    SubBlock_Test1          = 64,
    SubBlock_Test2          = 65,
    SubBlock_Test3          = 66,
    SubBlock_Test4          = 67,
#endif

    SubBlock_Last           = 255   // last valid subblock type

};

#if 0
typedef struct                 //   simple little continuity test subblock
{
    DWORD                   m_cSampleID;        // continuity counter
    DWORD                   m_cSampleSize;      // true size of the sample
    DWORD                   m_dwFirstDataWord;  // first 4 bytes of the sample...
} Test2_SubBlock;
#endif

// EncDecAttrs & EncDecAttrs2 are SubBlock_EncDecAttrs.
// Code that understands EncDecAttrsN should reject samples which
// are have size > sizeof(EncDecAttrsN). i.e. if the samples are tagged
// with an EncDecAttrs greater than the highest one known, then it should
// be considered incompatible.  If you need to add more data but still
// not break backwards compatibility then put that info in a seperate subblock.
struct EncDecAttrs
{
    DWORD                   m_EncryptionMethod;
    BYTE                    m_KID[KIDLEN+1];
    StoredTvRating          m_StoredTvRating;
};

struct EncDecAttrs2 : EncDecAttrs
{
    DWORD       m_dwMacroVision;
};


// ----------------------------------------------------
class CAttrSubBlock_List;

class CAttrSubBlock
{
public:
    CAttrSubBlock();
    ~CAttrSubBlock();

    HRESULT Get(
        EnAttrSubBlock_Class    *pEnSubBlock_Class,
        LONG                    *pSubBlock_SubClass,
        BSTR                    *pBstrOut
        );

    HRESULT Set(
        EnAttrSubBlock_Class    enSubBlock_Class,
        LONG                    subBlock_SubClass,
        BSTR                    bstrIn
        );

   HRESULT Get(
       EnAttrSubBlock_Class     *pEnSubBlock_Class,
       LONG                     *pSubBlock_SubClass,        // can be a 'value'
       LONG                     *pcbData,
       BYTE                     **ppbData                   // May be null if just want to determine size. Felease with CoTaskMemFree()
       );

   HRESULT Set(
       EnAttrSubBlock_Class     enSubBlock_Class,
       LONG                     lSubBlock_SubClass,        // can be a 'value'
       LONG                     cbData,
       BYTE                     *pbData
       );

    LONG CAttrSubBlock::ByteLength()
    {
        return sizeof(CAttrSubBlock)     // don't really need Pointer data here, but easier
               + SysStringByteLen(m_spbsData.m_str);        // want byte length, not # characters
    }
protected:
    friend  CAttrSubBlock_List;

    BOOL    IsEqual(
                    EnAttrSubBlock_Class    enSubBlock
                    )
    {
        return enSubBlock == m_enSubBlock_Class;
    }

    BOOL    IsEqual(EnAttrSubBlock_Class    enSubBlock,
                    LONG                    subBlock_SubClass
                    )
    {
        return         enSubBlock == m_enSubBlock_Class &&
                subBlock_SubClass == m_subBlock_SubClass;
    }

  private:
    EnAttrSubBlock_Class    m_enSubBlock_Class;
    LONG                    m_subBlock_SubClass;        // can be overwritten with data if only one LONG long
    VARIANT                 m_varData;      // todo - use this instead

    CComBSTR                m_spbsData;     // todo - make this go away

    CAttrSubBlock            *m_pNext ;  // simple list structure
};


        // -----------------------------------------------

class CAttrSubBlock_List
{
private:

    CAttrSubBlock *
    NewObj_ (
        )
    {
        return new CAttrSubBlock ;
    }

	void
	Recycle_(CAttrSubBlock *pObj)
	{
		delete pObj;
	}

    CAttrSubBlock * PopListHead_();
    CAttrSubBlock * GetIndexed_(IN LONG iIndex);
    CAttrSubBlock * FindInList_(IN EnAttrSubBlock_Class enClass);
    CAttrSubBlock * FindInList_(IN EnAttrSubBlock_Class enClass, IN LONG subClass);
    CAttrSubBlock * FindInList_(IN GUID &guid);

    HRESULT InsertInList_(IN  CAttrSubBlock *    pNew);
    HRESULT DeleteFromList_(IN  CAttrSubBlock * pToDelete);
public:
    CAttrSubBlock_List();
    ~CAttrSubBlock_List();


    void Reset();           // clear entire list

    HRESULT                 // will error if already there
    Add(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    subBlock_SubClass,
        IN  BSTR                    bstrIn
        ) ;

    HRESULT                 // will error if already there
    Add(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    lValue
        ) ;

    HRESULT                 // will error if already there
    Add(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    lValue,
        IN  LONG                    cBytes,
        IN  BYTE                   *pbBytes
        ) ;

    HRESULT                 // will error if already there
    Replace(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    subBlock_SubClass,
        IN  BSTR                    bstrIn
        ) ;


    HRESULT                 // will error if already there
    Replace(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    lValue
        ) ;

    HRESULT
    Replace(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    lValue,
        IN  LONG                    cBytes,
        IN  BYTE                   *pbBytes
        ) ;

    HRESULT
    Get(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    subBlock_SubClass,
        OUT BSTR                    *pbstrOut
        );

    HRESULT
    Get(
        IN  EnAttrSubBlock_Class    enSubBlock,
        OUT LONG                    *plValue
        );

    HRESULT
    Get(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    subBlock_SubClass,      // fixed subclass
        OUT LONG                    *pcBytes,
        OUT BYTE                    **pbBytes
        );

    HRESULT
    Get(
        IN  EnAttrSubBlock_Class    enSubBlock,
        OUT LONG                    *plValue,               // variable subclass
        OUT LONG                    *pcBytes,
        OUT BYTE                    **pbBytes
        );

    HRESULT                 // returns S_FALSE if not there to delete
    Delete(
        IN  EnAttrSubBlock_Class    enSubBlock,
        IN  LONG                    subBlock_SubClass
        ) ;

    HRESULT
    Delete(
        IN  EnAttrSubBlock_Class    enSubBlock
        ) ;

        // -------------------------
    HRESULT
    GetIndexed(
        IN  LONG                    iIndex,
        OUT EnAttrSubBlock_Class    *pEnSubBlock,
        OUT LONG                    *pSubBlock_SubClass,
        OUT BSTR                    *pBstrOut
        ) ;

    LONG GetCount()            { return m_cAttributes ; }
    LONG GetBlockByteLength();                                 // total length of the block in bytes


                // returns complete list in one giant block
    HRESULT
    GetAsOneBlock(
        OUT BSTR                    *pBstrOut
        ) ;

    HRESULT     // given a block, converts it into a list of blocks (in place)
    SetAsOneBlock(
        IN  BSTR                    bstrIn
        );

private:
    CAttrSubBlock   *m_pAttrListHead;  // inefficent impelementation for large numbers, but good enough for now
    long            m_cAttributes;
};

#endif //__ATTRBLOCK_H__
