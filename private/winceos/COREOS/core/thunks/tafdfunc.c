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
#include <afdfunc.h>
#include <raserror.h>

// SDK functions
DWORD xxx_RasDial(LPRASDIALEXTENSIONS dialExtensions, LPTSTR phoneBookPath,
                  LPRASDIALPARAMS rasDialParam, DWORD NotifierType,
                  LPVOID notifier, LPHRASCONN pRasConn)
{
    return RasDial(dialExtensions, sizeof(RASDIALEXTENSIONS), phoneBookPath, 
                    rasDialParam, sizeof(RASDIALPARAMS),
                    NotifierType, (DWORD)notifier, pRasConn);
}

DWORD xxx_RasHangup(HRASCONN Session)
{
    return RasHangup(Session);
}

DWORD xxx_RasEnumEntries(LPWSTR Reserved, LPWSTR lpszPhoneBookPath,
                         LPRASENTRYNAME lprasentryname, LPDWORD lpcb,
                         LPDWORD lpcEntries)
{
    return RasEnumEntries(Reserved, lpszPhoneBookPath, lprasentryname,
        lpcb ? *lpcb : 0, lpcb, lpcEntries);
}

DWORD xxx_RasGetEntryDialParams(LPWSTR lpszPhoneBook,
                                LPRASDIALPARAMS lpRasDialParams,
                                LPBOOL lpfPassword)
{
    return RasGetEntryDialParams(lpszPhoneBook, lpRasDialParams,
                                  sizeof(RASDIALPARAMS), lpfPassword);
}

DWORD xxx_RasSetEntryDialParams(LPWSTR lpszPhoneBook,
                                LPRASDIALPARAMS lpRasDialParams,
                                BOOL fRemovePassword)
{
    return RasSetEntryDialParams(lpszPhoneBook, lpRasDialParams,
                                  sizeof(RASDIALPARAMS), fRemovePassword);
}

DWORD xxx_RasGetEntryProperties(LPWSTR lpszPhoneBook, LPWSTR szEntry,
                                LPBYTE lpbEntry, LPDWORD lpdwEntrySize,
                                LPBYTE lpb, LPDWORD lpdwSize)
{
    return RasGetEntryProperties(lpszPhoneBook, szEntry, lpbEntry, lpdwEntrySize ? *lpdwEntrySize : 0,
        lpdwEntrySize, lpb, lpdwSize ? *lpdwSize : 0, lpdwSize);
}

DWORD xxx_RasSetEntryProperties(LPWSTR lpszPhoneBook, LPWSTR szEntry,
                                LPBYTE lpbEntry, DWORD dwEntrySize,
                                LPBYTE lpb, DWORD dwSize)
{
    return RasSetEntryProperties(lpszPhoneBook, szEntry, lpbEntry,
                                  dwEntrySize, lpb, dwSize);
}

DWORD xxx_RasValidateEntryName(LPWSTR lpszPhonebook, LPWSTR lpszEntry)
{
    return RasValidateEntryName(lpszPhonebook, lpszEntry);
}

DWORD xxx_RasDeleteEntry(LPWSTR lpszPhonebook, LPWSTR lpszEntry)
{
    return RasDeleteEntry(lpszPhonebook, lpszEntry);
}

DWORD xxx_RasRenameEntry(LPWSTR lpszPhonebook, LPWSTR lpszOldEntry,
                         LPWSTR lpszNewEntry)
{
    return RasRenameEntry(lpszPhonebook, lpszOldEntry, lpszNewEntry);
}

DWORD xxx_RasEnumConnections(LPRASCONN lprasconn, LPDWORD lpcb,
                             LPDWORD lpcConnections)
{
    return RasEnumConnections(lprasconn, lpcb ? *lpcb : 0, lpcb, lpcConnections);
}

DWORD xxx_RasGetConnectStatus(HRASCONN rasconn,
                              LPRASCONNSTATUS lprasconnstatus)
{
    return RasGetConnectStatus(rasconn, lprasconnstatus, sizeof(RASCONNSTATUS));
}

DWORD xxx_RasGetEntryDevConfig(LPCTSTR szPhonebook, LPCTSTR szEntry,
                               LPDWORD pdwDeviceID, LPDWORD pdwSize,
                               LPVARSTRING pDeviceConfig)
{
    return RasGetEntryDevConfig(szPhonebook, szEntry, pdwDeviceID,
                                 pdwSize, pDeviceConfig,
                                 pdwSize ? *pdwSize : 0);
}

DWORD xxx_RasSetEntryDevConfig(LPCTSTR szPhonebook, LPCTSTR szEntry,
                               DWORD dwDeviceID, LPVARSTRING lpDeviceConfig)
{
    return RasSetEntryDevConfig(szPhonebook, szEntry, dwDeviceID,
                                 lpDeviceConfig, 
                                 lpDeviceConfig ? (sizeof(VARSTRING) + lpDeviceConfig->dwStringSize) : 0);
}

DWORD xxx_RasIOControl(LPVOID hRasConn, DWORD dwCode, PBYTE pBufIn,
                       DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
                       PDWORD pdwActualOut)
{
    return RasIOControl(hRasConn, dwCode, pBufIn, dwLenIn, pBufOut,
                         dwLenOut, pdwActualOut);
}

DWORD RasEnumDevicesW (LPRASDEVINFOW pRasDevInfo, LPDWORD lpcb,
                       LPDWORD lpcDevices)
{
    return RasIOControl (NULL, RASCNTL_ENUMDEV, (PBYTE)pRasDevInfo, 0, (PBYTE)lpcb,
                         0, (PDWORD)lpcDevices);
}


DWORD RasGetProjectionInfoW(HRASCONN hrasconn, RASPROJECTION rasprojection,
                            LPVOID lpprojection, LPDWORD lpcb)
{
    return RasIOControl(hrasconn,RASCNTL_GETPROJINFO,lpprojection,rasprojection,NULL,0,lpcb);
}

DWORD RasGetLinkStatistics (HRASCONN hRasConn, DWORD dwSubEntry, PRAS_STATS lpStatistics)
{
    DWORD   dwRetLen;
    return RasIOControl(hRasConn, RASCNTL_STATISTICS, NULL, 0, (PBYTE)lpStatistics,
                        sizeof(RAS_STATS), &dwRetLen);
}

DWORD APIENTRY RasGetDispPhoneNumW(LPCWSTR szPhonebook,
                                   LPCWSTR szEntry,
                                   LPWSTR szPhoneNum,
                                   DWORD dwPhoneNumLen)
{
    DWORD   dwEntryLen;
    DWORD   dwRetLen;

    if (NULL != szPhonebook) {
        return ERROR_CANNOT_OPEN_PHONEBOOK;
    }
    if ((NULL == szEntry) || (NULL == szPhoneNum) || (0 == dwPhoneNumLen)) {
        return ERROR_INVALID_PARAMETER;
    }
    
    dwEntryLen = sizeof(WCHAR)*(_tcslen(szEntry)+1);
    return RasIOControl (NULL, RASCNTL_GETDISPPHONE, (LPBYTE)szEntry, dwEntryLen,
                         (LPBYTE)szPhoneNum, dwPhoneNumLen, &dwRetLen);
}

DWORD 
APIENTRY RasDevConfigDialogEditW (LPCWSTR szDeviceName, LPCWSTR szDeviceType,
                                        HWND hWndOwner, LPVOID lpDeviceConfigIn,
                                        DWORD dwSize, LPVARSTRING lpDeviceConfigOut)
{
    PRASCNTL_DEVCFGDLGED    pDevCfgDlgEdit;
    DWORD                   dwResult;

    if ((NULL == szDeviceName) || (NULL == szDeviceType)) {
        return ERROR_INVALID_PARAMETER;
    }
    
    if ((NULL == lpDeviceConfigIn) && (0 != dwSize)) {
        return ERROR_INVALID_PARAMETER;
    }
    
    pDevCfgDlgEdit = LocalAlloc (LPTR, sizeof(RASCNTL_DEVCFGDLGED) + dwSize);
    if (pDevCfgDlgEdit) {
        VERIFY (SUCCEEDED (StringCchCopyW (pDevCfgDlgEdit->szDeviceName, RAS_MaxDeviceName+1, szDeviceName)));
        VERIFY (SUCCEEDED (StringCchCopyW (pDevCfgDlgEdit->szDeviceType, RAS_MaxDeviceType+1, szDeviceType)));
        pDevCfgDlgEdit->hWndOwner = hWndOwner;
        pDevCfgDlgEdit->dwSize = dwSize;
        memcpy (pDevCfgDlgEdit->DataBuf, lpDeviceConfigIn, dwSize);

        dwResult = RasIOControl (NULL, RASCNTL_DEVCONFIGDIALOGEDIT, (PBYTE)pDevCfgDlgEdit,
                             sizeof(RASCNTL_DEVCFGDLGED) + dwSize,
                             (PBYTE)lpDeviceConfigOut, 0, NULL);
        LocalFree(pDevCfgDlgEdit);
        return dwResult;
    } else {
        return ERROR_ALLOCATING_MEMORY;
    }
}

DWORD
RasGetEapUserData(
    IN      HANDLE  hToken,           // access token for user 
    IN      LPCTSTR pszPhonebook,    // path to phone book to use 
    IN      LPCTSTR pszEntry,        // name of entry in phone book 
        OUT BYTE    *pbEapData,         // retrieved data for the user 
    IN  OUT DWORD   *pdwSizeofEapData  // size of retrieved data 
)
{
    DWORD                           dwResult;   
    
    if (NULL != pszPhonebook)
        return ERROR_CANNOT_OPEN_PHONEBOOK;

    if (NULL == pszEntry)
        return ERROR_INVALID_PARAMETER;

    dwResult = RasIOControl (NULL, RASCNTL_EAP_GET_USER_DATA, (PBYTE)pszEntry,
                             (wcslen(pszEntry) + 1) * sizeof(WCHAR),
                             pbEapData, *pdwSizeofEapData, pdwSizeofEapData);

    return dwResult;
}

DWORD
RasSetEapUserData(
    IN      HANDLE  hToken,           // access token for user 
    IN      LPCTSTR pszPhonebook,    // path to phone book to use 
    IN      LPCTSTR pszEntry,        // name of entry in phone book 
    IN      BYTE    *pbEapData,      // data for the user 
    IN      DWORD   dwSizeofEapData  // size of data 
)
{
    DWORD                           dwResult;   
    
    if (NULL != pszPhonebook)
        return ERROR_CANNOT_OPEN_PHONEBOOK;

    if (NULL == pszEntry)
        return ERROR_INVALID_PARAMETER;

    dwResult = RasIOControl (NULL, RASCNTL_EAP_SET_USER_DATA, (PBYTE)pszEntry,
                             (wcslen(pszEntry) + 1) * sizeof(WCHAR),
                             pbEapData, dwSizeofEapData, NULL);

    return dwResult;
}

DWORD
RasGetEapConnectionData(
    IN      LPCTSTR pszPhonebook,    // path to phone book to use 
    IN      LPCTSTR pszEntry,        // name of entry in phone book 
        OUT BYTE    *pbEapData,         // retrieved data for the user 
    IN  OUT DWORD   *pdwSizeofEapData  // size of retrieved data 
)
{
    DWORD                           dwResult;   
    
    if (NULL != pszPhonebook)
        return ERROR_CANNOT_OPEN_PHONEBOOK;

    if (NULL == pszEntry)
        return ERROR_INVALID_PARAMETER;

    dwResult = RasIOControl (NULL, RASCNTL_EAP_GET_CONNECTION_DATA, (PBYTE)pszEntry,
                             (wcslen(pszEntry) + 1) * sizeof(WCHAR),
                             pbEapData, *pdwSizeofEapData, pdwSizeofEapData);

    return dwResult;
}

DWORD
RasSetEapConnectionData(
    IN      LPCTSTR pszPhonebook,    // path to phone book to use 
    IN      LPCTSTR pszEntry,        // name of entry in phone book 
    IN      BYTE    *pbEapData,      // data for the user 
    IN      DWORD   dwSizeofEapData  // size of data 
)
{
    DWORD                           dwResult;   
    
    if (NULL != pszPhonebook)
        return ERROR_CANNOT_OPEN_PHONEBOOK;

    if (NULL == pszEntry)
        return ERROR_INVALID_PARAMETER;

    dwResult = RasIOControl (NULL, RASCNTL_EAP_SET_CONNECTION_DATA, (PBYTE)pszEntry,
                             (wcslen(pszEntry) + 1) * sizeof(WCHAR),
                             pbEapData, dwSizeofEapData, NULL);

    return dwResult;
}


SOCKHAND xxx_AFDSocket(DWORD AddressFamily, DWORD SocketType, DWORD Protocol,
        DWORD CatId, GUID *pProvId)
{
    return AFDSocket(AddressFamily, SocketType, Protocol, CatId, pProvId);
}
/*
#ifndef _SOCK_LIST_DEF
#define _SOCK_LIST_DEF

typedef struct _SOCK_LIST {
    DWORD               hSocket;            // handle to socket passed in from dll layer
    struct _SOCK_INFO   *Socket;             // the target socket
    DWORD               EventMask;          // events the client is interested in
    DWORD               Context;            // user-defined context value (handle?)
    struct _SOCK_INFO   *pSock;         // ptr to real socket
                // no need to confuse people Context is just a SOCKET
    HANDLE              hHandleLock;
    
} SOCK_LIST, *LPSOCK_LIST, *PSOCK_LIST;

#endif  // _SOCK_LIST_DEF
*/
DWORD xxx_AFDSelect(UINT ReadCount, LPSOCK_LIST ReadList, UINT WriteCount,
                    LPSOCK_LIST WriteList, UINT ExceptCount,
                    LPSOCK_LIST ExceptList, const struct timeval *timeout)
{
    return AFDSelect(ReadCount, ReadList, WriteCount, WriteList,
                     ExceptCount, ExceptList, timeout);
}


/*
// private oak functions
SOCKHAND xxx_AFDSocket(DWORD AddressFamily, DWORD SocketType, DWORD Protocol)
{
    return AFDSocket(AddressFamily, SocketType, Protocol);
}

DWORD xxx_AFDControl(DWORD Protocol, DWORD Action, LPVOID InputBuffer,
                     LPDWORD InputBufferLength, LPVOID OutputBuffer,
                     LPDWORD OutputBufferLength)
{
    return AFDControl(Protocol, Action, InputBuffer, InputBufferLength,
                      OutputBuffer, OutputBufferLength);
}

DWORD xxx_AFDEnumProtocolsW(LPDWORD lpiProtocols, LPVOID lpProtocolBuffer,
                            LPDWORD lpdwBufferLength)
{
    return AFDEnumProtocolsW(lpiProtocols, lpProtocolBuffer,
                              lpdwBufferLength);
}

DWORD xxx_AFDGetHostentByAttr(LPSOCK_THREAD pThread, LPSTR Name,
                              LPBYTE Address)
{
    return AFDGetHostentByAttr(pThread, Name, Address);
}

DWORD xxx_AFDAddIPHostent(LPWSTR Hostname, DWORD IpAddr, LPWSTR Aliases,
                          LPFILETIME lpFileTime)
{
    return AFDAddIPHostent(Hostname, IpAddr, Aliases, lpFileTime);
}

DWORD xxx_AFDAddResolver(DWORD CONTEXT, BOOL fDNS, BOOL fDelete, DWORD IpAddr)
{
    return AFDAddResolver(CONTEXT, fDNS, fDelete, IpAddr);
}

DWORD xxx_NETbios(DWORD x1, DWORD dwOpCode, PVOID pNCB,
                  DWORD cBuf1, PBYTE pBuf1, DWORD cBuf2, PDWORD pBuf2)
{
    return NETbios(x1, dwOpCode, pNCB, cBuf1, pBuf1, cBuf2, pBuf2);
}

*/
