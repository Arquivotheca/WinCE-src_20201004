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

#include <windows.h>
#include <windbase.h>
#include <memory.h>
#include <tchar.h>
#include <Strsafe.h>
#include "dbtst.h"

// New Parameter
CEGUID ceGuid;


// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO ssi;


TCHAR *szTestName = TEXT("DBTST");
TCHAR szFileName1[MAX_PATH];
TCHAR szFileName2[MAX_PATH];
TCHAR szFileName3[MAX_PATH];
TCHAR szPattern[MAX_PATH];

#define lidJapanese    0x0411
LANGID        lcid;
HINSTANCE    hInst = NULL;



#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#define    DB_DBASE_TYPE_START        0x777
#define    DB_DBASE_TYPE_A            DB_DBASE_TYPE_START+1
#define    DB_DBASE_TYPE_B            DB_DBASE_TYPE_START+2
#define    DB_DBASE_TYPE_C            DB_DBASE_TYPE_START+3

// Long db name - 32 chars including the null char
#define    DB_NAME32        TEXT("abcdefghijklmnopqrstuvwxyz12340")

// Long db name - 33 chars including the null char
#define    DB_NAME33        TEXT("abcdefghijklmnopqrstuvwxyz123456")
#define    DB_NAME32_A    TEXT("abcdefghijklmnopqrstuvwxyz1234a")

TCHAR DB_NAMEDBCS32[32];        // for Japanese
TCHAR DB_NAMEDBCS32_A[32];

UINT GetDBCount(DWORD    dwDBaseType);
void DeleteAllDatabases();
#define DB_CeOidGetInfoEx        CeOidGetInfoEx
#define DB_CeCreateDatabaseEx    CeCreateDatabaseEx
#define DB_CeOpenDatabaseEx        CeOpenDatabaseEx

void TestInit(void) 
{
    // under CEDB, just use the ObjectStore volume (system GUID)
    CREATE_SYSTEMGUID(&ceGuid);
    DeleteAllDatabases();
}

void SetNameAndType(CEDBASEINFO *ceDbaseInfo, const TCHAR *szName, DWORD dwType)
{
    ZeroMemory(ceDbaseInfo, sizeof(ceDbaseInfo));
    StringCchCopyN(ceDbaseInfo->szDbaseName, CEDB_MAXDBASENAMELEN, 
                    szName, CEDB_MAXDBASENAMELEN);

    ceDbaseInfo->szDbaseName[CEDB_MAXDBASENAMELEN-1] = _T('\0');
    ceDbaseInfo->dwDbaseType = dwType;
    ceDbaseInfo->wNumSortOrder = 0;
}

void DeleteAllDatabases()
{
    HANDLE hFind;
    CEOID ceOid;
    DWORD dwCount=0;

    hFind = CeFindFirstDatabaseEx(&ceGuid,  0);
    
    if (hFind != INVALID_HANDLE_VALUE)  {
        ceOid = CeFindNextDatabaseEx(hFind, &ceGuid);
        while(ceOid)  {
            if (!CHECK_SYSTEMGUID(&ceGuid) && CeDeleteDatabaseEx(&ceGuid,  ceOid))  {
                DETAIL("Deleted database # %ld", ++dwCount);
            }
            ceOid = CeFindNextDatabaseEx(hFind, &ceGuid);
        }
    }
    CloseHandle( hFind);
}

// Shellproc's
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      case SPM_LOAD_DLL: {
         // Sent once to the DLL immediately after it is loaded.  The DLL may
         // return SPR_FAIL to prevent the DLL from continuing to load.
         g_pKato = (CKato*)KatoGetDefaultObject();
         return SPR_HANDLED;
      }

      case SPM_UNLOAD_DLL: {
         // Sent once to the DLL immediately before it is unloaded.
         //DETAIL("ShellProc(SPM_UNLOAD_DLL, ...) called");
         return SPR_HANDLED;
      }

      case SPM_SHELL_INFO: {
         // Sent once to the DLL immediately after SPM_LOAD_DLL to give the
         // DLL some useful information about its parent shell.  The spParam
         // parameter will contain a pointer to a SPS_SHELL_INFO structure.
         // This pointer is only temporary and should not be referenced after
         // the processing of this message.  A copy of the structure should be
         // stored if there is a need for this information in the future.
         ssi = *(LPSPS_SHELL_INFO)spParam;
         return SPR_HANDLED;
      }

      case SPM_REGISTER: {
         // This is the only ShellProc() message that a DLL is required to
         // handle.  This message is sent once to the DLL immediately after
         // the SPM_SHELL_INFO message to query the DLL for it's function table.
         // The spParam will contain a pointer to a SPS_REGISTER structure.
         // The DLL should store its function table in the lpFunctionTable
         // member of the SPS_REGISTER structure return SPR_HANDLED.  If the
         // function table contains UNICODE strings then the SPF_UNICODE flag
         // must be OR'ed with the return value; i.e. SPR_HANDLED | SPF_UNICODE
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      case SPM_START_SCRIPT: {
         // Sent to the DLL immediately before a script is started.  It is
         // sent to all DLLs, including loaded DLLs that are not in the script.
         // All DLLs will receive this message before the first TestProc() in
         // the script is called.
         return SPR_HANDLED;
      }

      case SPM_STOP_SCRIPT: {
         // Sent to the DLL when the script has stopped.  This message is sent
         // when the script reaches its end, or because the user pressed
         // stopped prior to the end of the script.  This message is sent to
         // all DLLs, including loaded DLLs that are not in the script.
         return SPR_HANDLED;
      }

      case SPM_BEGIN_GROUP: {
         // Sent to the DLL before a group of tests from that DLL is about to
         // be executed.  This gives the DLL a time to initialize or allocate
         // data for the tests to follow.  Only the DLL that is next to run
         // receives this message.  The prior DLL, if any, will first receive
         // a SPM_END_GROUP message.  For global initialization and
         // de-initialization, the DLL should probably use SPM_START_SCRIPT
         // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
         return SPR_HANDLED;
      }

      case SPM_END_GROUP: {
         // Sent to the DLL after a group of tests from that DLL has completed
         // running.  This gives the DLL a time to cleanup after it has been
         // run.  This message does not mean that the DLL will not be called
         // again; it just means that the next test to run belongs to a
         // different DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
         // to track when it is active and when it is not active.
         return SPR_HANDLED;
      }

      case SPM_BEGIN_TEST: {
         // Sent to the DLL immediately before a test executes.  This gives
         // the DLL a chance to perform any common action that occurs at the
         // beginning of each test, such as entering a new logging level.
         // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
         // structure, which contains the function table entry and some other
         // useful information for the next test to execute.  If the ShellProc
         // function returns SPR_SKIP, then the test case will not execute.
         TestInit();
         return SPR_HANDLED;
      }

      case SPM_END_TEST: {
         // Sent to the DLL after a single test executes from the DLL.
         // This gives the DLL a time perform any common action that occurs at
         // the completion of each test, such as exiting the current logging
         // level.  The spParam parameter will contain a pointer to a
         // SPS_END_TEST structure, which contains the function table entry and
         // some other useful information for the test that just completed.
         return SPR_HANDLED;
      }

      case SPM_EXCEPTION: {
         // Sent to the DLL whenever code execution in the DLL causes and
         // exception fault.  TUX traps all exceptions that occur while
         // executing code inside a test DLL.
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// TestProc()'s
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// TestCreateDeleteOpenCloseDB
//  BVT test case for creating, deleting, opening, and closing database
//
//
// Test scenarios:
//        1. create db with an empty str.
//        2. create db with 32 chars.
//        3. create db with 33 chars.  The first 32 chars are the same as those in (2).
//        4. create db with 33 chars but with a different class.
//        5. OpenDB a non-existing DB - invalid oid
//        6. OpenDB a non-existing DB - bad name
//        7. open db using an oid
//        8. open db using its name.  The same db is now opened twice.
//        9. delete the db without closing it first.
//        10. close the db once.  then delete the db - there is still one OPEN outstanding.
//        11. close the db again.  then delete it.
//        12. delete the deleted db.
//        13. close the deleted db.
//
// Parameters:
//        uMsg    :    Message code.
//        tpParam    :    Additional message-dependent data.
//        lpFTE    :    Function table entry that generated this call.
//
// Return value:
//        TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI
TestCreateDeleteOpenCloseDB(
    UINT uMsg,
    TPPARAM /*tpParam*/,
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
)
{
    CEOID    oidDBTmp=0, oidDB1 =0;
    HANDLE    hDB1 =INVALID_HANDLE_VALUE, hDB2 = INVALID_HANDLE_VALUE;
    CEDBASEINFO ceDbaseInfo;
    WCHAR    db_name[CEDB_MAXDBASENAMELEN];
    int        cnt;
    int        jp = 1;

    if (uMsg != TPM_EXECUTE) 
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwRetVal = TPR_FAIL;
    StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAME32);
    DETAIL("Testing Create/Delete/Open/Close of DB...");

    lcid = GetSystemDefaultLangID() ;
    hInst = ssi.hLib;

    if ( lcid == lidJapanese )
    {
        LoadString (hInst, 1001, DB_NAMEDBCS32, 32);
        jp = 2;
    }
    
    for(cnt = 0;cnt < jp;cnt++) // for Japanese
    {
        //Ensure to delete if any existing
        oidDBTmp = 0;
        hDB2 = DB_CeOpenDatabaseEx(&ceGuid, &oidDBTmp,db_name , 0, 0, NULL);
        if(hDB2 != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hDB2);
            CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDBTmp));
            oidDBTmp=0;
            hDB2= INVALID_HANDLE_VALUE;
        }
        //

        // Test creating database
        DETAIL("CreateDB with 32 chars in name ");
        SetNameAndType(&ceDbaseInfo, db_name, DB_DBASE_TYPE_A);
        oidDB1 = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
        CHECKTRUE(oidDB1);
        DETAIL("CeCreateDatabaseEx    %s",db_name);

        // Test opening database
        // Open the db twice, once using the oid, once using the name
        DETAIL("OpenDB using the oid ");
        hDB1 = DB_CeOpenDatabaseEx(&ceGuid, &oidDB1, NULL, 0, 0, NULL);
        CHECKFALSE((hDB1 == INVALID_HANDLE_VALUE));

        // The second open
        DETAIL("OpenDB using the name ");
        oidDBTmp = 0;
        hDB2 = DB_CeOpenDatabaseEx(&ceGuid, &oidDBTmp,db_name , 0, 0, NULL);
        CHECKFALSE((hDB2 == INVALID_HANDLE_VALUE));

        // Test deleting a database
        DETAIL("DeleteDB an opened db ");
        DETAIL("This should not succeed (you might see errors in the next lines)");        
        CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
        CHECKTRUE((GetLastError() == ERROR_SHARING_VIOLATION));
        
        DETAIL("CloseHandle a DB");
        CHECKTRUE(CloseHandle(hDB1));
        
        DETAIL("DeleteDB an opened db again ");
        DETAIL("This should not succeed (you might see errors in the next lines)");        
        CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
        CHECKTRUE((GetLastError() == ERROR_SHARING_VIOLATION));
        
        DETAIL("CloseHandle a DB");
        CHECKTRUE(CloseHandle(hDB2));

        DETAIL("DeleteDB");
        CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, oidDB1));

        DETAIL("DeleteDB a non-existing db ");
        DETAIL("This should not succeed (you might see errors in the next lines)");        
        CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
        CHECKTRUE((GetLastError() == ERROR_INVALID_PARAMETER));
        
        DETAIL("CloseDB a deleted DB ");
        CHECKFALSE(CloseHandle(hDB1));
        StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAMEDBCS32);
        
    } //jp
    dwRetVal = TPR_PASS;
EXIT:
    CloseHandle(hDB2);
    CloseHandle(hDB1);
    return dwRetVal;
}
////////////////////////////////////////////////////////////////////////////////
// TestRWSeekDelRecordsFromEmptyDB
//  Test DB error handling
//
// Test scenarios:
//        1. create a new db.  then open it with auto-increment.
//        2. do seek's with different seek-types.
//        3. try to read props from the empty db.
//        4. write record 1 with CEVT_I4 and CEVT_BLOB.
//        5. write record 2 with CEVT_I4 and CEVT_BLOB.
//        6. delete CEVT_I4 value from record 2.
//        7. read record 1 and verify props.
//        8. rewind db to record 1.  re-read record 1 but with an unknown value
//            in the first propid.    verify props.
//        9. read record 2 and verify props.
//        10. repeat (8) and (9) after the DB has been closed and reopened.
//        11. do a bunch of getoidinfo to verify oidinfo and DB and record relationships.
//        12. delete the last record.
//        13. delete the first record.
//        14. do a bunch of getoidinfo to verify oidinfo and DB and record relationships.
//        15. close the db.
//        16. delete the db.
//        17. do a getoidinfo to verify DB info.
//
// Parameters:
//        uMsg    :    Message code.
//        tpParam    :    Additional message-dependent data.
//        lpFTE    :    Function table entry that generated this call.
//
// Return value:
//        TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI TestRWSeekDelRecordsFromEmptyDB(UINT uMsg,TPPARAM /*tpParam*/,LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    HANDLE            hDB = INVALID_HANDLE_VALUE,hHeap  =INVALID_HANDLE_VALUE;
    CEOID            oidDB =0;
    CEOID           rgOidRecord[2] = {0};
    CEOID           oidRecordTmp =0;
    CEOIDINFO        oiOidInfo ={0};
    CEPROPVAL        rgPropValue[2]={0};
    const CEPROPVAL*        pPropValue;
    WCHAR            db_name[CEDB_MAXDBASENAMELEN]={0};
    CEPROPID        rgPropID[2]={0};
    WORD            cPropID =0;
    DWORD            dwIndex =0,cbBuf =0,dwErr =0;
    LPBYTE            pBuf = NULL,pBlobBuf = NULL;
    int                i =0;
    CEDBASEINFO     ceDbaseInfo={0};
    int                cnt =0;
    int             jp = 1;
    DWORD            dwRetVal = TPR_FAIL;    

    if (uMsg != TPM_EXECUTE) 
    {
        return TPR_NOT_HANDLED;
    }

    StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAME32_A);
     //Ensure to delete if any existing
    oidRecordTmp = 0;
    hDB = DB_CeOpenDatabaseEx(&ceGuid, &oidRecordTmp,db_name , 0, 0, NULL);
    if(hDB != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hDB);
        if(!CeDeleteDatabaseEx(&ceGuid, oidRecordTmp))
            return TPR_NOT_HANDLED;
        oidRecordTmp=0;
        hDB = INVALID_HANDLE_VALUE;
    }
    //

    // alloc only 1 byte
    pBuf = (LPBYTE)LocalAlloc(0, 1);
    // for Blob prop
    pBlobBuf = (LPBYTE)LocalAlloc(0, 266);

    if (pBuf == NULL || pBlobBuf == NULL) 
    {
        ERRFAIL("LocalAlloc()");
        if (pBuf)
            LocalFree(pBuf);
        if (pBlobBuf)
            LocalFree(pBlobBuf);
        return TPR_NOT_HANDLED;
    }

    hHeap = HeapCreate(0,0,0);
    if (hHeap == NULL ) 
    {
        ERRFAIL("HeapCreate");
        return TPR_NOT_HANDLED;
    }
    DETAIL("Testing Read/Write/Seek/Delete of Records from an empty DB...");
    lcid = GetSystemDefaultLangID() ;
    hInst = ssi.hLib;
    
    if ( lcid == lidJapanese )
    {
        LoadString (hInst, 1001,DB_NAMEDBCS32_A , 32);
        jp = 2;
    }
    for(cnt =0; cnt <jp;cnt++)
    { 
        // Create DB
        DETAIL("CreateDB");
        SetNameAndType(&ceDbaseInfo, db_name, DB_DBASE_TYPE_B);
        oidDB = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
        DETAIL("CeCreateDatabaseEx %s", ceDbaseInfo.szDbaseName);
        CHECKTRUE(oidDB);
        
        // Open the db once using the oid
        DETAIL("OpenDB using the oid");
        hDB = DB_CeOpenDatabaseEx(&ceGuid, &oidDB, NULL, 0, CEDB_AUTOINCREMENT, NULL);
        CHECKFALSE(hDB == INVALID_HANDLE_VALUE);

        // Read Props
        DETAIL("Go to the first record");
        CHECKFALSE(CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, &dwIndex));

        DETAIL("ReadRecord an empty db - CEDB_ALLOWREALLOC");
        rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 1));
        rgPropID[1] = DWORD(MAKELONG(CEVT_I4, 2));
        cbBuf = 1;
        cPropID = 2;
        CHECKFALSE(CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &cPropID, &rgPropID[0], &pBuf, &cbBuf, hHeap));
        dwErr = GetLastError();
        CHECKTRUE((dwErr == ERROR_NO_DATA) || (dwErr == ERROR_NO_MORE_ITEMS));

        // Write Props
        DETAIL("WriteRecord");
        DETAIL("Writing first record...");
        for (i=0; i<266; i++)
            *(pBlobBuf+i) = (BYTE)(i % 256);
        rgPropValue[0].propid = (DWORD)MAKELONG(CEVT_I4, 1);
        rgPropValue[0].wFlags = 0;
        rgPropValue[0].val.lVal = -999;
        rgPropValue[1].propid = (DWORD)MAKELONG(CEVT_BLOB, 2);
        rgPropValue[1].wFlags = 0;
        rgPropValue[1].val.blob.dwCount = 266;
        rgPropValue[1].val.blob.lpb = pBlobBuf;
        rgOidRecord[0] = CeWriteRecordProps(hDB, 0, 2, rgPropValue);
        CHECKTRUE(rgOidRecord[0]);
        DETAIL("Writing second record...");
        rgPropValue[0].propid = (DWORD)MAKELONG(CEVT_I4, 9);
        rgPropValue[0].val.lVal = 100;
        rgPropValue[1].propid = (DWORD)MAKELONG(CEVT_BLOB, 10);
        rgOidRecord[1] = CeWriteRecordProps(hDB, 0, 2, rgPropValue);
        CHECKTRUE(rgOidRecord[1]);
        DETAIL("Deleting the first prop in the second record...");
        rgPropValue[0].wFlags = CEDB_PROPDELETE;                        // delete this one!
        rgPropValue[1].val.blob.dwCount = 1;
        *pBlobBuf = 99;
        oidRecordTmp = CeWriteRecordProps(hDB, rgOidRecord[1], 2, rgPropValue);
        CHECKTRUE(oidRecordTmp == rgOidRecord[1]);
        
        // Read the props back
        DETAIL("ReadRecord - CEDB_ALLOWREALLOC");
        BOOL        fReopen = FALSE;                          // close and reopen DB?
        for(int n=0;n<2;n++)                                  // here we loop twice one for close and the other for reopen
        {
            CEOID        oidSeek, oidRead;
            if (fReopen) {
                CHECKTRUE(CloseHandle(hDB));
                hDB = DB_CeOpenDatabaseEx(&ceGuid, &oidDB, NULL, 0, CEDB_AUTOINCREMENT, NULL);
                CHECKFALSE(hDB == INVALID_HANDLE_VALUE);
            }

            // go to the first record
            oidSeek = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, rgOidRecord[0], &dwIndex);
            CHECKTRUE(oidSeek);
            CHECKTRUE(oidSeek == rgOidRecord[0]);

            // first record
            DETAIL("Reading the first record...");
            rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 1));
            rgPropID[1] = DWORD(MAKELONG(CEVT_BLOB, 2));
            cbBuf = 1;
            cPropID = 2;
            oidRead = CeReadRecordProps(hDB, CEDB_ALLOWREALLOC, &cPropID, rgPropID, &pBuf, &cbBuf);
            CHECKTRUE(oidSeek);
            CHECKTRUE(oidSeek == rgOidRecord[0]);

            // verify data
            for (i=0, pPropValue=(const CEPROPVAL*)pBuf; i<2; i++, pPropValue++) 
            {
                CHECKFALSE(TypeFromPropID(pPropValue->propid) != TypeFromPropID(rgPropValue[i].propid));
                CHECKFALSE(pPropValue->wFlags);
                CHECKFALSE(!i && (pPropValue->val.lVal != -999));
                CHECKFALSE(i && (pPropValue->val.blob.dwCount != 266));
            }

            pPropValue--;        //rewind
            for (i=0; i<266; i++) 
            {
                const BYTE* p=pPropValue->val.blob.lpb;
                CHECKTRUE(*(p+i) == (BYTE)(i % 256));
            }

            // rewind
            oidSeek = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, rgOidRecord[0], &dwIndex);
            CHECKTRUE(oidSeek);
            CHECKTRUE(oidSeek == rgOidRecord[0]);

            // first record again

            DETAIL("Reading the first record again...");
            rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 5));                // this propid is wrong!
            rgPropID[1] = DWORD(MAKELONG(CEVT_BLOB, 2));
            cbBuf = 1;
            cPropID = 2;
            oidRead = CeReadRecordProps(hDB, CEDB_ALLOWREALLOC, &cPropID, rgPropID, &pBuf, &cbBuf);
            CHECKTRUE(oidRead);
            CHECKTRUE(oidRead == rgOidRecord[0]);

            // verify data
            pPropValue=(const CEPROPVAL*)pBuf;
            CHECKTRUE(pPropValue->wFlags == CEDB_PROPNOTFOUND);

            pPropValue++;
            CHECKTRUE(TypeFromPropID(pPropValue->propid) == CEVT_BLOB);
            CHECKFALSE(pPropValue->wFlags);
            CHECKTRUE(pPropValue->val.blob.dwCount == 266);
            for (i=0; i<266; i++) 
            {
                const BYTE* p=pPropValue->val.blob.lpb;
                CHECKTRUE(*(p+i) == (BYTE)(i % 256));
            }

            // second record
            DETAIL("Reading the second record...");

            oidSeek = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, rgOidRecord[1], &dwIndex);
            CHECKTRUE(oidSeek);
            CHECKTRUE(oidSeek == rgOidRecord[1]);

            rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 9));
            rgPropID[1] = DWORD(MAKELONG(CEVT_BLOB, 10));
            cbBuf = 1;
            cPropID = 2;
            oidRead = CeReadRecordProps(hDB, CEDB_ALLOWREALLOC, &cPropID, rgPropID, &pBuf, &cbBuf);
            CHECKTRUE(oidRead);
            CHECKTRUE(oidRead == rgOidRecord[1]);

            // verify data
            pPropValue=(const CEPROPVAL*)pBuf;
            CHECKTRUE(pPropValue->wFlags == CEDB_PROPNOTFOUND);

            pPropValue++;
            CHECKTRUE(TypeFromPropID(pPropValue->propid) == CEVT_BLOB);
            CHECKFALSE(pPropValue->wFlags);
            CHECKTRUE(pPropValue->val.blob.dwCount == 1);
            CHECKTRUE(*pPropValue->val.blob.lpb == 99);

            if (fReopen)
                break;
            fReopen = TRUE;
            
        }
        

        // Get Oid Info
        DETAIL("GetOidInfo");
        DETAIL("Checking DB...");
        CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
        CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
        CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
        DETAIL("CeOidGetInfoEx %s", oiOidInfo.infDatabase.szDbaseName);
        CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
        CHECKTRUE(oiOidInfo.infDatabase.wNumRecords == 2);

        // Delete Record
        DETAIL("DeleteRecord");
        DETAIL("Deleting the second record...");
        CHECKTRUE(CeDeleteRecord(hDB, rgOidRecord[1]));
        DETAIL("Checking DB...");
        CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
        CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
        CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
        CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
        DETAIL("CeOidGetInfoEx %s", oiOidInfo.infDatabase.szDbaseName);
        CHECKTRUE(oiOidInfo.infDatabase.wNumRecords == 1);
        DETAIL("Deleting the first record...");
        CHECKTRUE(CeDeleteRecord(hDB, rgOidRecord[0]));
        DETAIL("Checking DB...");
        CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
        CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
        CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
        CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
        DETAIL("CeOidGetInfoEx %s", oiOidInfo.infDatabase.szDbaseName);
        CHECKFALSE(oiOidInfo.infDatabase.wNumRecords);
        DETAIL("Checking the first record...");
        // shouldn't succeed
        CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, rgOidRecord[0], &oiOidInfo));
        CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);

        DETAIL("Checking the second record...");
        // shouldn't succeed
        CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, rgOidRecord[1], &oiOidInfo));
        CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);


        DETAIL("SeekDB - go the beginning of the DB");
        // shouldn't succeed
        CHECKFALSE(CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, &dwIndex));

        // Delete
        DETAIL("CloseHandle a DB");
        CHECKTRUE(CloseHandle(hDB));

        DETAIL("DeleteDB");
        CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, oidDB));
        DETAIL("Checking the DB...");
        // shouldn't succeed
        CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
        CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);

        StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAMEDBCS32_A);
    }//jp
    dwRetVal = TPR_PASS;
EXIT:
    CloseHandle(hDB);
    LocalFree(pBuf);
    LocalFree(pBlobBuf);
    HeapDestroy(hHeap);
    return dwRetVal;
}

////////////////////////////////////////////////////////////////////////////////
// TestFindDB
//  BVT test case for finding database
//
//
// Test scenarios:
//        1. Create 26 DB of the same class
//        2. do findfirst and findnext's to get the count of db's created.
//        3. do findfirst and findnext to made sure those oid's are the ones that we got
//            from CeCreateDatabaseEx().
//        4. delete the 2nd DB, then check count.
//        5. delete the 1nd DB, then check count.
//        6. delete the last DB, then check count.
//        7. delete them all, then check count.
//
// Parameters:
//        uMsg    :    Message code.
//        tpParam    :    Additional message-dependent data.
//        lpFTE    :    Function table entry that generated this call.
//
// Return value:
//        TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI
TestFindDB(
    UINT uMsg,
    TPPARAM /*tpParam*/,
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
)
{
    TCHAR    szName[CEDB_MAXDBASENAMELEN];
    CEOID    rgOidDBCreated[26];
    CEOID    oidTmp;
    BOOL    rgfOidFound[26];
    UINT    i;
    UINT    uLen;
    BOOL    bSucceeded = TRUE;
    HANDLE    hEnum = INVALID_HANDLE_VALUE;
    UINT    cDB = 0;
    CEDBASEINFO ceDbaseInfo;
    int        cnt;
    int        jp = 1;
    TCHAR    JpName[32] ;
    size_t szlen = 0;
    if (uMsg != TPM_EXECUTE) 
    {
        return TPR_NOT_HANDLED;
    }
    
    DWORD dwRetVal = TPR_FAIL;
    DETAIL("Testing FindDB functions...");

    lcid = GetSystemDefaultLangID() ;
    hInst = ssi.hLib;
    
    if ( lcid == lidJapanese )
    {
        LoadString (hInst, 1003,JpName , 32);
        jp = 2;
    }
    
    for(cnt = 0;cnt < jp;cnt++) // for Japanese
    { 
        
        if(cnt == 0)
        {
            StringCchCopy(szName, CEDB_MAXDBASENAMELEN, TEXT("DB Name A"));            
            StringCchLength(szName, STRSAFE_MAX_CCH, &szlen);
            uLen = szlen;
        }
        else
        {
            cDB = 0;
            StringCchCopy(szName, CEDB_MAXDBASENAMELEN, JpName);
            StringCchLength(szName, STRSAFE_MAX_CCH, &szlen);
            uLen = szlen;
        }
        
        for (i=0; i<26; i++) 
        {
            //Ensure to delete if any existing
            oidTmp = 0;
            hEnum = DB_CeOpenDatabaseEx(&ceGuid, &oidTmp,szName , 0, 0, NULL);
            if(hEnum != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hEnum);
                CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidTmp));
                oidTmp=0;
                hEnum = INVALID_HANDLE_VALUE;
            }
           
            rgfOidFound[i] = FALSE;
            SetNameAndType(&ceDbaseInfo, szName, DB_DBASE_TYPE_B);
            rgOidDBCreated[i] = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
            DETAIL("CeCreateDatabaseEx %s", ceDbaseInfo.szDbaseName);
            CHECKTRUE(rgOidDBCreated[i]);
            szName[uLen-1]++;
        }
        

        DETAIL("GetCount");
        CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 26);

        DETAIL("Enum all DB oid");
        CHECKFALSE((hEnum = CeFindFirstDatabaseEx(&ceGuid, DB_DBASE_TYPE_B)) == INVALID_HANDLE_VALUE);
        
        oidTmp = CeFindNextDatabaseEx(hEnum, &ceGuid);
        while (oidTmp) 
        {
            // see if it's what I created
            for (i=0; i<26; i++) 
            {
                if (rgOidDBCreated[i] == oidTmp) 
                {
                    if (rgfOidFound[i]) 
                    {
                        DETAIL("We got duplicate oid's");
                        bSucceeded = FALSE;
                    }
                    else
                        rgfOidFound[i] = TRUE;
                    break;
                }
            }
            
            if (i == 26) 
            {
                DETAIL("CeFindNextDatabaseEx didn't return what test created");
                bSucceeded = FALSE;
                // let it continue.....
            }
            
            if (++cDB > 26) 
            {
                DETAIL("WARNING : We have more oid's than we created!!!!!!");
                break;
            }
            oidTmp = CeFindNextDatabaseEx(hEnum, &ceGuid);
        }

        if (!CloseHandle(hEnum)) 
        {
            ERRFAIL("CloseHandle()");
            bSucceeded = FALSE;
        }
    
        for (i=0; i<26; i++) 
        {
            if (!rgfOidFound[i]) 
            {
                DETAIL("We lost an oid!!!!!");
                bSucceeded = FALSE;
            }
        }
        CHECKTRUE(bSucceeded);

        DETAIL("Delete the 2nd DB");
        CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[1]));
        CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 25);

        DETAIL("Delete the 1st DB");
        CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[0]));
        CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 24);

        DETAIL("Delete the last DB");
        CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[25]));
        CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 23);

        DETAIL("Delete the rest of the DB's");
        for (i=2; i<25; i++) 
        {
            CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[i]));
        }
        // should be none left
        CHECKFALSE(GetDBCount(DB_DBASE_TYPE_B));
    }//lnag
dwRetVal = TPR_PASS;
EXIT:
    CloseHandle(hEnum);
    return dwRetVal;
}





///////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////

UINT
GetDBCount(
    DWORD    dwDBaseType
)
{
    HANDLE    hEnum;
    UINT    cDB = 0;

    if ((hEnum = CeFindFirstDatabaseEx(&ceGuid, dwDBaseType)) == INVALID_HANDLE_VALUE) 
    {
        ERRFAIL("CeFindFirstDatabaseEx()");
        return 0xffffffff;
    }

    while (CeFindNextDatabaseEx(hEnum, &ceGuid))
        cDB++;

    if (GetLastError() != ERROR_NO_MORE_ITEMS) 
    {
        ERRFAIL("CeFindNextDatabaseEx()");
        if (!CloseHandle(hEnum))
            ERRFAIL("CloseHandle()");
        return 0xffffffff;
    }

    if (!CloseHandle(hEnum))
        ERRFAIL("CloseHandle()");

    return cDB;
}
