//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include <pnp.h>
#include <devload.h>
#include <msgqueue.h>
#include <storemgr.h>

typedef UINT (*PFN_TESTPROC)(CEGUID*);

// test function prototypes

UINT Tst_EnumVolumes(IN CEGUID* pguid);
UINT Tst_EnumDBs(IN CEGUID* pguid);
UINT Tst_FlushVolume(IN CEGUID* pguid);
UINT Tst_FlushAllVolumes(IN CEGUID* pguid);
UINT Tst_ReadRandomDatabase(IN CEGUID* pguid);
UINT Tst_CreateRecords(IN CEGUID* pguid);
UINT Tst_DeleteRecord(IN CEGUID* pguid);
UINT Tst_ThinDatabase(IN CEGUID* pguid);

// constants 

static const DWORD GLOBAL_SORT_KEYS[]           = {
    CEVT_I2, CEVT_UI2, CEVT_I4,
    CEVT_UI4, CEVT_FILETIME, CEVT_LPWSTR,
    CEVT_BOOL, CEVT_R8
};
static const DWORD GLOBAL_SORT_KEY_COUNT = sizeof(GLOBAL_SORT_KEYS)/sizeof(GLOBAL_SORT_KEYS[0]);

static const DWORD MAX_DB_VOLS          = 10;
static const DWORD DEF_THREAD_COUNT     = 3;
static const DWORD MAX_RECORDS_PER_WRITE = 2;

static const PFN_TESTPROC TEST_FUNCTION_TABLE[] = {
    Tst_EnumVolumes,
    Tst_EnumDBs,
    Tst_FlushVolume,
    Tst_FlushAllVolumes,
    Tst_ReadRandomDatabase,
    Tst_CreateRecords,
    Tst_DeleteRecord,
    Tst_ThinDatabase
};
static const DWORD TEST_FUNCTION_TABLE_COUNT = sizeof(TEST_FUNCTION_TABLE)/sizeof(TEST_FUNCTION_TABLE[0]);

static const TCHAR TEST_DATABASE_NAME[] = TEXT("StressTestDB");
static const TCHAR TEST_DBVOLUME_NAME[] = TEXT("StressTestVolume");
static const DWORD TEST_DATABASE_TYPE   = 0xBADDB123;
static const DWORD TEST_DATABASE_SIZE   = 1000;
static const TCHAR DB_FIRSTRUN_REGKEY[] = TEXT("Software\\Stress\\s2_db");

// message queue support

static const DWORD MAX_DEVNAME_LEN      = 100;
static const DWORD QUEUE_ITEM_SIZE      = (sizeof(DEVDETAIL) + MAX_DEVNAME_LEN);

// globals

TCHAR   g_szModuleName[MAX_PATH]        = TEXT("");
HANDLE  g_hInst                         = NULL;
LPTSTR  g_pszCmdLine                    = NULL;
DWORD   g_cThreads                      = DEF_THREAD_COUNT;

// global dbvol table

CEGUID g_guidDBVols[MAX_DB_VOLS]        = {0};
DWORD g_cDBVols                         = 0;

// functions

/////////////////////////////////////////////////////////////////////////////////////
HANDLE CreatePnpMsgQueue(VOID)
{
    // create a message queue
    MSGQUEUEOPTIONS msgqopts = {0};    
    msgqopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgqopts.dwFlags = 0;
    msgqopts.cbMaxMessage = QUEUE_ITEM_SIZE;
    msgqopts.bReadAccess = TRUE;
    
    return CreateMsgQueue(NULL, &msgqopts);
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL ReadPnpMsgQueue(
    IN HANDLE hQueue, 
    OUT PDEVDETAIL pDevDetail, 
    IN DWORD dwTimeout = 0)
{
    // we don't need either of these values from storage manager
    DWORD dwRead = 0; 
    DWORD dwFlags =0;
    return ReadMsgQueue(hQueue, pDevDetail, QUEUE_ITEM_SIZE, &dwRead, 
        dwTimeout, &dwFlags);
}

/////////////////////////////////////////////////////////////////////////////////////
inline BOOL FlipACoin(VOID)
{
    return (Random() % 2);
}

/////////////////////////////////////////////////////////////////////////////////////
void LogVolumeGUID(DWORD logLevel, PCEGUID pGuidVol) {
    Log(logLevel, TEXT("database volume guid {%08X %08X %08X %08X}"), pGuidVol->Data1,
        pGuidVol->Data2, pGuidVol->Data3, pGuidVol->Data4);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT EnumVols(
    VOID
    )
{
    UINT retVal = CESTRESS_PASS;
    DWORD cVols;
    DWORD dwLastErr;
    CEGUID guidVol;
    HANDLE hEnum = INVALID_HANDLE_VALUE;
    LPTSTR pszVolName = NULL;
    DWORD cbVolName = MAX_PATH*sizeof(TCHAR);

    pszVolName = (LPTSTR)LocalAlloc(LMEM_FIXED, cbVolName);
    if(NULL == pszVolName) {
        LogFail(TEXT("LocalAlloc() of %u bytes failed; error %u"), cbVolName, GetLastError()); 
        retVal = CESTRESS_FAIL;
        goto done;
    }

    CREATE_INVALIDGUID(&guidVol);

    cVols = 0;
    do {
        
        while(CeEnumDBVolumes(&guidVol, pszVolName, cbVolName/sizeof(TCHAR))) {
            cVols++;
            LogVerbose(TEXT("found database volume \"%s\""), pszVolName);
        }
        
        dwLastErr = GetLastError();

        if(ERROR_NO_MORE_ITEMS == dwLastErr) {

            LogVerbose(TEXT("finished enumerating databases"));
            break;

        } else if(ERROR_INSUFFICIENT_BUFFER == dwLastErr) {

            // try doubling the size of the buffer
            LPTSTR pszTmp;
            cbVolName *= 2;
            pszTmp = (LPTSTR)LocalReAlloc(pszVolName, cbVolName, LMEM_MOVEABLE);
            if(NULL == pszTmp) {
                LogFail(TEXT("LocalReAlloc() of %u bytes failed; error %u"), cbVolName, GetLastError()); 
                retVal = CESTRESS_FAIL;
                goto done;
            }
            pszVolName = pszTmp;
            continue;

        } else {
        
            LogFail(TEXT("CeEnumDBVolumes failed; error %u"), dwLastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }
    } while(retVal == CESTRESS_PASS);

done:
    if(pszVolName) {
        LocalFree(pszVolName);
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT EnumDBs(
    IN PCEGUID pGuidVol,
    IN DWORD dbType = 0
    )
{
    UINT retVal = CESTRESS_PASS;
    DWORD cDBs, lastErr;
    CEOID oidDb;
    CEOIDINFOEX oidInfoEx;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    
    cDBs = 0;

    LogVerbose(TEXT("enumerating databases"));
    LogVolumeGUID(SLOG_VERBOSE, pGuidVol);
    hFind = CeFindFirstDatabaseEx(pGuidVol, dbType);
    if(INVALID_HANDLE_VALUE == hFind) {
        LogFail(TEXT("CeFindFirstDatabaseEx() failed; error %u"), GetLastError());
        LogVolumeGUID(SLOG_FAIL, pGuidVol);
        retVal = CESTRESS_FAIL;
        goto done;
    }

    while(oidDb = CeFindNextDatabaseEx(hFind, pGuidVol)) {

        oidInfoEx.wVersion = 1;
        if(!CeOidGetInfoEx2(pGuidVol, oidDb, &oidInfoEx)) {
            LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
            goto done;
        }

        if(OBJTYPE_DATABASE != oidInfoEx.wObjType) {
            LogFail(TEXT("object is not of type OBJTYPE_DATABASE"));
            retVal = CESTRESS_FAIL;
            goto done;
        }

        LogVerbose(TEXT("enumerated database \"%s\" on volume"), oidInfoEx.infDatabase.szDbaseName);
        
        cDBs++;
    }
    lastErr = GetLastError();
    
    if(ERROR_NO_MORE_ITEMS == lastErr) {
        LogVerbose(TEXT("enumerated %u databases on the volume"), cDBs);
        LogVolumeGUID(SLOG_VERBOSE, pGuidVol);
    } else if(ERROR_KEY_DELETED == lastErr) {
        LogComment(TEXT("a database was deleted during volume enumeration"));
        LogVolumeGUID(SLOG_COMMENT, pGuidVol);
    }

done:
    if(INVALID_HANDLE_VALUE != hFind) {
        if(!CloseHandle(hFind)) {
            LogFail(TEXT("CloseHandle() failed on CeFindFirstDatabaseEx enumeration HANDLE; error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }   
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD CheckDbInfo(
    PCEGUID pceguid,
    CEOID oid
    )
{
    DWORD retVal = CESTRESS_PASS;
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOIDINFOEX oidInfoEx;
    BY_HANDLE_DB_INFORMATION dbByHandleInfo;

    // open the DB using the oid
    hDb = CeOpenDatabaseEx2(pceguid, &oid, NULL, NULL, 0, NULL);
    if(INVALID_HANDLE_VALUE == hDb) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on db OID=%08X; error %u"), oid, GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    // query info by handle
    dbByHandleInfo.wVersion = 1;
    if(!CeGetDBInformationByHandle(hDb, &dbByHandleInfo)) {
        LogFail(TEXT("CeGetDBInformationByHandle() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    // verify info looks correct
    if(0 != memcmp(&dbByHandleInfo.guidVol, pceguid, sizeof(CEGUID))) {
        LogWarn1(TEXT("volume GUID returned by CeGetDBInformationByHandle() is incorrect"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    if(dbByHandleInfo.oidDbase != oid) {
        LogWarn1(TEXT("volume OID returned by CeGetDBInformationByHandle() is incorrect"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    oidInfoEx.wVersion = 1;
    if(!CeOidGetInfoEx2(pceguid, oid, &oidInfoEx)) {
        LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;    
    }

    if(OBJTYPE_DATABASE != oidInfoEx.wObjType) {
        LogWarn1(TEXT("oid type returned by CeOidGetInfoEx() is not OBJTYPE_DATABASE as expected"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    if(0 != memcmp(&oidInfoEx.infDatabase, &dbByHandleInfo.infDatabase, sizeof(CEDBASEINFOEX))) {
        LogWarn1(TEXT("CEDBASEINFOEX structure returned by CeOidGetInfoEx2() differs from CeGetDBInformationByHandle()"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

done:
    if(INVALID_HANDLE_VALUE != hDb) {
        if(!CloseHandle(hDb)) {
            LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
CEOID GetRandomDatabaseOid(PCEGUID pguid) 
{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    CEOID oidDB = 0;
    DWORD cDB;

    hFind = CeFindFirstDatabaseEx(pguid, 0);
    if(INVALID_HANDLE_VALUE == hFind) {
        LogFail(TEXT("CeFindFirstDatabaseEx() failed; error %u"), GetLastError());
        return 0;
    }

    cDB = 0;
    while(CeFindNextDatabaseEx(hFind, pguid)) {
        cDB++;
    }

    if(!CloseHandle(hFind)) {
        LogFail(TEXT("CloseHandle() failed on CeFindFirstDatabaseEx() handle; error %u"), GetLastError());
        return 0;
    }

    LogVerbose(TEXT("there are %u databases on the volume"), cDB);

    // there are no DBs on the volume
    if(0 == cDB) {
        return 0;
    }

    cDB = (Random() % cDB) + 1;

    LogVerbose(TEXT("dumping the contents of the #%u database on the volume"), cDB);

    hFind = CeFindFirstDatabaseEx(pguid, 0);
    if(INVALID_HANDLE_VALUE == hFind) {

        LogFail(TEXT("CeFindFirstDatabaseEx() failed; error %u"), GetLastError());
        return 0;
        
    }

    while(oidDB = CeFindNextDatabaseEx(hFind, pguid)) {
        
        if(0 == --cDB) {
            break;
        }
    }

    if(!CloseHandle(hFind)) {
        LogFail(TEXT("CloseHandle() failed on CeFindFirstDatabaseEx() handle; error %u"), GetLastError());
        return 0;
    }

    return oidDB;
}

/////////////////////////////////////////////////////////////////////////////////////
CEOID GetTestDatabaseOid(PCEGUID pguid) 
{
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOID oidDB = 0;

    hDb = CeOpenDatabaseEx2(pguid, &oidDB, (LPTSTR)TEST_DATABASE_NAME, NULL, 0, NULL);
    if(INVALID_HANDLE_VALUE == hDb || 0 == oidDB) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on database \"%s\"; error %u"), TEST_DATABASE_NAME, GetLastError());
        return 0;
    }

    if(!CloseHandle(hDb)) {
        LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
    }
    
    return oidDB;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD DeleteRandomRecord(
    PCEGUID pceguid,
    CEOID oid
    )
{
    DWORD retVal = CESTRESS_PASS;
    DWORD targetRecord;
    CEOID oidRecord;
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOIDINFOEX oidInfoEx;

    // open the DB using the oid
    hDb = CeOpenDatabaseEx2(pceguid, &oid, NULL, NULL, 0, NULL);
    if(INVALID_HANDLE_VALUE == hDb) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on db OID=%08X; error %u"), oid, GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    oidInfoEx.wVersion = 1;
    if(!CeOidGetInfoEx2(pceguid, oid, &oidInfoEx)) {
        LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;    
    }

    if(OBJTYPE_DATABASE != oidInfoEx.wObjType) {
        LogWarn1(TEXT("oid type returned by CeOidGetInfoEx() is not OBJTYPE_DATABASE as expected"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    if(0 == oidInfoEx.infDatabase.dwNumRecords) {

        LogComment(TEXT("the database OID=%08X contains no records to delete"));
        
    } else {

        targetRecord = Random() % oidInfoEx.infDatabase.dwNumRecords;
        LogVerbose(TEXT("Seeking to record %u of %u in database OID=%08X"), targetRecord, oidInfoEx.infDatabase.dwNumRecords, oid);
        oidRecord = CeSeekDatabaseEx(hDb, CEDB_SEEK_BEGINNING, targetRecord, 0, NULL);
        if(!oidRecord) {
            
            LogFail(TEXT("CeSeekDatabaseEx() failed on database OID=%08X; error %u"), oid, GetLastError());
            retVal = CESTRESS_FAIL;
            goto done;
            
        } else {

            // delete the record (ERROR_INVALID_PARAMETER if the object is gone)
            if(!CeDeleteRecord(hDb, oidRecord) 
               && ERROR_INVALID_PARAMETER != GetLastError()) {
                
                LogFail(TEXT("CeDeleteRecord() failed on database OID=%08X record OID=%08X; error %u"),
                    oid, oidRecord, GetLastError());
                retVal = CESTRESS_FAIL;
                goto done;
            }
        
        }
    }

done:
    if(INVALID_HANDLE_VALUE != hDb) {
        if(!CloseHandle(hDb)) {
            LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD ThinDatabase(
    PCEGUID pceguid,
    CEOID oid
    )
{
    DWORD retVal = CESTRESS_PASS;
    CEOID oidRecord;
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOIDINFOEX oidInfoEx;

    // open the DB using the oid
    hDb = CeOpenDatabaseEx2(pceguid, &oid, NULL, NULL, 0, NULL);
    if(INVALID_HANDLE_VALUE == hDb) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on db OID=%08X; error %u"), oid, GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    oidInfoEx.wVersion = 1;
    if(!CeOidGetInfoEx2(pceguid, oid, &oidInfoEx)) {
        LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;    
    }

    if(OBJTYPE_DATABASE != oidInfoEx.wObjType) {
        LogWarn1(TEXT("oid type returned by CeOidGetInfoEx() is not OBJTYPE_DATABASE as expected"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    if(0 == oidInfoEx.infDatabase.dwNumRecords) {

        LogComment(TEXT("the database OID=%08X contains no records to delete"));
        
    } else {

        // seek 2 positions, delete, repeat
        while(NULL != (oidRecord = CeSeekDatabaseEx(hDb, CEDB_SEEK_CURRENT, 2, 0, NULL))) 
        {
            // delete the record
            if(!CeDeleteRecord(hDb, oidRecord)) {
                LogFail(TEXT("CeDeleteRecord() failed on database OID=%08X record OID=%08X; error %u"),
                    oid, oidRecord, GetLastError());
                retVal = CESTRESS_FAIL;
                goto done;
            }
        }
    }

done:
    if(INVALID_HANDLE_VALUE != hDb) {
        if(!CloseHandle(hDb)) {
            LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD CreateRandomRecords(
    PCEGUID pceguid,
    CEOID oid
    )
{
    DWORD retVal = CESTRESS_PASS;
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOID oidRecord;
    CEOIDINFOEX oidInfoEx;
    DWORD prop, sortorder, sortprop;
    DWORD cRecords;
    CEPROPVAL propVals[CEDB_MAXSORTPROP*CEDB_MAXSORTORDER];
    WORD cPropid = CEDB_MAXSORTPROP*CEDB_MAXSORTORDER;

    // open the DB using the oid
    hDb = CeOpenDatabaseEx2(pceguid, &oid, NULL, NULL, 0, NULL);
    if(INVALID_HANDLE_VALUE == hDb) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on db OID=%08X; error %u"), oid, GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    oidInfoEx.wVersion = 1;
    if(!CeOidGetInfoEx2(pceguid, oid, &oidInfoEx)) {
        LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;    
    }

    if(OBJTYPE_DATABASE != oidInfoEx.wObjType) {
        LogWarn1(TEXT("oid type returned by CeOidGetInfoEx() is not OBJTYPE_DATABASE as expected"));
        retVal = CESTRESS_WARN1;
        goto done;
    }

    cRecords = (Random() % MAX_RECORDS_PER_WRITE + 1);

    LogVerbose(TEXT("writing %u records to database OID=%08X"), cRecords, oid);

    while(cRecords--) {
        
        // create properties
        memset(propVals, 0, sizeof(propVals));
        prop = 0;
        for(sortorder = 0; sortorder < oidInfoEx.infDatabase.wNumSortOrder; sortorder++) {

            for(sortprop = 0; sortprop < oidInfoEx.infDatabase.rgSortSpecs[sortorder].wNumProps; sortprop++) {
                //oidInfoEx.infDatabase.rgSortSpecs[sortorder].rgPropID[sortprop];
                //oidInfoEx.infDatabase.rgSortSpecs[sortorder].rgdwFlags[sortprop];
                propVals[prop++].propid = oidInfoEx.infDatabase.rgSortSpecs[sortorder].rgPropID[sortprop];
            }
        }

        for( ; prop < cPropid; prop++) {

            propVals[prop].propid = GLOBAL_SORT_KEYS[Random() % GLOBAL_SORT_KEY_COUNT];
            propVals[prop].propid |= (prop<<16 & 0xFFFF000);
        }

        for(prop =0; prop < cPropid; prop++) {

            switch(LOWORD(propVals[prop].propid)) {

                case CEVT_I2:
                case CEVT_UI2:
                    propVals[prop].val.uiVal = (WORD)Random();
                    break;
                     
                case CEVT_I4:
                case CEVT_UI4: 
                    propVals[prop].val.ulVal = Random();
                    break;
                    
                case CEVT_BOOL:
                    propVals[prop].val.boolVal = FlipACoin();
                    break;

                case CEVT_R8:
                    propVals[prop].val.dblVal = (double)Random();
                    break;

                case CEVT_FILETIME: 
                    SYSTEMTIME sysTime;
                    GetSystemTime(&sysTime);
                    if(!SystemTimeToFileTime(&sysTime, &propVals[prop].val.filetime)) {
                        LogFail(TEXT("SystemTimeToFileTime() failed; error %u"), GetLastError());
                        retVal = CESTRESS_FAIL;
                        goto done;
                    }
                    break;
                    
                case CEVT_LPWSTR:
                    TCHAR szString[MAX_PATH];
                    StringCchPrintf(szString, MAX_PATH, TEXT("s2_db %08X record prop_val string %u"), Random(), Random());
                    propVals[prop].val.lpwstr = szString;
                    break;

                default:
                    LogWarn2(TEXT("unknown prop val type"));
                    retVal = CESTRESS_WARN2;
                    goto done;
            }
        }

        // write the randomly generated record
        oidRecord = CeWriteRecordProps(hDb, 0, cPropid, propVals);
        // a duplicate record when a uniqueness contstraint exists will fail ERROR_ALREADY_EXISTS
        if(!oidRecord && ERROR_ALREADY_EXISTS != GetLastError()) {
            LogFail(TEXT("CeWriteRecordProps() failed; error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
            goto done;
        }
    }

done:
    if(INVALID_HANDLE_VALUE != hDb) {
        if(!CloseHandle(hDb)) {
            LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD ReadEntireDatabase(
    PCEGUID pceguid,
    CEOID oid
    )
{
    DWORD retVal = CESTRESS_PASS;
    DWORD cRecords = 0;
    DWORD lastErr;
    DWORD cbBuffer;
    WORD cProps;
    HANDLE hDb = INVALID_HANDLE_VALUE;
    CEOIDINFOEX oidInfo;
    CEPROPID* pPropID = NULL;
    LPBYTE pBuffer = NULL;
    SORTORDERSPECEX* psort= NULL;

    oidInfo.wVersion = 1;
    if(!CeOidGetInfoEx2(pceguid, oid, &oidInfo)) {
        LogFail(TEXT("CeOidGetInfoEx2() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    if(OBJTYPE_DATABASE != oidInfo.wObjType) {
        LogFail(TEXT("oid is not of OBJTYPE_DATABASE!"));
        retVal = CESTRESS_FAIL;
        goto done;
    }

    ASSERT(CEDB_MAXSORTORDER >= oidInfo.infDatabase.wNumSortOrder);

    if(FlipACoin() && oidInfo.infDatabase.wNumSortOrder > 0) {
        // pick a random sort order to use, if one exists
        psort = &oidInfo.infDatabase.rgSortSpecs[Random() % oidInfo.infDatabase.wNumSortOrder];
    } else {
        // don't specify a sort order
        psort = NULL;
    }
        

    // open the database
    LogVerbose(TEXT("opending database \"%s\"..."), oidInfo.infDatabase.szDbaseName);
    if(FlipACoin()) {
        // open the DB by OID
        hDb = CeOpenDatabaseEx2(pceguid, &oid, NULL, psort, CEDB_AUTOINCREMENT, NULL);
    } else {
        // open the DB by name
        oid = 0;
        hDb = CeOpenDatabaseEx2(pceguid, &oid, oidInfo.infDatabase.szDbaseName, psort, CEDB_AUTOINCREMENT, NULL);
    }
    if(INVALID_HANDLE_VALUE == hDb) {
        LogFail(TEXT("CeOpenDatabaseEx2() failed on db \"%s\"; error %u"), oidInfo.infDatabase.szDbaseName, GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    // read records
    cbBuffer = 0;
    pBuffer = NULL;
    do {
        // read the record
        if(psort) {
            // if a sort order was specified, choose only the props from it
            cProps = psort->wNumProps;
            pPropID = psort->rgPropID;
        } else {
            // propid=NULL indicates that all properties will be read
            cProps = 0;
            pPropID = NULL;
        }

        oid = CeReadRecordPropsEx(hDb, CEDB_ALLOWREALLOC, &cProps, pPropID, &pBuffer, &cbBuffer, GetProcessHeap());
        lastErr = GetLastError();
        
        if(oid) {
            //LogVerbose(TEXT("read record from \"%s\" with %u props"), oidInfo.infDatabase.szDbaseName, cProps);
            cRecords++;
        }
        
    } while (oid || ERROR_KEY_DELETED == lastErr);

    if(ERROR_NO_MORE_ITEMS == lastErr) {

        LogVerbose(TEXT("read %u total records from \"%s\""), cRecords, oidInfo.infDatabase.szDbaseName);
        
    } else if(ERROR_NO_DATA == lastErr) {
    
        LogWarn1(TEXT("none of the requested database properties were present in the record"));
        retVal = CESTRESS_WARN1;
        goto done;

    }

    
done:
    if(INVALID_HANDLE_VALUE != hDb) {
        if(!CloseHandle(hDb)) {
            LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2(); error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    // the buffer will have been LocalAlloc'd by filesys on behalf of this process
    if(NULL != pBuffer) {
        LocalFree(pBuffer);
        pBuffer = NULL;
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL AddDBVol(
    IN PCEGUID pGuidVol
    )
{
    BOOL fRet = FALSE;
    
    if(g_cDBVols < MAX_DB_VOLS) {
        if(!CHECK_SYSTEMGUID(pGuidVol)) {
            LogComment(TEXT("adding mounted database volume..."));
            LogVolumeGUID(SLOG_COMMENT, pGuidVol);
        }
        memcpy(&g_guidDBVols[g_cDBVols++], pGuidVol, sizeof(CEGUID));
        fRet = TRUE;
    }

    return fRet;
}


/////////////////////////////////////////////////////////////////////////////////////
DWORD MountDBVolumes(
    IN const GUID *pFSGuid
    )
{
    DWORD cVolumes = 0;
    CEGUID guidVol;
    HANDLE hMsgQueue = NULL;
    HANDLE hNotifications = NULL;
    TCHAR szVolumeName[MAX_PATH];
    BYTE bQueueData[QUEUE_ITEM_SIZE] = {0};
    DEVDETAIL *pDevDetail = (DEVDETAIL *)bQueueData;
    
    // enumerate all mounted FATFS volumes and add them to the test list
    hMsgQueue = CreatePnpMsgQueue();
    if(NULL == hMsgQueue) {
        LogWarn2(TEXT("CreateMsgQueue() failed; system error %u"), GetLastError());
        goto done;
    }

    hNotifications = RequestDeviceNotifications(pFSGuid, hMsgQueue, TRUE);
    if(NULL == hNotifications) {
        LogWarn2(TEXT("RequestDeviceNotifications() failed; system error %u"), GetLastError());
        goto done;
    }

    // read the queue until there are no more items
    while(ReadPnpMsgQueue(hMsgQueue, pDevDetail)) {
        
        if(pDevDetail->fAttached) {

            // mount the store as a database volume
            CREATE_INVALIDGUID(&guidVol);
            StringCchPrintf(szVolumeName, MAX_PATH, TEXT("%s\\%s"), pDevDetail->szName, TEST_DBVOLUME_NAME);
            if(CeMountDBVol(&guidVol, szVolumeName, OPEN_ALWAYS) && !CHECK_INVALIDGUID(&guidVol)) {
                LogComment(TEXT("mounted database volume \"%s\""), szVolumeName);
                AddDBVol(&guidVol);
                cVolumes++;
            }
        }        
    }
    
done:
    if(hNotifications) {
        if(!StopDeviceNotifications(hNotifications)) {
            LogWarn2(TEXT("StopDeviceNotifications() failed; system error %u"), GetLastError());
        }
    }
    if(hMsgQueue) {
        if(!CloseMsgQueue(hMsgQueue)) {
            LogWarn2(TEXT("CloseMsgQueue() failed; system error %u"), GetLastError());
        }
    }
    return cVolumes;
}

BOOL InitializeStressTestDBs(
    VOID
    )
{
    BOOL fRet = FALSE;
    BOOL fIsStringType;
    DWORD dbVolIndex, lastErr, disp, sortOrder, sortProp;
    HANDLE hDB = INVALID_HANDLE_VALUE;
    HKEY hKey;
    CEOID oidDB;
    CEGUID* pGuidDBVol;
    CEDBASEINFOEX dbInfoEx;

    // Use the creatino of registry key as a mutex: Only the first process to call this 
    // will get REG_CREATED_NEW_KEY returned from disp. This process will be responsible 
    // for creating the test databases on all volumes.

    lastErr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, DB_FIRSTRUN_REGKEY, 0, NULL, 0, 0, NULL, &hKey, &disp);
    if(ERROR_SUCCESS != lastErr || NULL == hKey) {
        LogFail(TEXT("RegCreateKeyEx(HKEY_LOCAL_MACHINE, \"%s\") failed; error %u"), DB_FIRSTRUN_REGKEY, lastErr);
        goto done;
    }
    lastErr = RegCloseKey(hKey);
    if(ERROR_SUCCESS != lastErr) {
        LogFail(TEXT("RegCloseKey() failed; error %u"), lastErr);
        goto done;
    }

    if(REG_CREATED_NEW_KEY != disp) {
        
        LogComment(TEXT("Not the first DB test process; assuming test DBs are already created on all volumes."));
        Sleep(30000); // sleep for 30 seconds
        
    } else {

        LogComment(TEXT("First DB process creating test databases on all volumes..."));

        // delete all existing stress test databases
        for(dbVolIndex = 0; dbVolIndex < g_cDBVols; dbVolIndex++) {

            pGuidDBVol = &g_guidDBVols[dbVolIndex];
            memset(&oidDB, 0, sizeof(CEOID));

            // try to open existing database, delete it if
            hDB = CeOpenDatabaseEx2(pGuidDBVol, &oidDB, (LPTSTR)TEST_DATABASE_NAME, NULL, 0, NULL);
            lastErr = GetLastError();
            if(INVALID_HANDLE_VALUE != hDB) {
                // the database already existed, close the handle and delete it
                if(!CloseHandle(hDB)) {
                    LogFail(TEXT("CloseHandle() failed on CeOpenDatabaseEx2 handle; error %u"), GetLastError());
                    goto done;
                }
                hDB = INVALID_HANDLE_VALUE;
                if(!CeDeleteDatabaseEx(pGuidDBVol, oidDB)) {
                    LogFail(TEXT("CeDeleteDatabaseEx() failed; error %u"), GetLastError());
                    goto done;
                }
                
            } else if(ERROR_FILE_NOT_FOUND != lastErr) {

                LogFail(TEXT("CeOpenDatabaseEx2(\"%s\") failed; error %u"), TEST_DATABASE_NAME, lastErr);
                goto done;
                
            } // else the DB doesn't exist
        }

        // create all new stress test databases for this run...
        for(dbVolIndex = 0; dbVolIndex < g_cDBVols; dbVolIndex++) {            

            // construct the CEDBASEINFOEX structure
            dbInfoEx.wVersion = 1;
            dbInfoEx.dwFlags = CEDB_VALIDNAME;
            StringCchCopy(dbInfoEx.szDbaseName, CEDB_MAXDBASENAMELEN, TEST_DATABASE_NAME);

            dbInfoEx.dwDbaseType = TEST_DATABASE_TYPE;
            dbInfoEx.dwFlags |= CEDB_VALIDTYPE;

            dbInfoEx.dwNumRecords = TEST_DATABASE_SIZE;

            dbInfoEx.wNumSortOrder = (WORD)((Random() % CEDB_MAXSORTORDER) + 1);
            dbInfoEx.dwFlags |= CEDB_VALIDSORTSPEC;
            for(sortOrder = 0; sortOrder < dbInfoEx.wNumSortOrder; sortOrder++) {
                
                dbInfoEx.rgSortSpecs[sortOrder].wVersion = 1;
                dbInfoEx.rgSortSpecs[sortOrder].wNumProps = (WORD)((Random() % CEDB_MAXSORTPROP) + 1);
                dbInfoEx.rgSortSpecs[sortOrder].wKeyFlags = 0;

                for(sortProp = 0; sortProp < dbInfoEx.rgSortSpecs[sortOrder].wNumProps; sortProp++) {
                    dbInfoEx.rgSortSpecs[sortOrder].rgPropID[sortProp] = GLOBAL_SORT_KEYS[Random() % GLOBAL_SORT_KEY_COUNT];
                    dbInfoEx.rgSortSpecs[sortOrder].rgPropID[sortProp] |= ((sortProp<<16 & 0x00FF000) | (sortOrder<<24 & 0xFF000000));
                    dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] = 0;

                    fIsStringType = (CEVT_LPWSTR == LOWORD(dbInfoEx.rgSortSpecs[sortOrder].rgPropID[sortProp]));                    

                    // require this item be non-NULL? else, require NULL items be enumerated first?
                    if(FlipACoin()) {
                        dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_NONNULL;
                    } else if(FlipACoin()) {
                        dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_UNKNOWNFIRST;
                    } 

                    // sort descending?
                    if(FlipACoin()) {
                        dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_DESCENDING;
                    }

                    // add all string-only flags
                    if(fIsStringType) {
                        
                        // sort case insensitive (strings only)?
                        if(FlipACoin()) {
                            dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_DESCENDING;
                        }

                        // sort ignoring nonspacing characters (strings only)?
                        if(FlipACoin()) {
                            dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_IGNORENONSPACE;
                        }

                        // sort ignoring symbols (strings only)?
                        if(FlipACoin()) {
                            dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_IGNORESYMBOLS;
                        }

                        // sort ignoring Japanese Katakana/Hiragana character distinction (strings only)?
                        if(FlipACoin()) {
                            dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_IGNOREKANATYPE;
                        }

                        // sort ignoring WCHAR/CHAR distinction (strings only)?
                        if(FlipACoin()) {
                            dbInfoEx.rgSortSpecs[sortOrder].rgdwFlags[sortProp] |= CEDB_SORT_IGNOREWIDTH;
                        }
                    }
                    
                }
            }

            // done constructing CEDBASEINFOEX struct, now create the DB
            pGuidDBVol = &g_guidDBVols[dbVolIndex];

            LogComment(TEXT("Creating database \"%s\" with %u sort orders"), dbInfoEx.szDbaseName, dbInfoEx.wNumSortOrder);
            oidDB = CeCreateDatabaseEx2(pGuidDBVol, &dbInfoEx); 
            if(NULL == oidDB) {
                LogFail(TEXT("CeCreateDatabaseEx2() failed; error %u"), GetLastError());
                goto done;
            }
        }
        
    }
    
    fRet = TRUE;

done:
    return fRet;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{

    // save off the command line for this module
    g_pszCmdLine = pmp->tszUser;

    *pnThreads = g_cThreads;

    InitializeStressUtils (
                            _T("S2_DB"),                                  // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_FILESYS, SLOG_DEFAULT),    // Logging zones used by default
                            pmp                                           // Forward the Module params passed on the cmd line
                            );

    GetModuleFileName((HINSTANCE) g_hInst, g_szModuleName, 256);

    LogComment(_T("Module File Name: %s"), g_szModuleName);   

    // add the default object store guid as a mounted volume
    CEGUID guidVol;
    CREATE_SYSTEMGUID(&guidVol);
    AddDBVol(&guidVol);

    // mount an object store file as a database volume
    CREATE_INVALIDGUID(&guidVol);
    if(CeMountDBVol(&guidVol, (LPTSTR)TEST_DBVOLUME_NAME, OPEN_ALWAYS) && !CHECK_INVALIDGUID(&guidVol)) {
        LogComment(TEXT("mounted database volume \"%s\""), TEST_DBVOLUME_NAME);
        AddDBVol(&guidVol);
    }

    // mount all external volumes on FAT storage
    MountDBVolumes(&FATFS_MOUNT_GUID);

    if(0 == g_cDBVols) {
        LogWarn1(TEXT("found no database volumes to test, aborting"));
        return FALSE;
    }

    return InitializeStressTestDBs();
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv /*unused*/)
{ 
    // pick a random test and a random database volume
    DWORD indexTest = Random() % TEST_FUNCTION_TABLE_COUNT;
    DWORD indexDbVol = Random() % g_cDBVols;
    
    return (TEST_FUNCTION_TABLE[indexTest])(&g_guidDBVols[indexDbVol]);
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    DWORD i;

    LogComment(TEXT("unmounting %u mounted database volumes"), g_cDBVols);
    for(i = 0; i < g_cDBVols; i++) {
        // unmount all database volumes that are not the system volume
        if(!CHECK_SYSTEMGUID(&g_guidDBVols[i]) && !CeUnmountDBVol(&g_guidDBVols[i])) {
            LogFail(TEXT("CeUnmoundDBVol() failed; error %u"), GetLastError());
            LogVolumeGUID(SLOG_FAIL, &g_guidDBVols[i]);
        }
    }
    return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance, 
                    ULONG dwReason, 
                    LPVOID lpReserved
                    )
{
    g_hInst = hInstance;
    return TRUE;
}

//
// TEST PROCs
//

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumVolumes(
    IN CEGUID* pguid
    )
{
    // ignore pguid
    LogComment(TEXT("enumerating all mounted database volumes..."));
    return EnumVols();
}


/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumDBs(
    IN CEGUID* pguid
    )
{
    LogComment(TEXT("enumerating databases on volume..."));
    return EnumDBs(pguid);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_FlushVolume(
    IN CEGUID* pguid
    )
{
    LogComment(TEXT("flushing database volume..."));
    // can't flush the system volume
    if(!CHECK_SYSTEMGUID(pguid) && !CeFlushDBVol(pguid)) {
        LogFail(TEXT("CeFlushDBVol() failed; error %u"), GetLastError());
        return CESTRESS_FAIL;
    }
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_FlushAllVolumes(
    IN CEGUID* pguid
    )
{
    // ignore pguid
    CEGUID guidInvalid;
    memset(&guidInvalid, 0xFF, sizeof(CEGUID));
    
    LogComment(TEXT("flushing all database volumes..."));
    if(!CeFlushDBVol(NULL/*&guidInvalid*/)) {
        LogFail(TEXT("CeFlushDBVol() (all volumes) failed; error %u"), GetLastError());
        return CESTRESS_FAIL;
    }
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_ReadRandomDatabase(
    IN CEGUID* pguid
    )
{
    CEOID oid = GetRandomDatabaseOid(pguid);

    LogComment(TEXT("reading a random database..."));
    if(0 == oid) {
        return CESTRESS_FAIL;
    } else {
        return ReadEntireDatabase(pguid, oid);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_CheckDbInfo(
    IN CEGUID* pguid
    )
{
    CEOID oid = GetRandomDatabaseOid(pguid);
    
    LogComment(TEXT("checking db information..."));
    if(0 == oid) {
        return CESTRESS_FAIL;
    } else {
        return CheckDbInfo(pguid, oid);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_CreateRecords(
    IN CEGUID* pguid
    )
{
    CEOID oid = GetTestDatabaseOid(pguid);
    
    LogComment(TEXT("creating random records..."));
    if(0 == oid) {
        return CESTRESS_FAIL;
    } else {
        return CreateRandomRecords(pguid, oid);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_DeleteRecord(
    IN CEGUID* pguid
    )
{
    CEOID oid = GetTestDatabaseOid(pguid);
    
    LogComment(TEXT("deleting random record..."));
    if(0 == oid) {
        return CESTRESS_FAIL;
    } else {
        return DeleteRandomRecord(pguid, oid);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_ThinDatabase(
    IN CEGUID* pguid
    )
{
    CEOID oid = GetTestDatabaseOid(pguid);
    
    LogComment(TEXT("thinning the database..."));
    if(0 == oid) {
        return CESTRESS_FAIL;
    } else {
        return ThinDatabase(pguid, oid);
    }
}

