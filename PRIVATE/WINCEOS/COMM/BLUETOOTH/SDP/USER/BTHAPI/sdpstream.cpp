//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SdpStream.cpp : Implementation of CSdpStream
#include "stdafx.h"
#include "BthAPI.h"
#include "SdpStream.h"
#include "sdplib.h"
#include "util.h"

/////////////////////////////////////////////////////////////////////////////
// CSdpStream

STDMETHODIMP CSdpStream::Validate(UCHAR *pStream, ULONG size, ULONG_PTR *pErrorByte)
{
    if (pStream == NULL) {
        return E_INVALIDARG;
    }

    if (NT_SUCCESS(ValidateStream(pStream, size, NULL, NULL, pErrorByte))) {
        if (pErrorByte != NULL) {
            *pErrorByte = 0;
        }
        return S_OK;
    }
    else {
        return E_FAIL;
    }
}

#define S_NO_RECURSE ERROR_NOT_A_REPARSE_POINT 

NTSTATUS
WalkFunc(
    ISdpWalk *pWalk,
    UCHAR DataType,
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    HRESULT err =
        pWalk->WalkStream(DataType, DataSize, Data);

    if ((DataType == SDP_TYPE_SEQUENCE || DataType == SDP_TYPE_ALTERNATIVE) &&
        err == S_NO_RECURSE) {
        return STATUS_REPARSE_POINT_NOT_RESOLVED;
    }
    else {
        return (NTSTATUS) err;
    }
}

STDMETHODIMP CSdpStream::Walk(UCHAR *pStream, ULONG size, ISdpWalk *pWalk)
{
    ULONG_PTR errByte;
    HRESULT err = Validate(pStream, size, &errByte);
    if (!SUCCEEDED(err)) {
        return E_INVALIDARG;
    }

    return (HRESULT)
        SdpWalkStream(pStream, size, (PSDP_STREAM_WALK_FUNC) WalkFunc , pWalk);
}

STDMETHODIMP CSdpStream::RetrieveRecords(
    UCHAR *pStream,
    ULONG size,
    ISdpRecord **ppSdpRecords,
    ULONG *pNumRecords
    )
{
    HRESULT hres = S_OK;

    if (pNumRecords == NULL) {
        return E_INVALIDARG;
    }

    NTSTATUS status = ValidateStream(pStream, size, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return MapNtStatusToHresult(status);
    }

    SDP_TYPE type;
    SDP_SPECIFICTYPE specType;
    ULONG storageSize;
    ULONG idxRecords = 0;

    if (!SUCCEEDED(hres)) {
        return hres;
    }


    //
    // Find out how big the stream *contents* are.  As a result, stream will now
    // point to the first subsequence in this uber sequence
    //
    RetrieveElementInfo(pStream,
                        &type, 
                        &specType,
                        &size,
                        &storageSize,
                        &pStream);

    if (type != SDP_TYPE_SEQUENCE) {
        return E_INVALIDARG;
    }

    while (size) {

        PUCHAR recordStream = pStream;
        ULONG recordSize = 0;

        RetrieveElementInfo(recordStream,
                            &type,
                            &specType,
                            &recordSize,
                            &storageSize,
                            NULL);

        if (type != SDP_TYPE_SEQUENCE) {
            return E_INVALIDARG;
        }

        //
        // recordSize contains the size of the sequence contents, but we must also
        // account for the header (1 byte, fixed) and the storage size
        //
        recordSize += 1 + storageSize;

        //
        // Increment past this sequence in the outter sequence 
        // 
        pStream += recordSize;
        size -= recordSize;


        if (ppSdpRecords) {
            ISdpRecord *pIRecord = NULL;

            hres = CoCreateInstance(__uuidof(SdpRecord),
                                    NULL,
                                    CLSCTX_INPROC_SERVER,
                                    __uuidof(ISdpRecord),
                                    (LPVOID *) &pIRecord);


            //
            // This will fail if the stream is malformed or does not follow the correct
            // server format or has attribute values which are of the wrong type
            //
            if (!SUCCEEDED(pIRecord->CreateFromStream(recordStream, recordSize))) {
                pIRecord->Release();
                continue;
            }
            else if (idxRecords < *pNumRecords) {
                ppSdpRecords[idxRecords] = pIRecord;
            }
        }

        idxRecords++;
    }

    *pNumRecords = idxRecords;

    return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveUuid128(UCHAR *pStream, GUID* pUuid128)
{
    SdpRetrieveUuid128(pStream, pUuid128);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveUint16(UCHAR *pStream, USHORT *pUint16)
{
    SdpRetrieveUint16(pStream, pUint16);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveUint32(UCHAR *pStream, ULONG *pUint32)
{
    SdpRetrieveUint32(pStream, pUint32);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveUint64(UCHAR *pStream, ULONGLONG *pUint64)
{
    SdpRetrieveUint64(pStream, pUint64);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveUint128(UCHAR *pStream, PSDP_ULARGE_INTEGER_16 pUint128)
{
    SdpRetrieveUint128(pStream, pUint128);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveInt16(UCHAR *pStream, SHORT *pInt16)
{
    SdpRetrieveUint16(pStream, (USHORT *) pInt16);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveInt32(UCHAR *pStream, LONG *pInt32)
{
	SdpRetrieveUint32(pStream, (ULONG *) pInt32);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveInt64(UCHAR *pStream, LONGLONG *pInt64)
{
	SdpRetrieveUint64(pStream, (ULONGLONG *) pInt64);
	return S_OK;
}

STDMETHODIMP CSdpStream::RetrieveInt128(UCHAR *pStream, PSDP_LARGE_INTEGER_16 pInt128)
{
	SdpRetrieveUint128(pStream, (PSDP_ULARGE_INTEGER_16) pInt128);
	return S_OK;
}


STDMETHODIMP CSdpStream::ByteSwapUuid128(GUID *pInUuid128, GUID *pOutUuid128)
{
	SdpByteSwapUuid128(pInUuid128, pOutUuid128);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapUint128(PSDP_ULARGE_INTEGER_16 pInUint128, PSDP_ULARGE_INTEGER_16 pOutUint128)
{
    SdpByteSwapUint128(pInUint128, pOutUint128);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapUint64(ULONGLONG inUint64, ULONGLONG *pOutUint64)
{
	*pOutUint64 = SdpByteSwapUint64(inUint64);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapUint32(ULONG uint32, ULONG *pUint32)
{
	*pUint32 = SdpByteSwapUint32(uint32);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapUint16(USHORT uint16, USHORT *pUint16)
{
	*pUint16 = SdpByteSwapUint16(uint16);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapInt128(PSDP_LARGE_INTEGER_16 pInInt128, PSDP_LARGE_INTEGER_16 pOutInt128)
{
    SdpByteSwapUint128((PSDP_ULARGE_INTEGER_16) pInInt128,
                       (PSDP_ULARGE_INTEGER_16) pOutInt128);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapInt64(LONGLONG inInt64, LONGLONG *pOutInt64)
{
	*pOutInt64 = (LONGLONG) SdpByteSwapUint64((LONGLONG) inInt64);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapInt32(LONG int32, LONG *pInt32)
{
	*pInt32 = (LONG) SdpByteSwapUint32((ULONG) int32);
	return S_OK;
}

STDMETHODIMP CSdpStream::ByteSwapInt16(SHORT int16, SHORT *pInt16)
{
	*pInt16 = (SHORT) SdpByteSwapUint16((USHORT) int16);
	return S_OK;
}

STDMETHODIMP CSdpStream::NormalizeUuid(
    NodeData *pDataUuid,
    GUID* pNormalizedUuid
    )
{
    return pNormalizeUuid(pDataUuid, pNormalizedUuid);
}

STDMETHODIMP CSdpStream::RetrieveElementInfo(
    UCHAR *pStream,
    SDP_TYPE *pElementType,
    SDP_SPECIFICTYPE *pElementSpecificType,
    ULONG *pElementSize,
    ULONG *pStorageSize,
    UCHAR **ppData
    )
{
    HRESULT hres = S_OK;

    ULONG sizes[] = { 1, 2, 4, 8, 16 };
//  ULONG stype;
    UCHAR type;
    UCHAR sizeIndex;

    SdpRetrieveHeader(pStream, type, sizeIndex);
    *pElementType = (SDP_TYPE) type;
    pStream++;

    if (pElementSpecificType != NULL) {
        *pElementSpecificType = SDP_ST_NONE;
    }
    *pStorageSize = 0;

    if (ppData != NULL) {
        *ppData = pStream;
    }

    switch (*pElementType) {
    case SDP_TYPE_NIL:
        if (ppData != NULL) {
            *ppData = NULL;
        }
        *pElementSize = 0;
        break;

    case SDP_TYPE_BOOLEAN:
        *pElementSize = 1;
        break;

    case SDP_TYPE_UINT:
    case SDP_TYPE_INT:
    case SDP_TYPE_UUID:
        if (sizeIndex > 4) {
            hres = E_FAIL;
            break;
        }

        if (pElementSpecificType != NULL) {
            *pElementSpecificType = (SDP_SPECIFICTYPE)
                ((sizeIndex << 8) | ((*pElementType) << 4));
        }
        *pElementSize = sizes[sizeIndex];
        break;

    case SDP_TYPE_URL:
    case SDP_TYPE_STRING:
    case SDP_TYPE_SEQUENCE:
    case SDP_TYPE_ALTERNATIVE:
        if (sizeIndex < 5 || sizeIndex > 7) {
            hres = E_FAIL;
            break;
        }

        SdpRetrieveVariableSize(pStream, sizeIndex, pElementSize, pStorageSize);

        if (ppData != NULL) {
            *ppData = pStream + *pStorageSize;
        }
        break;

    default:
        hres = E_FAIL;
    }

    return hres;
}

HRESULT CSdpStream::VerifySequenceOf(
    UCHAR *pStream,
    ULONG size,
    SDP_TYPE ofType,
    UCHAR *pSpecificSizes,
    ULONG *pNumFound
    )
{
    NTSTATUS status;

    //
    // TODO:  In theory, the user can pass an ISdpWalk * to this function and it
    //        would be called for every item we do find, but then we would have 
    //        to rewrite SdpVerifySequenceOf inline and conver the stream to an
    //        ISdpNodeContainer first...too much work for little gain!
    //
    status = SdpVerifySequenceOf(pStream, size, ofType, pSpecificSizes, pNumFound, NULL, NULL);

    if (NT_SUCCESS(status)) {
        return S_OK;
    }
    else {
        return E_FAIL;
    }
}
