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

typedef UINT (*PFN_TESTPROC)(LPCTSTR pszRootDirectory);

// test function prototypes

UINT Tst_EnumDirectoryBreadthFirst(IN LPCTSTR pszRootDirectory);
UINT Tst_EnumDirectoryDepthFirst(IN LPCTSTR pszRootDirectory);
UINT Tst_CreateRandomFilesAndDirectories(IN LPCTSTR pszRootDirectory);

// constants 

static const DWORD DEF_MAX_RECURSION    = 3;
static const DWORD DEF_MAX_WIDTH        = 3;
static const DWORD DEF_THREAD_COUNT     = 3;
static const DWORD MAX_FS_ROOTS         = 10;
static const TCHAR ROM_TEST_ROOT[]      = TEXT("\\windows");
static const TCHAR NET_TEST_ROOT[]      = TEXT("\\\\scratch\\scratch");
static const BOOL DEF_USE_NETWORKFS     = FALSE;

static const PFN_TESTPROC TEST_FUNCTION_TABLE[] = {
    Tst_EnumDirectoryBreadthFirst,
    Tst_EnumDirectoryDepthFirst,
    Tst_CreateRandomFilesAndDirectories
};
static const DWORD TEST_FUNCTION_TABLE_COUNT = sizeof(TEST_FUNCTION_TABLE)/sizeof(TEST_FUNCTION_TABLE[0]);

// message queue support

static const DWORD MAX_DEVNAME_LEN      = 100;
static const DWORD QUEUE_ITEM_SIZE      = (sizeof(DEVDETAIL) + MAX_DEVNAME_LEN);

// globals

TCHAR   g_szModuleName[MAX_PATH]        = TEXT("");
HANDLE  g_hInst                         = NULL;
LPTSTR  g_pszCmdLine                    = NULL;
DWORD   g_maxRecursion                  = DEF_MAX_RECURSION;
DWORD   g_cThreads                      = DEF_THREAD_COUNT;
DWORD   g_maxWidth                      = DEF_MAX_WIDTH;
BOOL    g_fTestNetworkFS                = DEF_USE_NETWORKFS;

// global fsroot table

TCHAR g_szFSRoots[MAX_FS_ROOTS][MAX_DEVNAME_LEN] = {0};
DWORD g_cFSRoots                        = 0;

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
UINT TestOneFile(
    IN LPCTSTR pszFileName
    )
{
    UINT retVal = CESTRESS_PASS;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    BYTE *pBuffer = NULL;
    BYTE *pMappedFile = NULL;
    FILETIME ftCreation, ftAccess, ftWrite;
    DWORD attributes, cbBuffer, cbRead, fileSizeHigh, fileSizeLow, lastErr;

    LogVerbose(TEXT("testing file \"%s\""), pszFileName);

    // try to allocate consecutively smaller buffers until success
    cbBuffer = 65536; // 64 KB
    while(NULL == pBuffer && 0 != cbBuffer) {
        pBuffer = (BYTE*)VirtualAlloc(NULL, cbBuffer, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        cbBuffer >>= 1;
    }

    if(NULL == pBuffer) {
        LogFail(TEXT("VirtualAlloc() failed; system error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    LogVerbose(TEXT("allocated a %u byte read buffer"), cbBuffer);

    // get the file attributes
    attributes = GetFileAttributes(pszFileName);
    lastErr = GetLastError();
    if(0xFFFFFFFF == attributes) {
        if(ERROR_FILE_NOT_FOUND == lastErr) {
            // file deleted before we could get attributes
            retVal = CESTRESS_PASS;
            goto done;
        }
        LogFail(TEXT("GetFileAttributes(\"%s\") failed; system error %u"),
            pszFileName, lastErr);
        retVal = CESTRESS_FAIL;
        goto done;
    }

    // we aren't able to open and read ROM modules
    if(FILE_ATTRIBUTE_ROMMODULE & attributes) {
        LogVerbose(TEXT("\"%s\" is a ROM MODULE; skipping test"), pszFileName);
        retVal = CESTRESS_PASS;
        goto done;
    }

    if(FlipACoin()) {

        // open a read, all-access handle to the file
        hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        lastErr = GetLastError();
        if(INVALID_HANDLE_VALUE == hFile) {
            if(ERROR_FILE_NOT_FOUND == lastErr) {
                // file deleted before we could open it
                LogVerbose(TEXT("unable to CreateFile \"%s\" because file no longer exists"), pszFileName);
                retVal = CESTRESS_PASS;
            } else if(ERROR_SHARING_VIOLATION == lastErr) {
                LogVerbose(TEXT("unable to CreateFile \"%s\" due to sharing violation"), pszFileName);
                retVal = CESTRESS_PASS;
            } else {
                LogWarn2(TEXT("CreateFile(\"%s\") failed; system error %u"), pszFileName, lastErr);
                retVal = CESTRESS_WARN2;
            }
            goto done;
        }

        // get the file size
        fileSizeLow = GetFileSize(hFile, &fileSizeHigh);
        lastErr = GetLastError();
        if(0xFFFFFFFF == fileSizeLow && ERROR_SUCCESS != lastErr) {
            LogFail(TEXT("GetFileSize(\"%s\") failed; system error %u"), pszFileName, lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }

        // get the file time
        if(!GetFileTime(hFile, &ftCreation, &ftAccess, &ftWrite)) {
            LogFail(TEXT("GetFileTime(\"%s\") failed; system error %u"), pszFileName, GetLastError());
            retVal = CESTRESS_FAIL;
            goto done;
        }

    } else {

        // map and read the file
        hFile = CreateFileForMapping(pszFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE_VALUE == hFile) {
            if(ERROR_FILE_NOT_FOUND == lastErr) {
                // file deleted before we could open it
                LogVerbose(TEXT("unable to CreateFileForMapping \"%s\" because file no longer exists"), pszFileName);
                retVal = CESTRESS_PASS;
            } else if(ERROR_SHARING_VIOLATION == GetLastError()) {
                LogVerbose(TEXT("unable to CreateFileForMapping \"%s\" due to sharing violation"), pszFileName);
                retVal = CESTRESS_PASS;
            } else {
                LogWarn2(TEXT("CreateFileForMapping(\"%s\") failed; system error %u"), pszFileName, GetLastError());
                retVal = CESTRESS_WARN2;
            }
            goto done;
        }

        // get the file size
        fileSizeLow = GetFileSize(hFile, &fileSizeHigh);
        lastErr = GetLastError();
        if(0xFFFFFFFF == fileSizeLow && ERROR_SUCCESS != lastErr) {
            LogFail(TEXT("GetFileSize(\"%s\") failed; system error %u"), pszFileName, lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }

        // don't bother mapping 0-byte files...
        if(fileSizeLow > 0) {

            hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
            if(NULL == hMapping) {
                LogFail(TEXT("CreateFileMapping() failed; system error %u"), GetLastError());
                retVal = CESTRESS_FAIL;
                // a failed CreateFileMapping() call closes the file handle
                hFile = INVALID_HANDLE_VALUE;             
                goto done;
            }

            pMappedFile = (BYTE*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
            if(NULL == pMappedFile) {
                LogFail(TEXT("MapViewOfFile() failed; system error %u"), GetLastError());
                retVal = CESTRESS_FAIL;
                goto done;
            }

            cbRead = fileSizeLow > cbBuffer ? cbBuffer : fileSizeLow;
            memcpy(pBuffer, pMappedFile, cbRead);
        }
    }

done:
    if(NULL != pMappedFile) {
        if(!UnmapViewOfFile(pMappedFile)) {
            LogFail(TEXT("UnmapViewOfFile() failed; system error %u"), GetLastError());
        }
    }
    if(NULL != hMapping) {
        if(!CloseHandle(hMapping)) {
            LogFail(TEXT("CloseHandle() (file map) failed; system error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
        // closing the map handle autmatically closes the file handle
        hFile = INVALID_HANDLE_VALUE; 
    }
    if(INVALID_HANDLE_VALUE != hFile) {
        if(!CloseHandle(hFile)) {
            LogFail(TEXT("CloseHandle() failed; system error %u"), GetLastError());
            retVal = CESTRESS_FAIL; 
        }
    }
    if(NULL != pBuffer) {
        if(!VirtualFree(pBuffer, 0, MEM_RELEASE)) {
            LogWarn1(TEXT("VirtualFree() failed; system eror %u"), GetLastError());
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT CreateRandomFile(
    IN LPCTSTR pszFileName
    )
{
    UINT retVal = CESTRESS_PASS;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE *pBuffer = NULL;
    DWORD cbBuffer, cbWritten, cbWrite;
    DWORD dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes;

    LogVerbose(TEXT("creating file \"%s\""), pszFileName);

    // try to allocate consecutively smaller buffers until success
    cbBuffer = 65536; // 64 KB
    while(NULL == pBuffer && 0 != cbBuffer) {
        pBuffer = (BYTE*)VirtualAlloc(NULL, cbBuffer, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if(NULL == pBuffer) {
            cbBuffer >>= 1;
        }
    }

    if(NULL == pBuffer || 0 == cbBuffer) {
        LogFail(TEXT("VirtualAlloc() failed; system error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    LogVerbose(TEXT("allocated a %u byte write buffer"), cbBuffer);
    
    // randomly pick some CreateFile params that will work
    dwDesiredAccess = FlipACoin() ? GENERIC_READ | GENERIC_WRITE : GENERIC_WRITE;
    dwShareMode = FlipACoin() ? 0 : FlipACoin() ? FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE;
    dwCreationDisposition = FlipACoin() ? CREATE_ALWAYS : OPEN_ALWAYS;
    dwFlagsAndAttributes = FlipACoin() ? FILE_ATTRIBUTE_NORMAL : FILE_FLAG_RANDOM_ACCESS;

    // create the file
    hFile = CreateFile(pszFileName, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition,
        dwFlagsAndAttributes, NULL);
    if(INVALID_HANDLE_VALUE == hFile) {
        if(ERROR_SHARING_VIOLATION == GetLastError()) {
            LogVerbose(TEXT("unable to CreateFile \"%s\" due to sharing violation"), pszFileName);
            retVal = CESTRESS_PASS;
        } else {
            LogFail(TEXT("CreateFile(\"%s\") failed; system error %u"), pszFileName, GetLastError());
            retVal = CESTRESS_FAIL;
        }
        goto done;
    }

    // write a random size (1-cbBuffer bytes)
    cbWrite = (Random() % cbBuffer)+1;
    LogVerbose(TEXT("writing %u bytes to file \"%s\""), cbWrite, pszFileName);
    if(!WriteFile(hFile, pBuffer, cbWrite, &cbWritten, NULL)) {
        if(ERROR_DISK_FULL == GetLastError()) {
            goto done;
        }
        LogFail(TEXT("WriteFile() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }

    if(!FlushFileBuffers(hFile)) {
        LogFail(TEXT("FlushFileBuffers() failed; error %u"), GetLastError());
        retVal = CESTRESS_FAIL;
        goto done;
    }
        
done:
    if(INVALID_HANDLE_VALUE != hFile) {
        if(!CloseHandle(hFile)) {
            LogFail(TEXT("CloseHandle() failed; system error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    if(NULL != pBuffer) {
        if(!VirtualFree(pBuffer, 0, MEM_RELEASE)) {
            LogWarn1(TEXT("VirtualFree() failed; system eror %u"), GetLastError());
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT EnumFiles(
    IN LPCTSTR pszRootDirectory
    )
{
    UINT retVal = CESTRESS_PASS;
    DWORD lastErr ;
    HANDLE hFind = INVALID_HANDLE_VALUE;    
    TCHAR szFilePath[MAX_PATH] = TEXT("");
    WIN32_FIND_DATA w32fd = {0};

    StringCchPrintf(szFilePath, MAX_PATH, TEXT("%s\\*"), pszRootDirectory);
    LogVerbose(TEXT("searching for files matching \"%s\""), szFilePath);
    hFind = FindFirstFile(szFilePath, &w32fd);
    lastErr = GetLastError();
    if(INVALID_HANDLE_VALUE == hFind 
        && ERROR_NO_MORE_ITEMS != lastErr
        && ERROR_NO_MORE_FILES != lastErr
        && ERROR_FILE_NOT_FOUND != lastErr
        && ERROR_PATH_NOT_FOUND != lastErr) {
        LogFail(TEXT("FindFirstFile(\"%s\") failed; system error %u"), szFilePath, lastErr);
        retVal = CESTRESS_FAIL;
        goto done;
    }

    if(INVALID_HANDLE_VALUE != hFind) {
        // enumerate all the files
        do
        {
            StringCchPrintf(szFilePath, MAX_PATH, TEXT("%s\\%s"), pszRootDirectory, w32fd.cFileName);        

            if(FILE_ATTRIBUTE_DIRECTORY & w32fd.dwFileAttributes) {
                // found a directory...
                continue;
            }

            LogVerbose(TEXT("found file \"%s\" with attributes 0x%08X"), szFilePath, w32fd.dwFileAttributes);

            if(NULL != _tcsstr(szFilePath, TEXT("FIL_"))) {
                LogVerbose(TEXT("\"%s\" is a test file, skipping"), szFilePath);
                retVal = CESTRESS_PASS;
                goto done;
            }

            retVal = TestOneFile(szFilePath);
            if(CESTRESS_FAIL == retVal) {
                goto done;
            }
            
        } while(FindNextFile(hFind, &w32fd));

        lastErr = GetLastError();
        if(ERROR_NO_MORE_FILES != lastErr) {
            LogFail(TEXT("FindNextFile() failed; system error %u"), lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }    
    }

done:
    if(INVALID_HANDLE_VALUE != hFind) {
        if(!FindClose(hFind)) {
            LogFail(TEXT("FindClose() failed; system error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT RecursiveEnumDirectory(
    IN LPCTSTR pszRootDirectory,
    IN BOOL fDepthFirst,
    IN DWORD maxRecursion
    )
{
    UINT retVal = CESTRESS_PASS;
    DWORD lastErr;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    TCHAR szFilePath[MAX_PATH] = TEXT("");
    WIN32_FIND_DATA w32fd = {0};

    if(!fDepthFirst) {
        retVal = EnumFiles(pszRootDirectory);
        if(CESTRESS_FAIL == retVal) {
            goto done;
        }
    }

    StringCchPrintf(szFilePath, MAX_PATH, TEXT("%s\\*"), pszRootDirectory);
    LogVerbose(TEXT("searching for directories matching \"%s\" %s"), szFilePath,
        fDepthFirst ? TEXT("depth-first") : TEXT("breadth-first"));

#ifdef USE_FIND_EX
    // use the FindFirstFileEx function to try to limit the search to directories
    hFind = FindFirstFileEx(szFilePath, FindExInfoStandard, &w32fd, 
        FindExSearchLimitToDirectories, NULL, 0);
#else   
    hFind = FindFirstFile(szFilePath, &w32fd);
#endif // USE_FIND_EX

    lastErr = GetLastError();
    if(INVALID_HANDLE_VALUE == hFind 
        && ERROR_NO_MORE_ITEMS != lastErr 
        && ERROR_NO_MORE_FILES != lastErr 
        && ERROR_FILE_NOT_FOUND != lastErr
        && ERROR_PATH_NOT_FOUND != lastErr) {

#ifdef USE_FIND_EX
        LogFail(TEXT("FindFirstFileEx(\"%s\") failed; system error %u"), szFilePath, lastErr);
#else   
        LogFail(TEXT("FindFirstFile(\"%s\") failed; system error %u"), szFilePath, lastErr);
#endif // USE_FIND_EX

        retVal = CESTRESS_FAIL;
        goto done;
    }

    if(INVALID_HANDLE_VALUE != hFind) {
        do
        {
            StringCchPrintf(szFilePath, MAX_PATH, TEXT("%s\\%s"), pszRootDirectory, w32fd.cFileName);

            // make sure this is a directory before continuing
            if(!(FILE_ATTRIBUTE_DIRECTORY & w32fd.dwFileAttributes)) {
                // found a file...
                continue;
            }

            LogVerbose(TEXT("found directory \"%s\""), szFilePath);

            if(maxRecursion > 0) {
                retVal = RecursiveEnumDirectory(szFilePath, fDepthFirst, maxRecursion-1);
                if(CESTRESS_FAIL == retVal) {
                    goto done;
                }
            }

        } while(FindNextFile(hFind, &w32fd));

        lastErr = GetLastError();
        if(ERROR_NO_MORE_FILES != lastErr) {
            LogFail(TEXT("FindNextFile() failed; system error %u"), lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }    
    }

    if(fDepthFirst) {
        retVal = EnumFiles(pszRootDirectory);
        if(CESTRESS_FAIL == retVal) {
            goto done;
        }
    }
    
done:
    if(INVALID_HANDLE_VALUE != hFind) {
        if(!FindClose(hFind)) {
            LogFail(TEXT("FindClose() failed; system error %u"), GetLastError());
            retVal = CESTRESS_FAIL;
        }
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT RecursiveCreateFilesAndDirectories(
    IN LPCTSTR pszRootDirectory,
    IN DWORD maxRecursion
    )
{
    UINT retVal = CESTRESS_PASS;
    TCHAR szSubPath[MAX_PATH] = TEXT("");
    TCHAR szMovePath[MAX_PATH] = TEXT("");
    DWORD index;

    if(_tcslen(pszRootDirectory) >= MAX_PATH - 30) {
        // this file name would be too long to acually create
        goto done;
    }

    LogVerbose(TEXT("creating random files & directories in \"%s\""), pszRootDirectory);

    for(index = 0; index < g_maxWidth; index++) {

        // randomly select between creating a directory or a file
        if(maxRecursion > 0 && FlipACoin()) {

            // create a new random directory with a unique name
            StringCchPrintf(szSubPath, MAX_PATH, TEXT("%s\\DIR_%x_%x"), 
                pszRootDirectory, GetCurrentThreadId(), GetTickCount());

            if(CreateDirectory(szSubPath, NULL)) {
                retVal = RecursiveCreateFilesAndDirectories(szSubPath, maxRecursion-1);
                if(CESTRESS_FAIL == retVal) {
                    goto done;
                }
                LogVerbose(TEXT("removing directory %s"), szSubPath);
                if(!RemoveDirectory(szSubPath)) {
                    if(ERROR_DIR_NOT_EMPTY != GetLastError()) {
                        LogWarn1(TEXT("RemoveDirectory(\"%s\") failed; system error %u"),
                            szSubPath, GetLastError());
                    }
                }
                
            } else {
                LogWarn1(TEXT("CreateDirectory(\"%s\") failed; system error %u"), szSubPath, GetLastError());
            }
            
        } else {
        
            // create a new random file with a unique name
            StringCchPrintf(szSubPath, MAX_PATH, TEXT("%s\\FIL_%x_%x"), 
                pszRootDirectory, GetCurrentThreadId(), GetTickCount());

            retVal = CreateRandomFile(szSubPath);
            if(CESTRESS_FAIL == retVal) {
                goto done;
            }   

            if(FlipACoin()) {
                // move the file to a sub directory before deleting it
                StringCchPrintf(szMovePath, MAX_PATH, TEXT("%s\\MOVED"), pszRootDirectory);
                CreateDirectory(szMovePath, NULL);
                StringCchPrintf(szMovePath, MAX_PATH, TEXT("%s\\MOVED\\FIL_%x_%x"),
                    pszRootDirectory, GetCurrentThreadId(), GetTickCount());

                // try to move the file to sub directory
                LogVerbose(TEXT("moving \"%s\" to \"%s\""), szSubPath, szMovePath);
                if(!MoveFile(szSubPath, szMovePath) && ERROR_ACCESS_DENIED != GetLastError()) {
                    LogWarn1(TEXT("MoveFile(\"%s\", \"%s\") failed; system error %u"), 
                        szSubPath, szMovePath, GetLastError());
                    LogVerbose(TEXT("deleting file %s"), szSubPath);
                    if(!DeleteFile(szSubPath) && ERROR_ACCESS_DENIED != GetLastError()) {
                        LogWarn1(TEXT("DeleteFile(\"%s\") failed; system error %u"),
                            szSubPath, GetLastError());
                    }
                } else {
                    // successfully move the file, delete it
                    LogVerbose(TEXT("deleting file %s"), szMovePath);
                    if(!DeleteFile(szMovePath) && ERROR_ACCESS_DENIED != GetLastError()) {
                        LogWarn1(TEXT("DeleteFile(\"%s\") failed; system error %u"),
                            szMovePath, GetLastError());
                    } else {
                        StringCchPrintf(szMovePath, MAX_PATH, TEXT("%s\\MOVED"), pszRootDirectory);
                        LogVerbose(TEXT("removing directory %s"), szMovePath);
                        if(!RemoveDirectory(szMovePath) && ERROR_DIR_NOT_EMPTY != GetLastError()) {
                            LogWarn1(TEXT("RemoveDirectory(\"%s\") failed; system error %u"),
                                szMovePath, GetLastError());
                        }
                    }   
                }
            } else {
                // just delete the file without moving it
                LogVerbose(TEXT("deleting file %s"), szSubPath);
                if(!DeleteFile(szSubPath) && ERROR_ACCESS_DENIED != GetLastError()) {
                    LogWarn1(TEXT("DeleteFile(\"%s\") failed; system error %u"),
                        szSubPath, GetLastError());
                }
            }
        }
    }

done:
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL AddFSRoot(LPCTSTR pszNewFSRoot)
{
    BOOL fRet = FALSE;
    
    if(g_cFSRoots < MAX_FS_ROOTS) {
        LogComment(TEXT("adding file system test root \"%s\""), pszNewFSRoot);
        StringCbCopy(g_szFSRoots[g_cFSRoots++], sizeof(g_szFSRoots[0]), pszNewFSRoot);
        fRet = TRUE;
    }

    return fRet;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD InitStorageVolumes(
    IN const GUID *pFSGuid
    )
{
    DWORD cVolumes = 0;
    HANDLE hMsgQueue = NULL;
    HANDLE hNotifications = NULL;
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
            cVolumes++;
            AddFSRoot(pDevDetail->szName);
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
                            _T("S2_FILE"),                               // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_FILESYS, SLOG_DEFAULT),    // Logging zones used by default
                            pmp                                         // Forward the Module params passed on the cmd line
                            );

    GetModuleFileName((HINSTANCE) g_hInst, g_szModuleName, 256);

    LogComment(_T("Module File Name: %s"), g_szModuleName);   

    // add the initial root directory (create it if it does not already exist)
    if(0xFFFFFFFF != GetFileAttributes(ROM_TEST_ROOT) || CreateDirectory(ROM_TEST_ROOT, NULL)) {
        AddFSRoot(ROM_TEST_ROOT);
    }

    if(g_fTestNetworkFS) {
        AddFSRoot(NET_TEST_ROOT);
    }

    DWORD cVols = InitStorageVolumes(&FATFS_MOUNT_GUID);
    LogComment(TEXT("found %u mounted FATFS volumes"), cVols);

    if(0 == g_cFSRoots) {
        LogWarn1(TEXT("found no file systems to test, aborting"));
        return FALSE;
    }

    return TRUE;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv /*unused*/)
{ 
    DWORD indexTest = Random() % TEST_FUNCTION_TABLE_COUNT;
    DWORD indexRoot = Random() % g_cFSRoots;
    
    return (TEST_FUNCTION_TABLE[indexTest])(g_szFSRoots[indexRoot]);
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    // no cleanup
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

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumDirectoryBreadthFirst(
    IN LPCTSTR pszRootDirectory
    )
{
    LogComment(TEXT("enumerating \"%s\" breadth-first"), pszRootDirectory);
    return RecursiveEnumDirectory(pszRootDirectory, FALSE, g_maxRecursion);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumDirectoryDepthFirst(
    IN LPCTSTR pszRootDirectory
    )
{
    LogComment(TEXT("enumerating \"%s\" depth-first"), pszRootDirectory);
    return RecursiveEnumDirectory(pszRootDirectory, TRUE, g_maxRecursion);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_CreateRandomFilesAndDirectories(
    IN LPCTSTR pszRootDirectory
    )
{
    LogComment(TEXT("creating random directories & files in \"%s\""), pszRootDirectory);
    return RecursiveCreateFilesAndDirectories(pszRootDirectory, g_maxRecursion);
}
