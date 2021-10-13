//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef RAPI_H
#define RAPI_H

#include <list.hxx>

#include "SMB_Globals.h"
#include "FixedAlloc.h"


#define RAPI_FLOATER_ALLOC       pool_allocator<10, RAPI_FLOATER>


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
