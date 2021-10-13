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
#include <katoex.h>
#include <tux.h>
#include <string.h>
#include "tuxstuff.h"


TESTPROCAPI CacheTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
{
    TEST_ENTRY;

    char                            contentBuffer[BUFFER],
                                    asciiCompareFile[BUFFER];
    DWORD                           bufferLen = BUFFER - 1,
	                                readLen = 0,
                                    testResult = TPR_FAIL,
                                    cacheEntryInfoBufferSize = BUFFER;
    FILETIME                        expireTime,
                                    lastModifiedTime; 
    HINTERNET                       openHandle, 
                                    urlHandle;
    HRESULT                         completeUrlResult,
                                    compareFileResult;
    int                             converter, 
                                    compareResult, 
                                    i;
    LPCTSTR                         urlFormat = TEXT("%s%d%s");
    LPINTERNET_CACHE_ENTRY_INFO     cacheEntryInfo;
    memset                          (&expireTime, 0, sizeof(expireTime));
    memset                          (&lastModifiedTime, 0, sizeof(lastModifiedTime));
    size_t                          cchDest = URL_LENGTH,
                                    cchDest1 = BUFFER;
    TCHAR                           completeUrl[URL_LENGTH],
                                    urlStart[] = TEXT(TESTURL),
                                    fileExtension[BUFFER] = TEXT(""),
                                    localFileName[BUFFER],
                                    compareFile[BUFFER];
    const TCHAR                     urlEnd[] = TEXT(".html"),
                                    *gszMainProgName = _T("cache.cpp"),
                                    compareStart[] = TEXT("<html><body>This is HTML file number "),
                                    compareEnd[] = TEXT("</body></html>");
 

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
        // build the URL string to know which file to retreive
        completeUrlResult = StringCchPrintf(completeUrl, cchDest, urlFormat, urlStart, i, urlEnd);
        if (!SUCCEEDED(completeUrlResult))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the URL, %d"), GetLastError());
                goto cleanup;
            }


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
        // call InternetOpenUrl to open a connection to the URL, using the handle created by InternetOpen
        g_pKato->Log(LOG_DETAIL, TEXT("Connecting to %s"),completeUrl);

	    urlHandle = InternetOpenUrl(openHandle, completeUrl, NULL, 0, 0, 0);
	    if (urlHandle == NULL)
	    {
		    g_pKato->Log(LOG_FAIL, TEXT("Creating the InternetOpenUrl handle failed, %d"), GetLastError());
            goto cleanup;
	    }
        g_pKato->Log(LOG_DETAIL, TEXT("InternetOpenUrl handle created successfully"));


        //--------------------------------------------------------------------------
        // create a cache entry for the file we're about to download
        if (!CreateUrlCacheEntry(completeUrl, bufferLen, fileExtension, localFileName, 0))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Creating the cache entry failed, %d"), GetLastError());
		    goto cleanup;
        }
	    g_pKato->Log(LOG_DETAIL, TEXT("Cache entry created successfully"));
  

        //--------------------------------------------------------------------------
        // Download the resource file	    
        if (!InternetReadFile(urlHandle, contentBuffer, bufferLen, &readLen))
	    {
		    g_pKato->Log(LOG_FAIL, TEXT("Reading in the remote file failed, %d"), GetLastError());
            goto cleanup;
        }


        //--------------------------------------------------------------------------
        // insert a NULL after the characters in the array; the upcoming comparison will fail without it
        contentBuffer[readLen] = '\0'; 

	    g_pKato->Log(LOG_DETAIL, TEXT("The remote file %s was retrieved successfully\n"), completeUrl);
        g_pKato->Log(LOG_DETAIL, TEXT("File Content: %hs"), contentBuffer);


        //--------------------------------------------------------------------------
        // build the string to compare to the contents of the retrieved file
        compareFileResult = StringCchPrintf(compareFile, cchDest1, urlFormat, compareStart, i, compareEnd);
        if (!SUCCEEDED(compareFileResult))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string needed to compare with the retrieved file, %d"), GetLastError());
            goto cleanup;
        }
        
        //--------------------------------------------------------------------------
        // convert compareFile to ASCII so the strings are in the same format
        converter = wcstombs(asciiCompareFile, compareFile, BUFFER);
        g_pKato->Log(LOG_DETAIL, TEXT("Compare Content: %s"), compareFile);
        
        //--------------------------------------------------------------------------
        // compare the retrieved file to what we know should be in the file     
        compareResult = strcmp(contentBuffer, asciiCompareFile);
        if (compareResult != 0)
        {
            g_pKato->Log(LOG_DETAIL, TEXT("Verification failed!"));
            goto cleanup;
        }
        g_pKato->Log(LOG_DETAIL, TEXT("Verification passed!"));
        
       
        //--------------------------------------------------------------------------
        // commit the cache entry
        if (!CommitUrlCacheEntry(completeUrl, localFileName, expireTime, lastModifiedTime, 0, NULL, NULL, fileExtension, 0))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Committing the cache entry failed, %d"), GetLastError());
            goto cleanup;
        }


        ////////////////////////////////////////////////////////////////////////
        // Retrieve Cache Entry Data test 
        ////////////////////////////////////////////////////////////////////////
           
        if (lpFTE->dwUniqueID == CACHE_RETRIEVEDATA)
        {
            cacheEntryInfo = (LPINTERNET_CACHE_ENTRY_INFO)LocalAlloc(LPTR,BUFFER);
            cacheEntryInfoBufferSize = BUFFER;

            g_pKato->Log(LOG_DETAIL, TEXT("Trying to retrieve the cache entry data"));
            // retrieve the cache entry data    
            if (!GetUrlCacheEntryInfo(completeUrl, cacheEntryInfo, &cacheEntryInfoBufferSize))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Retrieving the cache entry data failed, %d"), GetLastError());
                goto cleanup;
            } 
            g_pKato->Log(LOG_DETAIL, TEXT("Cache entry data retrieved successfully"));           

            // TODO: need a way of verifying the retrieved data is the same as the expected page content
        }



        ////////////////////////////////////////////////////////////////////////
        // Retrieve Cache Entry Files test 
        ////////////////////////////////////////////////////////////////////////

        if (lpFTE->dwUniqueID == CACHE_RETRIEVEFILES)
        {
            cacheEntryInfo = (LPINTERNET_CACHE_ENTRY_INFO)LocalAlloc(LPTR,BUFFER);
            cacheEntryInfoBufferSize = BUFFER;
            

            //--------------------------------------------------------------------------
            // retrieve the entry file
            if (!RetrieveUrlCacheEntryFile(completeUrl, cacheEntryInfo, &cacheEntryInfoBufferSize, 0))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Retrieving the cache entry file failed, %d"), GetLastError());
                goto cleanup;
            }         
            // MSDN: The caller should check the return value in this parameter. If the return size is less than or equal to the size passed in, all the relevant data has been returned. 
            // if (cacheEntryInfoBufferSize <= 
            g_pKato->Log(LOG_DETAIL, TEXT("Cache entry file retrieved successfully"));

            
            //--------------------------------------------------------------------------
            // unlock the cache entry
            if (!UnlockUrlCacheEntryFile(completeUrl, 0))
            {
                g_pKato->Log(LOG_FAIL, TEXT("Unlocking the cache entry failed, %d"), GetLastError());
                goto cleanup;
            }
            g_pKato->Log(LOG_DETAIL, TEXT("The cache entry was unlocked successfully"));
    
        }
  
        
        //--------------------------------------------------------------------------
        // delete the cache entry
        if(!DeleteCacheEntry(completeUrl, cacheEntryInfo, cacheEntryInfoBufferSize))
        {
            goto cleanup;
        }
        
        //--------------------------------------------------------------------------
        // the iteration actually passed
        HandleCleanup(openHandle, urlHandle);
        g_pKato->Log(LOG_DETAIL, TEXT("This test iteration completed successfully"));
  
    }

    //--------------------------------------------------------------------------
    // all iterations passed
    testResult = TPR_PASS;

    //--------------------------------------------------------------------------
    // any previous failures will jump here to finish, with the test result still set to FAIL
cleanup:
    if (testResult != TPR_PASS)
    {
        HandleCleanup(openHandle, urlHandle);
    }
    return testResult;        
}