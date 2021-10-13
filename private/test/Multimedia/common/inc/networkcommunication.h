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

#include "network.h"

// Internal helper function
DWORD ConvertUnicodeToUtf8(IN LPCWSTR pwszIn,IN DWORD dwInLen,OUT LPBYTE pszOut,IN DWORD dwOutLen,IN BOOL bEncode);

/*
Routine Description:
    Convert a string of UNICODE characters to UTF-8:
        0000000000000000..0000000001111111: 0xxxxxxx
        0000000010000000..0000011111111111: 110xxxxx 10xxxxxx
        0000100000000000..1111111111111111: 1110xxxx 10xxxxxx 10xxxxxx
*/
DWORD ConvertUnicodeToUtf8(IN LPCWSTR pwszIn,IN DWORD dwInLen,OUT LPBYTE pszOut,IN DWORD dwOutLen,IN BOOL bEncode)
{

    DWORD outputSize = bEncode ? 3 : 1;
    static char hexArray[] = "0123456789ABCDEF";

    if (dwOutLen < outputSize)
        return 0;
    while (dwInLen-- && dwOutLen)
    {
        WORD wchar = *pwszIn++;
        BYTE bchar;

        if (wchar <= 0x007F)
        {
            *pszOut++ = (BYTE)(wchar);
            --dwOutLen;
            continue;
        }

        BYTE lead = ((wchar >= 0x0800)? 0xE0 : 0xC0);
        int shift = ((wchar >= 0x0800)? 12 : 6);

        bchar = lead | (BYTE)(wchar >> shift);
        if (bEncode)
        {
            *pszOut++ = '%';
            *pszOut++ = hexArray[bchar >> 4];
            bchar = hexArray[bchar & 0x0F];
        }
        *pszOut++ = bchar;
        if (wchar >= 0x0800)
        {
            bchar = 0x80 | (BYTE)((wchar >> 6)& 0x003F);
            if (bEncode)
            {
                *pszOut++ = '%';
                *pszOut++ = hexArray[bchar >> 4];
                bchar = hexArray[bchar & 0x0F];
            }
            *pszOut++ = bchar;
        }
        bchar = 0x80 | (BYTE)(wchar & 0x003F);
        if (bEncode)
        {
            *pszOut++ = '%';
            *pszOut++ = hexArray[bchar >> 4];
            bchar = hexArray[bchar & 0x0F];
        }
        *pszOut++ = bchar;
    }

    return 1;
}

class CProtocol
{
public:
    virtual bool SendCommand(char *lpObject, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength) = 0;
    virtual bool SendCommandAndWait(char *lpObject, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, long nWait) = 0;
    virtual HRESULT URLDownloadToFileHelper(LPCWSTR szURL, LPCWSTR szFileName) = 0;
};

class CWebURL : CProtocol
{
public:

    CWebURL()
    {
        // Initialize Winsock
        int nWSAStartup = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
        if (nWSAStartup != 0)
        {
char strBuffer[1024];
sprintf_s(strBuffer, "NetworkCommunication: WinSock Initialization.  WSAStartup Failed: %d\r\n", nWSAStartup);
ATLTRACE(strBuffer);
        }
    }

    ~CWebURL()
    {
        // Cleanup WinSock
        WSACleanup();
    }

    ///////////////////////////////////
    // SendCommand
    //
    bool SendCommand(char *lpObject, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength)
    {
        return SendHTTPCommand(lpObject, true, true, lpData, nDataLength, lpHeader, nHeaderLength, 0);
    }

    ///////////////////////////////////
    // SendCommandAndWait
    //
    bool SendCommandAndWait(char *lpObject, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, long nWait)
    {
        return SendHTTPCommandAndWait(lpObject, true, true, lpData, nDataLength, lpHeader, nHeaderLength, NULL, 0, nWait);
    }

    ///////////////////////////////////
    // SendCommand
    //
    //        bRemoveHeader - specifies whether to include the HTTP header in the lpData package or not
    //
    bool SendCommand(char *lpObject, bool bRemoveHeader, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength)
    {
        return SendHTTPCommand(lpObject, true, bRemoveHeader, lpData, nDataLength, lpHeader, nHeaderLength, 0);
    }

    ///////////////////////////////////
    // SendCommandAndWait
    //
    //        bRemoveHeader - specifies whether to include the HTTP header in the lpData package or not
    //
    bool SendCommandAndWait(char *lpObject, bool bRemoveHeader, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, long nWait)
    {
        return SendHTTPCommandAndWait(lpObject, true, bRemoveHeader, lpData, nDataLength, lpHeader, nHeaderLength, NULL, 0, nWait);
    }

    ///////////////////////////////////
    // URLDownloadToFileHelper
    //
    // Attempt #1: CopyFile
    // Attempt #2: DirectTCP
    // Attempt #3: DirectShow
    // Attempt #4: URLMon: URLDownloadToFile
    //
    HRESULT URLDownloadToFileHelper(LPCWSTR szURLSource, LPCWSTR szFileNameTarget)
    {
        HRESULT hr = S_OK;
        BOOL bReturn = FALSE;
        HANDLE hFile = INVALID_HANDLE_VALUE;
        char *lpHeader = NULL;
        DWORD dwFileSize = INVALID_FILE_SIZE;

#ifdef DEBUG
        DWORD dwStart = 0, dwDiff = 0;
#endif

#ifdef _UNICODE
        PWCHAR pwszUnicodeUrl = NULL;
        PCHAR pszUtf8Url = NULL;
#endif

        CHK(szURLSource != NULL);
        CHK(szFileNameTarget != NULL);

//-Cache File
//-Get Size from HTTP Request or Wininnet or if copy file then getattributes
//-Delete cache file at end regardless
        DWORD dwFileNameSize = 0;

        // Make copy of filename and temp to name to provide caching before updating actual file
        int nFileNameSize = wcslen(szFileNameTarget) + 64;
        LPWSTR lpFileNameTargetCache = new WCHAR[nFileNameSize];
        if (!lpFileNameTargetCache)
        {
            LOG(TEXT("%S: ERROR %d@%S - DirectTCP: Run out of heap"), __FUNCTION__, __LINE__, __FILE__);
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        // NOTE: Writing to the root drive because there is a performance issue if the destination is \\harddrive where using the \\harddrive for the temp file is incredibly slow
        //          On CEPC 1KB Frames
        //              Writing to Root:            42 Mbits/second
        //              Writing to \\HardDrive:     0.88 MBits/second
        //
        //          On CEPC 1MB Frames
        //              Writing to Root:            42 Mbits/second
        //              Writing to \\HardDrive:     0.88 MBits/second
        //
//        swprintf_s(lpFileNameTargetCache, nFileNameSize, L"\\%ld.tmp", GetTickCount());      // Not worried about exceeding MAX_PATH because writing all files to Root on the embedded device
        swprintf_s(lpFileNameTargetCache, nFileNameSize, L"%s.%ld.tmp", szFileNameTarget, GetTickCount());      // Not worried about exceeding MAX_PATH because writing all files to Root on the embedded device

        // Check if Target File Exists
        DWORD dwFileAttributes = GetFileAttributes(szFileNameTarget);
        if (INVALID_FILE_ATTRIBUTES != dwFileAttributes)
        {
            bReturn = DeleteFile(szFileNameTarget);         // Make sure file is deleted

            dwFileAttributes = GetFileAttributes(szFileNameTarget);
            if (INVALID_FILE_ATTRIBUTES != dwFileAttributes || !bReturn)
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to remove previous file before downloading %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szFileNameTarget, hr, GetLastError());
                hr = E_FAIL;
                goto cleanup;
            }
        }

        // If Source File not http:// (search for ://) then check if Exists
        bool bHTTPSource = true;
        if (!wcsstr(szURLSource, L"://"))
        {
            bHTTPSource = false;
            dwFileAttributes = GetFileAttributes(szURLSource);
            if (INVALID_FILE_ATTRIBUTES == dwFileAttributes)
            {
                LOG(TEXT("%S: ERROR %d@%S - Source file is local or UNC.  Failed to find file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szURLSource, hr, GetLastError());

                goto urlmon;        // Attempt to download with URLMon
            }
        }

        // Check valid target location cache
        hFile = CreateFile(lpFileNameTargetCache,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            LOG(TEXT("%S: ERROR %d@%S - Check Target - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
            hr = E_FAIL;
            goto cleanup;
        }
        CHK_BOOL(CloseHandle(hFile), E_FAIL);
        hFile = INVALID_HANDLE_VALUE;

        //*****************************
        // Attempt #1: CopyFile
        //*****************************
        if (!bHTTPSource)
        {
            hFile = CreateFile(szURLSource,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                LOG(TEXT("%S: ERROR %d@%S - Check Source - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szURLSource, hr, GetLastError());

                goto urlmon;        // Attempt to download with URLMon
            }

            // Make sure source file exists
            dwFileSize = GetFileSize(hFile, NULL);        // Max Value 2 to the 32nd power minus 1 = 4294967295 = 4 GBs

            CHK_BOOL(CloseHandle(hFile), E_FAIL);
            hFile = INVALID_HANDLE_VALUE;

#ifdef DEBUG
            dwStart = ::GetTickCount();
#endif

            bReturn = ::CopyFile(szURLSource, lpFileNameTargetCache, false);

#ifdef DEBUG
            dwDiff = ::GetTickCount() - dwStart;
            LOG(TEXT("CopyFile Download of Source: %s Target: %s took %ld seconds to complete."), szURLSource, lpFileNameTargetCache, dwDiff / 1000);
#endif

            dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
            if (bReturn && INVALID_FILE_ATTRIBUTES != dwFileAttributes)
            {
                hFile = CreateFile(lpFileNameTargetCache,
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL);
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    LOG(TEXT("%S: ERROR %d@%S - Check Source Cache - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                    hr = E_FAIL;
                    goto cleanup;
                }

                // Make sure source file exists
                DWORD dwFileSizeAfter = INVALID_FILE_SIZE;
                dwFileSizeAfter = GetFileSize(hFile, NULL);        // Max Value 2 to the 32nd power minus 1 = 4294967295 = 4 GBs

                CHK_BOOL(CloseHandle(hFile), E_FAIL);
                hFile = INVALID_HANDLE_VALUE;
                if (dwFileSize == INVALID_FILE_SIZE)                       // Issue we getting original file size so use local cache file size
                    dwFileSize = dwFileSizeAfter;
                if (dwFileSize != INVALID_FILE_SIZE && dwFileSizeAfter != INVALID_FILE_SIZE && dwFileSize == dwFileSizeAfter)
                {
                    if (::MoveFile(lpFileNameTargetCache, szFileNameTarget))
                    {
                        hr = S_OK;
                        goto cleanup;
                    }
                }
            }

            // CopyFile Failed
            hr = HRESULT_FROM_WIN32(GetLastError());
            LOG(TEXT("%S: WARNING %d@%S - unable to use CopyFile for source: %s target %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szURLSource, lpFileNameTargetCache, hr, GetLastError());
        }

        //*****************************
        // Attempt #2: DirectTCP
        //*****************************
        if (bHTTPSource)
        {
            hFile = CreateFile(lpFileNameTargetCache,
                                    GENERIC_WRITE,
                                    0,                  // Disable Sharing
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                    NULL);

            if (hFile == INVALID_HANDLE_VALUE)
            {
                LOG(TEXT("%S: ERROR %d@%S - DirectTCP - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                hr = E_FAIL;
                goto cleanup;
            }

#ifdef DEBUG
            dwStart = ::GetTickCount();
#endif

            bool bSendReturn = true;

#ifdef _UNICODE
            pszUtf8Url = new CHAR[MAX_PATH * 6];
            if (!pszUtf8Url)
            {
                LOG(TEXT("%S: ERROR %d@%S - DirectTCP: Run out of heap"), __FUNCTION__, __LINE__, __FILE__);
                hr = E_OUTOFMEMORY;
                goto cleanup;
            }

            memset(pszUtf8Url, 0, MAX_PATH * 6);

            // It seems that the URLDownloadToFile can not handle
            // international character in URL correctly. So we do
            // it ourselves
            if (0 == ConvertUnicodeToUtf8(szURLSource,
                                          wcslen(szURLSource),
                                          (LPBYTE) pszUtf8Url,
                                          MAX_PATH * 6,
                                          TRUE))
            {
                LOG(TEXT("%S: ERROR %d@%S - DirectTCP: ConvertUnicodeToUtf8() failed"), __FUNCTION__, __LINE__, __FILE__);
                hr = E_INVALIDARG;
                goto cleanup;
            }

            lpHeader = new CHAR[2048];
            if (!lpHeader)
            {
                LOG(TEXT("%S: ERROR %d@%S - DirectTCP: Run out of heap"), __FUNCTION__, __LINE__, __FILE__);
                hr = E_OUTOFMEMORY;
                goto cleanup;
            }
            memset(lpHeader, 0, 2048);

            bSendReturn = SendHTTPCommandAndWait(pszUtf8Url, true, true, NULL, 0, lpHeader, 2048, hFile, 64 * 1024, HTTPSENDWAIT * 5);     // 10 Minutes
#else
            bSendReturn = SendHTTPCommandAndWait(szURLSource, true, true, NULL, 0, lpHeader, 2048, hFile, 64 * 1024, HTTPSENDWAIT * 5);
#endif

            if (!bSendReturn)
            {
                LOG(TEXT("%S: ERROR %d@%S - SendHTTPCommandAndWait - failed to properly receive file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
            }

#ifdef DEBUG
            dwDiff = ::GetTickCount() - dwStart;
            LOG(TEXT("DirectTCP Download of Source: %s Target: %s took %ld seconds to complete."), szURLSource, lpFileNameTargetCache, dwDiff / 1000);
#endif

            bReturn = CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;

            // Check for Valid File
            dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
            if (bSendReturn && bReturn && dwFileAttributes != INVALID_FILE_ATTRIBUTES)
            {
                hFile = CreateFile(lpFileNameTargetCache,
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL);
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    LOG(TEXT("%S: ERROR %d@%S - Check Source Cache - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                    hr = E_FAIL;
                    goto cleanup;
                }

                DWORD dwFileSizeAfter = GetFileSize(hFile, NULL);               // Max Value 2 to the 32nd power minus 1 = 4294967295 = 4 GBs
                CHK_BOOL(CloseHandle(hFile), E_FAIL);
                hFile = INVALID_HANDLE_VALUE;

                if (dwFileSizeAfter != INVALID_FILE_SIZE && lpHeader)                       // Issue we getting original file size so use local cache file size
                {
                    char strLength[64];
                    ::_ltoa((long)dwFileSizeAfter, &strLength[0], 10);
                    char *lpContentLength = strstr(lpHeader, &strLength[0]);
                    if (lpContentLength)       // Check if length match found in header
                    {
                        if (::MoveFile(lpFileNameTargetCache, szFileNameTarget))
                        {
                            hr = S_OK;
                            goto cleanup;
                        }
                    }
                }
            }

            LOG(TEXT("%S: ERROR %d@%S - failed to open downloaded file %s via DirectTCP to Target: %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szURLSource, szFileNameTarget, hr, GetLastError());
        }

        //*****************************
        // Attempt #3: HTTP Source Filter, Buffering Filter, and File Sink
        //*****************************
/*        if (bHTTPSource)
        {
            hFile = CreateFile(lpFileNameTargetCache,
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    CREATE_ALWAYS,         // Create always as this point because file was potentially partial written
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);

            if (hFile == INVALID_HANDLE_VALUE)
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                hr = E_FAIL;
                goto cleanup;
            }

#ifdef DEBUG
            dwStart = ::GetTickCount();
#endif

//            TBD

#ifdef DEBUG
            dwDiff = ::GetTickCount() - dwStart;
            LOG(TEXT("DirectShow Download of Source: %s Target: %s took %ld seconds to complete."), szURLSource, lpFileNameTargetCache, dwDiff / 1000);
#endif

            bReturn = CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;

            dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
            if (bReturn && SUCCEEDED(hr) && dwFileAttributes != INVALID_FILE_ATTRIBUTES)
            {
                if (::MoveFile(lpFileNameTargetCache, szFileNameTarget))
                {
                    hr = S_OK;
                    goto cleanup;
                }
            }

            LOG(TEXT("%S: ERROR %d@%S - failed to open downloaded file %s via DirectShow: %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, szURLSource, lpFileNameTargetCache, hr, GetLastError());
        }
*/
        //*****************************
        // Attempt #4: URLMon
        //*****************************
urlmon:

        // Check if Target File Exists
        dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
        if (INVALID_FILE_ATTRIBUTES != dwFileAttributes)
        {
            bReturn = DeleteFile(lpFileNameTargetCache);         // Make sure file is deleted

            dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
            if (INVALID_FILE_ATTRIBUTES != dwFileAttributes || !bReturn)
            {
                LOG(TEXT("%S: ERROR %d@%S - URLMon - failed to remove previous file before downloading %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                hr = E_FAIL;
                goto cleanup;
            }
        }

#ifdef DEBUG
        dwStart = ::GetTickCount();
#endif

        // Try using URLMON to download the file
#ifdef _UNICODE
        if (!pszUtf8Url)
        {
            pszUtf8Url = new CHAR[MAX_PATH * 6];
            if (!pszUtf8Url)
            {
                LOG(TEXT("%S: ERROR %d@%S - URLMon: Run out of heap"), __FUNCTION__, __LINE__, __FILE__);
                hr = E_OUTOFMEMORY;
                goto cleanup;
            }

            memset(pszUtf8Url, 0, MAX_PATH * 6);

            // It seems that the URLDownloadToFile can not handle
            // international character in URL correctly. So we do
            // it ourselves
            if (0 == ConvertUnicodeToUtf8(szURLSource,
                                          wcslen(szURLSource),
                                          (LPBYTE) pszUtf8Url,
                                          MAX_PATH * 6,
                                          TRUE))
            {
                LOG(TEXT("%S: ERROR %d@%S - URLMon: ConvertUnicodeToUtf8() failed"), __FUNCTION__, __LINE__, __FILE__);
                hr = E_INVALIDARG;
                goto cleanup;
            }
        }

        pwszUnicodeUrl = new WCHAR[MAX_PATH * 6];
        if (!pwszUnicodeUrl)
        {
            LOG(TEXT("%S: ERROR %d@%S - URLMon: Run out of heap"), __FUNCTION__, __LINE__, __FILE__);
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        memset(pwszUnicodeUrl, 0, MAX_PATH * 6 * sizeof (WCHAR));
        if (NULL == MultiByteToWideChar(CP_UTF8,
                            0,
                            pszUtf8Url,
                            -1,
                            pwszUnicodeUrl,
                            MAX_PATH * 6))
        {
            LOG(TEXT("%S: ERROR %d@%S - URLMON: MultiByteToWideChar() failed"), __FUNCTION__, __LINE__, __FILE__);
            hr = E_INVALIDARG;
            goto cleanup;
        }

        hr = URLDownloadToFile(NULL, pwszUnicodeUrl, lpFileNameTargetCache, 0, NULL);
#else
        hr = URLDownloadToFile(NULL, szURLSource, lpFileNameTargetCache, 0, NULL);
#endif

#ifdef DEBUG
        dwDiff = ::GetTickCount() - dwStart;
        LOG(TEXT("URLMon URLDownloadtoFile of Source: %s Target: %s took %ld seconds to complete."), szURLSource, lpFileNameTargetCache, dwDiff / 1000);
#endif

        dwFileAttributes = GetFileAttributes(lpFileNameTargetCache);
        if (SUCCEEDED(hr) && dwFileAttributes != INVALID_FILE_ATTRIBUTES)
        {
            hFile = CreateFile(lpFileNameTargetCache,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                LOG(TEXT("%S: ERROR %d@%S - URLDownloadtoFile - failed to open downloaded file %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());
                hr = E_FAIL;
                goto cleanup;
            }

            DWORD dwFileSizeAfter = GetFileSize(hFile, NULL);               // Max Value 2 to the 32nd power minus 1 = 4294967295 = 4 GBs
            CHK_BOOL(CloseHandle(hFile), E_FAIL);
            hFile = INVALID_HANDLE_VALUE;

            bool bUpdateFile = true;

            if (dwFileSize != INVALID_FILE_SIZE && dwFileSizeAfter != INVALID_FILE_SIZE && dwFileSize == dwFileSizeAfter)
                bUpdateFile = false;
            if (lpHeader)
            {
                char strLength[64];
                ::_ltoa((long)dwFileSizeAfter, &strLength[0], 10);
                char *lpContentLength = strstr(lpHeader, &strLength[0]);
                if (!lpContentLength)                                       // Check if length match found in header
                    bUpdateFile = false;
            }

            if (bUpdateFile)
            {
                if (::MoveFile(lpFileNameTargetCache, szFileNameTarget))
                {
                    hr = S_OK;
                    goto cleanup;
                }
            }
        }

        LOG(TEXT("%S: ERROR %d@%S - failed to open downloaded file via URLMon: %s error %ld GetLastError: %ld"), __FUNCTION__, __LINE__, __FILE__, lpFileNameTargetCache, hr, GetLastError());

cleanup:                // No Chk in Cleanup otherwise recursive loop potential

#ifdef _UNICODE
        if (lpHeader)
        {
            delete [] lpHeader;
            lpHeader = NULL;
        }
        if (pszUtf8Url)
        {
            delete [] pszUtf8Url;
            pszUtf8Url = NULL;
        }
        if (pwszUnicodeUrl)
        {
            delete [] pwszUnicodeUrl;
            pwszUnicodeUrl = NULL;
        }
#endif

        if (INVALID_HANDLE_VALUE != hFile)
        {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }

        // Check for Valid Target File
        dwFileAttributes = GetFileAttributes(szFileNameTarget);
        if (FAILED(hr) || dwFileAttributes == INVALID_FILE_ATTRIBUTES)
            return E_FAIL;

        return hr;
    }

    WSADATA m_wsaData;
};