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
#include <wininet.h>
#include <winbase.h>    // for GetSystemTime()
#include <time.h>       // used to seed srand
#include <katoex.h>
#include <tux.h>
#include "tuxstuff.h"


TESTPROCAPI FtpTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
{
    TEST_ENTRY;

    // variables used outside the individual test cases
    DWORD           appContext = 1;
    DWORD           testResult = TPR_FAIL;
    HINTERNET       openHandle, 
                    connectHandle;
    int             i;
    const TCHAR     *gszMainProgName = _T("ftp.cpp");
    TCHAR           localFileDir[] = TEXT(LOCAL_FILE_DIR);
    const TCHAR     ftpServerName[] = TEXT(FTP_SERVER);
    const TCHAR     ftpUserName[] = TEXT(FTP_USERNAME);
    const TCHAR     ftpPassword[] = TEXT(FTP_PASSWORD);   

    // variables that are test case specific but should only be initialized once
    TCHAR           uniqueUploadDir[MAX_PATH] = TEXT("");

   
    g_pKato->Log(LOG_DETAIL, TEXT("Number of files to retreive: %d"), lpFTE->dwUserData);


    //--------------------------------------------------------------------------
    //
    // main program loop
    //
    for (i = 1; i <= lpFTE->dwUserData; i++)
    {
        g_pKato->Log(LOG_DETAIL, TEXT(" "));
        g_pKato->Log(LOG_DETAIL, TEXT("Running iteration number %d"), i);   

        
        //--------------------------------------------------------------------------
        // call InternetOpen to initilalize an Internet handle
        openHandle = InternetOpen(gszMainProgName, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	    if (openHandle == NULL)
	    {
		    g_pKato->Log(LOG_FAIL, TEXT("Creating the InternetOpen handle failed, %d"), GetLastError());
		    goto cleanup;
        }
        g_pKato->Log(LOG_DETAIL, TEXT("InternetOpen handle created successfully"));

        
        //--------------------------------------------------------------------------
        // call InternetConnect to open a FTP session
        g_pKato->Log(LOG_DETAIL, TEXT("Connecting to FTP server %s"), ftpServerName);
	    connectHandle = InternetConnect(openHandle, ftpServerName, INTERNET_DEFAULT_FTP_PORT, ftpUserName, ftpPassword, INTERNET_SERVICE_FTP, 0, appContext); 
	    if (connectHandle == NULL)
	    {
            g_pKato->Log(LOG_FAIL, TEXT("Creating the InternetConnect handle failed, %d"), GetLastError());
		    goto cleanup;
	    }
        g_pKato->Log(LOG_DETAIL, TEXT("InternetConnect handle created successfully"));

        
        //--------------------------------------------------------------------------
        // if this is the first iteration, create a local directory to store locally-created and downloaded test files
        if (i == 1)
        {
            if (!LocalDirectorySetup(localFileDir))
            {
                g_pKato->Log(LOG_FAIL, TEXT("The local directory setup routine failed"));
                goto cleanup;
            }
        }
        

        ////////////////////////////////////////////////////////////////////////
        // FTP Small Downloads Test
        ////////////////////////////////////////////////////////////////////////

        if (lpFTE->dwUniqueID == FTP_SMALLDOWNLOAD)
        {
            char            fileContent[SMALL_FILE_LENGTH],
                            downloadedFileBuffer[SMALL_FILE_LENGTH + 1];
            const char      fileBodyFormat[] = "%s%d"; 
            const char      fileBodyStart[] = "This is FTP file number ";
            DWORD           downloadedNumberOfBytesToRead = SMALL_FILE_LENGTH,
                            downloadedNumberOfBytesRead;
            HANDLE          createDownloadedFileHandle;
            HRESULT         contentResult, 
                            downloadedFileResult, 
                            remoteFileResult;
            int             compareContent;
            const LPCTSTR   downloadedFileNameFormat = TEXT("%s\\%s%s%d%s"),
                            fullDirFormat = TEXT("%s%s/%s"),
                            remoteFileNameFormat = TEXT("%s%s%d%s");
            TCHAR           remoteFile[MAX_PATH],
                            downloadedFile[MAX_PATH];
            TCHAR           downloadDir[] = TEXT(ROOT_DOWNLOAD_DIR);
            TCHAR           localFileSubDir[] = TEXT(LOCAL_DOWNLOAD_TESTS_DIR);
            const TCHAR     fileNameBeginning[] = TEXT("WininetStressFtpDownloadTestTxtFile");
            const TCHAR     txtFileExtension[] = TEXT(".txt"); 
            const TCHAR     fileNameOriginRemote[] = TEXT("Remote");
            const TCHAR     fileNameOriginDownloaded[] = TEXT("Downloaded");
            size_t          cchDest = MAX_PATH;

            


            //--------------------------------------------------------------------------
            // if this is the first iteration, we need to create a download_tests subdir in the local test directory
            if (i == 1)
            {
                if (!LocalDirectorySetup(localFileSubDir))
                {  
                    g_pKato->Log(LOG_FAIL, TEXT("The local directory setup routine failed"));
                    goto cleanup;
                }
            }
           

            //--------------------------------------------------------------------------
            // build the remote file name, and downloaded file name (including path)
            remoteFileResult = StringCchPrintf(remoteFile, cchDest, remoteFileNameFormat, fileNameBeginning, fileNameOriginRemote, i, txtFileExtension);
            if (!SUCCEEDED(remoteFileResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the remote file name, %d"), GetLastError());
                goto cleanup;
            }
                
            downloadedFileResult = StringCchPrintf(downloadedFile, cchDest, downloadedFileNameFormat, localFileDir, fileNameBeginning, fileNameOriginDownloaded, i, txtFileExtension);
            if (!SUCCEEDED(downloadedFileResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the deonloaded file name, %d"), GetLastError());
                goto cleanup;
            }

            g_pKato->Log(LOG_DETAIL, TEXT("    remoteFile is %s"), remoteFile);
            g_pKato->Log(LOG_DETAIL, TEXT("downloadedFile is %s"), downloadedFile);


            //--------------------------------------------------------------------------
            // create the content to compare with the content of the downloaded file
            contentResult = StringCchPrintfA(fileContent, cchDest, fileBodyFormat, fileBodyStart, i);
            if (!SUCCEEDED(contentResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the content for comparison, %d"), GetLastError());
                goto cleanup;
            }

            
            //--------------------------------------------------------------------------
            // navigate to the right directory on the server  
            if (!FtpSetCurrentDirectory(connectHandle, downloadDir))
            {
		        g_pKato->Log(LOG_FAIL, TEXT("Changing the directory failed, %d"), GetLastError());
		        goto cleanup;
	        }


            //--------------------------------------------------------------------------
            // download the file 
            g_pKato->Log(LOG_DETAIL, TEXT("Downloading file %s"), remoteFile);
            
            if (!FtpGetFile(connectHandle, remoteFile, downloadedFile, FALSE, 0, FTP_TRANSFER_TYPE_ASCII | INTERNET_FLAG_NO_CACHE_WRITE, 0))
	        {
		        g_pKato->Log(LOG_FAIL, TEXT("Downloading the file failed, %d"), GetLastError());
		        goto cleanup;
	        }


            //--------------------------------------------------------------------------
            // use CreateFile to get a handle to downloadedFile
            createDownloadedFileHandle = CreateFile(downloadedFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            if (!SUCCEEDED(createDownloadedFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not create a handle to the downloaded file, %d"), GetLastError());
                goto cleanup;
            }
            
            
            //--------------------------------------------------------------------------
            // use ReadFile to read the contents of downloadedFile into memory           
            if (!ReadFile(createDownloadedFileHandle, downloadedFileBuffer, downloadedNumberOfBytesToRead, &downloadedNumberOfBytesRead, NULL))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not read the contents of the downloaded file into memory, %d"), GetLastError());
                goto cleanup;
            }

            downloadedFileBuffer[downloadedNumberOfBytesRead] = '\0';
            g_pKato->Log(LOG_DETAIL, TEXT("Downloaded file content: %hs"), downloadedFileBuffer);


            //--------------------------------------------------------------------------
            // close the CreateFile handle to downloadedFile
            if (!CloseHandle(createDownloadedFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not close the downloaded CreateFile handle, %d"), GetLastError());
                goto cleanup;
            }


            //--------------------------------------------------------------------------
            // compare buffers for local and downloaded contents; if equal, proceed        
            compareContent = strcmp(fileContent, downloadedFileBuffer);
        
            if (compareContent != 0)
            {
                g_pKato->Log(LOG_DETAIL, TEXT("The local and downloaded contents do not match, %d"), GetLastError());
                goto cleanup;
            }
            g_pKato->Log(LOG_DETAIL, TEXT("The local and downloaded content is the same"));


            //--------------------------------------------------------------------------
            // delete the downloaded file
            g_pKato->Log(LOG_DETAIL, TEXT("Deleting the downloaded file"));
            
            if (!DeleteFile(downloadedFile))
            {
                g_pKato->Log(LOG_DETAIL, TEXT("Could not delete the downloaded file, %d"), GetLastError());
                goto cleanup;
            }

            //--------------------------------------------------------------------------
            // the iteration actually passed
            HandleCleanup(openHandle, connectHandle);
            g_pKato->Log(LOG_DETAIL, TEXT("This test iteration completed successfully"));
		}

        


        ////////////////////////////////////////////////////////////////////////
        // FTP Small Uploads Test
        ////////////////////////////////////////////////////////////////////////

        if (lpFTE->dwUniqueID == FTP_SMALLUPLOAD)
        {                                            
            char            fileContent[SMALL_FILE_LENGTH],
                            localFileBuffer[SMALL_FILE_LENGTH + 1],   
                            downloadedFileBuffer[SMALL_FILE_LENGTH + 1];
            const char      fileBodyFormat[] = "%s%d"; 
            const char      fileBodyStart[] = "This is FTP file number ";
            DWORD           downloadedNumberOfBytesToRead = SMALL_FILE_LENGTH,
                            downloadedNumberOfBytesRead;
            DWORD           localNumberOfBytesToRead = SMALL_FILE_LENGTH;
            DWORD           localNumberOfBytesRead, 
                            localNumberOfBytesWritten;
            HANDLE          createDownloadedFileHandle, 
                            createLocalFileHandle;
            HRESULT         contentResult, 
                            downloadedFileResult, 
                            fullUploadDirResult, 
                            localFileResult, 
                            remoteFileResult, 
                            smallFileLengthResult;
            int             compareContent;
            const LPCTSTR   fullDirFormat = TEXT("%s%s/%s"),
                            localFileNameFormat = TEXT("%s\\%s%s%d%s"),
                            remoteFileNameFormat = TEXT("%s%s%d%s");
            TCHAR           deviceUploadDir[MAX_PATH];
            TCHAR           fullUploadDir[MAX_PATH],
                            localFile[MAX_PATH],
                            remoteFile[MAX_PATH],
                            downloadedFile[MAX_PATH];
            TCHAR           localFileSubDir[] = TEXT(LOCAL_UPLOAD_TESTS_DIR);
            TCHAR           rootUploadDir[] = TEXT(ROOT_UPLOAD_DIR);
            const TCHAR     fileNameBeginning[] = TEXT("WininetStressFtpUploadTestTxtFile");
            const TCHAR     fileNameOriginLocal[] = TEXT("Local");
            const TCHAR     txtFileExtension[] = TEXT(".txt"); 
            const TCHAR     fileNameOriginRemote[] = TEXT("Remote");
            const TCHAR     fileNameOriginDownloaded[] = TEXT("Downloaded");
            size_t          cchDest = MAX_PATH;
            size_t          smallFileLength;

            
            //--------------------------------------------------------------------------
            // if this is the first iteration, we need to create a unique directory for the files on the FTP server
            if (i == 1)
            {
                if (!RemoteDirectorySetup(connectHandle, rootUploadDir, deviceUploadDir, uniqueUploadDir))
                {
                    g_pKato->Log(LOG_FAIL, TEXT("The remote directory setup routine failed"));
                    goto cleanup;
                }
            }


            //--------------------------------------------------------------------------
            // if this is the first iteration, we need to create an upload_tests subdir in the local test directory
            if (i == 1)
            {
                if (!LocalDirectorySetup(localFileSubDir))
                {
                    g_pKato->Log(LOG_FAIL, TEXT("The local directory setup routine failed"));
                    goto cleanup;
                }
            }


            //--------------------------------------------------------------------------
            // build the local file name (including path), remote file name, and downloaded file name (including path)
            localFileResult = StringCchPrintf(localFile, cchDest, localFileNameFormat, localFileDir, fileNameBeginning, fileNameOriginLocal, i, txtFileExtension);
            if (!SUCCEEDED(localFileResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the local file name and path, %d"), GetLastError());
                goto cleanup;
            }

            remoteFileResult = StringCchPrintf(remoteFile, cchDest, remoteFileNameFormat, fileNameBeginning, fileNameOriginRemote, i, txtFileExtension);
            if (!SUCCEEDED(remoteFileResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the remote file name, %d"), GetLastError());
                goto cleanup;
            }

            downloadedFileResult = StringCchPrintf(downloadedFile, cchDest, localFileNameFormat, localFileDir, fileNameBeginning, fileNameOriginDownloaded, i, txtFileExtension);
            if (!SUCCEEDED(downloadedFileResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the downloaded file name and path, %d"), GetLastError());
                goto cleanup;
            }

            g_pKato->Log(LOG_DETAIL, TEXT("     localFile is %s"), localFile);
            g_pKato->Log(LOG_DETAIL, TEXT("    remoteFile is %s"), remoteFile);
            g_pKato->Log(LOG_DETAIL, TEXT("downloadedFile is %s"), downloadedFile);
            

            //--------------------------------------------------------------------------
            // create the empty file to be uploaded
            createLocalFileHandle = CreateFile(localFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
            if (!SUCCEEDED(createLocalFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not create the local file, %d"), GetLastError());
                goto cleanup;
            }
                            
            
            //--------------------------------------------------------------------------
            // create the content for the file
            contentResult = StringCchPrintfA(fileContent, cchDest, fileBodyFormat, fileBodyStart, i);
            if (!SUCCEEDED(contentResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the content for the file, %d"), GetLastError());
                goto cleanup;
            }


            //--------------------------------------------------------------------------
            // calculate the length of fileContent            
            smallFileLengthResult = StringCbLengthA(fileContent, SMALL_FILE_LENGTH, &smallFileLength);


            //--------------------------------------------------------------------------
            // write the content into the file
            if (!WriteFile(createLocalFileHandle, fileContent, smallFileLength, &localNumberOfBytesWritten, NULL))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not write the content to the local file, %d"), GetLastError());
                goto cleanup;
            }
            
            
            //--------------------------------------------------------------------------
            // compare localNumberOfBytesWritten and smallFileLength; if equal, proceed
            if (localNumberOfBytesWritten != smallFileLength)
            {
                g_pKato->Log(LOG_FAIL, TEXT("The length of the content is incorrect, %d"), GetLastError());
                goto cleanup;
            }

            
            //--------------------------------------------------------------------------
            // close the handle for CreateFile since we're done with it
            if (!CloseHandle(createLocalFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not close the CreateFile handle, %d"), GetLastError());
                goto cleanup;
            }

            
            //--------------------------------------------------------------------------
            // construct the full path to the correct upload directory on the server         
            fullUploadDirResult = StringCchPrintf(fullUploadDir, cchDest, fullDirFormat, rootUploadDir, deviceUploadDir, uniqueUploadDir);
            if (!SUCCEEDED(fullUploadDirResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the full upload path string, %d"), GetLastError());
                return 0;
            }

            
            //--------------------------------------------------------------------------
            // navigate to the right directory on the server  
            g_pKato->Log(LOG_DETAIL, TEXT("Switching to directory %s"), fullUploadDir);
            
            if (!FtpSetCurrentDirectory(connectHandle, fullUploadDir))
            {
		        g_pKato->Log(LOG_FAIL, TEXT("Changing the directory failed, %d"), GetLastError());
		        goto cleanup;
	        }


            //--------------------------------------------------------------------------
            // upload the file, finally
            g_pKato->Log(LOG_DETAIL, TEXT("Uploading file %s"), localFile);

            if (!FtpPutFile(connectHandle, localFile, remoteFile, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_TRANSFER_ASCII, 0))
	        {
		        g_pKato->Log(LOG_FAIL, TEXT("Uploading the file failed, %d"), GetLastError());
                goto cleanup;
            }

            
            //--------------------------------------------------------------------------
            // download the file we just uploaded
            g_pKato->Log(LOG_DETAIL, TEXT("Downloading file %s"), remoteFile);
            
            if (!FtpGetFile(connectHandle, remoteFile, downloadedFile, FALSE, 0, FTP_TRANSFER_TYPE_ASCII | INTERNET_FLAG_NO_CACHE_WRITE, 0))
	        {
		        g_pKato->Log(LOG_FAIL, TEXT("Downloading the file failed, %d"), GetLastError());
		        goto cleanup;
	        }

            
            //--------------------------------------------------------------------------
            // use CreateFile to get a handle to localFile
            createLocalFileHandle = CreateFile(localFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            if (!SUCCEEDED(createLocalFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not create a handle to the local file, %d"), GetLastError());
                goto cleanup;
            }

            
            //--------------------------------------------------------------------------
            // use ReadFile to read the contents of localFile into memory      
            if (!ReadFile(createLocalFileHandle, localFileBuffer, localNumberOfBytesToRead, &localNumberOfBytesRead, NULL))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not read the contents of the local file into memory, %d"), GetLastError());
                goto cleanup;
            }
            
            localFileBuffer[localNumberOfBytesRead] = '\0';
            g_pKato->Log(LOG_DETAIL, TEXT("Local file content: %hs"), localFileBuffer);


            //--------------------------------------------------------------------------
            // close the CreateFile handle to localFile
            if (!CloseHandle(createLocalFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not close the CreateFile handle, %d"), GetLastError());
                goto cleanup;
            }
      
            
            //--------------------------------------------------------------------------
            // use CreateFile to get a handle to downloadedFile
            createDownloadedFileHandle = CreateFile(downloadedFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            if (!SUCCEEDED(createDownloadedFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not create a handle to the downloaded file, %d"), GetLastError());
                goto cleanup;
            }
            
            
            //--------------------------------------------------------------------------
            // use ReadFile to read the contents of downloadedFile into memory           
            if (!ReadFile(createDownloadedFileHandle, downloadedFileBuffer, downloadedNumberOfBytesToRead, &downloadedNumberOfBytesRead, NULL))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not read the contents of the downloaded file into memory, %d"), GetLastError());
                goto cleanup;
            }

            downloadedFileBuffer[downloadedNumberOfBytesRead] = '\0';
            g_pKato->Log(LOG_DETAIL, TEXT("Downloaded file content: %hs"), downloadedFileBuffer);


            //--------------------------------------------------------------------------
            // close the CreateFile handle to downloadedFile
            if (!CloseHandle(createDownloadedFileHandle))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not close the downloaded CreateFile handle, %d"), GetLastError());
                goto cleanup;
            }


            //--------------------------------------------------------------------------
            // compare buffers for local and remote contents; if equal, proceed        
            compareContent = strcmp(localFileBuffer, downloadedFileBuffer);
        
            if (compareContent != 0)
            {
                g_pKato->Log(LOG_DETAIL, TEXT("The local and downloaded contents do not match, %d"), GetLastError());
                goto cleanup;
            }
            g_pKato->Log(LOG_DETAIL, TEXT("The local and downloaded content is the same"));

            
            //--------------------------------------------------------------------------
            // delete the local and downloaded files
            g_pKato->Log(LOG_DETAIL, TEXT("Deleting the local file"));
            if (!DeleteFile(localFile))
            {
                g_pKato->Log(LOG_DETAIL, TEXT("Could not delete the local file, %d"), GetLastError());
                goto cleanup;
            }
            
            g_pKato->Log(LOG_DETAIL, TEXT("Deleting the downloaded file"));
            if (!DeleteFile(downloadedFile))
            {
                g_pKato->Log(LOG_DETAIL, TEXT("Could not delete the downloaded file, %d"), GetLastError());
                goto cleanup;
            }

            //--------------------------------------------------------------------------
            // the iteration actually passed
            HandleCleanup(openHandle, connectHandle);
            g_pKato->Log(LOG_DETAIL, TEXT("This test iteration completed successfully"));
        }       
    }

    //--------------------------------------------------------------------------
    // all iterations passed
    testResult = TPR_PASS;

    //--------------------------------------------------------------------------
    // any previous failures will jump here to finish, with the test result still set to FAIL
cleanup:
    if (testResult != TPR_PASS)
    {
        HandleCleanup(openHandle, connectHandle);
    }
    return testResult;
}