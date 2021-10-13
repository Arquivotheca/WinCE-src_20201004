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

#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__

#define BUFFER 4096
#define countof(a) (sizeof(a)/sizeof(*(a)))
#define FTP_USERNAME "wininetstresstest"
#define FTP_PASSWORD "StressTest2006"
#define FTP_SERVER "wcewininet2"

#define LOCAL_DOWNLOAD_TESTS_DIR "\\temp\\wininet_stress_files\\download_tests"
#define LOCAL_FILE_DIR "\\temp\\wininet_stress_files"
#define LOCAL_UPLOAD_TESTS_DIR "\\temp\\wininet_stress_files\\upload_tests"
#define ROOT_DOWNLOAD_DIR "/automation/stress/files/download/"
#define ROOT_UPLOAD_DIR "/automation/stress/files/upload/"
#define SMALL_FILE_LENGTH 100
#define TESTURL "http://wcewininet2/automation/stress/pages/"
#define THREAD_COUNT 0
#define URL_LENGTH 100

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.
#define BASE 0x00010000
#define HTTP_GETPAGES BASE + 1
#define CACHE_PAGES_DELETE BASE + 10
#define CACHE_RETRIEVEDATA BASE + 11
#define CACHE_RETRIEVEFILES BASE + 12
#define COOKIE_SET BASE + 20
#define COOKIE_GET BASE + 21
#define FTP_SMALLDOWNLOAD BASE + 30
#define FTP_SMALLUPLOAD BASE + 31
#define FTP_BIGDOWNLOAD BASE + 32
#define FTP_BIGUPLOAD BASE + 33


// TEST_ENTRY
#define TEST_ENTRY \
            if (uMsg == TPM_QUERY_THREAD_COUNT) \
            { \
                ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = THREAD_COUNT; \
                return SPR_HANDLED; \
            } \
            else if (uMsg != TPM_EXECUTE) \
            { \
                return TPR_NOT_HANDLED; \
            }


// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato;


// function prototypes
bool DeleteCacheEntry(TCHAR                         szCompleteUrl[URL_LENGTH], 
                      LPINTERNET_CACHE_ENTRY_INFO   lpCacheEntryInfo,
                      DWORD                         dwCacheEntryInfoBufferSize);
bool GetDeviceName (char* pszDeviceName, DWORD cLen);
void HandleCleanup(HINTERNET openHandle, HINTERNET openUrlHandle);
int LocalDirectorySetup(TCHAR *localDir);
int RemoteDirectorySetup(HINTERNET connectHandle, TCHAR* rootUploadDir, TCHAR* deviceUploadDir, TCHAR* uniqueUploadDir);



// Test function prototypes (TestProc's)
TESTPROCAPI HttpTests			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CacheTests			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
//TESTPROCAPI CookieTests			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI FtpTests			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {  
   
   TEXT("HTTP Tests"                        ), 0,    0,                       0, NULL,
   TEXT(   "Get Pages"                      ), 1, 6000,           HTTP_GETPAGES, HttpTests,     // 6000 default
   TEXT("Cache Tests"                       ), 0,    0,                       0, NULL,
   TEXT(   "Cache/Delete Pages"             ), 1, 6000,      CACHE_PAGES_DELETE, CacheTests,    // 6000 default
   TEXT(   "Retrieve Cache Entry Data"      ), 1,  500,      CACHE_RETRIEVEDATA, CacheTests,    //  500 default
   TEXT(   "Retrieve Cache Entry Files"     ), 1,  500,     CACHE_RETRIEVEFILES, CacheTests,    //  500 default
   
// TEXT("CookieTests"                       ), 0,    0,                       0, NULL,
// TEXT(   "Set Cookies"                    ), 1, 1500,              COOKIE_SET, CookieTests,
// TEXT(   "Get Cookies"                    ), 1, 1500,              COOKIE_GET, CookieTests,

   TEXT("FtpTests"                          ), 0,    0,                       0, NULL,
   TEXT(   "Small Files Download"           ), 1,  500,       FTP_SMALLDOWNLOAD, FtpTests,      //  500 default
   TEXT(   "Small Files Upload"             ), 1,  500,         FTP_SMALLUPLOAD, FtpTests,      //  500 default
// TEXT(   "Big File Download"              ), 1,    1,         FTP_BIGDOWNLOAD, FtpTests,      
// TEXT(   "Big File Upload"                ), 1,    1,           FTP_BIGUPLOAD, FtpTests,      
   NULL,                                       0,    0,                       0, NULL // marks end of list
};

#endif // __TUXDEMO_H__