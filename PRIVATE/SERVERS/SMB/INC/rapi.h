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
#ifndef RAPI_H
#define RAPI_H

#include <list.hxx>
#include <block_allocator.hxx>

#include "SMB_Globals.h"

#ifndef NO_POOL_ALLOC
#define RAPI_FLOATER_ALLOC       ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(RAPI_FLOATER) >
#else
#define RAPI_FLOATER_ALLOC ce::allocator
#endif

struct RAPI_FLOATER
{
    BYTE *pPtr;
    UINT uiIdx;
    CHAR **ppDest;
    CHAR *pFixedBase;
};

class RAPIBuilder {
    public:
        RAPIBuilder(BYTE *_pBuffer, UINT uiBufSize, UINT uiMaxParams, UINT uiMaxData);
        ~RAPIBuilder();

        HRESULT MakeParamByteGap(UINT uiSize);
        HRESULT ReserveParams(UINT uiSize, BYTE **pBuf);
        HRESULT ReserveBlock(UINT uiSize, BYTE **pBuf, UINT uiAlignOn = 0);
        HRESULT ReserveFloatBlock(UINT uiSize, UINT *uiIdx);
        HRESULT ReserveFloatBlock(UINT uiSize, CHAR **ppDest, CHAR *pFixedBase);
        HRESULT MapFloatToFixed(UINT uiIdx, BYTE **pBuf);

        HRESULT Coalesce();
        BYTE *ParamPtr() { return pParamHeadPointer; }
        BYTE *DataPtr() { return pDataHeadPointer; }
        BYTE *GapPtr() { return pGapPointer; }
        USHORT ParamOffset(BYTE *pBase) {
           //if(0 != ParamBytesUsed()) {
           ASSERT(NULL != ParamPtr());
           return (USHORT)(ParamPtr() - pBase); 
           /*} else {
               return 0;
           } */   
        }
        USHORT DataOffset(BYTE *pBase) {
            if(0 != DataBytesUsed()) {
                return (USHORT)(DataPtr() - pBase); 
            } else {
                return 0;
            }
        }

        USHORT DataBytesUsed() { return uiUsedBytes - uiParamBytes - uiGapBytes; }
        USHORT ParamBytesUsed() { return uiParamBytes; }
        USHORT TotalBytesUsed() { return uiUsedBytes; }
    private:
        BOOL fParamsSet;

        //pointers to data and params
        BYTE *pDataHeadPointer;
        BYTE *pParamHeadPointer;
        BYTE *pGapPointer; //points to the gap between data and params.. could be NULL if nothing set
        
        BYTE *pBegin;
        BYTE *pEnd;  
        BYTE *pBufferEnd; //the REAL end of the buffer
        UINT uiMaxParams;
        UINT uiMaxData;
        UINT uiUsedBytes;
        UINT uiIdxCnt;

        UINT uiParamBytes;    
        UINT uiGapBytes;

        ce::list<RAPI_FLOATER, RAPI_FLOATER_ALLOC> floaterList;
};

#endif
