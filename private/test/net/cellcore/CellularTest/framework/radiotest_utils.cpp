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
#include <cmnet.h>
#include "radiotest.h"
#include "radiotest_utils.h"

#define DELETE_CONNECTION_DELAY     60000
#define DTPT_CONNECTION_NAME        L"DTPT"
#define DTPT_CONNECTION_DESCRIPTION L"DTPT over USB Serial"

CM_CONNECTION_CONFIG *g_pDTPTConn = NULL;
DWORD g_cbDTPTConn = 0;


// Convert File Time into an 64-bit integer
UINT64 FileTimeToUINT64(const FILETIME *lpFileTime)
{
    _ULARGE_INTEGER uliFileTime;

    if (!lpFileTime)
    {
        return 0;
    }

    uliFileTime.LowPart = lpFileTime->dwLowDateTime;
    uliFileTime.HighPart = lpFileTime->dwHighDateTime;

    return uliFileTime.QuadPart;
}

// Convert an 64-bit integer into a File Time
void UINT64ToFileTime(const UINT64 time64, FILETIME* lpFileTime)
{
    _ULARGE_INTEGER uliFileTime;

    if (!lpFileTime)
    {
        return;
    }

    uliFileTime.QuadPart = time64;

    lpFileTime->dwLowDateTime = uliFileTime.LowPart;
    lpFileTime->dwHighDateTime = uliFileTime.HighPart;
}

// Get the system time and then return its 64-bit representation
UINT64 GetSystemTimeUINT64(void)
{
    SYSTEMTIME sysTime;
    FILETIME   fileTime;

    GetSystemTime(&sysTime);
    SystemTimeToFileTime((const SYSTEMTIME*)&sysTime, &fileTime);
    return FileTimeToUINT64(&fileTime);
}

// Convert an 64-bit integer into system time
void UINT64ToSystemTime(const UINT64 time64, SYSTEMTIME *sysTime)
{
    FILETIME fileTime;

    if (!sysTime)
    {
        return;
    }

    UINT64ToFileTime(time64, &fileTime);
    FileTimeToSystemTime((const FILETIME*)&fileTime, sysTime);
}

// Get the local time and then return its 64-bit representation
UINT64 GetLocalTimeUINT64(void)
{
    SYSTEMTIME localTime;
    FILETIME   fileTime;

    GetLocalTime(&localTime);
    SystemTimeToFileTime((const SYSTEMTIME*)&localTime, &fileTime);
    return FileTimeToUINT64(&fileTime);
}

// Calculate the number of ticks that has elaspsed
DWORD GetTicksElapsed(const DWORD start)
{
    DWORD now = GetTickCount();

    if (start > now)
    {
        return (0xffffffffLU - start + now + 1);
    }
    else
    {
        return (now - start);
    }
}

// Get OS version and build number
BOOL GetBuildInfo(
    DWORD *pdwMajorOSVer,
    DWORD *pdwMinorOSVer,
    DWORD *pdwOSBuildNum,
    DWORD *pdwROMBuildNum
    )
{
    OSVERSIONINFO osVer = {0};
    if (GetVersionEx(&osVer))
    {
        *pdwMajorOSVer = osVer.dwMajorVersion;
        *pdwMinorOSVer = osVer.dwMinorVersion;
        *pdwOSBuildNum = osVer.dwBuildNumber;

        HINSTANCE hInstance  = NULL;
        FARPROC   pProc      = NULL;
        LPCTSTR   tszLibrary = _T("ossvcs.dll");
        DWORD     dwResource = 72;

        hInstance = LoadLibrary(tszLibrary);

        if (hInstance)
        {
            pProc = GetProcAddress(hInstance, MAKEINTRESOURCE(dwResource)) ;

            if (pProc)
            {
                *pdwROMBuildNum = (*pProc)();
            }

            FreeLibrary(hInstance);
        }
        return TRUE;
    }
    return FALSE;
}

// Get Platform name
BOOL GetPlatformName(LPTSTR tszPlatname, const DWORD dwMaxStrLen)
{
    DWORD dwBytesReturned, dwIn = SPI_GETOEMINFO;

    if (KernelIoControl(
        IOCTL_HAL_GET_DEVICE_INFO,
        &dwIn,
        sizeof(DWORD),
        tszPlatname,
        dwMaxStrLen * sizeof(TCHAR),
        &dwBytesReturned
        ))
    {
        return TRUE;
    }
    return FALSE;
}

BOOL ExitRemoteProcess(LPCTSTR ptszProcName, DWORD dwExitCode)
{
    HANDLE hThlp = NULL;
    PROCESSENTRY32 pe32;
    DWORD dwProcID = 0;
    BOOL fRet = TRUE;

    if (INVALID_HANDLE_VALUE != (hThlp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS|TH32CS_SNAPNOHEAPS, 0)))
    {
        pe32.dwSize = sizeof(pe32);
        if (Process32First(hThlp, &pe32))
        {
            do
            {
                if (!_tcsicmp(ptszProcName, pe32.szExeFile))
                {
                    dwProcID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hThlp, &pe32));
        }
        CloseToolhelp32Snapshot(hThlp);
    }

    if (dwProcID)
    {
        fRet = FALSE;
        HANDLE hProc = OpenProcess(0, FALSE, dwProcID);
        if (hProc)
        {
            fRet = TerminateProcess(hProc, 0);
            CloseHandle(hProc);
        }
    }
    return fRet;    
}

void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[])
{
    int nArgc = 0;
    BOOL fInQuotes, fEndFound = FALSE;
    TCHAR *pCommandLine = szCommandLine;

    if (NULL == pCommandLine || NULL == argc || NULL == argv)
    {
        return;
    }

    for (int i = 0; i < *argc; i++)
    {
        // Clear our quote flag
        fInQuotes = FALSE;

        // Move past and zero out any leading whitespace
        while (*pCommandLine && _istspace(*pCommandLine))
        {
            *(pCommandLine++) = _T('\0');
        }

        // If the next character is a quote, move over it and remember it
        if (_T('\"') == *pCommandLine)
        {
            *(pCommandLine++) = _T('\0');
            fInQuotes = TRUE;
        }

        // Point the current argument to our current location
        argv[i] = pCommandLine;

        // If this argument contains some text, then update our argument count
        if (*pCommandLine)
        {
            nArgc = i + 1;
        }

        // Move over valid text of this argument.
        while (*pCommandLine)
        {
            if (_istspace(*pCommandLine)
                || (fInQuotes && (_T('\"') == *pCommandLine)))
            {
                *(pCommandLine++) = _T('\0');
                break;
            }
            pCommandLine++;
        }

        // If reach end of string break;
        if (!(*pCommandLine))
        {
            break;
        }
    }
    *argc = nArgc;
}

WCHAR ToHexWCHAR(const BYTE bData)
{
    if (bData <= 9 && bData >= 0)
    {
        return L'0' + bData;
    }
    switch (bData)
    {
        case 10:
            return L'A';
        case 11:
            return L'B';
        case 12:
            return L'C';
        case 13:
            return L'D';
        case 14:
            return L'E';
        case 15:
            return L'F';
        default:
            return L'X'; // means error
    }
}

BOOL TerminateSMSApps()
{
    LPWSTR lpwszAppName = L"SmsTransport.exe";
    if (ExitRemoteProcess(lpwszAppName, 0))
    {
        g_pLog->Log(LOG, L"%s has been terminated", lpwszAppName);
        return TRUE;
    }
    else
    {
        g_pLog->Log(LOG, L"%s maybe running ...", lpwszAppName);
        return FALSE;
    }
}

BOOL DeleteConnections()
{
    BOOL fRet = FALSE;
    CM_RESULT cmRes = CMRE_UNEXPECTED;
    CM_CONNECTION_NAME_LIST *pNameList = NULL;
    CM_CONNECTION_CONFIG *pConfig = NULL;
    CM_NOTIFICATIONS_LISTENER_REGISTRATION regListener;
    CM_NOTIFICATIONS_LISTENER_HANDLE hListener = NULL;
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    memset(&regListener, 0, sizeof(regListener));
    regListener.Version = CM_CURRENT_VERSION;
    regListener.cRegistration = 1;
    regListener.Registration[0].NotificationType = CMNT_CONNECTION_CONFIGURATION_DELETED;
    regListener.Registration[0].Connection.Selection = CMST_CONNECTION_ALL;
    regListener.hNotificationsAvailableEvent = hEvent;

    cmRes = CmRegisterNotificationsListener(&regListener, sizeof(regListener), &hListener);
    if (cmRes != CMRE_SUCCESS)
    {
        g_pLog->Log(LOG, L"DeleteConnections: failed to register notification");
    }
    
    DWORD cbBufSize = sizeof(CM_CONNECTION_NAME_LIST);
    BYTE *pBuf = new BYTE[cbBufSize];
    if (!pBuf)
    {
        g_pLog->Log(LOG, L"DeleteConnections: OOM");
        goto Exit;
    }
    pNameList = (CM_CONNECTION_NAME_LIST *)&pBuf[0];
    cmRes = CmEnumConnectionsConfig(pNameList, &cbBufSize);
    if (cmRes == CMRE_INSUFFICIENT_BUFFER)
    {
        delete []pBuf;
        pBuf = new BYTE[cbBufSize];
        if (!pBuf)
        {
            g_pLog->Log(LOG, L"DeleteConnections: OOM");
            goto Exit;
        }
        pNameList = (CM_CONNECTION_NAME_LIST *)&pBuf[0];
        cmRes = CmEnumConnectionsConfig(pNameList, &cbBufSize);
    }
    
    if (cmRes != CMRE_SUCCESS)
    {
        g_pLog->Log(LOG, L"DeleteConnections: CmEnumConnectionsConfig failed with %d", cmRes);
        goto Exit;
    }
    else
    {
        g_pLog->Log(LOG, L"DeleteConnections: %d connections to delete", pNameList->cConnection);
    }

    for (DWORD i = 0 ; i < pNameList->cConnection; ++i) 
    {
        // Save DTPT connection config
        if (wcscmp(pNameList->Connection[i].szName, DTPT_CONNECTION_NAME) == 0)
        {
            DWORD cbConfigSize = sizeof(CM_CONNECTION_CONFIG);
            pConfig = (CM_CONNECTION_CONFIG *) new BYTE[cbConfigSize];
            if (!pConfig)
            {
                g_pLog->Log(LOG, L"DeleteConnections: OOM");
                goto Exit;
            }
            cmRes = CmGetConnectionConfig(pNameList->Connection[i].szName, pConfig, &cbConfigSize);

            if (cmRes == CMRE_INSUFFICIENT_BUFFER)
            {
                delete []((BYTE *)pConfig); // avoid OACR error
                pConfig = (CM_CONNECTION_CONFIG *) new BYTE[cbConfigSize];
                if (!pConfig)
                {
                    g_pLog->Log(LOG, L"DeleteConnections: OOM");
                    goto Exit;
                }
                cmRes = CmGetConnectionConfig(pNameList->Connection[i].szName, pConfig, &cbConfigSize);
            }

            if (cmRes != CMRE_SUCCESS)
            { 
                g_pLog->Log(LOG, L"DeleteConnections: Failed to get connection config for %s", pNameList->Connection[i].szName);
            }
            else
            {
                if (wcscmp(pConfig->szDescription, DTPT_CONNECTION_DESCRIPTION) == 0)
                {
                    if (!g_pDTPTConn)
                    {
                        g_pDTPTConn = pConfig;
                        g_cbDTPTConn = cbConfigSize;
                        pConfig = NULL;
                    }
                }
            }
        }

        cmRes = CmDeleteConnectionConfig(pNameList->Connection[i].szName);
        
        DWORD dwWait = WaitForSingleObject(hEvent, DELETE_CONNECTION_DELAY); 
        if (dwWait == WAIT_OBJECT_0)           
        {         
            CM_NOTIFICATION notification;
            memset(&notification, 0, sizeof(notification));
            notification.Version = CM_CURRENT_VERSION;
            DWORD cbBufSize = sizeof(notification);

            if (CMRE_SUCCESS == CmGetNotification(hListener, &notification, &cbBufSize))
            {
                if (notification.Type == CMNT_CONNECTION_CONFIGURATION_DELETED)                               
                {  
                     g_pLog->Log(LOG, L"DeleteConnections: Get notification DELETED for connection %s", 
                        notification.ConfigUpdate.szConnection);
                     g_pLog->Log(LOG, L"DeleteConnections: CmDeleteConnectionConfig: deleting %s %s with %d."
                              , pNameList->Connection[i].szName
                              , (cmRes == CMRE_SUCCESS) ? L"succeeded" : L"failed"
                              , cmRes);
                }
                else                                
                {
                     g_pLog->Log(LOG, L"DeleteConnections: Received incorrect notification");
                }
            }
            else
            {
                g_pLog->Log(LOG, L"DeleteConnections: GetNotification failed");
            }
        }
        else if (dwWait == WAIT_TIMEOUT)           
        {    
            g_pLog->Log(LOG, L"DeleteConnections: Connection is not yet deleted");
        }
    }
    
    fRet = TRUE;
        
    if (CMRE_SUCCESS != CmUnregisterNotificationsListener(hListener))
    {
        g_pLog->Log(LOG, L"DeleteConnections: Unregister notification listener failed");
    }

Exit:
    if (pConfig)
    {
        delete []((BYTE *)pConfig);
    }
    if (pBuf)
    {
        delete []pBuf;
    }
    if (hEvent)
    {
        CloseHandle(hEvent);
    }
    return fRet;
}


BOOL RestoreDTPTConnection()
{
    BOOL fRet = TRUE;
    
    if (g_pDTPTConn)
    {
        if (CMRE_SUCCESS != CmAddConnectionConfig(DTPT_CONNECTION_NAME, g_pDTPTConn, g_cbDTPTConn))
        {
            g_pLog->Log(LOG, L"RestoreDTPTConnection: Failed to add connection");
            fRet = FALSE;
        }
        delete g_pDTPTConn;
        g_pDTPTConn = NULL;
        g_cbDTPTConn = 0;
    }
    return fRet;
}