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
    
// SdpStream.h : Declaration of the CSdpStream

#ifndef __SDPSTREAM_H_
#define __SDPSTREAM_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpStream
class CSdpStream : 
    public ISdpStream
{
public:
    CSdpStream()
    {
        refCount = 0;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

// ISdpStream
public:
    STDMETHOD(ByteSwapInt16)(SHORT int16, SHORT *pInt16);
    STDMETHOD(ByteSwapInt32)(LONG int32, LONG *pInt32);
    STDMETHOD(ByteSwapInt64)(LONGLONG inInt64, LONGLONG *pOutInt64);
    STDMETHOD(ByteSwapInt128)(PSDP_LARGE_INTEGER_16 pInInt128, PSDP_LARGE_INTEGER_16 pOutInt128);
    STDMETHOD(ByteSwapUint16)(USHORT uint16, USHORT *pUint16);
    STDMETHOD(ByteSwapUint32)(ULONG uint32, ULONG *pUint32);
    STDMETHOD(ByteSwapUint64)(ULONGLONG inUint64, ULONGLONG* pOutUint64);
    STDMETHOD(ByteSwapUint128)(PSDP_ULARGE_INTEGER_16 pInUint128, PSDP_ULARGE_INTEGER_16 pOutUint128);
    STDMETHOD(ByteSwapUuid128)(GUID *pInUuid128, GUID *pOutUuid128);
    STDMETHOD(Walk)(UCHAR *pStream, ULONG size, ISdpWalk *pWalk);
    STDMETHOD(Validate)(UCHAR *pStream, ULONG size, ULONG_PTR *pErrorByte);
    STDMETHOD(RetrieveRecords)(UCHAR *pStream, ULONG size, ISdpRecord **ppSdpRecords, ULONG *pNumRecords);
    STDMETHOD(RetrieveInt128)(UCHAR *pStream, PSDP_LARGE_INTEGER_16 pInt128);
    STDMETHOD(RetrieveInt64)(UCHAR *pStream, LONGLONG *pInt64);
    STDMETHOD(RetrieveInt32)(UCHAR *pStream, LONG *pInt32);
    STDMETHOD(RetrieveInt16)(UCHAR *pStream, SHORT *pInt16);
    STDMETHOD(RetrieveUint128)(UCHAR *pStream, PSDP_ULARGE_INTEGER_16 pUint128);
    STDMETHOD(RetrieveUint64)(UCHAR *pStream, ULONGLONG *pUint64);
    STDMETHOD(RetrieveUint32)(UCHAR *pStream, ULONG *pUint32);
    STDMETHOD(RetrieveUint16)(UCHAR *pStream, USHORT *pUint16);
    STDMETHOD(RetrieveUuid128)(UCHAR *pStream, GUID* pUuid128);
    STDMETHOD(NormalizeUuid)(NodeData *pDataUuid, GUID* pNormalizeUuid);
    STDMETHOD(RetrieveElementInfo)(UCHAR *pStream, SDP_TYPE *pElementType, SDP_SPECIFICTYPE *pElementSpecificType, ULONG *pElementSize, ULONG *pStorageSize, UCHAR **ppData);
    STDMETHOD(VerifySequenceOf)(UCHAR *pStream, ULONG size, SDP_TYPE ofType, UCHAR *pSpecificSizes, ULONG *pNumFound);

protected:
    LONG refCount;

};

#endif //__SDPSTREAM_H_

