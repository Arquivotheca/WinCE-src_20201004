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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <disk.h>
#include <filelist.h>
#include <windows.h>
#include <diskio.h>
#include <tchar.h>
#include <storemgr.h>
#include <const.h>
#include <fstestutil.h>
////////////////////////////////////////////////////////////////////////////////
// TestProc
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

#define ONE_KB 1024

#define ONE_MINUTE 60000
enum
{
    MOVEDELETE_FOLDER = 1,
    MULTIWRITE_FILE,
    OVERWRITEDELETE_FILE,
    OVERWRITEOVERWRITE_FILE,
    MULTIFILE_MULTITHREAD,
    MUTLITHREAD_MULTIFOLDER,
    FINDFILESDIRS_FOLDER,
    SCATTER_GATHER,
//Different thread  quantums
    THQ_MOVEDELETE_FOLDER = 21,
    THQ_MULTIWRITE_FILE,
    THQ_OVERWRITEDELETE_FILE,
    THQ_OVERWRITEOVERWRITE_FILE,
    THQ_MUTLITHREAD_MULTIFOLDER,
    THQ_FINDFILESDIRS_FOLDER,
    THQ_SCATTER_GATHER
};


WCHAR       g_szProfile[PROFILENAMESIZE] = L"";
VolumeEntry g_VolumeTable[256];
int         g_iVolumeEntry = 0;
int         g_iPartitionIndex = 0;                          // Index of the partition to worry about.  Currently only using 0.
int         g_iDevice=0;


DWORD WINAPI ThreadProc(LPVOID lpParameter);
DWORD WINAPI ThreadMultiProc(WCHAR* strFileName);
DWORD WINAPI ThreadMultiFolderProc(WCHAR* strFileName);
DWORD WINAPI ThreadCreateFilesProc(WCHAR* pszFn);
DWORD WINAPI ThreadCreateDirectoriesProc(WCHAR* pszFn);
DWORD WINAPI ThreadFindFilesProc(WCHAR* pszFn);
DWORD WINAPI ThreadScatterGather(WCHAR *pszFn);



////////////////////////////////////////////////////////////////////////////////////////
// TEST PROC
//

TESTPROCAPI TestProc(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    int nTestID = (int)lpFTE->dwUserData;
    if(!CleanDevice(g_iDevice))
    {
        FS_TRACE(LOG_FAIL, L"Failed to cleanup device!");
        return TPR_FAIL;
        
    }
    if(!cfCreateDirectory( g_iDevice, L"MTH_TESTDIR"))
    {
        FS_TRACE(LOG_FAIL, L"Failed to create root folder, error %d", GetLastError());
        return TPR_FAIL;
    }


    switch(nTestID)
    {
////////////////////////////////////////////////////////////////////////////////////////////
        case THQ_SCATTER_GATHER:
        case SCATTER_GATHER:
            {
                DWORD dwExitCodeSG1, dwExitCodeSG2;
                HANDLE hThreadSG1 = NULL;
                HANDLE hThreadSG2 = NULL;
                HANDLE hThreadFind = NULL;
                HANDLE hThreadCreateDirs = NULL;

                // start a scatter/gather thread
                hThreadSG1 = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadScatterGather, 
                    (LPVOID)TEXT("MTH_TESTDIR\\sg_1_file"), CREATE_SUSPENDED, NULL);
                if(NULL == hThreadSG1)
                {   
                    FS_TRACE(LOG_FAIL, L"Failed to create sg file thread");
                    return TPR_FAIL;
                }

                // start a second scatter/gather thread
                hThreadSG2 = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadScatterGather, 
                    (LPVOID)TEXT("MTH_TESTDIR\\sg_2_file"), CREATE_SUSPENDED, NULL);
                if(NULL == hThreadSG2)
                {   
                    FS_TRACE(LOG_FAIL, L"Failed to create sg file thread");
                    VERIFY(CloseHandle(hThreadSG1));
                    return TPR_FAIL;
                }

                // start a thread that will create directories
                hThreadCreateDirs = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadCreateDirectoriesProc,
                    (LPVOID)TEXT("MTH_TESTDIR"), CREATE_SUSPENDED, NULL);
                if(NULL == hThreadCreateDirs)
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create directory creation thread");
                    VERIFY(CloseHandle(hThreadSG1));
                    VERIFY(CloseHandle(hThreadSG2));
                    return TPR_FAIL;
                }

                // start a thread that will enumerate files/directories
                hThreadFind = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadFindFilesProc,
                    (LPVOID)TEXT("MTH_TESTDIR"), CREATE_SUSPENDED, NULL);
                if(NULL == hThreadFind)
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create find file thread");
                    VERIFY(CloseHandle(hThreadSG1));
                    VERIFY(CloseHandle(hThreadSG2));
                    VERIFY(CloseHandle(hThreadCreateDirs));
                    return TPR_FAIL;
                }

                if(THQ_SCATTER_GATHER == nTestID)
                {
                    // set up short thread quantums
                    CeSetThreadQuantum(hThreadSG1, 2);
                    CeSetThreadQuantum(hThreadSG2, 2);
                    CeSetThreadQuantum(hThreadCreateDirs, 2);
                    CeSetThreadQuantum(hThreadFind, 2);
                    CeSetThreadQuantum(GetCurrentThread(), 2);
                }

                // release all of the threads
                ResumeThread(hThreadSG1);
                ResumeThread(hThreadSG2);
                ResumeThread(hThreadCreateDirs);
                ResumeThread(hThreadFind);

                // use this thread to create and write to files
                ThreadCreateFilesProc(TEXT("MTH_TESTDIR"));

                // wait for the other threads to finish 
                
                VERIFY(WAIT_OBJECT_0 == WaitForSingleObject(hThreadSG1, INFINITE));
                VERIFY(GetExitCodeThread(hThreadSG1, &dwExitCodeSG1));
                VERIFY(CloseHandle(hThreadSG1));

                VERIFY(WAIT_OBJECT_0 == WaitForSingleObject(hThreadSG2, INFINITE));
                VERIFY(GetExitCodeThread(hThreadSG2, &dwExitCodeSG2));
                VERIFY(CloseHandle(hThreadSG2));

                VERIFY(WAIT_OBJECT_0 == WaitForSingleObject(hThreadCreateDirs, INFINITE));
                VERIFY(CloseHandle(hThreadCreateDirs));

                VERIFY(WAIT_OBJECT_0 == WaitForSingleObject(hThreadFind, INFINITE));
                VERIFY(CloseHandle(hThreadFind));

                // SG file thread returns an exit code indicating success or failure
                if(ERROR_SUCCESS != dwExitCodeSG1)
                {
                    FS_TRACE(LOG_FAIL, L"FAILURE: scatter/gather thread 1 failed");
                    return TPR_FAIL;
                }

                if(ERROR_SUCCESS != dwExitCodeSG2)
                {
                    FS_TRACE(LOG_FAIL, L"FAILURE: scatter/gather thread 2 failed");
                    return TPR_FAIL;
                }

                // verify ThreadCreateFilesProc did what we expect:
                WCHAR szFileName[MAX_PATH];
                for( int k = 0; k < 10; k++)
                {
                    VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"MTH_TESTDIR\\file%d", k)));
                    FS_TRACE(LOG_DETAIL, L"Checking file \"%s\"...", szFileName);
                    
                    if(!cfFileExists(g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: File \"%s\" does not exist", szFileName);
                        return TPR_FAIL;
                    } 

                    if(!cfGetFileSize(g_iDevice, szFileName, ONE_KB*10))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: Bad size for file \"%s\"", szFileName);
                        return TPR_FAIL;
                    }
                    
                }

                // verify ThreadCreateDirectoriesProc did what we expect:
                for( k = 0; k < 200; k++)
                {
                    VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"MTH_TESTDIR\\folder%d", k)));
                    FS_TRACE(LOG_DETAIL, L"Checking directory \"%s\"...", szFileName);

                    if(!cfDirectoryExists(g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: Directory \"%s\" does not exist", szFileName);
                        return TPR_FAIL;
                    } 
                }

                // no verification required for FindFileThread
            }
        break;

////////////////////////////////////////////////////////////////////////////////////////////
        case THQ_FINDFILESDIRS_FOLDER:
            CeSetThreadQuantum(GetCurrentThread(), 2);
        case FINDFILESDIRS_FOLDER:
            {
                HANDLE hFileThread = NULL;
                HANDLE hDirThread = NULL;

                // this thread will enumerate files in the directory
                hFileThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadCreateFilesProc, (LPVOID)TEXT("MTH_TESTDIR"), CREATE_SUSPENDED, NULL);
                if(NULL == hFileThread)
                {   
                    FS_TRACE(LOG_FAIL, L"Failed to create file creation thread");
                    return TPR_FAIL;
                }

                hDirThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadCreateDirectoriesProc, (LPVOID)TEXT("MTH_TESTDIR"), CREATE_SUSPENDED, NULL);
                if(NULL == hDirThread)
                {   
                    FS_TRACE(LOG_FAIL, L"Failed to create file creation thread");
                    VERIFY(CloseHandle(hFileThread));
                    return TPR_FAIL;
                }

                if(THQ_FINDFILESDIRS_FOLDER == nTestID)
                {
                    CeSetThreadQuantum(hFileThread, 2);
                    CeSetThreadQuantum(hDirThread, 2);
                }

                ResumeThread(hFileThread);
                ResumeThread(hDirThread);

                ThreadFindFilesProc(TEXT("MTH_TESTDIR"));

                WaitForSingleObject(hFileThread, INFINITE);
                CloseHandle(hFileThread);
                WaitForSingleObject(hDirThread, INFINITE);
                CloseHandle(hDirThread);

                WCHAR szFileName[MAX_PATH];
                for( int k = 0; k < 10; k++)
                {
                    VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"MTH_TESTDIR\\file%d", k)));
                    FS_TRACE(LOG_DETAIL, L"Checking file \"%s\"...", szFileName);
                    
                    if(!cfFileExists(g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: File \"%s\" does not exist", szFileName);
                        return TPR_FAIL;
                    } 

                    if(!cfGetFileSize(g_iDevice, szFileName, ONE_KB*10))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: Bad size for file \"%s\"", szFileName);
                        return TPR_FAIL;
                    }
                    
                }

                for( k = 0; k < 200; k++)
                {
                    VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"MTH_TESTDIR\\folder%d", k)));
                    FS_TRACE(LOG_DETAIL, L"Checking directory \"%s\"...", szFileName);

                    if(!cfDirectoryExists(g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"FAILURE: Directory \"%s\" does not exist", szFileName);
                        return TPR_FAIL;
                    } 
                }

            }
        break;
////////////////////////////////////////////////////////////////////////////////////////////
        case THQ_MOVEDELETE_FOLDER:
            CeSetThreadQuantum(GetCurrentThread(), 2);
        case MOVEDELETE_FOLDER:     
            {
                //Create 500 folders
                WCHAR szFileName[_MAX_PATH];
                for( int k=0; k < 300; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\folder%d"), k); 
                    if(!cfCreateDirectory( g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create folder %s", szFileName);
                        return TPR_FAIL;
                    }
                }
                
                //Start working thread  
                HANDLE hThread = CreateThread( NULL, NULL, ThreadProc, (LPVOID)nTestID, CREATE_SUSPENDED, NULL);
                if( NULL == hThread )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create working thread");                 
                    return TPR_FAIL;
                }

                //Start Thread
                ResumeThread(hThread);
                if(THQ_MOVEDELETE_FOLDER == nTestID)
                    CeSetThreadQuantum(hThread, 5);
                
                //Start moving subfolders same time
                WCHAR szTarget[_MAX_PATH];
                for( k=0; k < 300; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\folder%d"), k); 
                    wsprintf(szTarget, TEXT("MTH_TESTDIR\\movefolder%d"), k);   

                    FS_TRACE(LOG_COMMENT, L"Move folder %s ---> %s", szFileName, szTarget); 
                    // IGNORE ALL ERRORS - folder can be already delete from other thread!
                    cfMoveFile(g_iDevice, szFileName, szTarget);
                } // for
                
                // Wait until thread finished
                if(hThread)
                {
                    WaitForSingleObject( hThread, INFINITE);
                    CloseHandle(hThread);
                }

                //Check file system:
                //Clean-up device
                if(!CleanDevice(g_iDevice))
                {
                    FS_TRACE(LOG_FAIL, L"Failed to cleanup device!");
                    return TPR_FAIL;
                    
                }

                if(!cfCreateDirectory( g_iDevice, L"MTH_TESTDIR"))
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create root folder");
                    return TPR_FAIL;
                }
                
                //Create 300 subfolders 
                for( k=0; k < 300; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\folder%d"), k); 
                    if(!cfCreateDirectory( g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create folder %s", szFileName);
                        return TPR_FAIL;
                    }
                }
                
            }
            break;

////////////////////////////////////////////////////////////////////////////////////////////
        case THQ_MULTIWRITE_FILE:
            CeSetThreadQuantum(GetCurrentThread(), 5);
        case MULTIWRITE_FILE:       
            {

                WCHAR szRootFolder[_MAX_PATH];
                if( !GetPathForDeviceID( g_iDevice, szRootFolder))              
                {
                    FS_TRACE(LOG_FAIL,L"FAILED to get root path");
                    return TPR_FAIL;
                }
                // Create file
                WCHAR szFileName[_MAX_PATH];
                FS_TRACE(LOG_FAIL,L"Root folder:%s\n", szRootFolder);
                
                wsprintf( szFileName, TEXT("%sMTH_TESTDIR\\TESTFILE.txt"), szRootFolder);   
                HANDLE hFile = CreateFile( szFileName,  GENERIC_WRITE,       
                                     0, NULL,  CREATE_NEW,    0,  NULL);

                if ( hFile == INVALID_HANDLE_VALUE ) // we can't open the drive
                {
                    FS_TRACE(LOG_FAIL,L"FAILED to create file:%s\n", szFileName);   
                    return TPR_FAIL;
                }

				BOOL b;
                VERIFY(b = CloseHandle(hFile));
				if(!b)
				{
					DWORD dw = GetLastError(); 
					VERIFY(0);
				}

                //Start working thread  
                HANDLE hThread = CreateThread( NULL, NULL, ThreadProc, (LPVOID)nTestID, CREATE_SUSPENDED, NULL);
                if( NULL == hThread )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create working thread");                 
                    return TPR_FAIL;
                }

                //Start Thread
                ResumeThread(hThread);

                if(THQ_MULTIWRITE_FILE == nTestID)
                    CeSetThreadQuantum(hThread, 2);

                //Now start thread and start writing file 100 bytes X 1000 times 
                DWORD dwTotal = NULL;
                for(int i=0;  i < 1000; i++)
                {
                    hFile = CreateFile( szFileName,  GENERIC_WRITE,       
                                     FILE_SHARE_WRITE, NULL,  OPEN_EXISTING,    0,  NULL);

                
                    if ( hFile == INVALID_HANDLE_VALUE ) // we can't open the drive
                    {
                        FS_TRACE(LOG_FAIL,L"FAILED to open file:%s Last error: %d\n", szFileName, GetLastError());  
                        return TPR_FAIL;
                    }
                    
                    BYTE bBuff[100];
                    DWORD dwWritten;
                    if( !WriteFile( hFile, bBuff, 100, &dwWritten, NULL))
                    {
                        FS_TRACE(LOG_FAIL,L"FAILED to write file Last error: %d\n", GetLastError());    
                        return TPR_FAIL;
                    }

                    dwTotal += dwWritten;
                    FS_TRACE(LOG_FAIL,L"MAINTHREAD: Wrote: %d bytes\n", dwTotal);   
                    
                    CloseHandle(hFile);
                }

                // Wait until thread finished
                if(hThread)
                {
                    VERIFY(WAIT_OBJECT_0 == WaitForSingleObject( hThread, INFINITE));
                    VERIFY(CloseHandle(hThread));
                }   

                //TODO: Validations: Get file size, delete file
                
            }
            break;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
            case THQ_OVERWRITEDELETE_FILE:
            case THQ_OVERWRITEOVERWRITE_FILE:   
                CeSetThreadQuantum(GetCurrentThread(), 5);
            case OVERWRITEOVERWRITE_FILE:
            case OVERWRITEDELETE_FILE:
            {
                // Create 100 files
                WCHAR szFileName[_MAX_PATH];
                DWORD dwFileSize = 1024*10; // 10 K files
                for( int k=0; k < 100; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\testfile%d.bin"), k);   
                    if(!cfCreateFile( g_iDevice, szFileName, dwFileSize, 'A', 1, FALSE))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create file %s", szFileName);
                        return TPR_FAIL;
                    }
                }
                
                //Start thread - delete files or overwrite with other data

                HANDLE hThread = CreateThread( NULL, NULL, ThreadProc, (LPVOID)nTestID, CREATE_SUSPENDED, NULL);
                if( NULL == hThread )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create working thread");                 
                    return TPR_FAIL;
                }

                //Start Thread
                ResumeThread(hThread);

                if(THQ_OVERWRITEDELETE_FILE == nTestID || THQ_OVERWRITEOVERWRITE_FILE == nTestID)
                    CeSetThreadQuantum(hThread, 2);
                
                //Overwrite existing files
                dwFileSize = 1024;
                for( k=0; k < 100; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\testfile%d.bin"), k);
                    
                    FS_TRACE(LOG_COMMENT, L"MAIN Thread: overwrite file %s", szFileName);                       
                    cfCreateFile( g_iDevice, szFileName, dwFileSize, 'A', 1, (OVERWRITEOVERWRITE_FILE == nTestID), TRUE);
                }

                // Wait until thread finished
                if(hThread)
                {
                    WaitForSingleObject( hThread, INFINITE);
                    CloseHandle(hThread);
                }   
                // Validation:
                // Delete all files from subfolder and create a new ones
                for( k=0; k < 100; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\testfile%d.bin"), k);
                    cfDeleteFile(g_iDevice, szFileName);                    

                    if(!cfCreateFile( g_iDevice, szFileName, dwFileSize, 'A', 1, FALSE))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create file %s", szFileName);
                        return TPR_FAIL;
                    }
                }
                
                
            }
            break;
///////////////////////////////////////////////////////////////////////////////////////////         
        case MULTIFILE_MULTITHREAD:
            {
                //Create 5 subfolders
                //Create 5 files in subfolders
                
                WCHAR szFileName[_MAX_PATH];
                DWORD dwFileSize = 512;
                //Threads handles
                HANDLE phThreads[20]; // 20 new threads
                ZeroMemory(phThreads, sizeof(phThreads));

                HANDLE hThread;
                
                for( int k=0; k < 5; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\folder%d"), k); 
                    if(!cfCreateDirectory( g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create folder %s", szFileName);
                        return TPR_FAIL;
                    }

                    lstrcat( szFileName, L"\\test.bin");

                    //Create file in subfolder:
                    if(!cfCreateFile( g_iDevice, szFileName, dwFileSize, 'A', 1, FALSE))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create file %s", szFileName);
                        return TPR_FAIL;
                    }
                    //Create 4 threads writing each file
                    for( int i=0; i<4; i++ )    
                    {
                        FS_TRACE(LOG_FAIL, L"Starting thread for file %s", szFileName);                                                                             
                        hThread= CreateThread( NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadMultiProc, szFileName, CREATE_SUSPENDED, NULL);
                        if( NULL == hThread )
                        {
                            FS_TRACE(LOG_FAIL, L"Failed to create working thread %d", i);                   
                            FS_TRACE(LOG_FAIL, L"Last Error: %d", GetLastError());

                            //In ideal situation we should terminate all threads that are already created,
                            //But since it is a test code and this is really critical error and all threads are created 
                            //suspended, we ignore this
                            return TPR_FAIL;    
                        }

                        //Set threads priorities
                        SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL+1);
                        CeSetThreadQuantum(hThread, i+1);
                        phThreads[ i+k*4 ] = hThread;           
                    }
                                    
                }
                
                // Now resume all threads and wait for them to finish
                
                for(int i=0; i<20; i++)
                {
                    if(phThreads[i])
                    {
                        FS_TRACE(LOG_FAIL, L"Starting thread handle=0x%x", phThreads[i]);                                                       
                        ResumeThread(phThreads[i]);
                    }
                }
                
                //Wait until threads finish
                //Wait 3 minutes

                for(i=0; i<20; i++)
                {
                    if(phThreads[i])
                    {
                        WaitForSingleObject( phThreads[i], INFINITE);
                        CloseHandle(phThreads[i]);
                    }
                }                   

                // test done
                FS_TRACE(LOG_FAIL, L"Done");    
            }
        break;
///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Write different files in different folders from different threads.
        case THQ_MUTLITHREAD_MULTIFOLDER:
                CeSetThreadQuantum(GetCurrentThread(), 5);
        case MUTLITHREAD_MULTIFOLDER:
            {

                FS_TRACE(LOG_FAIL, L"Test started - creating folder structure");    
                
                WCHAR szFileName[_MAX_PATH];
                                
                // Create Root folder \A
                if(!cfCreateDirectory( g_iDevice, L"A") || !cfCreateDirectory( g_iDevice, L"A\\A1"))
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create A  folders or subfolders");
                    return TPR_FAIL;
                }
                
                // Create second root folder \B
                if(!cfCreateDirectory( g_iDevice, L"B"))
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create B root folder");
                    return TPR_FAIL;
                }
                
                
                // Create \B\B1\B2
                if(!cfCreateDirectory( g_iDevice, L"B\\B1") || !cfCreateDirectory( g_iDevice, L"B\\B1\\B2") )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create B sub folders");
                    return TPR_FAIL;
                }
                
                FS_TRACE(LOG_FAIL, L"Launching threads");   
                // Create first thread - write file \A\A1\test1.bin
                    HANDLE hThread_A = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadMultiFolderProc, L"A\\A1\\test1.bin", CREATE_SUSPENDED, NULL);
                if( NULL == hThread_A )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create working thread A");                   
                    FS_TRACE(LOG_FAIL, L"Last Error: %d", GetLastError());
                    return TPR_FAIL;    
                }

                // Create second thread - write file \B\B1\B2\test2.bin

                HANDLE hThread_B = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadMultiFolderProc, L"B\\B1\\B2\\test2.bin", CREATE_SUSPENDED, NULL);
                if( NULL == hThread_B )
                {
                    FS_TRACE(LOG_FAIL, L"Failed to create working thread B");                   
                    FS_TRACE(LOG_FAIL, L"Last Error: %d", GetLastError());
                    return TPR_FAIL;    
                }

                //Launch threads
                ResumeThread( hThread_A );
                ResumeThread( hThread_B );

                if( THQ_MUTLITHREAD_MULTIFOLDER == nTestID)
                {
                    CeSetThreadQuantum(hThread_A, 2);
                    CeSetThreadQuantum(hThread_B, 2);
                }
                // Main thread - create 100 subfolders at \B\B1\sub1(2,3,4...)

                FS_TRACE(LOG_FAIL, L"Creating folders in main thread"); 
                
                BOOL bResult = TRUE;
                for(int i=0; i<100; i++)
                {
                    wsprintf( szFileName, L"B\\B1\\sub%d", i);
                    if(!cfCreateDirectory( g_iDevice, szFileName))
                    {
                        FS_TRACE(LOG_FAIL, L"Failed to create folder %s", szFileName);
                        bResult = TRUE;
                        break;
                    }                   
                }

                // Wait for them to finish (since B started later wait for it to finish first)
                FS_TRACE(LOG_FAIL, L"Waiting for threads to finish");   
                if(hThread_B)
                {
                    WaitForSingleObject( hThread_B, INFINITE);
                    CloseHandle(hThread_B);
                }                   

                if(hThread_A)
                {
                    WaitForSingleObject( hThread_A, INFINITE);
                    CloseHandle(hThread_A);
                }                   

                // If we failed to create subfolders in main thread - quit right now
                if(!bResult)
                    return TPR_FAIL;    
                
                // Validations:
                // 1. Read file \A\A1\test1.bin
                FS_TRACE(LOG_FAIL, L"Validations start");   
                wsprintf( szFileName, L"A\\A1\\test1.bin");
                if (!cfReadFile(g_iDevice, szFileName, 1024*ONE_KB, 'A', 1))
                {
                    FS_TRACE(LOG_FAIL, TEXT("Error reading file %s\n"), szFileName);
                    return TPR_FAIL;
                }               
                // 2. Read file \B\B1\B2\test2.bin

                wsprintf( szFileName, L"B\\B1\\B2\\test2.bin");
                if (!cfReadFile(g_iDevice, szFileName, 1024*ONE_KB, 'A', 1))
                {
                    FS_TRACE(LOG_FAIL, TEXT("Error reading file %s\n"), szFileName);
                    return TPR_FAIL;
                }                           

                // 3. Create file in \B\B1\sub1\test3.bin
                wsprintf( szFileName, L"B\\B1\\sub3\\test_folder.bin");
                if( !cfCreateFile( g_iDevice,  szFileName, ONE_KB, 'A', 1, FALSE, FALSE))
                {
                    FS_TRACE(LOG_FAIL,L"THREAD: Error creating file: %s; Last error: 0x%x",  szFileName, GetLastError());
                    return 0;
                }
                
                FS_TRACE(LOG_FAIL, L"Test DONE");                   
                // Done
            }
        break;
    }

        
    return TPR_PASS;
}


/*---------------------------------------------------------------
Thread function
-----------------------------------------------------------------*/

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    int nTestCase = (int)lpParameter;
    WCHAR szFileName[_MAX_PATH];
    
    FS_TRACE(LOG_COMMENT, L"Working thread started for test case: %d", nTestCase);
    
    switch(nTestCase )
    {

////////////////////////////////////////////////////////////////////////////////////////    
        case THQ_MOVEDELETE_FOLDER:     
        case MOVEDELETE_FOLDER:     
            {
                for( int k=0; k < 300; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\folder%d"), k); 
                    
                    FS_TRACE(LOG_COMMENT, L"Thread: Delete folder %s", szFileName); 
                    // IGNORE ALL ERRORS - folder can be already delete from other thread!
                    cfRemoveDirectory(g_iDevice, szFileName);
                } // for                
            }
            break;

            
/////////////////////////////////////////////////////////////////////////////////////////           
        case THQ_MULTIWRITE_FILE:       
        case MULTIWRITE_FILE:       
            {
                WCHAR szRootFolder[_MAX_PATH];
                if( !GetPathForDeviceID( g_iDevice, szRootFolder))              
                {
                    FS_TRACE(LOG_FAIL,L"FAILED to get root path");
                    return TPR_FAIL;
                }               


                wsprintf( szFileName, TEXT("%sMTH_TESTDIR\\TESTFILE.txt"), szRootFolder);   

                DWORD dwTotal = NULL;
                for(int i=0;  i < 1000; i++)
                {
                    HANDLE hFile = CreateFile( szFileName,  GENERIC_WRITE,       
                                     FILE_SHARE_WRITE, NULL,  OPEN_EXISTING,    0,  NULL);

                    if ( hFile == INVALID_HANDLE_VALUE ) // we can't open the drive
                    {
                        FS_TRACE(LOG_FAIL,L"FAILED to open file:%s Last error: %d\n", szFileName, GetLastError());  
                        return TPR_FAIL;
                    }
                    
                    BYTE bBuff[100];
                    DWORD dwWritten;
                    if( !WriteFile( hFile, bBuff, 100, &dwWritten, NULL))
                    {
                        FS_TRACE(LOG_FAIL,L"FAILED to write file Last error: %d\n", GetLastError());    
                        return TPR_FAIL;
                    }

                    dwTotal += dwWritten;
                    FS_TRACE(LOG_FAIL,L"SECOND THREAD: Wrote: %d bytes\n", dwTotal);    
                    
                    CloseHandle(hFile);
                }               

            }
            break;

///////////////////////////////////////////////////////////////////////////////////////////////
        case THQ_OVERWRITEOVERWRITE_FILE:
        case THQ_OVERWRITEDELETE_FILE:
        case OVERWRITEOVERWRITE_FILE:
        case OVERWRITEDELETE_FILE:
            {
                for( int k=0; k < 100; k++)
                {
                    wsprintf(szFileName, TEXT("MTH_TESTDIR\\testfile%d.bin"), k);       
                    if( OVERWRITEDELETE_FILE == nTestCase )
                    {
                        FS_TRACE(LOG_COMMENT, L"SECOND Thread: Delete file %s", szFileName);    
                        // IGNORE ALL ERRORS - folder can be already delete from other thread!
                        cfDeleteFile(g_iDevice, szFileName);
                    }
                    else // overwrite overwrite
                    {
                        DWORD dwFileSize = 512;
                        FS_TRACE(LOG_COMMENT, L"SECOND Thread: Overwriting file:%s", szFileName);   
                        cfCreateFile( g_iDevice, szFileName, dwFileSize, 'F', 1, TRUE, TRUE);
                    }
                } // for                                
            }
            break;
            
    }
    
    return NULL;
}


/*---------------------------------------------------------------
Thread function for Multi threading scenario MULTIFILE_MULTITHREAD
-----------------------------------------------------------------*/

DWORD WINAPI ThreadMultiProc(WCHAR* pszFn)
{
    //WCHAR* pszFn = (WCHAR*)lpParameter;


    FS_TRACE(LOG_FAIL,L"Thread started for file: %s", pszFn);
    // Get "absolute" path
    WCHAR szRootFolder[_MAX_PATH];
    if( !GetPathForDeviceID( g_iDevice, szRootFolder))              
    {
        FS_TRACE(LOG_FAIL,L"FAILED to get root path");
        return FALSE;
    }               

    WCHAR szFileName[_MAX_PATH];
    wsprintf( szFileName, TEXT("%s%s"), szRootFolder, pszFn);   // full path to file

    // Open a file 
    for( int i=0; i<100; i++)
    {

        FS_TRACE(LOG_FAIL,L"Thread is openning file: %s", pszFn);
        
        HANDLE  hFile = CreateFile( szFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);

        if( hFile == INVALID_HANDLE_VALUE )
        {
            FS_TRACE(LOG_FAIL,L"Thread:Can't open file %s last error: %x", szFileName, GetLastError());
            return FALSE;
        }

        //Write buffer 100 times
        BYTE    abBuffer[512];
        DWORD   dwWritten;

        SetFilePointer(hFile, 0, NULL, FILE_END);
        memset(abBuffer, '\xF0', sizeof(abBuffer));

        FS_TRACE(LOG_FAIL,L"Thread is Writing file: %s", pszFn);
        WriteFile(hFile, abBuffer, sizeof(abBuffer), &dwWritten, NULL);

        FS_TRACE(LOG_FAIL,L"Thread is closing handle file: %s", pszFn);
        CloseHandle(hFile); 
        
    }   
    
    return TRUE;
    
}


/*---------------------------------------------------------------
Thread function for Multi threading scenario MUTLITHREAD_MULTIFOLDER
-----------------------------------------------------------------*/

DWORD WINAPI ThreadMultiFolderProc(WCHAR* pszFn)
{
    FS_TRACE(LOG_FAIL,L"Thread started for file: %s", pszFn);
    // Testing create file 1MB:

    //Create/Delete file 20 times
    for( int k=0; k < 20; k++)
    {
        if( !cfCreateFile( g_iDevice, pszFn, ONE_KB, 'A', 1, FALSE, FALSE))
        {
            FS_TRACE(LOG_FAIL,L"THREAD: Error creating file: %s; Last error: 0x%x", pszFn, GetLastError());
            return 0;
        }

        if( !cfDeleteFile( g_iDevice, pszFn))
        {
            FS_TRACE(LOG_FAIL,L"THREAD: Error deleteing file: %s; Last error: 0x%x", pszFn, GetLastError());
            return 0;           
        }
        
    }

    //Create file again.
    
    if( !cfCreateFile( g_iDevice, pszFn, 1024*ONE_KB, 'A', 1, FALSE, FALSE))
    {
        FS_TRACE(LOG_FAIL,L"THREAD: Error creating file: %s; Last error: 0x%x", pszFn, GetLastError());
        return 0;
    }

    return 0;
}

// create 10 files (file0 through file9) of size 10 KB in the directory
DWORD WINAPI ThreadCreateFilesProc(WCHAR* pszFn)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    WCHAR szRootFolder[MAX_PATH] = L"";
    WCHAR szFileName[MAX_PATH] = L"";
    DWORD cbWritten = 0;
    BYTE bBuffer[ONE_KB];

    memset(bBuffer, '\xF0', sizeof(bBuffer));
    
    if(!GetPathForDeviceID(g_iDevice, szRootFolder))
    {
        FS_TRACE(LOG_FAIL,L"FAILED to get root path");
        return 0;
    }
    
    for( int k=0; k < 10; k++)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"%s%s\\file%d", szRootFolder, pszFn, k)));

        FS_TRACE(LOG_DETAIL, L"FILETHREAD: Creating file \"%s\" for write", szFileName);
        hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE_VALUE == hFile)
        {
            FS_TRACE(LOG_FAIL, L"FILETHREAD: FAILED creating file \"%s\"", szFileName);
            return 0;
        }

        for( int j=0; j < 10; j++)
        {
            FS_TRACE(LOG_DETAIL, L"FILETHREAD: Writing %u bytes to file \"%s\"", ONE_KB, szFileName);
            if(!WriteFile(hFile, bBuffer, ONE_KB, &cbWritten, NULL))
            {
                FS_TRACE(LOG_FAIL, L"FILETHREAD: FAILED writing to file \"%s\"", szFileName);
                VERIFY(CloseHandle(hFile));
                return 0;
            }

            FS_TRACE(LOG_DETAIL, L"FILETHREAD: Flushing file \"%s\"", szFileName);
            if(!FlushFileBuffers(hFile))
            {
                FS_TRACE(LOG_FAIL, L"FILETHREAD: FAILED flushing file \"%s\"", szFileName);
                VERIFY(CloseHandle(hFile));
                return 0;
            }            
        }

        if(!CloseHandle(hFile))
        {
            FS_TRACE(LOG_FAIL, L"FILETHREAD: FAILED closing file \"%s\"", szFileName);
            return 0;
        }

    }
    return 0;
}

// create 200 sub directories (folder0 through folder199) in the directory
DWORD WINAPI ThreadCreateDirectoriesProc(WCHAR* pszFn)
{
    WCHAR szDirectoryName[MAX_PATH] = L"";
    for( int k=0; k < 200; k++)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szDirectoryName, MAX_PATH, L"%s\\folder%d", pszFn, k)));
        FS_TRACE(LOG_DETAIL, L"FOLDERTHREAD: Creating folder \"%s\"", szDirectoryName);
        if(!cfCreateDirectory( g_iDevice, szDirectoryName))
        {
            FS_TRACE(LOG_FAIL, L"FOLDERTHREAD: Failed to create folder %s", szDirectoryName);        
        }
    }
    return 0;
}

// perform 200 findfirst/nextfile loops in the directory
DWORD WINAPI ThreadFindFilesProc(WCHAR* pszFn)
{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR szFileName[MAX_PATH] = L"";
    WIN32_FIND_DATA w32fd = {0};
    VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, L"%s\\*", pszFn)));
    for( int k=0; k < 200; k++)
    {
        hFind = FindFirstFile(szFileName, &w32fd);
        if(INVALID_HANDLE_VALUE != hFind)
        {
            do
            {   
                FS_TRACE(LOG_DETAIL, L"FINDTHREAD: Found file %s\\%s", pszFn, w32fd.cFileName);

            } while(FindNextFile(hFind, &w32fd));

            VERIFY(FindClose(hFind));
        }
		
    }
    
    return 0;
}

#define PAGE_SIZE   4096
#define NUM_BUFS    5

// create 10 files, WriteFileGather 5 pages of data to the file, ReadFileScatter and verify
DWORD WINAPI ThreadScatterGather(WCHAR *pszFn)
{
    int i, j;
    DWORD dwRetVal = ERROR_GEN_FAILURE;
    HANDLE hFile = INVALID_HANDLE_VALUE, hHeap = NULL;
    WCHAR szFileName[MAX_PATH];
    WCHAR szRootFolder[MAX_PATH];
    FILE_SEGMENT_ELEMENT writeSegments[NUM_BUFS], readSegments[NUM_BUFS];
    FILE_SEGMENT_ELEMENT writeOffsets[NUM_BUFS], readOffsets[NUM_BUFS];    

    // create a heap
    hHeap = HeapCreate(0, 2*NUM_BUFS*PAGE_SIZE, 0);
    if(NULL == hHeap)
    {
        FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: HeapCreate() failed; error %u", GetLastError());
        goto done;
    }

    // allocate read and write buffers
    for( i=0; i < NUM_BUFS; i++)
    {
        writeSegments[i].Buffer = (PVOID)HeapAlloc(hHeap, 0, PAGE_SIZE);
        readSegments[i].Buffer = (PVOID)HeapAlloc(hHeap, 0, PAGE_SIZE);
        if(NULL == writeSegments[i].Buffer || NULL == readSegments[i].Buffer)
        {
            FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: HeapAlloc() failed; error %u", GetLastError());
            goto done;
        }
        memset(writeSegments[i].Buffer, (BYTE)i, PAGE_SIZE);        
    }

    if(!GetPathForDeviceID(g_iDevice, szRootFolder))
    {
    
        FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: FAILED to get root path");
        goto done;
    }

    // create 10 files
    for(i=0; i < 10; i++)
    {
        // build the sg file name
        StringCchPrintfEx(szFileName, MAX_PATH, NULL, NULL, STRSAFE_NULL_ON_FAILURE, L"%s%s%u", 
            szRootFolder, pszFn, i);        

        hFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if(INVALID_HANDLE_VALUE == hFile)
        {
            FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: CreateFile() failed; error %u", GetLastError());
            goto done;
        }
        
        for(j=0; j < NUM_BUFS; j++)
        {
            writeOffsets[j].Alignment = ((i * NUM_BUFS) + j) * PAGE_SIZE;
            readOffsets[j].Alignment = ((i * NUM_BUFS) + j) * PAGE_SIZE;
        }

        FS_TRACE(LOG_DETAIL, L"SCATTER_GATHER_THREAD: WriteFileGather %u bytes to file %s", NUM_BUFS*PAGE_SIZE, szFileName);

        if(!WriteFileGather(hFile, writeSegments, NUM_BUFS*PAGE_SIZE, (LPDWORD)writeOffsets, NULL))
        {
            FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: WriteFileGather() failed; error %u", GetLastError());
            goto done;
        }

        FlushFileBuffers(hFile);

        FS_TRACE(LOG_DETAIL, L"SCATTER_GATHER_THREAD: ReadFileScatter %u bytes from file %s", NUM_BUFS*PAGE_SIZE, szFileName);

        if(!ReadFileScatter(hFile, readSegments, NUM_BUFS*PAGE_SIZE, (LPDWORD)readOffsets, NULL))
        {
            FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: ReadFileScatter() failed; error %u", GetLastError());
            goto done;
        }

        // compare data
        for(j=0; j < NUM_BUFS; j++)
        {
            if(0 != memcmp(writeSegments[j].Buffer, readSegments[j].Buffer, PAGE_SIZE))
            {
                FS_TRACE(LOG_FAIL, L"SCATTER_GATHER_THREAD: data mismatch");
                goto done;
            }
        }

        // close the file
        VERIFY(CloseHandle(hFile));
        hFile = INVALID_HANDLE_VALUE;
    }

    FS_TRACE(LOG_DETAIL, L"SCATTER_GATHER_THREAD: success!");

    dwRetVal = ERROR_SUCCESS;

done:
    if(INVALID_HANDLE_VALUE != hFile) VERIFY(CloseHandle(hFile));
    if(hHeap) VERIFY(HeapDestroy(hHeap));
    return dwRetVal;
}
