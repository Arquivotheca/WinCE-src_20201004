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

#include <windows.h>
#include <windbase.h>
#include <memory.h>
#include <tchar.h>
#include <Strsafe.h>
#include "harness.h"
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

#define lidJapanese	0x0411
LANGID		lcid;
HINSTANCE	hInst = NULL;

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#define	DB_DBASE_TYPE_START		0x777
#define	DB_DBASE_TYPE_A			DB_DBASE_TYPE_START+1
#define	DB_DBASE_TYPE_B			DB_DBASE_TYPE_START+2
#define	DB_DBASE_TYPE_C			DB_DBASE_TYPE_START+3

// Long db name - 32 chars including the null char
#define	DB_NAME32		TEXT("abcdefghijklmnopqrstuvwxyz12340")

// Long db name - 33 chars including the null char
#define	DB_NAME33		TEXT("abcdefghijklmnopqrstuvwxyz123456")
#define	DB_NAME32_A	TEXT("abcdefghijklmnopqrstuvwxyz1234a")

TCHAR DB_NAMEDBCS32[32];		// for Japanese
TCHAR DB_NAMEDBCS32_A[32];

UINT GetDBCount(DWORD	dwDBaseType);
void DeleteAllDatabases();

#ifdef EDB

#pragma message("Building against EDB backend")

#define EDB_VOLUME_NAME L"TEST.EDB"

// Helper for converting between SORTORDERSPEC versions.
// ToEx is lossless
static void CopySortsToEx(SORTORDERSPECEX* pDest, const SORTORDERSPEC* pSrc, WORD wNumSorts)
{
	WORD wSort;

	for (wSort = 0; (wSort < wNumSorts) && (wSort < CEDB_MAXSORTORDER); wSort++) 
	{
		pDest->wVersion = SORTORDERSPECEX_VERSION;
		pDest->wNumProps = 1;
		pDest->rgPropID[0] = pSrc->propid;
		pDest->rgdwFlags[0] = pSrc->dwFlags;

		// Since there is only one prop in the Ex version, the unique flag
		// is kept there.  But in the Ex2 version, it's in the keyflags.
		if (pDest->rgdwFlags[0] & CEDB_SORT_UNIQUE) 
		{
			pDest->rgdwFlags[0] &= ~CEDB_SORT_UNIQUE;
			pDest->wKeyFlags = CEDB_SORT_UNIQUE;
		} 
		else 
		{
			pDest->wKeyFlags = 0;
		}

		pDest++;
		pSrc++;
	}
}

// Helper for converting between SORTORDERSPEC versions.
// FromEx could throw away some information
void CopySortsFromEx(SORTORDERSPEC* pDest, const SORTORDERSPECEX* pSrc, WORD wNumSorts)
{
	WORD wSort;

	for (wSort = 0; (wSort < wNumSorts) && (wSort < CEDB_MAXSORTORDER); wSort++) 
	{
		// If there are multiple props, only copy the first
		DEBUGMSG(pSrc->wNumProps > 1,
				 (TEXT("DB: Warning - returning incomplete information, use OidGetInfoEx2 instead\r\n")));
		pDest->propid = pSrc->rgPropID[0];
		pDest->dwFlags = pSrc->rgdwFlags[0];

		// Since there is only one prop in the Ex version, the unique flag
		// is kept there.  But in the Ex2 version, it's in the keyflags.
		if (pSrc->wKeyFlags & CEDB_SORT_UNIQUE) 
		{
			pDest->dwFlags |= CEDB_SORT_UNIQUE;
		}

		pSrc++;
		pDest++;
	}
}

CEOID DB_CeCreateDatabaseEx(PCEGUID pguid, const CEDBASEINFO *pInfo)
{
	CEDBASEINFOEX exInfo;
	BOOL fRet = FALSE;

	__try {

		// wNumRecords, dwSize, ftLastModified do not need to be copied

		exInfo.wVersion = CEDBASEINFOEX_VERSION;
		exInfo.dwFlags = pInfo->dwFlags;
		StringCchCopyN(exInfo.szDbaseName, CEDB_MAXDBASENAMELEN, 
			pInfo->szDbaseName, sizeof(pInfo->szDbaseName) / sizeof(WCHAR) - 1);
		
		exInfo.szDbaseName[sizeof(pInfo->szDbaseName) / sizeof(WCHAR) - 1] = 0;
		exInfo.dwDbaseType = pInfo->dwDbaseType;
		exInfo.wNumSortOrder = pInfo->wNumSortOrder;

		CopySortsToEx(exInfo.rgSortSpecs, pInfo->rgSortSpecs, exInfo.wNumSortOrder);

	} __except (EXCEPTION_EXECUTE_HANDLER) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	fRet = CeCreateDatabaseEx2(pguid, &exInfo);
	if (!fRet) 
	{
		LogDetail(TEXT("!!! ERROR !!! CeCreateDatabaseEx2 failed (error=%u)\r\n"), GetLastError());
	}
	return fRet;
}

HANDLE DB_CeOpenDatabaseEx(
	 PCEGUID	pguid,
	 PCEOID		poid,
	 LPWSTR		lpszName,
	 CEPROPID	propid,
	 DWORD		dwFlags,
	 CENOTIFYREQUEST*	pRequest
 )
{
	HANDLE h = INVALID_HANDLE_VALUE;

	if (propid) 
	{
		SORTORDERSPECEX sort = {0};
		sort.wVersion = SORTORDERSPECEX_VERSION;
		sort.wNumProps = 1;
		sort.rgPropID[0] = propid;
		sort.rgdwFlags[0] = 0; // No flags to pass
		h = CeOpenDatabaseEx2(pguid, poid, lpszName, &sort, dwFlags, pRequest);
	} 
	else 
	{
		h = CeOpenDatabaseEx2(pguid, poid, lpszName, NULL, dwFlags, pRequest);
	}

	if (INVALID_HANDLE_VALUE == h) 
	{
		LogDetail(TEXT("!!! ERROR !!! CeOpenDatabaseEx2 failed (error=%u)\r\n"), GetLastError());
	}
	return h;
}

BOOL DB_CeOidGetInfoEx(PCEGUID pguid, CEOID oid, CEOIDINFO *oidInfo)
{
	CEOIDINFOEX exInfo;

	exInfo.wVersion = CEOIDINFOEX_VERSION;

	if (!CeOidGetInfoEx2(pguid, oid, &exInfo)) 
	{
		return FALSE;
	}

	__try {

		oidInfo->wObjType = exInfo.wObjType;
		switch (exInfo.wObjType) {
		case OBJTYPE_FILE:
			memcpy((LPBYTE)&oidInfo->infFile, (LPBYTE)&(exInfo.infFile),
				   sizeof(CEFILEINFO));
			break;

		case OBJTYPE_DIRECTORY:
			memcpy((LPBYTE)&(oidInfo->infDirectory), (LPBYTE)&(exInfo.infDirectory),
				   sizeof(CEDIRINFO));
			break;

		case OBJTYPE_DATABASE:
			{
				// Can't copy exactly
				const CEDBASEINFOEX* pdbSrc = &(exInfo.infDatabase);
				CEDBASEINFO* pdbDest = &(oidInfo->infDatabase);

				// If the number of records exceeds a WORD's worth, we can't convert
				if (pdbSrc->dwNumRecords & 0xFFFF0000) {
					SetLastError(ERROR_NOT_SUPPORTED);
					return FALSE;
				}

				pdbDest->wNumRecords = (WORD)pdbSrc->dwNumRecords;
				pdbDest->dwFlags = pdbSrc->dwFlags;
				StringCchCopyN(pdbDest->szDbaseName, CEDB_MAXDBASENAMELEN,
								pdbSrc->szDbaseName, sizeof(pdbSrc->szDbaseName) / sizeof(WCHAR) - 1);
				
				pdbDest->szDbaseName[sizeof(pdbSrc->szDbaseName) / sizeof(WCHAR) - 1] = 0;
				pdbDest->dwDbaseType = pdbSrc->dwDbaseType;
				pdbDest->wNumSortOrder = pdbSrc->wNumSortOrder;
				pdbDest->dwSize = pdbSrc->dwSize;
				memcpy(&(pdbDest->ftLastModified), &(pdbSrc->ftLastModified),
					   sizeof(FILETIME));

				CopySortsFromEx(pdbDest->rgSortSpecs, pdbSrc->rgSortSpecs, pdbSrc->wNumSortOrder);
			}
			break;

		case OBJTYPE_RECORD:
			memcpy((LPBYTE)&(oidInfo->infRecord), (LPBYTE)&(exInfo.infRecord),
				   sizeof(CERECORDINFO));
			break;

		default:
			// No data to copy
			;
		}

		return TRUE;

	} __except (EXCEPTION_EXECUTE_HANDLER) {
		SetLastError(ERROR_INVALID_PARAMETER);
	}

	return FALSE;
}

void TestInit(void) 
{
	// under EDB, mount a volume
	static BOOL fInit = FALSE;
	if (!fInit) 
	{
		WCHAR szVolName[MAX_PATH];
		StringCchCopy(szVolName, MAX_PATH, EDB_VOLUME_NAME);
		
		if (!CeMountDBVol(&ceGuid, szVolName, CREATE_ALWAYS | EDB_MOUNT_FLAG)) 
		{
			// failed to mount the EDB volume
			LogDetail(TEXT("!!! ERROR !!! Failed to mount EDB volume \"%s\" error=%u; expect tests to fail !!!\r\n"),
				EDB_VOLUME_NAME, GetLastError());
		} else 
		{
			fInit = TRUE;
		}
	}
	DeleteAllDatabases();
}

void SetNameAndType(CEDBASEINFO *ceDbaseInfo, const TCHAR *szName, DWORD dwType){

	ZeroMemory(ceDbaseInfo, sizeof(ceDbaseInfo));
	StringCchCopyN(ceDbaseInfo->szDbaseName, CEDB_MAXDBASENAMELEN, 
					szName, CEDB_MAXDBASENAMELEN);
	
	ceDbaseInfo->szDbaseName[CEDB_MAXDBASENAMELEN-1] = _T('\0');
	ceDbaseInfo->dwDbaseType = dwType;
	ceDbaseInfo->wNumSortOrder = 0;
	// EDB requires these flags
	ceDbaseInfo->dwFlags = CEDB_VALIDDBFLAGS | CEDB_VALIDNAME | CEDB_VALIDTYPE;
}

#else // EDB

#pragma message("Building against CEDB backend")

#define DB_CeOidGetInfoEx		CeOidGetInfoEx
#define DB_CeCreateDatabaseEx	CeCreateDatabaseEx
#define DB_CeOpenDatabaseEx	CeOpenDatabaseEx

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

#endif // !EDB

void DeleteAllDatabases()
{
	HANDLE hFind;
	CEOID ceOid;
	DWORD dwCount=0;

	hFind = CeFindFirstDatabaseEx(&ceGuid,  0);
	
	if (hFind != INVALID_HANDLE_VALUE)  {
		while(ceOid = CeFindNextDatabaseEx(hFind, &ceGuid))  {
			if (!CHECK_SYSTEMGUID(&ceGuid) && CeDeleteDatabaseEx(&ceGuid,  ceOid))  {
				LogDetail( TEXT("Deleted database # %ld\r\n"), ++dwCount);
			}
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
		 PRINT(TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));
		 CreateLog( szTestName);
		 return SPR_HANDLED;
	  }

	  case SPM_UNLOAD_DLL: {
		 // Sent once to the DLL immediately before it is unloaded.
		 PRINT(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\r\n"));
		 CloseLog();
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
//		1. create db with an empty str.
//		2. create db with 32 chars.
//		3. create db with 33 chars.  The first 32 chars are the same as those in (2).
//		4. create db with 33 chars but with a different class.
//		5. OpenDB a non-existing DB - invalid oid
//		6. OpenDB a non-existing DB - bad name
//		7. open db using an oid
//		8. open db using its name.  The same db is now opened twice.
//		9. delete the db without closing it first.
//		10. close the db once.  then delete the db - there is still one OPEN outstanding.
//		11. close the db again.  then delete it.
//		12. delete the deleted db.
//		13. close the deleted db.
//
// Parameters:
//		uMsg	:	Message code.
//		tpParam	:	Additional message-dependent data.
//		lpFTE	:	Function table entry that generated this call.
//
// Return value:
//		TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI
TestCreateDeleteOpenCloseDB(
	UINT uMsg,
	TPPARAM tpParam,
	LPFUNCTION_TABLE_ENTRY lpFTE
)
{
	HANDLE	hTest, hStep;
	CEOID	oidDBTmp, oidDB1;
	HANDLE	hDB1, hDB2;
	BOOL	bSucceeded = TRUE;
	CEDBASEINFO ceDbaseInfo;
	WCHAR	db_name[CEDB_MAXDBASENAMELEN];
	int		cnt;
	int		jp = 1;

	if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
	}

	StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAME32);
	hTest = StartTest(TEXT("Testing Create/Delete/Open/Close of DB..."));

	lcid = GetSystemDefaultLangID() ;
	hInst = ssi.hLib;

	if ( lcid == lidJapanese )
	{
		LoadString (hInst, 1001, DB_NAMEDBCS32, 32);
		jp = 2;
	}
	
	for(cnt = 0;cnt < jp;cnt++) // for Japanese
	{
		// Test creating database
		BEGINSTEP(hTest, hStep, TEXT("CreateDB with 32 chars in name "));
		SetNameAndType(&ceDbaseInfo, db_name, DB_DBASE_TYPE_A);
		oidDB1 = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
		CHECKTRUE(oidDB1);
		LogDetail(TEXT("CeCreateDatabaseEx	%s"),db_name);
		ENDSTEP(hTest, hStep);

		// Test opening database
		// Open the db twice, once using the oid, once using the name
		BEGINSTEP(hTest, hStep, TEXT("OpenDB using the oid "));
		hDB1 = DB_CeOpenDatabaseEx(&ceGuid, &oidDB1, NULL, 0, 0, NULL);
		CHECKFALSE((hDB1 == INVALID_HANDLE_VALUE));
		ENDSTEP(hTest, hStep);

		// The second open
		BEGINSTEP(hTest, hStep, TEXT("OpenDB using the name "));
		oidDBTmp = 0;
		hDB2 = DB_CeOpenDatabaseEx(&ceGuid, &oidDBTmp,db_name , 0, 0, NULL);
		CHECKFALSE((hDB2 == INVALID_HANDLE_VALUE));
		ENDSTEP(hTest, hStep);

		// Test deleting a database
		BEGINSTEP(hTest, hStep, TEXT("DeleteDB an opened db "));
#ifndef EDB
		LogDetail(TEXT("This should not succeed (you might see errors in the next lines)"));		
#endif
		CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
		CHECKTRUE((GetLastError() == ERROR_SHARING_VIOLATION));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("CloseHandle a DB"));
		CHECKTRUE(CloseHandle(hDB1));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("DeleteDB an opened db again "));
#ifndef EDB
		LogDetail(TEXT("This should not succeed (you might see errors in the next lines)"));		
#endif
		CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
		CHECKTRUE((GetLastError() == ERROR_SHARING_VIOLATION));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("CloseHandle a DB"));
		CHECKTRUE(CloseHandle(hDB2));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("DeleteDB"));
		CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("DeleteDB a non-existing db "));
#ifndef EDB
		LogDetail(TEXT("This should not succeed (you might see errors in the next lines)"));		
#endif
		CHECKFALSE(CeDeleteDatabaseEx(&ceGuid, oidDB1));
#ifndef EDB
		CHECKTRUE((GetLastError() == ERROR_INVALID_PARAMETER));
#else
		// EDB returns a different error code than CEDB
		CHECKTRUE(GetLastError() == ERROR_NOT_FOUND);
#endif
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("CloseDB a deleted DB "));
		CHECKFALSE(CloseHandle(hDB1));
		ENDSTEP(hTest, hStep);
		StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAMEDBCS32);
		
	} //jp
	return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


////////////////////////////////////////////////////////////////////////////////
// TestFindDB
//  BVT test case for finding database
//
//
// Test scenarios:
//		1. Create 26 DB of the same class
//		2. do findfirst and findnext's to get the count of db's created.
//		3. do findfirst and findnext to made sure those oid's are the ones that we got
//			from CeCreateDatabaseEx().
//		4. delete the 2nd DB, then check count.
//		5. delete the 1nd DB, then check count.
//		6. delete the last DB, then check count.
//		7. delete them all, then check count.
//
// Parameters:
//		uMsg	:	Message code.
//		tpParam	:	Additional message-dependent data.
//		lpFTE	:	Function table entry that generated this call.
//
// Return value:
//		TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI
TestFindDB(
	UINT uMsg,
	TPPARAM tpParam,
	LPFUNCTION_TABLE_ENTRY lpFTE
)
{
	HANDLE	hTest, hStep;
	TCHAR	szName[CEDB_MAXDBASENAMELEN];
	CEOID	rgOidDBCreated[26];
	CEOID	oidTmp;
	BOOL	rgfOidFound[26];
	UINT	i;
	UINT	uLen;
	BOOL	bSucceeded = TRUE;
	HANDLE	hEnum;
	UINT	cDB = 0;
	CEDBASEINFO ceDbaseInfo;
	int		cnt;
	int		jp = 1;
	TCHAR	JpName[32] ;
	
	if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
	}
	
	hTest = StartTest(TEXT("Testing FindDB functions..."));

	lcid = GetSystemDefaultLangID() ;
	hInst = ssi.hLib;
	
	if ( lcid == lidJapanese )
	{
		LoadString (hInst, 1003,JpName , 32);
		jp = 2;
	}
	
	for(cnt = 0;cnt < jp;cnt++) // for Japanese
	{ 
		BEGINSTEP(hTest, hStep, TEXT("Create multiple DB's"));

		if(cnt == 0)
		{
			StringCchCopy(szName, CEDB_MAXDBASENAMELEN, TEXT("DB Name A"));			
			uLen = _tcslen(szName);
		}
		else
		{
			cDB = 0;
			StringCchCopy(szName, CEDB_MAXDBASENAMELEN, JpName);
			uLen = _tcslen(szName);
		}

		for (i=0; i<26; i++) 
		{
			rgfOidFound[i] = FALSE;
			SetNameAndType(&ceDbaseInfo, szName, DB_DBASE_TYPE_B);
			rgOidDBCreated[i] = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
			LogDetail(TEXT("CeCreateDatabaseEx %s"), ceDbaseInfo.szDbaseName);
			CHECKTRUE(rgOidDBCreated[i]);
			szName[uLen-1]++;
		}
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("GetCount"));
		CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 26);
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("Enum all DB oid"));
		CHECKFALSE((hEnum = CeFindFirstDatabaseEx(&ceGuid, DB_DBASE_TYPE_B)) == INVALID_HANDLE_VALUE);
		
		while ((oidTmp = CeFindNextDatabaseEx(hEnum, &ceGuid))) 
		{
			// see if it's what I created
			for (i=0; i<26; i++) 
			{
				if (rgOidDBCreated[i] == oidTmp) 
				{
					if (rgfOidFound[i]) 
					{
						LogDetail(TEXT("We got duplicate oid's"));
						bSucceeded = FALSE;
					}
					else
						rgfOidFound[i] = TRUE;
					break;
				}
			}
			
			if (i == 26) 
			{
				LogDetail(TEXT("CeFindNextDatabaseEx didn't return what I created"));
				bSucceeded = FALSE;
				// let it continue.....
			}
			
			if (++cDB > 26) 
			{
				LogDetail(TEXT("WARNING : We have more oid's than we created!!!!!!"));
				break;
			}
		}

		if (!CloseHandle(hEnum)) 
		{
			LogDetail(TEXT("FAILED: CloseHandle returned %lu"), GetLastError());
			bSucceeded = FALSE;
		}
	
		for (i=0; i<26; i++) 
		{
			if (!rgfOidFound[i]) 
			{
				LogDetail(TEXT("We lost an oid!!!!!"));
				bSucceeded = FALSE;
			}
		}
		CHECKTRUE(bSucceeded);
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("Delete the 2nd DB"));
		CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[1]));
		CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 25);
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("Delete the 1st DB"));
		CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[0]));
		CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 24);
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("Delete the last DB"));
		CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[25]));
		CHECKTRUE(GetDBCount(DB_DBASE_TYPE_B) == 23);
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("Delete the rest of the DB's"));
		for (i=2; i<25; i++) 
		{
			CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, rgOidDBCreated[i]));
		}
		// should be none left
		CHECKFALSE(GetDBCount(DB_DBASE_TYPE_B));
		ENDSTEP(hTest, hStep);
	}//lnag
	return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


////////////////////////////////////////////////////////////////////////////////
// TestRWSeekDelRecordsFromEmptyDB
//  Test DB error handling
//
// Test scenarios:
//		1. create a new db.  then open it with auto-increment.
//		2. do seek's with different seek-types.
//		3. try to read props from the empty db.
//		4. write record 1 with CEVT_I4 and CEVT_BLOB.
//		5. write record 2 with CEVT_I4 and CEVT_BLOB.
//		6. delete CEVT_I4 value from record 2.
//		7. read record 1 and verify props.
//		8. rewind db to record 1.  re-read record 1 but with an unknown value
//    		in the first propid.	verify props.
//		9. read record 2 and verify props.
//		10. repeat (8) and (9) after the DB has been closed and reopened.
//		11. do a bunch of getoidinfo to verify oidinfo and DB and record relationships.
//		12. delete the last record.
//		13. delete the first record.
//		14. do a bunch of getoidinfo to verify oidinfo and DB and record relationships.
//		15. close the db.
//		16. delete the db.
//		17. do a getoidinfo to verify DB info.
//
// Parameters:
//		uMsg	:	Message code.
//		tpParam	:	Additional message-dependent data.
//		lpFTE	:	Function table entry that generated this call.
//
// Return value:
//		TPR_PASS if successful, TPR_FAIL to indicate an error condition.

TESTPROCAPI
TestRWSeekDelRecordsFromEmptyDB(
	UINT uMsg,
	TPPARAM tpParam,
	LPFUNCTION_TABLE_ENTRY lpFTE
)
{
	HANDLE			hTest, hStep;
	CEOID			oidDB;
	CEOID			rgOidRecord[2], oidRecordTmp;
	CEOIDINFO		oiOidInfo;
	HANDLE			hDB;
	DWORD			dwIndex;
	CEPROPVAL		rgPropValue[2];
	const CEPROPVAL*		pPropValue;
	CEPROPID		rgPropID[2];
	WORD			cPropID;
	LPBYTE			pBuf;
	DWORD			cbBuf;
	DWORD			dwErr;
	LPBYTE			pBlobBuf;
	UINT			i;
	CEDBASEINFO 	ceDbaseInfo;
	HANDLE			hHeap;
	int				cnt;
	int				jp = 1;
	WCHAR			db_name[CEDB_MAXDBASENAMELEN];

	if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
	}

	StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAME32_A);

	// alloc only 1 byte
	pBuf = (LPBYTE)LocalAlloc(0, 1);
	// for Blob prop
	pBlobBuf = (LPBYTE)LocalAlloc(0, 266);

	if (pBuf == NULL || pBlobBuf == NULL) 
	{
		LogDetail(TEXT("FAILED: LocalAlloc buffer\r\n"));
		if (pBuf)
			LocalFree(pBuf);
		if (pBlobBuf)
			LocalFree(pBlobBuf);
		return TPR_NOT_HANDLED;
	}

	hHeap = HeapCreate(0,0,0);
	hTest = StartTest(TEXT("Testing Read/Write/Seek/Delete of Records from an empty DB..."));

	lcid = GetSystemDefaultLangID() ;
	hInst = ssi.hLib;

	if ( lcid == lidJapanese )
	{
		LoadString (hInst, 1001,DB_NAMEDBCS32_A , 32);
		jp = 2;
	}
	
	for(cnt = 0;cnt < jp;cnt++) // for Japanese
	{ 
		// Create DB
		BEGINSTEP(hTest, hStep, TEXT("CreateDB"));
		SetNameAndType(&ceDbaseInfo, db_name, DB_DBASE_TYPE_B);
		// SetNameAndType(&ceDbaseInfo, DB_NAME32_A, DB_DBASE_TYPE_B);
		oidDB = DB_CeCreateDatabaseEx(&ceGuid, &ceDbaseInfo);
		LogDetail(TEXT("CeCreateDatabaseEx %s"), ceDbaseInfo.szDbaseName);
		CHECKTRUE(oidDB);
		ENDSTEP(hTest, hStep);

		// Open the db once using the oid
		BEGINSTEP(hTest, hStep, TEXT("OpenDB using the oid"));
		hDB = DB_CeOpenDatabaseEx(&ceGuid, &oidDB, NULL, 0, CEDB_AUTOINCREMENT, NULL);
		CHECKFALSE(hDB == INVALID_HANDLE_VALUE);
		ENDSTEP(hTest, hStep);

		// Read Props
		BEGINSTEP(hTest, hStep, TEXT("Go to the first record"));
		CHECKFALSE(CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, &dwIndex));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("ReadRecord an empty db - CEDB_ALLOWREALLOC"));

		rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 1));
		rgPropID[1] = DWORD(MAKELONG(CEVT_I4, 2));
		cbBuf = 1;
		cPropID = 2;
		CHECKFALSE(CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &cPropID, &rgPropID[0], &pBuf, &cbBuf, hHeap));
		dwErr = GetLastError();
		CHECKTRUE((dwErr == ERROR_NO_DATA) || (dwErr == ERROR_NO_MORE_ITEMS));
		ENDSTEP(hTest, hStep);

		// Write Props
		BEGINSTEP(hTest, hStep, TEXT("WriteRecord"));
		
		LogDetail(TEXT("Writing first record..."));
		
		for (i=0; i<266; i++)
			*(pBlobBuf+i) = (BYTE)(i % 256);
		
		rgPropValue[0].propid = (DWORD)MAKELONG(CEVT_I4, 1);
		rgPropValue[0].wFlags = 0;
		rgPropValue[0].val.lVal = -999;
		rgPropValue[1].propid = (DWORD)MAKELONG(CEVT_BLOB, 2);
		rgPropValue[1].wFlags = 0;
		rgPropValue[1].val.blob.dwCount = 266;
		rgPropValue[1].val.blob.lpb = pBlobBuf;
		
		CHECKTRUE((rgOidRecord[0] = CeWriteRecordProps(hDB, 0, 2, rgPropValue)));
		LogDetail(TEXT("Writing second record..."));
		
#ifndef EDB
		rgPropValue[0].propid = (DWORD)MAKELONG(CEVT_I4, 9);
		rgPropValue[0].val.lVal = 100;
		rgPropValue[1].propid = (DWORD)MAKELONG(CEVT_BLOB, 10);
#else
		rgPropValue[0].propid = (DWORD)MAKELONG(CEVT_I4, 1);
		rgPropValue[0].val.lVal = 100;
		rgPropValue[1].propid = (DWORD)MAKELONG(CEVT_BLOB, 2);
#endif
		CHECKTRUE((rgOidRecord[1] = CeWriteRecordProps(hDB, 0, 2, rgPropValue)));
		
		LogDetail(TEXT("Deleting the first prop in the second record..."));
		
		rgPropValue[0].wFlags = CEDB_PROPDELETE;						// delete this one!
		rgPropValue[1].val.blob.dwCount = 1;
		*pBlobBuf = 99;
		oidRecordTmp = CeWriteRecordProps(hDB, rgOidRecord[1], 2, rgPropValue);
		CHECKTRUE(oidRecordTmp == rgOidRecord[1]);
		ENDSTEP(hTest, hStep);

		// Read the props back
		BEGINSTEP(hTest, hStep, TEXT("ReadRecord - CEDB_ALLOWREALLOC"));

		BOOL		fReopen;
		fReopen = FALSE;						  // close and reopen DB?
		while (1) 
		{
			CEOID		oidSeek, oidRead;

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
			LogDetail(TEXT("Reading the first record..."));
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

			pPropValue--;		//rewind
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

			LogDetail(TEXT("Reading the first record again..."));
			rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 5));				// this propid is wrong!
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
			LogDetail(TEXT("Reading the second record..."));

			oidSeek = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, rgOidRecord[1], &dwIndex);
			CHECKTRUE(oidSeek);
			CHECKTRUE(oidSeek == rgOidRecord[1]);

			rgPropID[0] = DWORD(MAKELONG(CEVT_I4, 9));
			rgPropID[1] = DWORD(MAKELONG(CEVT_BLOB, 10));
			cbBuf = 1;
			cPropID = 2;
			oidRead = CeReadRecordProps(hDB, CEDB_ALLOWREALLOC, &cPropID, rgPropID, &pBuf, &cbBuf);
			CHECKTRUE(oidRead);
			CHECKTRUE(oidRead != rgOidRecord[1]);

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

		ENDSTEP(hTest, hStep);

		// Get Oid Info
		BEGINSTEP(hTest, hStep, TEXT("GetOidInfo"));

		LogDetail(TEXT("Checking DB..."));
		CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
		CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
		CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
		LogDetail(TEXT("CeOidGetInfoEx %s"), oiOidInfo.infDatabase.szDbaseName);
		CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
		CHECKTRUE(oiOidInfo.infDatabase.wNumRecords == 2);

		ENDSTEP(hTest, hStep);

		// Delete Record
		BEGINSTEP(hTest, hStep, TEXT("DeleteRecord"));

		LogDetail(TEXT("Deleting the second record..."));
		CHECKTRUE(CeDeleteRecord(hDB, rgOidRecord[1]));

		LogDetail(TEXT("Checking DB..."));
		CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
		CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
		CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
		CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
		LogDetail(TEXT("CeOidGetInfoEx %s"), oiOidInfo.infDatabase.szDbaseName);
		CHECKTRUE(oiOidInfo.infDatabase.wNumRecords == 1);

		LogDetail(TEXT("Deleting the first record..."));
		CHECKTRUE(CeDeleteRecord(hDB, rgOidRecord[0]));

		LogDetail(TEXT("Checking DB..."));
		CHECKTRUE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
		CHECKTRUE(oiOidInfo.wObjType == OBJTYPE_DATABASE);
		CHECKTRUE(oiOidInfo.infDatabase.dwDbaseType == DB_DBASE_TYPE_B);
		CHECKFALSE(_tcscmp(oiOidInfo.infDatabase.szDbaseName, db_name));
		LogDetail(TEXT("CeOidGetInfoEx %s"), oiOidInfo.infDatabase.szDbaseName);
		CHECKFALSE(oiOidInfo.infDatabase.wNumRecords);

		LogDetail(TEXT("Checking the first record..."));

		// shouldn't succeed
		CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, rgOidRecord[0], &oiOidInfo));
#ifndef EDB
		CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);
#else
		// EDB returns a different error code than CEDB
		CHECKTRUE(GetLastError() == ERROR_KEY_DELETED);
#endif

		LogDetail(TEXT("Checking the second record..."));
		// shouldn't succeed
		CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, rgOidRecord[1], &oiOidInfo));
#ifndef EDB
		CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);
#else
		// EDB returns a different error code than CEDB
		CHECKTRUE(GetLastError() == ERROR_KEY_DELETED);
#endif

		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("SeekDB - go the beginning of the DB"));
		// shouldn't succeed
		CHECKFALSE(CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, &dwIndex));
		ENDSTEP(hTest, hStep);

		// Delete
		BEGINSTEP(hTest, hStep, TEXT("CloseHandle a DB"));
		CHECKTRUE(CloseHandle(hDB));
		ENDSTEP(hTest, hStep);

		BEGINSTEP(hTest, hStep, TEXT("DeleteDB"));
		CHECKTRUE(CeDeleteDatabaseEx(&ceGuid, oidDB));

		LogDetail(TEXT("Checking the DB..."));
		// shouldn't succeed
		CHECKFALSE(DB_CeOidGetInfoEx(&ceGuid, oidDB, &oiOidInfo));
#ifndef EDB
		CHECKTRUE(GetLastError() == ERROR_INVALID_PARAMETER);
#else
		// EDB returns a different error code than CEDB
		CHECKTRUE(GetLastError() == ERROR_KEY_DELETED);
#endif
		ENDSTEP(hTest, hStep);
		StringCchCopy(db_name, CEDB_MAXDBASENAMELEN, DB_NAMEDBCS32_A);		
	}//jp

	LocalFree(pBuf);
	LocalFree(pBlobBuf);

	return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


///////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////

UINT
GetDBCount(
	DWORD	dwDBaseType
)
{
	HANDLE	hEnum;
	UINT	cDB = 0;

	if ((hEnum = CeFindFirstDatabaseEx(&ceGuid, dwDBaseType)) == INVALID_HANDLE_VALUE) 
	{
		LogDetail(TEXT("FAILED: CeFindFirstDatabase returned %lu"), GetLastError());
		return 0xffffffff;
	}

	while (CeFindNextDatabaseEx(hEnum, &ceGuid))
		cDB++;

	if (GetLastError() != ERROR_NO_MORE_ITEMS) 
	{
		LogDetail(TEXT("FAILED: CeFindNextDatabaseEx returned %lu"), GetLastError());
		if (!CloseHandle(hEnum))
			LogDetail(TEXT("FAILED: CloseHandle returned %lu"), GetLastError());
		return 0xffffffff;
	}

	if (!CloseHandle(hEnum))
		LogDetail(TEXT("FAILED: CloseHandle returned %lu"), GetLastError());

	return cDB;
}
