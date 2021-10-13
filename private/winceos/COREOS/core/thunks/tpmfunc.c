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

#include <pmfunc.h>
#include <raserror.h>
#include <winsock2.h>
#include <ws2spi.h>

/*
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
        _tcsncpy (pDevCfgDlgEdit->szDeviceName, szDeviceName, RAS_MaxDeviceName);
        _tcsncpy (pDevCfgDlgEdit->szDeviceType, szDeviceType, RAS_MaxDeviceType);
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
*/


DNS_STATUS xxx_AFDDnsQuery_W(PCWSTR pszName, WORD wType, DWORD Options, 
        PIP4_ARRAY aipServers, DWORD cbServers, PDNS_RECORD *ppQueryResults,
        PVOID *pReserved)
{
    return AFDDnsQuery_W(pszName, wType, Options, aipServers, cbServers,
            ppQueryResults, pReserved);
}


void xxx_AfdDNSRecordListFree(PDNS_RECORD pRecordList, DNS_FREE_TYPE FreeType)
{
    AfdDNSRecordListFree(pRecordList, FreeType);
}


DWORD xxx_AFD_UpdateOrModifyRecords(PDNS_RECORD pAddRecords, 
        PDNS_RECORD pDeleteRecords, DWORD Options, HANDLE hContext,
        PIP4_ARRAY pServerList, DWORD cbServerList, PVOID pReserved, BOOL fReplace)
{
    return AFD_UpdateOrModifyRecords(pAddRecords, pDeleteRecords, Options, 
            hContext, pServerList, cbServerList, pReserved, fReplace);
}


SOCKHAND xxx_PMSocket(DWORD AddressFamily, DWORD SocketType, DWORD Protocol,
        DWORD CatId, GUID *pProvId, DWORD cProvId)
{
    return PMSocket(AddressFamily, SocketType, Protocol, CatId, pProvId, cProvId);
}


int xxx_PMSelect(UINT ReadCount, PPMSOCK_LIST ReadList, uint cbReadList,
        UINT WriteCount, PPMSOCK_LIST WriteList, uint cbWriteList,
        UINT ExceptCount, PPMSOCK_LIST ExceptList, uint cbExceptList,
        const struct timeval *timeout) {

    return PMSelect(ReadCount, ReadList, cbReadList, WriteCount,  WriteList,
            cbWriteList, ExceptCount,  ExceptList,  cbExceptList, timeout);

}


int xxx_PMInstallProvider(IN GUID *pProviderId, IN const WCHAR *pszProviderDllPath,
    IN const LPWSAPROTOCOL_INFOW pProtocolInfoList, IN DWORD cbProtocolInfoList,
    IN DWORD NumberOfEntries, IN DWORD Flags)
{
    return PMInstallProvider(pProviderId, 
        pszProviderDllPath, 
        pProtocolInfoList,
        cbProtocolInfoList, NumberOfEntries, Flags);
}


int xxx_PMEnumProtocols(int *pProtocols, WSAPROTOCOL_INFOW *pProtocolBuffer,
    DWORD cbBuffer, DWORD *pBufferLength, DWORD *pFlags, int *pErrno)
{
    return PMEnumProtocols(pProtocols, pProtocolBuffer, cbBuffer,
        pBufferLength, pFlags, pErrno);
}


int xxx_PMFindProvider(IN int af, IN int type, IN int protocol,
    IN DWORD CatalogEntryId, IN DWORD dwFlags, OUT WSAPROTOCOL_INFOW *pProtocolInfo,
    IN DWORD cbProtocolInfo, OUT LPWSTR pszPath, IN DWORD cbPath)
{
    return PMFindProvider(af, type, protocol, CatalogEntryId, dwFlags, 
        pProtocolInfo, cbProtocolInfo, pszPath, cbPath);
}


int xxx_PMInstallNameSpace(IN LPWSTR pszIdentifier, IN LPWSTR pszPathName,
    IN DWORD dwNameSpace, IN DWORD dwVersion, IN GUID *pProviderId, IN OUT DWORD *pFlags)
{
    return PMInstallNameSpace(pszIdentifier, pszPathName, dwNameSpace, 
                dwVersion, pProviderId, pFlags);
}


int xxx_PMEnumNameSpaceProviders(OUT DWORD *pcBuf, OUT WSANAMESPACE_INFOW *pBuf,
    IN DWORD cOrigSize, IN OUT DWORD *pFlags, OUT int *pErr)
{
    return PMEnumNameSpaceProviders(pcBuf, pBuf, cOrigSize, pFlags, pErr);
}


int xxx_PMFindNameSpaces(WSAQUERYSETW *pQuery, void *pBuf, DWORD cInBuf,
    int *pcBuf, int *pErr)
{
    return PMFindNameSpaces(pQuery, pBuf, cInBuf, pcBuf, pErr);
}


int xxx_PMAddrConvert(DWORD Op, DWORD AddrFamily, SOCKADDR *pSockAddr, 
        DWORD cSockAddr, DWORD *pcSockAddr, LPWSAPROTOCOL_INFOW pProtInfo, 
        DWORD cbProtInfo, PWSTR psAddr, DWORD cAddr, DWORD *pcAddr) {

    return PMAddrConvert(Op, AddrFamily, pSockAddr, cSockAddr, pcSockAddr, 
            pProtInfo, cbProtInfo, psAddr, cAddr, pcAddr);

}


/*
DWORD xxx_PmSelect(UINT ReadCount, LPSOCK_LIST ReadList, UINT WriteCount,
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



int xxx_PMAccept(UINT hListeningSock, UINT *phAcceptedSock, PSOCKADDR pAddr, 
    DWORD cbAddr, DWORD *pcbAddr, LPCONDITIONPROC pfnCondition, DWORD CallbackData) {

    return PMAccept(hListeningSock, phAcceptedSock, pAddr, cbAddr, pcbAddr, 
            pfnCondition, CallbackData);
}


int xxx_PMBind(uint hPmSock, PSOCKADDR pAddr, DWORD cAddr) {

    return PMBind(hPmSock, pAddr, cAddr);
}


int xxx_PMConnect(UINT hPmSock, PSOCKADDR pAddr, DWORD cbAddr) {

    return PMConnect(hPmSock, pAddr, cbAddr);
}


int xxx_PMIoctl(UINT hPmSock, DWORD Code, VOID *pInBuf, DWORD cbInBuf, 
        VOID *pOutBuf, DWORD cbOutBuf, DWORD *pcbOutBuf, 
        WSAOVERLAPPED *pOv, DWORD cbOv, 
        LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn, LPWSATHREADID pThreadId) {

    return PMIoctl(hPmSock, Code, pInBuf, cbInBuf, pOutBuf, cbOutBuf, 
            pcbOutBuf, pOv, cbOv, pCompRtn, pThreadId);
}


int xxx_PMListen(UINT hPmSock, DWORD Backlog) {

    return PMListen(hPmSock, Backlog);
}


int xxx_PMRecv(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbRcvd, 
        DWORD *pFlags) {

    return PMRecv(hPmSock, pWsaBufs, cWsaBufs, pcbRcvd, pFlags);
}


int xxx_PMRecvFrom(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbRcvd,
        DWORD *pFlags, PSOCKADDR pAddr, DWORD cbAddr, DWORD *pcbAddr) {

    return PMRecvFrom(hPmSock, pWsaBufs, cWsaBufs, pcbRcvd, pFlags, 
            pAddr, cbAddr, pcbAddr);
}


int xxx_PMRecvEx(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbRcvd, 
        DWORD *pFlags, PSOCKADDR pAddr, DWORD cbAddr, DWORD *pcbAddr, 
        WSAOVERLAPPED *pOv, LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn, 
        LPWSATHREADID pThreadId, WSABUF *pControl) {

    return PMRecvEx(hPmSock, pWsaBufs, cWsaBufs, pcbRcvd, pFlags, 
            pAddr, cbAddr, pcbAddr, pOv, pCompRtn, pThreadId, pControl);
}


int xxx_PMSend(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbSent, 
        DWORD Flags) {

    return PMSend(hPmSock, pWsaBufs, cWsaBufs, pcbSent, Flags);
}


int xxx_PMSendTo(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbSent, 
        DWORD Flags, PSOCKADDR pAddr, DWORD cbAddr) {

    return PMSendTo(hPmSock, pWsaBufs, cWsaBufs, pcbSent, Flags, pAddr, cbAddr);
}


int xxx_PMSendEx(UINT hPmSock, WSABUF *pWsaBufs, DWORD cWsaBufs, DWORD *pcbSent, 
        DWORD Flags, PSOCKADDR pAddr, DWORD cbAddr, WSAOVERLAPPED *pOv, 
        LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn, LPWSATHREADID pThreadId) {

    return PMSendEx(hPmSock, pWsaBufs, cWsaBufs, pcbSent, Flags, 
            pAddr, cbAddr, pOv, pCompRtn, pThreadId);
}


int xxx_PMSendMsg(IN UINT hPmSock, IN LPWSAMSG pMsg, IN DWORD Flags,
    __out_opt LPDWORD pcbSent, __in_opt LPWSAOVERLAPPED pOv,
    __in_opt LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn, LPWSATHREADID pThreadId) {

    return PMSendMsg(hPmSock, pMsg, sizeof(*pMsg), Flags, pcbSent, pOv, 
        pCompRtn, pThreadId);
}


int xxx_PMShutdown(UINT hPmSock, DWORD How) {

    return PMShutdown(hPmSock, How);
}


int xxx_PMGetsockname (UINT hPmSock, LPSOCKADDR Address, DWORD cbAddr, 
        LPDWORD pcbAddr) {

    return PMGetsockname(hPmSock, Address, cbAddr, pcbAddr);
}


int xxx_PMGetpeername (UINT hPmSock, LPSOCKADDR pAddr, DWORD cbAddr, 
        LPDWORD pcbAddr) {

    return PMGetpeername (hPmSock, pAddr, cbAddr, pcbAddr);
}


int xxx_PMGetSockOpt(UINT hPmSock, DWORD Level, DWORD OptionName, LPVOID pBuf, 
        DWORD cbBuf, LPDWORD pcBuf) {

    return PMGetSockOpt(hPmSock, Level, OptionName, pBuf, cbBuf, pcBuf);
}


int xxx_PMSetSockOpt (UINT hPmSock, DWORD Level, DWORD OptionName, 
        LPVOID pBuf, DWORD cbBuf) {

    return PMSetSockOpt (hPmSock, Level, OptionName, pBuf, cbBuf);
}


int xxx_PMWakeup (UINT hPmSock, DWORD Event, DWORD Status) {

    return PMWakeup (hPmSock, Event, Status);
}


int xxx_PMGetOverlappedResult (UINT hPmSock, WSAOVERLAPPED *pOv, 
            DWORD *pcTransfer, DWORD fWait, DWORD *pFlags, DWORD *pErr) {

    return PMGetOverlappedResult (hPmSock, pOv, pcTransfer, fWait, pFlags, pErr);
}


int xxx_PMEventSelect (UINT hPmSock, WSAEVENT hEvent, long NetworkEvents) {

    return PMEventSelect(hPmSock, hEvent, NetworkEvents);
}


int xxx_PMEnumNetworkEvents (UINT hPmSock, WSAEVENT hEvent, 
        LPWSANETWORKEVENTS  pNetworkEvents, DWORD cbNetworkEvents) {

    return PMEnumNetworkEvents (hPmSock, hEvent, pNetworkEvents, cbNetworkEvents);
}


int xxx_PMCloseSocket (UINT hPmSock) {

    return PMCloseSocket(hPmSock);
}





