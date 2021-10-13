//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <rapi.h>

#include "SMB_Globals.h"


RAPIBuilder::RAPIBuilder(BYTE * _pBuffer, UINT _uiBufSize, UINT _uiMaxParams, UINT _uiMaxData)
{
    pBegin = _pBuffer;
    pBufferEnd = pEnd = _pBuffer + _uiBufSize;
    fParamsSet = FALSE;
    uiMaxParams = _uiMaxParams;
    uiMaxData = _uiMaxData;
    uiUsedBytes = 0;
    uiIdxCnt = 0;
    uiGapBytes = 0;
    pParamHeadPointer = NULL;
    pDataHeadPointer = NULL;
    
    uiParamBytes = 0;
}

RAPIBuilder::~RAPIBuilder()
{
}

HRESULT 
RAPIBuilder::ReserveParams(UINT uiSize, BYTE **pBuf)
{
    if(fParamsSet || uiSize > uiMaxParams) 
        return E_INVALIDARG;
        
    fParamsSet = TRUE;
    
    *pBuf = pBegin;
    pBegin += uiSize;
    uiUsedBytes += uiSize; 
        
    pParamHeadPointer = *pBuf;
    pDataHeadPointer = *pBuf + uiSize;
    uiParamBytes = uiSize;
  
    return S_OK;
}

HRESULT 
RAPIBuilder::ReserveBlock(UINT uiSize, BYTE **pBuf, UINT uiAlignOn)
{
    HRESULT hr = S_OK;
    //
    // make sure alignment is okay
    if(0 != uiAlignOn) {
        UINT uiToJunk = uiAlignOn - ((UINT)pBegin % uiAlignOn);
        BYTE *pJunk;
        
        //
        //  only reserve memory if its required
        if(0 != uiToJunk && uiAlignOn > uiToJunk) {
            //NOTE: 0 to prevent recursion
            if(FAILED(ReserveBlock(uiToJunk, &pJunk, 0)))  {
                return E_INVALIDARG;
            }
            memset(pJunk, 0, uiToJunk);
        }    
    }
    if(FAILED(hr) || !fParamsSet || uiSize+uiUsedBytes > uiMaxData || (UINT)(pEnd - pBegin) < uiSize)
        return E_INVALIDARG;
    
#ifdef DEBUG
    if(uiAlignOn) {
        ASSERT(0 == ((UINT)pBegin % uiAlignOn));
    }    
#endif

    *pBuf = pBegin;
    pBegin += uiSize;
    uiUsedBytes += uiSize;  
    ASSERT(SUCCEEDED(hr));
    return hr;
}

HRESULT 
RAPIBuilder::ReserveFloatBlock(UINT uiSize, UINT *uiIdx)
{
   if(!fParamsSet || uiSize+uiUsedBytes > uiMaxData || (UINT)(pEnd - pBegin) < uiSize)
        return E_INVALIDARG;
        
   *uiIdx = (uiIdxCnt++);   
   pEnd -= uiSize;
   uiUsedBytes += uiSize;
   
   RAPI_FLOATER floater;
   floater.pPtr = pEnd;
   floater.uiIdx = *uiIdx;
   floater.ppDest = NULL;
   floater.pFixedBase = NULL;
   
   floaterList.push_front(floater);
   
   return S_OK;
}


HRESULT 
RAPIBuilder::ReserveFloatBlock(UINT uiSize, CHAR **ppDest, char *pFixedBase)
{
   if(!fParamsSet || uiSize+uiUsedBytes > uiMaxData || (UINT)(pEnd - pBegin) < uiSize)
        return E_INVALIDARG;
          
   pEnd -= uiSize;
   uiUsedBytes += uiSize;
   
   RAPI_FLOATER floater;
   floater.pPtr = pEnd;
   floater.uiIdx = 0xFFFFFFFF;
   floater.ppDest = ppDest;
   floater.pFixedBase = pFixedBase;
   
   char *p1 = (char *)ppDest;
   char *p2 = (char *)&pEnd;
   memcpy(p1, p2, sizeof(char *));
   //*ppDest = (char *)pEnd;
   
   floaterList.push_front(floater);
   
   return S_OK;
}



HRESULT 
RAPIBuilder::MapFloatToFixed(UINT uiIdx, BYTE **pBuf)
{
    ce::list<RAPI_FLOATER, RAPI_FLOATER_ALLOC>::iterator it = floaterList.begin();
    ce::list<RAPI_FLOATER, RAPI_FLOATER_ALLOC>::iterator end = floaterList.end();
    while(it != end) {
        if((*it).uiIdx == uiIdx) {
            *pBuf = (*it).pPtr;             
            return S_OK;
        }
        it ++;
    }
    
    return E_FAIL;   
}

HRESULT 
RAPIBuilder::MakeParamByteGap(UINT uiSize)
{
    //
    // If no previous gap, set the gap pointer
    if(0 == uiGapBytes)
        pGapPointer = pBegin;
    
    //
    // Make a gap between params and byte
    pDataHeadPointer +=uiSize;
    uiUsedBytes += uiSize;
    pBegin += uiSize;
    uiGapBytes += uiSize;
    return S_OK;
}

HRESULT 
RAPIBuilder::Coalesce()
{
    UINT uiDistance = pEnd - pBegin;
    UINT uiFloatLength = pBufferEnd - pEnd;
    
    //move the data
    memmove(pBegin, pEnd, uiFloatLength);
    
    //update the index list
    ce::list<RAPI_FLOATER, RAPI_FLOATER_ALLOC>::iterator it = floaterList.begin();
    ce::list<RAPI_FLOATER, RAPI_FLOATER_ALLOC>::iterator end = floaterList.end();
    while(it != end) {
        (*it).pPtr -= uiDistance;
        
        //
        // if they have given us a char** relocate the memory for them
        if(0xFFFFFFFF == (*it).uiIdx) {
            char *pNew = (CHAR *)((CHAR *)((*it).pPtr)  - (CHAR *)((*it).pFixedBase));
            
            char *p1 = (char *)((*it).ppDest);
            char *p2 = (char *)&pNew;
            memcpy(p1, p2, sizeof(char *));
            
        }
        
        it ++;
    }    
    return S_OK;
}
