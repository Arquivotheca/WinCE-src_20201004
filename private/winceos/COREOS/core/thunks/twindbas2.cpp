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
#define EDB
#define BUILDING_FS_STUBS
#include <windows.h>

//
// SDK exports
//

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeGetDBInformationByHandle
extern "C"
BOOL xxx_CeGetDBInformationByHandle(HANDLE hDbase, LPBY_HANDLE_DB_INFORMATION lpDBInfo)
{
    return CeGetDBInformationByHandle_Trap(hDbase, lpDBInfo);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeFindFirstDatabase

extern "C"
HANDLE xxx_CeFindFirstDatabaseEx(PCEGUID pguid, DWORD dwClassID)
{
    return CeFindFirstDatabaseEx_Trap(pguid, dwClassID);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeFindNextDatabase

extern "C"
CEOID xxx_CeFindNextDatabaseEx(HANDLE hEnum, PCEGUID pguid)
{
    return CeFindNextDatabaseEx_Trap(hEnum, pguid);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeCreateDatabase

extern "C"
CEOID xxx_CeCreateDatabaseEx2(PCEGUID pguid, CEDBASEINFOEX *pInfo)
{
    return CeCreateDatabaseEx2_Trap(pguid, pInfo);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeSetDatabaseInfo

extern "C"
BOOL xxx_CeSetDatabaseInfoEx2(PCEGUID pguid, CEOID oidDbase, CEDBASEINFOEX *pNewInfo)
{
    return CeSetDatabaseInfoEx2_Trap(pguid, oidDbase, pNewInfo);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeOpenDatabase

extern "C"
HANDLE
xxx_CeOpenDatabaseEx2(
                      PCEGUID             pguid,
                      PCEOID              poid,
                      LPWSTR              lpszName,
                      SORTORDERSPECEX*    pSort,
                      DWORD               dwFlags,
                      CENOTIFYREQUEST*    pRequest
                      )
{
    return CeOpenDatabaseEx2_Trap(pguid, poid, lpszName, pSort, dwFlags, pRequest);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeDeleteDatabase

extern "C"
BOOL xxx_CeDeleteDatabaseEx(PCEGUID pguid, CEOID oid)
{
    return CeDeleteDatabaseEx_Trap(pguid, oid);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeSeekDatabase

extern "C"
CEOID xxx_CeSeekDatabaseEx(HANDLE hDatabase, DWORD dwSeekType, DWORD dwValue,
                           WORD wNumVals, LPDWORD lpdwIndex)
{
    // The PSL expects a DWORD, not a WORD.
    DWORD dwNumVals = (DWORD)wNumVals;
    return CeSeekDatabaseEx_Trap(hDatabase, dwSeekType, dwValue, dwNumVals, lpdwIndex);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeDeleteRecord

extern "C"
BOOL xxx_CeDeleteRecord(HANDLE hDatabase, CEOID oidRecord)
{
    return CeDeleteRecord_Trap(hDatabase,oidRecord);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeReadRecordProps

extern "C"
CEOID xxx_CeReadRecordPropsEx(HANDLE hDbase, DWORD dwFlags, LPWORD lpcPropID,
                              CEPROPID *rgPropID, LPBYTE *lplpBuffer,
                              LPDWORD lpcbBuffer, HANDLE hHeap)
{   
    CEOID oidRet = NULL;

    if (NULL == lplpBuffer) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (NULL == hHeap) {
        hHeap = GetProcessHeap();
    }

    for (;;)
    {
        // Remove the re-alloc flag. This thunk performs the re-alloc instead of the db engine.
        DWORD dwLocalFlags = dwFlags & ~CEDB_ALLOWREALLOC;        

        // NOTE: The PSL signature is different from the CeReadRecordPropsEx API. Expand the
        // I/O parameters into simpler parameters.
        C_ASSERT (MAXWORD < MAXDWORD / sizeof (CEPROPID)); // This should never overflow.
        DWORD cbPropID = (lpcPropID && rgPropID) ? (*lpcPropID * sizeof (CEPROPID)) : 0;
        BYTE* lpBuffer = *lplpBuffer;
        DWORD cbBuffer = (lpcbBuffer && lpBuffer) ? *lpcbBuffer : 0;
        
        oidRet = CeReadRecordPropsEx_Trap(hDbase, dwLocalFlags, rgPropID, cbPropID, lpcPropID, lpBuffer, 
            cbBuffer, lpcbBuffer, hHeap);
        
        if ((NULL != oidRet) || (NULL == lpcbBuffer) ||
            !(CEDB_ALLOWREALLOC & dwFlags) || (ERROR_INSUFFICIENT_BUFFER != GetLastError()))
        {
            // Exit for one of the following reasons:
            //
            // 1) Successfully read the record (returned a valid record oid).
            // 2) Caller passed in a NULL buffer pointer reference, so it cannot be re-allocated.
            // 3) Caller passed in a NULL buffer size pointer, so size cannot be calculated.
            // 3) Caller did not specify CEDB_ALLOWREALLOC flag, so cannot perform a re-allocation.
            // 4) Read operation failed for a reason other than ERROR_INSUFFICIENT_BUFFER.
            //
            break;
        }

        // Allocate a new, larger buffer on behalf of the caller. The caller is responsible for freeing
        // this memory.
        BYTE* lpLocalBuffer = (BYTE*)HeapAlloc(hHeap, 0, *lpcbBuffer);

        if (!lpLocalBuffer)
        {
            // Re-allocation of memory failed, nothing more we can do.
            break;
        }

        if (*lplpBuffer) 
        {
            // Free the existing buffer on behalf of the caller.
            HeapFree(hHeap, 0, *lplpBuffer);
        }
        
        *lplpBuffer = lpLocalBuffer;
        // Go to the top and retry the CeReadRecordPropsEx call with the new buffer.
    }
    
    return oidRet;    
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeSeekDatabase

extern "C"
CEOID
xxx_CeSeekDatabase(
                   HANDLE  hDatabase,
                   DWORD   dwSeekType,
                   DWORD   dwValue,
                   LPDWORD lpdwIndex
                   )
{
    // Call signature is the same except wNumVals is always 1
    return CeSeekDatabaseEx_Trap(hDatabase, dwSeekType, dwValue, 1, lpdwIndex);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeReadRecordProps


extern "C"
CEOID
xxx_CeReadRecordProps(
                      HANDLE      hDbase,
                      DWORD       dwFlags,
                      LPWORD      lpcPropID,
                      CEPROPID*   rgPropID,
                      LPBYTE*     lplpBuffer,
                      LPDWORD     lpcbBuffer
                      )
{
    return xxx_CeReadRecordPropsEx(hDbase, dwFlags, lpcPropID, rgPropID, lplpBuffer, lpcbBuffer, NULL);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeWriteRecordProps

extern "C"
CEOID
xxx_CeWriteRecordProps(
                       HANDLE      hDbase,
                       CEOID       oidRecord,
                       WORD        cPropID,
                       CEPROPVAL*  rgPropVal
                       )
{    
    // NOTE: The PSL signature is different from the CeWriterecordProps API. Expand the
    // I/O parameters into simpler parameters.
    C_ASSERT (MAXWORD < MAXDWORD / sizeof (CEPROPVAL)); // This should never overflow
    DWORD cbPropVal = cPropID * sizeof (CEPROPVAL);
    return CeWriteRecordProps_Trap(hDbase, oidRecord, rgPropVal, cbPropVal);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Volume API's

extern "C"
BOOL xxx_CeEnumDBVolumes(PCEGUID pguid, LPWSTR lpBuf, DWORD SizeInChars)
{
    if (SizeInChars > MAXDWORD / sizeof(WCHAR)) {
        SetLastError (ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    // PSL expects output buffer size in bytes, not characters.
    DWORD SizeInBytes = SizeInChars * sizeof(WCHAR);
    return CeEnumDBVolumes_Trap(pguid, lpBuf, SizeInBytes);
}

extern "C"
BOOL xxx_CeMountDBVol(PCEGUID pguid, LPWSTR lpszVol, DWORD dwFlags)
{
    return CeMountDBVol_Trap(pguid, lpszVol, dwFlags);
}

extern "C"
BOOL xxx_CeUnmountDBVol(PCEGUID pguid)
{
    return CeUnmountDBVol_Trap(pguid);
}    

extern "C"
BOOL xxx_CeFlushDBVol(PCEGUID pguid)
{
    return CeFlushDBVol_Trap(pguid);
}    

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeOidGetInfo


extern "C"
BOOL xxx_CeOidGetInfoEx2(PCEGUID pguid, CEOID oid, CEOIDINFOEX *oidInfo)
{
    return CeOidGetInfoEx2_Trap(pguid, oid, oidInfo);
}


//
// OAK exports
//

extern "C"
VOID xxx_CeChangeDatabaseLCID(PCEGUID pguid, DWORD LCID)
{
    CeChangeDatabaseLCID_Trap(pguid, LCID);
}

// New EDB Functions

extern "C" 
CEOID xxx_CeCreateDatabaseWithProps(PCEGUID pGuid, CEDBASEINFOEX* pInfo, DWORD cProps, CEPROPSPEC* prgProps)
{    
    if (cProps > (MAXDWORD / sizeof(CEPROPSPEC))) {
        // Integer multiplication overflow.
        SetLastError (ERROR_ARITHMETIC_OVERFLOW);
        return 0;
    } 
    DWORD PropsSize = sizeof(CEPROPSPEC) * cProps;
    return CeCreateDatabaseWithProps_Trap(pGuid, pInfo, cProps, prgProps, PropsSize);
}

extern "C" 
BOOL xxx_CeAddDatabaseProps(PCEGUID pGuid, CEOID oidDb, DWORD cProps, CEPROPSPEC* prgProps)
{
    if (cProps > (MAXDWORD / sizeof(CEPROPSPEC))) {
        // Integer multiplication overflow.
        SetLastError (ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    DWORD PropsSize = sizeof(CEPROPSPEC) * cProps;
    return CeAddDatabaseProps_Trap(pGuid,  oidDb, cProps,  prgProps, PropsSize);
}

extern "C" 
BOOL xxx_CeRemoveDatabaseProps(PCEGUID pGuid, CEOID oidDb, DWORD cPropID, CEPROPID* prgPropID)
{
    if (cPropID > (MAXDWORD / sizeof(CEPROPID))) {
        // Integer multiplication overflow.
        SetLastError (ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    DWORD PropIDSize = sizeof(CEPROPID) * cPropID;
    return CeRemoveDatabaseProps_Trap(pGuid,  oidDb, cPropID,  prgPropID, PropIDSize);
}


extern "C" 
BOOL xxx_CeMountDBVolEx(PCEGUID pGuid, LPWSTR pwszDBVol, CEVOLUMEOPTIONS* pOptions, DWORD dwFlags)
{
    return CeMountDBVolEx_Trap(pGuid, pwszDBVol, pOptions, dwFlags);
}


// Session API's
extern "C" 
HANDLE xxx_CeCreateSession(CEGUID* pGuid)
{
    return CeCreateSession_Trap(pGuid);
}

//-----------------------------------------------------------------------
// Tracking functions
//
extern "C" 
BOOL xxx_CeAddSyncPartner(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid, LPCWSTR pwszFriendlyName, LPCWSTR pwszFullName)
{
    return CeAddSyncPartner_Trap(pVolGuid, pSyncPartnerGuid, pwszFriendlyName, pwszFullName);
}
         
extern "C" 
BOOL xxx_CeRemoveSyncPartner(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid)
{
    return CeRemoveSyncPartner_Trap(pVolGuid, pSyncPartnerGuid);
}

extern "C" 
BOOL xxx_CeTrackDatabase(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid, CEOID oidDB, DWORD  dwTrackingFlags)
{
    return CeTrackDatabase_Trap( pVolGuid, pSyncPartnerGuid, oidDB, dwTrackingFlags);
}

extern "C" 
BOOL xxx_CeRemoveDatabaseTracking(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid, CEOID oidDB)
{
    return CeRemoveDatabaseTracking_Trap( pVolGuid, pSyncPartnerGuid, oidDB);
}
    
extern "C" 
BOOL xxx_CeTrackProperty(PCEGUID pVolGuid, CEOID oidDB, CEPROPID propid, BOOL fAddToScheme)
{
    return CeTrackProperty_Trap( pVolGuid, oidDB, propid, fAddToScheme);
}
    
extern "C" 
BOOL xxx_CePurgeTrackingData(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid, SYSTEMTIME* pstThreshold)
{
    return CePurgeTrackingData_Trap( pVolGuid, pSyncPartnerGuid, pstThreshold);
}

extern "C" 
BOOL xxx_CePurgeTrackingGenerations(PCEGUID pVolGuid, PCEGUID pSyncPartnerGuid, CEDBGEN genThreshold)
{
    // PSL requires pointers to 64-bit parameters (CEDBGEN)
    CEDBGEN* pGenThreshold  = &genThreshold;
    return CePurgeTrackingGenerations_Trap( pVolGuid, pSyncPartnerGuid, pGenThreshold);
}

extern "C" 
BOOL xxx_CeGetDatabaseProps(HANDLE hHandle, WORD* pcPropID, CEPROPID* prgPropID, CEPROPSPEC* prgProps)
{
    // Calculate buffer sizes for prgPropID and prgProps based on prop ID count.
    C_ASSERT (MAXWORD < MAXDWORD / sizeof (CEPROPVAL)); // This should never overflow
    C_ASSERT (MAXWORD < MAXDWORD / sizeof (CEPROPSPEC)); // This should never overflow
    DWORD PropIDSize    = pcPropID ? (*pcPropID * sizeof(CEPROPID)) : 0;
    DWORD PropsSize     = pcPropID ? (*pcPropID * sizeof(CEPROPSPEC)) : 0;

    // PSL requires the size of input buffer prgPropID and output buffer prgProps immediately
    // following the buffer.
    return CeGetDatabaseProps_Trap(hHandle, pcPropID, prgPropID, PropIDSize, prgProps, PropsSize);
}

extern "C" 
BOOL xxx_CeSetSessionOption(HANDLE hSession, ULONG ulOptionId, DWORD dwValue)
{
    return CeSetSessionOption_Trap(hSession, ulOptionId, dwValue);
}

extern "C" 
HANDLE xxx_CeGetDatabaseSession(HANDLE hDatabase)
{
    return CeGetDatabaseSession_Trap(hDatabase);
}

extern "C" 
BOOL xxx_CeBeginTransaction(HANDLE hSession, CEDBISOLATIONLEVEL isoLevel)
{
    return CeBeginTransaction_Trap(hSession, isoLevel);
}

extern "C" 
BOOL xxx_CeEndTransaction(HANDLE hSession, BOOL fCommit)
{
    return CeEndTransaction_Trap(hSession, fCommit);
}

extern "C" 
HANDLE xxx_CeOpenDatabaseInSession(HANDLE hSession, PCEGUID pGuid, PCEOID poid, LPWSTR pwszName, 
    SORTORDERSPECEX* pSort, DWORD dwFlags, CENOTIFYREQUEST* pRequest)
{
    if (NULL == hSession) {
        // call CeOpenDatabaseEx2 because there is no valid session in which to open the handle
        return xxx_CeOpenDatabaseEx2(pGuid, poid, pwszName, pSort, dwFlags, pRequest);
    } else {
        return CeOpenDatabaseInSession_Trap(hSession, pGuid, poid, pwszName, pSort, dwFlags, pRequest); 
    }
}

extern "C" 
HANDLE xxx_CeOpenStream(HANDLE hDatabase, CEPROPID propid, DWORD dwMode)
{
    return CeOpenStream_Trap(hDatabase, propid, dwMode);
}

extern "C" 
BOOL xxx_CeStreamRead(HANDLE hStream, BYTE* prgbBuffer, const DWORD cbRead, DWORD* pcbRead)
{
    return CeStreamRead_Trap(hStream, prgbBuffer, cbRead, pcbRead);
}

extern "C" 
BOOL xxx_CeStreamWrite(HANDLE hStream, BYTE* prgbBuffer, DWORD cbWrite, DWORD* pcbWritten)
{
    return CeStreamWrite_Trap(hStream, prgbBuffer, cbWrite, pcbWritten);
}

extern "C" 
BOOL xxx_CeStreamSaveChanges(HANDLE hStream)
{
    return CeStreamSaveChanges_Trap(hStream);
}

extern "C" 
BOOL xxx_CeStreamSeek(HANDLE hStream, DWORD cbMove, DWORD dwOrigin, DWORD* pcbNewOffset)
{
    return CeStreamSeek_Trap(hStream, cbMove, dwOrigin, pcbNewOffset);
}

extern "C" 
BOOL xxx_CeStreamSetSize(HANDLE hStream, const DWORD cbSize)
{    
    return CeStreamSetSize_Trap(hStream, cbSize);
}

extern "C" 
BOOL xxx_CeBeginSyncSession(HANDLE hSession, PCEGUID pSyncPartnerGuid, CEDBGEN genFrom, CEDBGEN genTo,
    DWORD dwFlags, CEDBGEN* pGenCur)
{    
    // PSL requires pointers to 64-bit parameters (CEDBGEN)
    CEDBGEN* pGenFrom    = &genFrom;
    CEDBGEN* pGenTo      = &genTo;
    return CeBeginSyncSession_Trap(hSession, pSyncPartnerGuid, pGenFrom, pGenTo, dwFlags, pGenCur);
}

extern "C" 
BOOL xxx_CeEndSyncSession(HANDLE hSession, DWORD dwOutcome)
{
    return CeEndSyncSession_Trap(hSession, dwOutcome);
}

extern "C" 
BOOL xxx_CeGetChangedRecordCnt(HANDLE hSession, CEOID oidDB, DWORD* pdwCnt)
{   
    return CeGetChangedRecordCnt_Trap(hSession, oidDB, pdwCnt);
}

extern "C" 
HANDLE xxx_CeGetChangedRecords(HANDLE hSession, CEOID oidDB, DWORD dwChangeType)
{
    return CeGetChangedRecords_Trap(hSession, oidDB, dwChangeType);
}


extern "C" 
CEOID xxx_CeFindNextChangedRecord(HANDLE hChangeEnum)
{
    return CeFindNextChangedRecord_Trap(hChangeEnum);
}

extern "C" 
BOOL xxx_CeGetPropChangeInfo(HANDLE hChangeEnum, CEOID oidChangedRecord, CEPROPID propid, BOOL* pfPropChanged)
{
    return CeGetPropChangeInfo_Trap(hChangeEnum, oidChangedRecord, propid, pfPropChanged);
}

extern "C" 
BOOL xxx_CeGetRecordChangeInfo(HANDLE hChangeEnum, CEOID oidChangedRecord, CECHANGEINFO* pInfo)
{
    return CeGetRecordChangeInfo_Trap(hChangeEnum, oidChangedRecord, pInfo);
}

extern "C" 
BOOL xxx_CeMarkRecord(HANDLE hChangeEnum, CEOID oidChangedRecord, BOOL fChanged)
{
    return  CeMarkRecord_Trap(hChangeEnum, oidChangedRecord, fChanged);
}

extern "C" 
BOOL xxx_CeAttachCustomTrackingData(HANDLE hDB, CEOID oidChangedRecord, BYTE* rgbData, DWORD ccb)
{
    return CeAttachCustomTrackingData_Trap(hDB, oidChangedRecord, rgbData, ccb);
}

extern "C" 
BOOL xxx_CeGetCustomTrackingData(HANDLE hDB, CEOID oidChangedRecord, BYTE* rgbData, DWORD* pccb)
{   
    // PSL requires the size of rgbData passed immediately following the buffer.
    DWORD cbData = pccb ? *pccb : 0;
    return CeGetCustomTrackingData_Trap(hDB, oidChangedRecord, rgbData, cbData, pccb);
}

