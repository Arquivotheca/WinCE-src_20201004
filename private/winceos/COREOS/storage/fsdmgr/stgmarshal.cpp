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

#include "storeincludes.hpp"
#include "stgmarshal.hpp"

LRESULT CeOpenSGRequest (
    __out PSG_REQ* ppDuplicateSgReq,
    __in_bcount(InBufSize) const SG_REQ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount,
    DWORD BytesPerSector
    )
{
    LRESULT lResult;
    DWORD MarshalledBufferCount = 0;
    SG_REQ* pDuplicateSgReq = NULL;
    DWORD TotalSize = 0;

    // Set all the backup pointers to NULL.
    ZeroMemory (pPtrBackup, sizeof (PBYTE) * PtrBackupCount);

    // The input buffer must be at least as large as the SG_REQ structure; it
    // can be larger, having up to MAX_SG_BUF trailing SG_BUF structures.
    if (InBufSize < sizeof (SG_REQ)) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Make a duplicate heap copy of the top-level pointer so that the embedded
    // pointers cannot be modified asynchronously after validation.
    HRESULT hResult;
    hResult = CeAllocDuplicateBuffer ((PVOID*)&pDuplicateSgReq, (PVOID)pSgReq,
        InBufSize, ARG_I_PTR);
    if (E_OUTOFMEMORY == hResult) {
        lResult = ERROR_OUTOFMEMORY;
        goto open_done;
    }
    else if (FAILED (hResult)) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Verify we have sufficient space to backup all of the SG_BUF pointers
    // in the input SG_REQ. We typically expect PtrBackupCount to be MAX_SG_BUF.
    if (pDuplicateSgReq->sr_num_sg > PtrBackupCount) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Marshall all of the embedded pointers.
    for (MarshalledBufferCount = 0;
         MarshalledBufferCount < pDuplicateSgReq->sr_num_sg;
         MarshalledBufferCount ++) {

        // Save the original pointer before marshalling because it is needed
        // for the subsequent call CeCloseCallerBuffer.
        pPtrBackup[MarshalledBufferCount] = 
            pDuplicateSgReq->sr_sglist[MarshalledBufferCount].sb_buf;

        if (TotalSize + pDuplicateSgReq->sr_sglist[MarshalledBufferCount].sb_len < TotalSize) {
            // Integer overflow
            lResult = ERROR_INVALID_PARAMETER;
            goto open_done;
        }

        hResult = CeOpenCallerBuffer (
            (LPVOID*)&pDuplicateSgReq->sr_sglist[MarshalledBufferCount].sb_buf,
            (VOID*)pPtrBackup[MarshalledBufferCount],
            pDuplicateSgReq->sr_sglist[MarshalledBufferCount].sb_len,
            ArgDescriptor, FALSE);

        if (E_OUTOFMEMORY == hResult) {
            lResult = ERROR_OUTOFMEMORY;
            goto open_done;
        }
        else if (FAILED (hResult)) {
            lResult = ERROR_INVALID_PARAMETER;
            goto open_done;
        }

        TotalSize += pDuplicateSgReq->sr_sglist[MarshalledBufferCount].sb_len;        
    }

    if (TotalSize != pDuplicateSgReq->sr_num_sec * BytesPerSector) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Success. Return the duplicated top-level buffer.
    *ppDuplicateSgReq = pDuplicateSgReq;
    lResult = ERROR_SUCCESS;

open_done:
    if ((ERROR_SUCCESS != lResult) && (NULL != pDuplicateSgReq)) {

        // Close the opened embedded buffers.
        for (DWORD i = 0; i < MarshalledBufferCount; i++) {

            DEBUGCHK (pPtrBackup[i]);
            VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                pDuplicateSgReq->sr_sglist[i].sb_buf, pPtrBackup[i],
                pDuplicateSgReq->sr_sglist[i].sb_len, ArgDescriptor)));
        }

        // Free the duplciate top-level buffer.
        VERIFY (SUCCEEDED (CeFreeDuplicateBuffer ((VOID*)pDuplicateSgReq,
            (VOID*)pSgReq, InBufSize, ARG_I_PTR)));
    }

    return lResult;
}

LRESULT CeCloseSGRequest (
    __in_bcount(InBufSize) SG_REQ* pDuplicateSgReq,
    __in_bcount(InBufSize) const SG_REQ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    )
{
    LRESULT lResult;

    if (pDuplicateSgReq->sr_num_sg > PtrBackupCount) {
        lResult = ERROR_INVALID_PARAMETER;
        goto close_done;
    }

    HRESULT hResult;

    // Close the opened embedded buffers.
    for (DWORD i = 0; i < pDuplicateSgReq->sr_num_sg; i++) {

        DEBUGCHK (pPtrBackup[i]);

        hResult = CeCloseCallerBuffer (pDuplicateSgReq->sr_sglist[i].sb_buf,
            pPtrBackup[i], pDuplicateSgReq->sr_sglist[i].sb_len, ArgDescriptor);

        if (FAILED (hResult)) {
            DEBUGCHK (0);
            lResult = ERROR_INVALID_PARAMETER;
            goto close_done;
        }
    }

    // Free the duplciate top-level buffer.
    hResult = CeFreeDuplicateBuffer ((VOID*)pDuplicateSgReq, (VOID*)pSgReq,
        InBufSize, ARG_I_PTR);

    if (FAILED (hResult)) {
        DEBUGCHK (0);
        lResult = ERROR_INVALID_PARAMETER;
        goto close_done;
    }

    lResult = ERROR_SUCCESS;

close_done:
    return lResult;
}

LRESULT CeOpenSGXRequest (
    __out PCDROM_READ* ppDuplicateSgReq,
    __in_bcount(InBufSize) const CDROM_READ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    )
{
    LRESULT lResult;
    DWORD MarshalledBufferCount = 0;
    CDROM_READ* pDuplicateSgReq = NULL;

    // Set all the backup pointers to NULL.
    ZeroMemory (pPtrBackup, sizeof (PBYTE) * PtrBackupCount);

    // The input buffer must be at least as large as the CDROM_READ structure;
    // it can be larger, having up to MAX_SG_BUF trailing SGX_BUF structures.
    if (InBufSize < sizeof (CDROM_READ)) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Make a duplicate heap copy of the top-level pointer so that the embedded
    // pointers cannot be modified asynchronously after validation.
    HRESULT hResult;
    hResult = CeAllocDuplicateBuffer ((PVOID*)&pDuplicateSgReq, (PVOID)pSgReq,
        InBufSize, ARG_I_PTR);
    if (E_OUTOFMEMORY == hResult) {
        lResult = ERROR_OUTOFMEMORY;
        goto open_done;
    }
    else if (FAILED (hResult)) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Verify we have sufficient space to backup all of the SG_BUF pointers
    // in the input SG_REQ. We typically expect PtrBackupCount to be MAX_SG_BUF.
    if (pDuplicateSgReq->sgcount > PtrBackupCount) {
        lResult = ERROR_INVALID_PARAMETER;
        goto open_done;
    }

    // Marshall all of the embedded pointers.
    for (MarshalledBufferCount = 0;
         MarshalledBufferCount < pDuplicateSgReq->sgcount;
         MarshalledBufferCount ++) {

        // Save the original pointer before marshalling because it is needed
        // for the subsequent call CeCloseCallerBuffer.
        pPtrBackup[MarshalledBufferCount] =
            pDuplicateSgReq->sglist[MarshalledBufferCount].sb_buf;

        hResult = CeOpenCallerBuffer (
            (LPVOID*)&pDuplicateSgReq->sglist[MarshalledBufferCount].sb_buf,
            (VOID*)pPtrBackup[MarshalledBufferCount],
            pDuplicateSgReq->sglist[MarshalledBufferCount].sb_len,
            ArgDescriptor, FALSE);

        if (E_OUTOFMEMORY == hResult) {
            lResult = ERROR_OUTOFMEMORY;
            goto open_done;
        }
        else if (FAILED (hResult)) {
            lResult = ERROR_INVALID_PARAMETER;
            goto open_done;
        }
    }

    // Success. Return the duplicated top-level buffer.
    *ppDuplicateSgReq = pDuplicateSgReq;
    lResult = ERROR_SUCCESS;

open_done:
    if ((ERROR_SUCCESS != lResult) && (NULL != pDuplicateSgReq)) {

        // Close the opened embedded buffers.
        for (DWORD i = 0; i < MarshalledBufferCount; i++) {

            DEBUGCHK (pPtrBackup[i]);
            VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                pDuplicateSgReq->sglist[i].sb_buf, pPtrBackup[i],
                pDuplicateSgReq->sglist[i].sb_len, ArgDescriptor)));
        }

        // Free the duplciate top-level buffer.
        VERIFY (SUCCEEDED (CeFreeDuplicateBuffer ((VOID*)pDuplicateSgReq,
            (VOID*)pSgReq, InBufSize, ARG_I_PTR)));
    }

    return lResult;
}

LRESULT CeCloseSGXRequest (
    __in_bcount(InBufSize) CDROM_READ* pDuplicateSgReq,
    __in_bcount(InBufSize) const CDROM_READ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    )
{
    LRESULT lResult;

    if (pDuplicateSgReq->sgcount > PtrBackupCount) {
        lResult = ERROR_INVALID_PARAMETER;
        goto close_done;
    }

    HRESULT hResult;

    // Close the opened embedded buffers.
    for (DWORD i = 0; i < pDuplicateSgReq->sgcount; i++) {

        DEBUGCHK (pPtrBackup[i]);

        hResult = CeCloseCallerBuffer (pDuplicateSgReq->sglist[i].sb_buf,
            pPtrBackup[i], pDuplicateSgReq->sglist[i].sb_len, ArgDescriptor);

        if (FAILED (hResult)) {
            DEBUGCHK (0);
            lResult = ERROR_INVALID_PARAMETER;
            goto close_done;
        }
    }

    // Free the duplciate top-level buffer.
    hResult = CeFreeDuplicateBuffer ((VOID*)pDuplicateSgReq, (VOID*)pSgReq,
        InBufSize, ARG_I_PTR);

    if (FAILED (hResult)) {
        DEBUGCHK (0);
        lResult = ERROR_INVALID_PARAMETER;
        goto close_done;
    }

    lResult = ERROR_SUCCESS;

close_done:
    return lResult;
}
