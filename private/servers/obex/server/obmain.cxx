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
//
//    Windows CE OBEX implementation.
//    (C) Microsoft 2000
//
//    Notes:
//    1. Need to move timeout and other parameters (ports and services) to registry.
//    2. Implement server class


#include <windows.h>
#include <strsafe.h>
#include <stdio.h>
#include <initguid.h>

#include "obexp.hxx"

#include <ws2bth.h>
#include <Ws2tcpip.h>
#include <md5.h>
#include <intsafe.h>

#include <bt_api.h> // For BTSecurityLevel


#if defined(DEBUG) || defined(_DEBUG)

    void *operator new(UINT size)
    {
        return g_funcAlloc(size, g_pvAllocData);
    }

    void operator delete(void *pVoid)
    {
        g_funcFree(pVoid, g_pvAllocData);
    }

    DBGPARAM dpCurSettings = {
        TEXT("ObexSrvr"), {
        TEXT("Init"),           // 0
        TEXT("Protocol"),       // 1
        TEXT("Maintain"),       // 2
        TEXT("Packet"),         // 3
        TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),
        TEXT("Warning"),      // 14
        TEXT("Error") },        // 15
        0xC000
    };

    void DbgPrintBytes(const unsigned char* data, const unsigned int size)
    {
        const unsigned int CharsPerByte = 5; // eg. "0x00 0x01 "
        const unsigned int wSize = (size * CharsPerByte) + 1;
        wchar_t* wbuf = new wchar_t[wSize];

        if (!wbuf)
            return;

        unsigned int offset;
        for (offset = 0; offset < size; offset++)
        {
            ASSERT(S_OK == StringCchPrintf(
                &wbuf[offset * CharsPerByte],
                wSize - offset * CharsPerByte,
                L"0x%02x ",
                data[offset]));
        }

        DEBUGMSG(ZONE_PACKET, (L"%s\r\n", wbuf));
        delete [] wbuf;

        // A little sanity check doesn't hurt...
        ASSERT(offset * CharsPerByte == wSize - 1);
    }
#else
#define DbgPrintBytes
#endif


#define WSAGetLastError GetLastError
//
//    Class definitions
//
class AddressFamily;
class Association;
class Connection;
class Server;

enum ServerState {
    STARTING,
    RUNNING,
    DOWN
};

enum ServerStatus {
    NOT_INITED,
    OPERATING,
    FAILED
};

static void ReverseByteOrder (void *pData, int size) {
    unsigned char *puc = (unsigned char *)pData;
    for (int i = 0 ; i < size / 2 ; ++i) {
        unsigned char c = puc[i];
        puc[i] = puc[size - 1 - i];
        puc[size - 1 - i] = c;
    }
}

class GlobalData : public SVSSynch {
public:
    FixedMemDescr    *pfmdServers;
    FixedMemDescr    *pfmdAssociations;
    FixedMemDescr    *pfmdConnections;

    AddressFamily    *pafList;
    Association        *passocList;
    Connection        *pconnList;
    Server            *pservList;

    SVSThreadPool   *pThreads;

    ServerState        fState;

    unsigned int    uiConnectionId;
    unsigned int    uiTransactionId;

    int                fMaintain;

    unsigned int    uiOBEX_MAINT_PERIOD;
    unsigned int    uiOBEX_SERVER_TIMEOUT;
    unsigned int    uiOBEX_CONNECTION_TIMEOUT;

    ISdpRecord      *pSdpRecord;

    GlobalData (void) {
        pfmdServers =
            pfmdAssociations =
            pfmdConnections = NULL;

        pafList = NULL;
        passocList = NULL;
        pconnList = NULL;
        pservList = NULL;
        pThreads = NULL;

        fState = STARTING;

        uiConnectionId = 1;
        uiTransactionId = 1;
        fMaintain = FALSE;

        uiOBEX_CONNECTION_TIMEOUT = OBEX_CONNECTION_TIMEOUT;
        uiOBEX_SERVER_TIMEOUT = OBEX_SERVER_TIMEOUT;
        uiOBEX_MAINT_PERIOD = OBEX_MAINT_PERIOD;

        pSdpRecord = NULL;
    }

    ~GlobalData (void);

    int Start (void);                                                        // +

    //    this starts locked and returns unlocked.
    int Stop (void);                                                        // +

    //    will unlock inside
    HRESULT CloseConnection (Connection *pConn);                                // +

    //    this starts locked and returns unlocked.
    int SendTrivialResponse (Connection *pConn, int iResponseCode);            // +

    void SetMaintain (void);                                                // +

    //    this starts locked and returns unlocked.
    void ServiceRequest (Connection *pConn);                                // +

    static DWORD WINAPI Compact (LPVOID pThis);                                // +
};



class AddressFamily : public SVSAllocClass {
public:
    AddressFamily    *pNext;

    SOCKET            aSocket[OBEX_MAXPORTS];
    //hold the address family for the socket
    //  (uses same index as aSocket)
    int                addFam[OBEX_MAXPORTS];
    int                cSockets;

    HANDLE            hThread;

    AddressFamily () {
        pNext = NULL;
        cSockets = 0;
        hThread = NULL;
        uiSDPRecordHandleCnt = 0;
    }

    ~AddressFamily (void) {
        //
        //  Flush out the BTH
        //
        while(uiSDPRecordHandleCnt)
        {
            int ret = obutil_SdpDelRecord(SDPRecordHandle[uiSDPRecordHandleCnt-1]);

            if(ERROR_SUCCESS != ret)
            {
                DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] AddressFamily::~AddressFamily -- removing handle failed\n"));
            }

            uiSDPRecordHandleCnt --;
        }


        if (hThread)
            CloseHandle (hThread);
    }

    UINT uiSDPRecordHandleCnt;
    ULONG SDPRecordHandle[MAX_NUM_STP_RECS];

    int Start (int af, int iPort);                                                    // +
    int Start (int af, char *szServiceName);                                        // +
    int Start (int af, ISdpRecord** ppSdpRecord);

    int Stop (void);                                                        // +
    int IsDown (void);                                                        // +

    void Listen (void);                                                        // +

    static DWORD WINAPI ThreadListen (LPVOID pThis) {
        AddressFamily *pA = (AddressFamily *)pThis;
        pA->Listen ();

        return 0;
    }
};

class Connection {
public:
    Connection        *pNext;

    AddressFamily    *paf;
    SOCKET            s;
    int             addFam;

    int                tickLastActive;

    unsigned int    uiCurrentTransaction;
    unsigned int    uiConnectionId;

    unsigned char    *pBuffer;
    int                cFilled;
    int                cBufSize;

    unsigned char   ucPeekBuff[3];
    int                cPeekFilled;
};

class Association {
public:
    Association        *pNext;

    Connection        *pConn;
    Server            *pServer;

    unsigned int    uiConnectionId;
    unsigned int    uiTransactionId;
    bool            fDefaultConnection;
};

class Server {
public:
    Server            *pNext;

    int                fServerStatus;

    int                iCallRef;
    int                tickLastActive;

    int                cProtSeq;
    unsigned char    aProtSeq[OBEX_PROTOCOLS];

    unsigned int    cServerId;
    unsigned char    ucServerId[OBEX_SERVICEID_LEN];

    HINSTANCE        hDll;

    ServiceCallback    pServiceCallback;

    WCHAR            szName[_MAX_PATH];

    int CanUseConnection (Connection *pConn);                                                // +

    int Load (int iLen, unsigned char *pserviceid);                                            // +

    //    will unlock - starts and returns locked
    int Unload (void);                                                                        // +

    //    This starts locked and returns unlocked
    int DispatchCall (unsigned int uiConnectionId, Connection *pConn, ObexParser *pParser);    // -
};

//
//    Variable declarations
//
static GlobalData    *gpState = NULL;


HRESULT obex_MD5_Helper(unsigned char *orig, unsigned int len, unsigned char *dest, unsigned int *pDestSize)
{
    MD5_CTX context;

    if(!orig || !dest || !pDestSize) {
        return E_INVALIDARG;
    }

    MD5Init(&context);
    MD5Update(&context, (UCHAR *)orig, len);
    MD5Final(&context);

    if(*pDestSize < 16) {
        *pDestSize = 16;
        return E_INVALIDARG;
    } else {
        *pDestSize = 16;
    }
    memcpy(dest, context.digest, 16);

    return S_OK;
}


HRESULT obex_GetBtRemoteInfo(unsigned int uiTransactionId, struct _obex_bt_remote_info& remoteInfo)
{
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_GetBtRemoteInfo \n"));

    HRESULT hr = E_FAIL;
    Connection *pConn = NULL;

    SOCKADDR_BTH sockAddr;
    int namelen = sizeof(SOCKADDR_BTH);

    gpState->Lock();

    if (gpState->fState != RUNNING) {
        goto Cleanup;
    }

    pConn = gpState->pconnList;
    while (pConn) {
        if (pConn->uiCurrentTransaction == uiTransactionId)
            break;
        pConn = pConn->pNext;
    }

    if(!pConn) {
        goto Cleanup;
    }

    if (pConn->addFam == AF_BT) {
        if (0 == getpeername(pConn->s, (SOCKADDR*)&sockAddr, &namelen)) {
            remoteInfo.address = sockAddr.btAddr;

            // currently cannot use setsockopt(SO_BTH_SET_READ_REMOTE_NAME) - see Medpg Bug 119559

            typedef int (WINAPI *PFN_BTHRNQ) (BT_ADDR* pba, unsigned int cBuffer, unsigned int* pcRequired, WCHAR* szString);

            HINSTANCE hLib = LoadLibrary(L"btdrt.dll");
            if (hLib) {
                PFN_BTHRNQ pqName = (PFN_BTHRNQ) GetProcAddressW(hLib, TEXT("BthRemoteNameQuery"));

                unsigned int required;
                int iRes = pqName(&remoteInfo.address, BTH_MAX_NAME_SIZE, &required, remoteInfo.pwszName);
                FreeLibrary(hLib);

                if (ERROR_SUCCESS == iRes) {
                    hr = S_OK;
                } else {
                    DEBUGMSG(ZONE_ERROR,
                        (L"[OBEX] obex_GetBtRemoteInfo - a call to BthRemoteNameQuery failed w/ result=%d.\n", iRes));
                    hr = E_FAIL;
                }
            } else {
                DEBUGMSG(ZONE_ERROR,
                    (L"[OBEX] obex_GetBtRemoteInfo - error loading btdrt.dll\n"));
                hr = E_FAIL;
            }
        }
    } else {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_GetBtRemoteInfo: addr family is not AF_BT.\n"));
        hr = ERROR_NOT_SUPPORTED;
    }

Cleanup:
    gpState->Unlock ();
    return hr;
}


//
//    GlobalData
//
GlobalData::~GlobalData()
    {
        SVSUTIL_ASSERT (fState != RUNNING);

        //
        //Lock  to make CloseConnection happy
        //
        gpState->Lock();

        //
        //    Shut down all active connections
        //
        while (pconnList)
        {
            DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::~GlobalData: shutting down connection\n"));
            if(FAILED(CloseConnection (pconnList))) {
                ASSERT(0);
                break;
            }
        }

        //
        //  Unload any DLL's that we might have
        //
        Server *pServ = pservList;
        while (pServ)
        {
            Server *pDel = pServ;
            pServ = pServ->pNext;
            DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::~GlobalData: retiring server %s\n", pDel->szName));

            pDel->pNext = NULL;
            pDel->Unload ();
        }

        gpState->Unlock();

        //
        //  do memory cleanup
        //
        if(pThreads)
            delete pThreads;

        if (pfmdServers)
            svsutil_ReleaseFixedNonEmpty (pfmdServers);

        if (pfmdAssociations)
            svsutil_ReleaseFixedNonEmpty (pfmdAssociations);

        if (pfmdConnections)
            svsutil_ReleaseFixedNonEmpty (pfmdConnections);

        SVSUTIL_ASSERT (! pafList);
    }



int GlobalData::Start (void) {
    DEBUGMSG(ZONE_INIT, (L"[OBEX] +GlobalData::Start\n"));

    SVSUTIL_ASSERT (IsLocked ());

    int fUseIRDA = FALSE;
    int fUseBTH  = FALSE;
    int fUseTCP  = FALSE;

    //
    //    Read registry parameters
    //
    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, OBEX_KEY_BASE, 0, KEY_READ, &hk)) {
        DWORD dw;
        DWORD dwType;
        DWORD dwSize;
        dwSize = sizeof(dw);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"IsEnabled",0,&dwType,(LPBYTE)&dw,&dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw == 0)) {
            // Obex disabled via registry.
            RegCloseKey(hk);
            return FALSE;
        }

        dwSize = sizeof(dw);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ServerTimeout", 0, &dwType, (LPBYTE)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw >= OBEX_SERVER_TIMEOUT_MIN))
            uiOBEX_SERVER_TIMEOUT = dw;

        dwSize = sizeof(dw);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ConnectionTimeout", 0, &dwType, (LPBYTE)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && ((dw == 0) || (dw >= OBEX_CONNECTION_TIMEOUT_MIN)))
            uiOBEX_CONNECTION_TIMEOUT = dw;

        dwSize = sizeof(dw);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"MaintPeriod", 0, &dwType, (LPBYTE)&dw, &dwSize)) &&
            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw > OBEX_MAINT_PERIOD_MIN))
            uiOBEX_MAINT_PERIOD = dw;

        WCHAR szProtocols[OBEX_SMALLBUFFER];
        dwSize = sizeof(szProtocols);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"protocols", 0, &dwType, (LPBYTE)szProtocols, &dwSize)) &&
            (dwType == REG_SZ) && (dwSize < sizeof(szProtocols))) {
            if (wcsstr (szProtocols, L"irda"))
                fUseIRDA = TRUE;

            if (wcsstr (szProtocols, L"bth"))
                fUseBTH = TRUE;

            if (wcsstr (szProtocols, L"tcp"))
                fUseTCP = TRUE;
        }

        RegCloseKey (hk);
    }

    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : Supported protocols : %s %s %s\n",
        fUseTCP ? L"TCP/IP" : L"", fUseIRDA ? L"IRDA" : L"", fUseBTH ? L"Bluetooth" : L""));
    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : uiOBEX_SERVER_TIMEOUT     = %d\n",
        uiOBEX_SERVER_TIMEOUT));
    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : uiOBEX_CONNECTION_TIMEOUT = %d\n",
        uiOBEX_CONNECTION_TIMEOUT));
    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : uiOBEX_MAINT_PERIOD       = %d\n",
        uiOBEX_MAINT_PERIOD));

    pfmdServers = svsutil_AllocFixedMemDescr (sizeof (Server), OBEX_MEM_SCALE);
    pfmdAssociations = svsutil_AllocFixedMemDescr (sizeof(Association), OBEX_MEM_SCALE);
    pfmdConnections = svsutil_AllocFixedMemDescr (sizeof(Connection), OBEX_MEM_SCALE);

    pThreads = new SVSThreadPool();

    if (! (pfmdServers && pfmdAssociations && pfmdConnections && pThreads)) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start: out of memory\n"));
        return FALSE;
    }

    //
    //    Initialize listeners
    //
    ASSERT(NULL == pafList);
    pafList = NULL;

#ifndef SDK_BUILD
    if (fUseTCP) {
        AddressFamily *pNewAF = new AddressFamily ();

        if(!pNewAF)
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start: could not initialize TCP/IP : out of memory\n"));
        else {
            if (! pNewAF->Start (PF_UNSPEC, OBEX_TCP_PORT)) {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Error registering OBEX name with TCP\n"));
                delete pNewAF;
            } else {
                DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : Registered OBEX name with TCP\n"));
                pNewAF->pNext = pafList;
                pafList = pNewAF;
            }
        }
    }
#endif
    if (fUseIRDA) {
        AddressFamily *pNewAF = new AddressFamily ();
        if(!pNewAF)
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start: could not initialize TCP/IP : out of memory\n"));
        else {
            BOOL fOBEXFailed = FALSE;
            BOOL fOBEXIrXFerFailed = FALSE;
            if (! pNewAF->Start (AF_IRDA, "OBEX")) {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Error registering OBEX name with IrDA\n"));
                fOBEXFailed = TRUE;
            }
            else {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Registered OBEX name with IrDA\n"));
            }
            if (! pNewAF->Start (AF_IRDA, "OBEX:IrXfer")) {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Error registering OBEX:IrXfer name with IrDA\n"));
                fOBEXIrXFerFailed = TRUE;
            }
            else {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Registered OBEX:IrXfer name with IrDA\n"));
            }

            //if both failed, we need to delete this address family
            //  otherwise chain our new AF onto the list
            if(fOBEXFailed && fOBEXIrXFerFailed)
                delete pNewAF;
            else {
                pNewAF->pNext = pafList;
                pafList = pNewAF;
            }
        }
    }

    if (fUseBTH) {
        AddressFamily *pNewAF = new AddressFamily ();
        if(!pNewAF)
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start: could not initialize TCP/IP : out of memory\n"));
        else {
            if (! pNewAF->Start (AF_BT, &pSdpRecord)) {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] GlobalData::Start : Error registering OBEX name with Bluetooth\n"));
                delete pNewAF;
            } else {
                DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start : Registered OBEX name with Bluetooth\n"));
                pNewAF->pNext = pafList;
                pafList = pNewAF;
            }
        }
    }


    if (pafList) {
        fState = RUNNING;

        DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start - successfully started OBEX server\n"));
        return TRUE;
    }

    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Start - started OBEX server has no protocols\n"));

    return FALSE;
}

int GlobalData::Stop (void) {
    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Stop - initiating unload\n"));

    fState = DOWN;

    AddressFamily *pAF = pafList;
    while (pAF) {
        pAF->Stop ();
        pAF = pAF->pNext;
    }

    Unlock ();

    SVSUTIL_ASSERT (! IsLocked ());

    if (pThreads)
        pThreads->Shutdown ();

    for ( ; ; ) {
        int fRepeat = FALSE;

        pAF = pafList;

        while (pAF) {
            if (! pAF->IsDown())
                fRepeat = TRUE;

            pAF = pAF->pNext;
        }

        if (! fRepeat)
            break;

        Sleep (1000);
    }

    while (pafList) {
        AddressFamily *pNext = pafList->pNext;

        delete pafList;

        pafList = pNext;
    }

    //
    // An instance of SdpRecord object is created on demand by Obutil and gets released here.
    // Note: SDP records get deleted from the registry when the library (btdrt.dll) is unloaded.
    //
    if (pSdpRecord) {
        pSdpRecord->Release();
        pSdpRecord = NULL;
    }

    DEBUGMSG(ZONE_INIT, (L"[OBEX] GlobalData::Stop - successfully unloaded\n"));

    return TRUE;
}

void GlobalData::ServiceRequest (Connection *pConn) {
    DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x\n", pConn->uiCurrentTransaction));

    SVSUTIL_ASSERT (pConn->cBufSize >= 3);
    SVSUTIL_ASSERT (pConn->cFilled == pConn->cBufSize);
    SVSUTIL_ASSERT (pConn->pBuffer);

    ObexParser p(pConn->pBuffer, pConn->cBufSize);
    unsigned int uiCId = OBEX_INVALID_CID;

    if ((p.Op () != OBEX_OP_CONNECT) &&
        (p.Op () != OBEX_OP_DISCONNECT) &&
        ((p.Op () & OBEX_OP_OPMASK) != OBEX_OP_PUT) &&
        ((p.Op () & OBEX_OP_OPMASK) != OBEX_OP_GET) &&
        (p.Op () != OBEX_OP_SETPATH) &&
        (p.Op () != OBEX_OP_ABORT)) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with bad opcode\n",
            pConn->uiCurrentTransaction));
        // This unlocks internally
        SendTrivialResponse (pConn, OBEX_STAT_NOTIMPLEMENTED | OBEX_OP_ISFINAL);
        SVSUTIL_ASSERT (! IsLocked ());
        return;
    }

#if defined (DEBUG) || defined (_DEBUG)
    if (p.Op () == OBEX_OP_CONNECT) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = CONNECT\n",
        pConn->uiCurrentTransaction));
    }

    if (p.Op () == OBEX_OP_DISCONNECT) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = DISCONNECT\n",
        pConn->uiCurrentTransaction));
    }

    if (p.Op () == OBEX_OP_SETPATH) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = SETPATH\n",
        pConn->uiCurrentTransaction));
    }

    if (p.Op () == OBEX_OP_ABORT) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = ABORT\n",
            pConn->uiCurrentTransaction));
    }

    if (p.Op () == (OBEX_OP_PUT | OBEX_OP_ISFINAL)) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = PUT, FINAL\n",
            pConn->uiCurrentTransaction));
    }

    if (p.Op () == OBEX_OP_PUT) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = PUT, CONTINUE\n",
            pConn->uiCurrentTransaction));
    }

    if (p.Op () == (OBEX_OP_GET | OBEX_OP_ISFINAL)) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = GET, FINAL\n",
            pConn->uiCurrentTransaction));
    }

    if (p.Op () == OBEX_OP_GET) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x OP = GET, CONTINUE\n",
            pConn->uiCurrentTransaction));
    }
#endif

    int fHaveConnectionId = FALSE;

    //if the packet has a connection ID quickly dispatch it
    if (p.IsA (OBEX_HID_CONNECTIONID)) {
        //
        //    Have connection id
        //
        if (! p.GetDWORD((unsigned long *)&uiCId)) {
            DEBUGMSG(ZONE_PACKET, (L"GlobalData::ServiceRequest: packet 0x%08x fails with BADREQUEST\n",
                pConn->uiCurrentTransaction));
            // This unlocks internally
            SendTrivialResponse (pConn, OBEX_STAT_BADREQUEST | OBEX_OP_ISFINAL);
            SVSUTIL_ASSERT (! IsLocked ());
            return;
        }

        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x cid = 0x%08x\n",
            pConn->uiCurrentTransaction, uiCId));

        fHaveConnectionId = TRUE;

        if (! (p.Op() & OBEX_OP_ISFINAL))
            pConn->uiConnectionId = uiCId;
    } else if ((pConn->uiConnectionId != OBEX_INVALID_CID) &&
                (((p.Op () & OBEX_OP_OPMASK) == OBEX_OP_PUT) || ((p.Op () & OBEX_OP_OPMASK) == OBEX_OP_GET))) {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x cid = 0x%08x from pending connection\n",
            pConn->uiCurrentTransaction, uiCId));

        uiCId = pConn->uiConnectionId;
        fHaveConnectionId = TRUE;
    }

    if (p.Op() & OBEX_OP_ISFINAL)
        pConn->uiConnectionId = OBEX_INVALID_CID;

    if (fHaveConnectionId) {
        Association *pA = passocList;

        while (pA) {
            if (pA->uiConnectionId == uiCId) {
                // if the connectionID's match but the connections dont drop the request -- most likely being hacked
                if(pA->pConn != pConn) {
                    break;
                }
                pA->uiTransactionId = pConn->uiCurrentTransaction;
                pA->pServer->DispatchCall (pA->uiConnectionId, pConn, &p);    // This unlocks internally
                SVSUTIL_ASSERT (! IsLocked ());
                return;
            }
            pA = pA->pNext;
        }

        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with SERVICEUNAVAIL\n",
            pConn->uiCurrentTransaction));
        SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
        SVSUTIL_ASSERT (! IsLocked ());
        return;
    } else if (p.Op() != OBEX_OP_CONNECT) {
        SVSUTIL_ASSERT(pConn != NULL);

        // If there was no connection ID included in the request and this is not another CONNECT request,
        // try matching it by the socket before giving up and hand it to the default service.
        // This is so that we can support profiles that transfer subsequent data packets wo/ conn id.

        Association* pA;
        for (pA = passocList; pA != NULL; pA = pA->pNext) {
            if(pA->pConn && pA->pConn->s == pConn->s) {
                // having socket matched the association implies we have already have a good connId.
                SVSUTIL_ASSERT(pA->uiConnectionId != OBEX_INVALID_CID);
                // bail out if the previous assocociated connection was invalid.
                if(pA->uiConnectionId == OBEX_INVALID_CID) {
                    break;
                }
                // if the connectionID's match but the connections dont drop the request -- most likely being hacked
                if(pA->pConn != pConn) {
                    break;
                }
                DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: dispatching based on matched socket.\n"));
                pA->uiTransactionId = pConn->uiCurrentTransaction;
                pA->pServer->DispatchCall(pA->uiConnectionId, pConn, &p);    // This unlocks internally
                SVSUTIL_ASSERT (!IsLocked());
                return;
            }
        }

        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: no connection ID in packet and could not match socket.\n"));
    }

    DEBUGMSG(ZONE_PACKET,
        (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x : connection id is not available. Searching by targetid...\n",
        pConn->uiCurrentTransaction));

    char serviceid[OBEX_SERVICEID_LEN + 1];
    int iLen = 0;


    //check to see the opcode is for a TARGET id
    if (p.IsA (OBEX_HID_TARGET)) {
        //
        //    Have target
        //
        iLen = p.Length ();
        if (iLen > OBEX_SERVICEID_LEN) {
            DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with SERVICEUNAVAIL (too big id)\n", pConn->uiCurrentTransaction));
            SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
            SVSUTIL_ASSERT (! IsLocked ());
            return;
        }

        //call GetBytes to fetch the service ID from the header
        //   on error, fail with a bad request
        if (! p.GetBytes (serviceid, iLen)) {
            DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with BADREQUEST (incorrect id)\n", pConn->uiCurrentTransaction));
            SendTrivialResponse (pConn, OBEX_STAT_BADREQUEST | OBEX_OP_ISFINAL);
            SVSUTIL_ASSERT (! IsLocked ());
            return;
        }
    }

    //if the packet has no TARGET or CONNECTION ID
    //  go on and eventally try to figure out something to do with the packet (default mailbox perhaps)
    else {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x : no targetid... Assuming default inbox.\n", pConn->uiCurrentTransaction));
        memset (serviceid, 0, sizeof(serviceid));
        iLen = 16;
    }

    Server *pServ = pservList;
    while (pServ) {
        if (((int)pServ->cServerId == iLen) && (memcmp (pServ->ucServerId, serviceid, iLen) == 0))
            break;
        pServ = pServ->pNext;
    }

    if (! pServ) {
        pServ = (Server *)svsutil_GetFixed (pfmdServers);
        if (! pServ) {
            DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with INTERNALERROR\n", pConn->uiCurrentTransaction));
            SendTrivialResponse (pConn, OBEX_STAT_INTERNALERROR | OBEX_OP_ISFINAL);
            SVSUTIL_ASSERT (! IsLocked ());
            return;
        }

        memset (pServ, 0, sizeof(*pServ));

        //if the serviceID = 0
        //load up the client dll by the serviceid
        if (! pServ->Load (iLen, (unsigned char *)serviceid)) {
            pServ->Unload ();
            svsutil_FreeFixed (pServ, pfmdServers);
            DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with SERVICEUNAVAIL (no server)\n", pConn->uiCurrentTransaction));
            SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
            SVSUTIL_ASSERT (! IsLocked ());
            return;
        }

        pServ->pNext = pservList;
        pservList = pServ;
    }

    SVSUTIL_ASSERT (pServ);
    if (pServ->CanUseConnection (pConn))
    {
        //NOTE:  the transaction ID *MUST* be set by DispatchCall
        //  in this one special case (where CID is INVALID!)
        pServ->DispatchCall (OBEX_INVALID_CID, pConn, &p);
    }
    else {
        DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::ServiceRequest: packet 0x%08x fails with SERVICEUNAVAIL (server won't serve)\n", pConn->uiCurrentTransaction));
        SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
    }

    DEBUGMSG(ZONE_PACKET, (L"[OBEX] -GlobalData::ServiceRequest\n"));
    SVSUTIL_ASSERT (! IsLocked ());
}

DWORD WINAPI GlobalData::Compact (LPVOID pVoid) {
    DEBUGMSG(ZONE_MAINTAIN, (L"GlobalData::Compact - entered\n"));

    if (! gpState)
        return 0;

    gpState->Lock ();
    if (gpState->fState != RUNNING) {
        gpState->Unlock ();
        return 0;
    }

    //
    //    Compaction pass on all structures.
    //
    //    Close long inactive connections.
    int tickNow = GetTickCount ();

    if (gpState->uiOBEX_CONNECTION_TIMEOUT) {
        for ( ; ; ) {
            int fResume = FALSE;
            Connection *pConn = gpState->pconnList;
            while (pConn) {
                if ((! pConn->uiCurrentTransaction) &&
                        (tickNow - pConn->tickLastActive > (int)gpState->uiOBEX_CONNECTION_TIMEOUT)) {
                    DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Compact: retiring connection %d\n", pConn->s));

                    gpState->CloseConnection (pConn);
                    if (gpState->fState != RUNNING) {
                        gpState->Unlock ();
                        return 0;
                    }

                    fResume = TRUE;
                    break;
                }
                pConn = pConn->pNext;
            }

            if (! fResume)
                break;
        }
    }

    for ( ; ; ) {
        int fResume = FALSE;
        Server *pParent = NULL;
        Server *pServ = gpState->pservList;
        while (pServ) {
            if ((! pServ->iCallRef) &&
                    (tickNow - pServ->tickLastActive > (int)gpState->uiOBEX_SERVER_TIMEOUT)) {
                Association *pA = gpState->passocList;
                while (pA) {
                    if (pA->pServer == pServ)
                        break;
                    pA = pA->pNext;
                }
                if (! pA) {
                    DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Compact: retiring server %s\n", pServ->szName));

                    if (pParent)
                        pParent->pNext = pServ->pNext;
                    else
                        gpState->pservList = gpState->pservList->pNext;

                    pServ->pNext = NULL;
                    pServ->Unload ();

                    if (gpState->fState != RUNNING) {
                        gpState->Unlock ();
                        return 0;
                    }

                    svsutil_FreeFixed (pServ, gpState->pfmdServers);

                    fResume = TRUE;
                    break;
                }
            }
            pParent = pServ;
            pServ = pServ->pNext;
        }

        if (! fResume)
            break;
    }

    if ((! gpState->pconnList) && (! gpState->pservList)) {
        gpState->fMaintain = FALSE;    // just exit... or
        DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Compact - exited\n"));
    } else {                            // schedule forward...
        gpState->pThreads->ScheduleEvent (Compact, NULL, gpState->uiOBEX_MAINT_PERIOD);
        DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Compact - rescheduled\n"));
    }
    gpState->Unlock();

    return 0;
}

int GlobalData::SendTrivialResponse (Connection *pConn, int iResponse) {
    DEBUGMSG(ZONE_PACKET, (L"[OBEX] GlobalData::SendTrivialResponse - response 0x%02x\n", iResponse));

    SVSUTIL_ASSERT (pConn->uiCurrentTransaction);
    SVSUTIL_ASSERT (pConn->pBuffer);
    SVSUTIL_ASSERT (pConn->cBufSize >= 3);
    SVSUTIL_ASSERT (pConn->cFilled == pConn->cBufSize);

    SOCKET s = pConn->s;

    pConn->uiCurrentTransaction = 0;    // We are responding...
    pConn->cBufSize = pConn->cFilled = 0;
    obex_Free (pConn->pBuffer);
    pConn->pBuffer = NULL;

    gpState->Unlock ();

    char x[3];
    x[0] = (char)iResponse;        // Response
    x[1] = 0;                    // Length
    x[2] = 3;

    DbgPrintBytes((const unsigned char*)x, 3);
    return send(s, x, 3, 0);
}

HRESULT GlobalData::CloseConnection (Connection *pConn) {
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] GlobalData::CloseConnection\n"));
    SVSUTIL_ASSERT (gpState->IsLocked ());

    //    Free resources
    closesocket (pConn->s);
    pConn->s = INVALID_SOCKET;

    if (pConn->pBuffer)
        obex_Free (pConn->pBuffer);

    pConn->pBuffer = NULL;
    pConn->cFilled = 0;
    pConn->cBufSize = 0;

    pConn->cPeekFilled = 0;

    //
    //        Take it out of the list
    //
    Connection *pParent = NULL;
    Connection *pRun = pconnList;
    while (pRun) {
        if (pConn == pRun)
            break;

        pParent = pRun;
        pRun = pRun->pNext;
    }

    if (! pRun) {
        SVSUTIL_ASSERT (0);
        return E_FAIL;
    }

    if (! pParent)
        pconnList = pconnList->pNext;
    else
        pParent->pNext = pRun->pNext;

    //    Clean up associations, notify servers
    for ( ; ; ) {
        Association *pAssocParent = NULL;
        Association *pA = passocList;
        while (pA) {
            if (pA->pConn == pConn) {
                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] GlobalData::CloseConnection : closing association to %s\n", pA->pServer->szName));

                if (! pAssocParent)
                    passocList = passocList->pNext;
                else
                    pAssocParent->pNext = pA->pNext;

                Server *pServ = pA->pServer;
                unsigned int uiCId = pA->uiConnectionId;
                svsutil_FreeFixed (pA, pfmdAssociations);
                pA->uiTransactionId = pConn->uiCurrentTransaction;
                pServ->DispatchCall (uiCId, NULL, NULL);

                SVSUTIL_ASSERT (! gpState->IsLocked());
                gpState->Lock ();
                break;
            }

            pAssocParent = pA;
            pA = pA->pNext;
        }

        if (! pA)
            break;
    }

    svsutil_FreeFixed (pConn, gpState->pfmdConnections);
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] -GlobalData::CloseConnection\n"));
    return S_OK;
}

void GlobalData::SetMaintain(void) {
    if (! fMaintain) {
        DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::SetMaintain :: scheduling maintenance\n"));

        fMaintain = TRUE;
        pThreads->ScheduleEvent (Compact, NULL, uiOBEX_MAINT_PERIOD);
    }
}


#define SOCKET_CREATION_ATTEMPTS 20


#ifndef SDK_BUILD

//
//    AddressFamily
//
int AddressFamily::Start (int af, int iPort) {
    DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Start :: TCP/IP\n"));

    SVSUTIL_ASSERT (gpState->IsLocked ());

    SOCKET sl;
    ADDRINFO aiHints, *aiAddrInfo, *AI;
    char portBuffer[21];
    int i;

    _itoa_s(iPort, portBuffer, _countof(portBuffer), 10);

    memset(&aiHints, 0, sizeof(aiHints));
    aiHints.ai_family = af;
    aiHints.ai_socktype = SOCK_STREAM;
    aiHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

    BOOL fGotValidBinding = FALSE;

    //use getaddrinfo for fetching addrinfo
    if(0 != getaddrinfo(NULL,portBuffer,&aiHints, &aiAddrInfo))
    {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::error with getaddrinfo! (%d)\n", WSAGetLastError()));
        return FALSE;
    }

    for (i = 0, AI = aiAddrInfo; AI != NULL; AI = AI->ai_next, i++)
    {
        // Highly unlikely, but check anyway.
        if (i == FD_SETSIZE) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] Too many addresses given\n"));
            break;
        }

        // Make sure we're not beyond the OBEX_MAXPORTS
        if(cSockets >= OBEX_MAXPORTS) {
            ASSERT(FALSE);
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] Too many address Families\n"));
            break;
        }

        // we only support PF_INET and PF_INET6 here
        if ((AI->ai_family != PF_INET) && (AI->ai_family != PF_INET6))
            continue;

        // Open a socket with the correct address family for this address.
        sl = aSocket[cSockets] = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
        if (sl == INVALID_SOCKET){
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] Too many addresses given\n"));
            continue;
        }

        //bind the socket
        if (bind (sl, AI->ai_addr, AI->ai_addrlen))
        {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: bind failed (ecode=%d)!\n", WSAGetLastError()));
            closesocket (sl);
            continue;
        }

        if (listen (sl, 5)) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start::Listen failed (ecode=%d)!\n", WSAGetLastError()));
            closesocket (sl);
            continue;
        }

        if (! hThread)
           hThread = CreateThread (NULL, 0, ThreadListen, (void *)this, 0, NULL);

        if (! hThread)
        {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start failed to create thread (error=0x%08x (%d))\n", GetLastError(), GetLastError ()));
            closesocket (sl);
            continue;
        }

        SVSUTIL_ASSERT (cSockets < OBEX_MAXPORTS);
        addFam[cSockets] = AI->ai_family;
        PREFAST_SUPPRESS(394,  "this is checked at the beginning of the for loop to make sure < OBEX_MAXPORT")
        ++cSockets;

        fGotValidBinding = TRUE;
        DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Start - successfully started af %d port %d\n", af, iPort));
    }

    freeaddrinfo(aiAddrInfo);
    return fGotValidBinding;
}

#endif


int AddressFamily::Start (int af, ISdpRecord** ppSdpRecord) {
    DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Start :: Bluetooth\n"));

    SVSUTIL_ASSERT (gpState->IsLocked ());

    SVSUTIL_ASSERT (cSockets < OBEX_MAXPORTS);

    addFam[cSockets] = af;


    SOCKET sl = INVALID_SOCKET;
    for(int i=0; i < SOCKET_CREATION_ATTEMPTS; i++) {
        sl = aSocket[cSockets] = socket (af, SOCK_STREAM, BTHPROTO_RFCOMM);

        if(sl != INVALID_SOCKET)
            break;
        else {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: no sockets (ecode=%d)! -- retrying(%d/%d)\n", WSAGetLastError(), i, SOCKET_CREATION_ATTEMPTS));
            Sleep(1000);
        }

    }

    if (sl == INVALID_SOCKET) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: no sockets (error=%d)!\n", WSAGetLastError()));
        return FALSE;
    }

    if (af == AF_BT) {
        SOCKADDR_BTH sa;
        memset (&sa, 0, sizeof(sa));
        sa.addressFamily = AF_BT;
        sa.port             = 0;

        if (bind (sl, (sockaddr *)&sa, sizeof(sa))) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: bind failed (error=%d)!\n", WSAGetLastError()));
            closesocket (sl);
             return FALSE;
        }

        int namelen = sizeof(SOCKADDR_BTH);
        if (getsockname(sl, (SOCKADDR *)&sa, &namelen))    {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: getsockname failed (error=%d)!\n", WSAGetLastError()));
            closesocket(sl);
            return FALSE;
        }

        // Set default security level for a new server socket to level 1. If ObexAuthRequest is called
        // for a connection then the security level will be increased.
        //
        // Security level 1 requires no authentication for pre-SSP connections (that is, no PIN entry required).
        // For SSP, Security Level 1 uses the "Just Works" procedure.
        ULONG SecurityLevel = BTSecurityLevel_1;
        if (SOCKET_ERROR == setsockopt(sl, SOL_RFCOMM, SO_BTH_SET_SECURITY_LEVEL, (char *)&SecurityLevel, sizeof(SecurityLevel)))
        {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] Error setsockopt SO_BTH_SET_SECURITY_LEVEL: 0x%x\n", WSAGetLastError()));
            closesocket(sl);
            return FALSE;
        }

        SVSUTIL_ASSERT(sa.port);
        uiSDPRecordHandleCnt = MAX_NUM_STP_RECS;

        if (ERROR_SUCCESS != obutil_RegisterPort(
                (unsigned char)sa.port,
                &SDPRecordHandle[0],
                &uiSDPRecordHandleCnt,
                ppSdpRecord) ) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: failed to register port with SDP\n"));
            closesocket(sl);
            return FALSE;
        }
    } else {
        SVSUTIL_ASSERT (0);
        closesocket (sl);
        return FALSE;
    }

    if (listen (sl, 5)) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start::Listen failed (error=%d)!\n", WSAGetLastError()));
        closesocket (sl);
        return FALSE;
    }

    ++cSockets;

    if (! hThread)
        hThread = CreateThread (NULL, 0, ThreadListen, (void *)this, 0, NULL);

    if (! hThread) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start failed to create thread (error=0x%08x (%d))\n", GetLastError(), GetLastError ()));
        closesocket (sl);
        return FALSE;
    }


    DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Start - successfully started af %d\n", af));

    return TRUE;
}

int AddressFamily::Start (int af, char *szServiceName) {
    DEBUGMSG(ZONE_MAINTAIN, (L"[OBEX] GlobalData::Start :: IRDA\n"));

    SVSUTIL_ASSERT (gpState->IsLocked ());

    SVSUTIL_ASSERT (cSockets < OBEX_MAXPORTS);

    addFam[cSockets] = af;
    SOCKET sl = INVALID_SOCKET;

    for(int i=0; i < SOCKET_CREATION_ATTEMPTS; i++) {
        sl = aSocket[cSockets] = socket (af, SOCK_STREAM, 0);

        if(sl != INVALID_SOCKET)
            break;
        else {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: no sockets (error=%d)! -- retrying(%d/%d)\n", WSAGetLastError(), i, SOCKET_CREATION_ATTEMPTS));
            Sleep(1000);
        }

    }


    if (sl == INVALID_SOCKET) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: no sockets (error=%d)!\n", WSAGetLastError()));
        return FALSE;
    }

    if (af == AF_IRDA) {
        SOCKADDR_IRDA sa;

        memset(&sa, 0, sizeof(SOCKADDR_IRDA));
        sa.irdaAddressFamily      = AF_IRDA;

        if(strlen(szServiceName) + 1 > sizeof(sa.irdaServiceName))
        {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AF_IRDA: name too long! (%s)\n", szServiceName));
            closesocket (sl);
            return FALSE;
        }

        StringCchCopyA(sa.irdaServiceName, _countof(sa.irdaServiceName), szServiceName);

        if (bind (sl, (const struct sockaddr *)&sa, sizeof(sa))) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AF_IRDA: bind failed (error=%d)!\n", WSAGetLastError()));
            closesocket (sl);
             return FALSE;
        }

    } else {
        SVSUTIL_ASSERT (0);
        closesocket (sl);
        return FALSE;
    }

    if (listen (sl, 5)) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start::Listen failed (error=%d)!\n", WSAGetLastError()));
        closesocket (sl);
        return FALSE;
    }

    ++cSockets;

    if (! hThread)
        hThread = CreateThread (NULL, 0, ThreadListen, (void *)this, 0, NULL);

    if (! hThread) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start failed to create thread (error=0x%08x (%d))\n", GetLastError(), GetLastError ()));
        closesocket (sl);
        --cSockets;
        return FALSE;
    }


    DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Start - successfully started af %d\n", af));

    return TRUE;
}

int AddressFamily::Stop (void) {
    DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Stop\n"));

    SVSUTIL_ASSERT (gpState->IsLocked ());
    for (int i = 0 ; i < cSockets ; ++i)
        closesocket (aSocket[i]);
    cSockets = 0;

    return TRUE;
}

int AddressFamily::IsDown (void) {
    if (! hThread)
        return TRUE;

    if (WAIT_TIMEOUT == WaitForSingleObject (hThread, 1000))
        return FALSE;

    return TRUE;
}

void AddressFamily::Listen (void) {

    //
    //  If gpState isnt here, we have to quit!
    //
    if(!gpState) {
        DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Listen -- no GLOBAL STATE!\n"));
        return;
    }



    SOCKET sockarray[FD_SETSIZE];
    int l_cSockets = 0;
    int l_cAccepts = 0;

    DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Listen\n"));

    for ( ; ; )
    {
        gpState->Lock ();

        if (gpState->fState != RUNNING) {
            gpState->Unlock ();
            break;
        }

        //
        //    This is here so this is not accessed without a lock...
        //
        l_cSockets = l_cAccepts = cSockets;
        Connection *pConn = gpState->pconnList;
        while (pConn) {
            if (pConn->paf == this)
                ++l_cSockets;
            pConn = pConn->pNext;
        }

        int iSocket = 0;
        if (cSockets > FD_SETSIZE)    {    // Do not accept any more...
            DEBUGMSG(ZONE_WARN, (L"[OBEX] Listen: too many sockets - some connections will not be served: %d sockets\n", l_cSockets));
            l_cAccepts = 0;
        } else {                        // Copy
            iSocket = cSockets;
            memcpy (sockarray, aSocket, sizeof(SOCKET) * cSockets);
        }

        pConn = gpState->pconnList;
        while (pConn) {
            if (pConn->paf == this)
                sockarray[iSocket++] = pConn->s;

            if (iSocket >= FD_SETSIZE)
                break;

            pConn = pConn->pNext;
        }

        gpState->Unlock ();


        SVSUTIL_ASSERT (! gpState->IsLocked ());

        FD_SET fd;
        FD_ZERO (&fd);
        for (int i = 0 ; i < l_cSockets ; ++i)
        {
// Suppress the warning for the do {} while(0) in the FD_SET macro
#pragma warning(push)
#pragma warning(disable:4127)
            FD_SET(sockarray[i], &fd);
#pragma warning(pop)
        }

        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] AddressFamily::Listen thread reenters select %d accept %d read\n", l_cAccepts, l_cSockets - l_cAccepts));

        if (SOCKET_ERROR == select (0, &fd, NULL, NULL, NULL)) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Listen: select failed - %d\n", WSAGetLastError ()));
            continue;
        }


        if (gpState->fState != RUNNING)
            break;

        int fExit = FALSE;

        for (i = 0 ; (i < l_cSockets) && (! fExit) ; ++i) {
            if (obutil_PollSocket (sockarray[i]) == 1) {
                if (i < l_cAccepts) {    // This is an accept call
                    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] AddressFamily::Listen : incoming connection on %s at index %d\n", (addFam[i] == PF_INET || addFam[i] == PF_INET6) ? L"TCP/IP" : ((addFam[i] == AF_IRDA) ? L"IRDA" : L"Bluetooth"),i));

                    union {
                        SOCKADDR_STORAGE      sa_in;
                        SOCKADDR_IRDA         sa_irda;
                        SOCKADDR_BTH          sa_bth;
                        SOCKADDR              sa;
                    } sa;

                    int addrlen = 0;


                    if (addFam[i] == PF_INET || addFam[i] == PF_INET6)
                        addrlen = sizeof (SOCKADDR_STORAGE);
                    else if (addFam[i] == AF_IRDA)
                        addrlen = sizeof(SOCKADDR_IRDA);
                    else if (addFam[i] == AF_BT)
                        addrlen = sizeof(SOCKADDR_BTH);


                    SVSUTIL_ASSERT (addrlen);

                    SOCKET s = accept (sockarray[i], &sa.sa, &addrlen);

                    if (s != INVALID_SOCKET) {
                        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Accepted connection\n"));

                        gpState->Lock ();

                        if (gpState->fState != RUNNING) {
                            closesocket (s);
                            gpState->Unlock ();
                            fExit = TRUE;
                            break;
                        }

                        //
                        //    This is a new connection.
                        //
                        pConn = (Connection *)svsutil_GetFixed (gpState->pfmdConnections);
                        if (pConn) {
                            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] AddressFamily::Listen: added new connection for socket %d\n", s));
                            pConn->s = s;
                            pConn->addFam = addFam[i];
                            pConn->paf = this;
                            pConn->uiCurrentTransaction = 0;
                            pConn->uiConnectionId = OBEX_INVALID_CID;
                            pConn->pNext = gpState->pconnList;
                            pConn->tickLastActive = GetTickCount ();
                            pConn->pBuffer = NULL;
                            pConn->cBufSize = pConn->cFilled = 0;
                            pConn->cPeekFilled = 0;
                            gpState->pconnList = pConn;

                            gpState->SetMaintain();
                        } else {
                            DEBUGMSG(ZONE_ERROR, (L"[OBEX] Listen: failed to allocate structure for incoming connection (OOM)\n"));
                            closesocket (s);
                        }
                        gpState->Unlock ();
                    } else
                        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] AddressFamily::Listen incoming connection failed to materialize\n"));

                    continue;
                }


                //
                //    Got a packet... do a RECV...
                //
                gpState->Lock ();
                if (gpState->fState != RUNNING) {
                    gpState->Unlock ();
                    break;
                }

                pConn = gpState->pconnList;
                while (pConn) {
                    if ((pConn->paf == this) && (pConn->s == sockarray[i]))
                        break;

                    pConn = pConn->pNext;
                }


                if (! pConn) {
                    DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Listen packet on %s : nonexistent connection\n", (pConn->addFam == PF_INET || pConn->addFam == PF_INET6) ? L"TCP/IP" : ((pConn->addFam == AF_IRDA) ? L"IRDA" : L"Bluetooth")));
                    gpState->Unlock ();
                    continue;
                }

                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] AddressFamily::Listen got some data on %s at index %d\n", (pConn->addFam == PF_INET || pConn->addFam == PF_INET6) ? L"TCP/IP" : ((pConn->addFam == AF_IRDA) ? L"IRDA" : L"Bluetooth"),i));

                int ires;

                if (pConn->uiCurrentTransaction) {
                    DEBUGMSG(ZONE_ERROR, (L"Listen: received data for connection currently in transaction 0x%08x\n", pConn->uiCurrentTransaction));
                    gpState->CloseConnection (pConn);
                    gpState->Unlock ();
                    continue;
                }

                pConn->tickLastActive = GetTickCount ();

                if (! pConn->pBuffer) {
                    ires = recv (pConn->s, (char *)pConn->ucPeekBuff + pConn->cPeekFilled, sizeof(pConn->ucPeekBuff) - pConn->cPeekFilled, 0);

                    if (ires <= 0) {
#if defined (DEBUG) || defined (_DEBUG)
                        if (ires == 0)
                            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: connection %d closed at the request of the client\n", pConn->s));
                        else if (ires < 0)
                            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: connection %d closed because of socket error %d (RECV returns %d)\n", pConn->s, WSAGetLastError (), ires));
#endif
                        gpState->CloseConnection (pConn);
                        gpState->Unlock ();
                        continue;
                    }

                    DEBUGMSG(ZONE_PACKET, (L"[OBEX] Contents of recv'ed buffer: "));
                    DbgPrintBytes(pConn->ucPeekBuff+pConn->cPeekFilled, ires);

                    pConn->cPeekFilled += ires;
                    SVSUTIL_ASSERT (pConn->cPeekFilled <= sizeof(pConn->ucPeekBuff));

                    if (pConn->cPeekFilled < sizeof(pConn->ucPeekBuff)) {
                        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: insufficient data for a packet (%d out of 3), waiting for more\n", pConn->cPeekFilled));
                        gpState->Unlock ();
                        continue;
                    }

                    int iLen = (pConn->ucPeekBuff[1] << 8) | pConn->ucPeekBuff[2];

                    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: got full packet header, packet length is %d\n", iLen));

                    if (iLen < sizeof(pConn->ucPeekBuff))    { // close me...
                        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: connection %d closed because of incorrect packet size (%d bytes)\n", pConn->s, iLen));

                        gpState->CloseConnection (pConn);
                        gpState->Unlock ();
                        continue;
                    }

                    pConn->pBuffer = (unsigned char *)obex_Alloc (iLen);
                    if (! pConn->pBuffer) {
                        DEBUGMSG(ZONE_ERROR, (L"[OBEX] Listen: failed to allocate %d bytes for incoming packet\n", iLen));
                        gpState->CloseConnection (pConn);
                        gpState->Unlock ();
                        continue;
                    }

                    memcpy (pConn->pBuffer + pConn->cFilled, pConn->ucPeekBuff, sizeof(pConn->ucPeekBuff));
                    pConn->cPeekFilled = 0;
                    pConn->cFilled = sizeof(pConn->ucPeekBuff);
                    pConn->cBufSize = iLen;
                } else {    // receive the rest of the data into the buffer...
                    ires = recv (pConn->s, (char *)(pConn->pBuffer + pConn->cFilled), pConn->cBufSize - pConn->cFilled, 0);
                    if (ires <= 0) {
#if defined (DEBUG) || defined (_DEBUG)
                        if (ires == 0)
                            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: connection %d closed at the request of the client\n", pConn->s));
                        else if (ires < 0)
                            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: connection %d closed because of socket error %d (RECV returns %d)\n", pConn->s, WSAGetLastError (), ires));
#endif

                        gpState->CloseConnection (pConn);
                        gpState->Unlock ();
                        continue;
                    }

                    DEBUGMSG(ZONE_PACKET, (L"[OBEX] Contents of recv'ed buffer: "));
                    DbgPrintBytes(pConn->pBuffer+pConn->cFilled, ires);

                    pConn->cFilled += ires;
                    SVSUTIL_ASSERT (pConn->cFilled <= pConn->cBufSize);
                }

                if (pConn->cFilled != pConn->cBufSize) {
                    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: insufficient data for a packet (%d out of %d), waiting for more\n", pConn->cFilled, pConn->cBufSize));
                    gpState->Unlock ();
                    continue;
                }

                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Listen: got a packet, will service it now\n"));

                pConn->uiCurrentTransaction = ++gpState->uiTransactionId;
                if (! pConn->uiCurrentTransaction)
                    pConn->uiCurrentTransaction = ++gpState->uiTransactionId;

                SVSUTIL_ASSERT(0 != pConn->uiCurrentTransaction);
                gpState->ServiceRequest(pConn);
            }
        }

        if (fExit)
            break;
    }

    SVSUTIL_ASSERT (! gpState->IsLocked ());
    DEBUGMSG(ZONE_INIT, (L"[OBEX] AddressFamily::Listen exited\n"));
}

//
//    Server
//
int Server::CanUseConnection (Connection *pConn) {
    if (cProtSeq == 0)
        return TRUE;

    int af = pConn->addFam;
    for (int i = 0 ; i < cProtSeq ; ++i) {
        if (af == aProtSeq[i])
            return TRUE;
    }

    return FALSE;
}

int Server::Load (int iLen, unsigned char *pserviceid) {
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load -- attempting to load request handler\n"));

    SVSUTIL_ASSERT (iLen <= OBEX_SERVICEID_LEN);
    fServerStatus = NOT_INITED;

    int fFoundKey = FALSE;
    HKEY hk = NULL;

    //
    //    Try GUID
    //
    if (iLen == sizeof(GUID)) {
        GUID guid = *(GUID *)pserviceid;

        ReverseByteOrder (&guid.Data1, sizeof(guid.Data1));
        ReverseByteOrder (&guid.Data2, sizeof(guid.Data2));
        ReverseByteOrder (&guid.Data3, sizeof(guid.Data3));

        WCHAR szKeyName[SVSUTIL_ARRLEN(OBEX_KEY_SERVERS) + SVSUTIL_GUID_STR_LENGTH + 3];
        if (FAILED(StringCchPrintfW(szKeyName, SVSUTIL_ARRLEN(szKeyName),
                   OBEX_KEY_SERVERS L"\\{" SVSUTIL_GUID_FORMAT L"}",
                   SVSUTIL_PGUID_ELEMENTS ((&guid)))))
        {
            SVSUTIL_ASSERT(0); // Buffer should always be large enough.
            return FALSE;
        }

        SVSUTIL_ASSERT (wcslen(szKeyName) < SVSUTIL_ARRLEN(szKeyName));

        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load - trying %s\n", szKeyName));

        if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &hk))
            fFoundKey = TRUE;
    }

    //
    //    Can this be ASCII string?
    //
    if (! fFoundKey) {
        WCHAR szKeyName[SVSUTIL_ARRLEN(OBEX_KEY_SERVERS) + OBEX_SERVICEID_LEN + 3];
        int fCanBeASCII = TRUE;
        for (int i = 0 ; i < iLen ; ++i) {
            if ((pserviceid[i] < ' ') || (pserviceid[i] > 126)) {
                fCanBeASCII = FALSE;
                break;
            }
        }

        if (fCanBeASCII) {
            int iChars = MultiByteToWideChar (CP_ACP, 0, (char *)pserviceid, iLen, &szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS) + 2], OBEX_SERVICEID_LEN);
            if (iChars > 0) {
                memcpy (szKeyName, OBEX_KEY_SERVERS, sizeof(OBEX_KEY_SERVERS));
                szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS)] = '\\';
                szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS) + 1] = '{';
                szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS) + 2 + iChars] = '}';
                szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS) + 2 + iChars + 1] = '\0';

                SVSUTIL_ASSERT (wcslen(szKeyName) < SVSUTIL_ARRLEN(szKeyName));

                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load - trying %s\n", szKeyName));

                if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &hk))
                    fFoundKey = TRUE;
            }
        }
    }

    //
    //    As a last resort, try stringized id
    //
    if (! fFoundKey) {
        WCHAR szKeyName[SVSUTIL_ARRLEN(OBEX_KEY_SERVERS) + 2 * OBEX_SERVICEID_LEN + 1];
        memcpy (szKeyName, OBEX_KEY_SERVERS, sizeof(OBEX_KEY_SERVERS));
        szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS)] = '\\';
        WCHAR *p = &szKeyName[SVSUTIL_CONSTSTRLEN(OBEX_KEY_SERVERS) + 1];
        for (int i = 0 ; i < iLen ; ++i) {
            int c = pserviceid[i];
            c = (c >> 4) & 0xf;
            *p++ = (WCHAR)((c > 9) ? c + 'a' - 10 : c + '0');
            c = pserviceid[i];
            c = c & 0xf;
            *p++ = (WCHAR)((c > 9) ? c + 'a' - 10 : c + '0');
        }
        *p++ = '\0';
        SVSUTIL_ASSERT (p - szKeyName < SVSUTIL_ARRLEN(szKeyName));

        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load - trying %s\n", szKeyName));

        if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &hk))
            fFoundKey = TRUE;
    }

    //
    //    If still not there, fail
    //
    if (! fFoundKey) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load failed - no such server\n"));
        return FALSE;
    }

    memcpy (ucServerId, pserviceid, iLen);
    cServerId = iLen;

    //
    //    Now, read the values. We need: DLL name, protocol sequence
    //
    DWORD dwType;
    DWORD dwSize;
    dwSize = sizeof(aProtSeq);
    if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ProtSeq", 0, &dwType, (LPBYTE) aProtSeq, &dwSize)) &&
        (dwSize <= sizeof(aProtSeq)) && (dwType == REG_BINARY))
        cProtSeq = dwSize;
    else
        SVSUTIL_ASSERT (cProtSeq == 0);

    dwSize = sizeof(szName);
    if ((ERROR_SUCCESS != RegQueryValueEx (hk, L"Server", 0, &dwType, (LPBYTE) szName, &dwSize)) ||
        (dwSize > sizeof(szName)) || (dwType != REG_SZ)) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load failed - no dll name\n"));
        RegCloseKey (hk);
        return FALSE;
    }

    RegCloseKey (hk);

    hDll = LoadLibrary (szName);
    if (! hDll) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load failed - no such dll %s\n", szName));
        return FALSE;
    }

    pServiceCallback = (ServiceCallback)GetProcAddress (hDll, GPAARG("ServiceCallback"));

    if (pServiceCallback == NULL) {
            return FALSE;
    }

    SVSUTIL_ASSERT (! iCallRef);

    tickLastActive = GetTickCount ();

    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Load - succeeded, loaded %s\n", szName));
    return TRUE;
}

int Server::Unload (void) {
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::Unload : unloading %s\n", szName));

    SVSUTIL_ASSERT (! pNext);
    SVSUTIL_ASSERT (! iCallRef);

    if (fServerStatus == OPERATING) {
        PREFAST_ASSERT (pServiceCallback);

        ObexTransaction o;
        memset (&o, 0, sizeof(o));
        o.uiOp = OBEX_REQ_UNLOAD;
        o.ObexAlloc = obex_Alloc;
        o.ObexFree  = obex_Free;
        //    no ObexExecute!
        ++iCallRef;
        __try {
            if(pServiceCallback) {
                pServiceCallback (&o);
            }
        } __except (1) {
            DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::Unload - server %s excepted on unload\n", szName));
        }
        --iCallRef;
    }

    if (hDll)    {
        if(0 == FreeLibrary (hDll)) {
            DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::Unloading module DLL failed!"));
        }
        else {
            DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::Unloading module DLL succeeded."));
        }
        pServiceCallback = NULL;
        fServerStatus = FAILED;
    }

    memset (this, 0, sizeof(*this));
    return TRUE;
}

int Server::DispatchCall (unsigned int uiCId, Connection *pConn, ObexParser *pParser) {
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x\n", szName, uiCId));

    //
    //    if uiCId != OBEX_INVALID_CID and pConn & pParser == 0, then we need to signal closing the connection
    //    if uiCId == OBEX_INVALID_CID and pConn & pParser != 0, then we need to allocate new association.
    //  if pConn != OBEX_INVALID_CID then pParser != 0 and we need to parse and supply data in ObexTransaction
    //    when client returns, if pConn != NULL and pConn->uiTransactionId != 0, we need to
    //    reject the call - server screwed up.
    //
    if (! pConn) {
        SVSUTIL_ASSERT (! pParser);
    }
    else {
        SVSUTIL_ASSERT (pParser);
    }

    ObexTransaction o;
    memset (&o, 0, sizeof(0));
    o.ObexAlloc = obex_Alloc;
    o.ObexFree = obex_Free;

    if ((uiCId != OBEX_INVALID_CID) && (! pConn)) {    // Association is already removed, we need to just signal.
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x : OBEX_REQ_CLOSE\n", szName, uiCId));

        int iRet = FALSE;

        if (fServerStatus == OPERATING) {
            o.uiOp = OBEX_REQ_CLOSE;
            o.uiConnectionId = uiCId;
            //    no ObexExecute!
            ++iCallRef;
            __try {
                if(pServiceCallback) {
                    iRet = pServiceCallback (&o);
                }
            } __except (1) {
                DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - server %s excepted (close connection)\n", szName));
            }
            --iCallRef;
        } else
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Skipping failed server %s CId 0x%08x : OBEX_REQ_CLOSE\n", szName, uiCId));

        gpState->Unlock ();
        return iRet;
    }

    if ((uiCId != OBEX_INVALID_CID) && (fServerStatus != OPERATING)) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x : has never initialized; skipping the call\n", szName, uiCId));
        gpState->SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
        return FALSE;
    }

    if (fServerStatus == FAILED) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x : server has failed in the past, skipping call\n", szName, uiCId));
        gpState->SendTrivialResponse (pConn, OBEX_STAT_SERVICEUNAVAIL | OBEX_OP_ISFINAL);
        return FALSE;
    }


    //see if we have a default connection for the packet
    //  if so, we set uiCId to the stored connection ID and continue
    if (uiCId == OBEX_INVALID_CID) {
        Association *pA = gpState->passocList;
        while (pA) {
            if ((pA->pConn == pConn) && (pA->pServer == this) && pA->fDefaultConnection)
                break;
            pA = pA->pNext;
        }

        if (pA && pParser->Op() == OBEX_OP_CONNECT)
        {
           DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : would like to reassociated id but this is the second CONNECT... must reject\n"));
           gpState->SendTrivialResponse (pConn, OBEX_STAT_FORBIDDEN);
           return FALSE;
        }
        else if(pA)
        {
            uiCId = pA->uiConnectionId;
            pA->uiTransactionId = pConn->uiCurrentTransaction;
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x (reassociated the id)\n", szName, uiCId));
        }
    }

    //
    //  if this connection has no connection ID and we couldnt find a default, allocate a new association
    //     by finding a unused connectionID
    if (uiCId == OBEX_INVALID_CID) {
        for ( ; ; ) {
            uiCId = ++ gpState->uiConnectionId;

            while ((! uiCId) || (uiCId == OBEX_INVALID_CID))
                uiCId = ++ gpState->uiConnectionId;

            Association *pA = gpState->passocList;
            while (pA) {
                if (pA->uiConnectionId == uiCId)
                    break;
                pA = pA->pNext;
            }

            if (! pA)
                break;

            pA->uiTransactionId = pConn->uiCurrentTransaction;
        }
        SVSUTIL_ASSERT (uiCId != OBEX_INVALID_CID);

        //
        // Create an association and chain it to the list of used associations
        //
        Association *pA = (Association *)svsutil_GetFixed (gpState->pfmdAssociations);
        if (! pA) {
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - OOM allocating association\n"));
            gpState->SendTrivialResponse (pConn, OBEX_STAT_INTERNALERROR | OBEX_OP_ISFINAL);
            return FALSE;
        }

        pA->pConn = pConn;
        pA->pServer = this;
        pA->uiConnectionId = uiCId;
        pA->fDefaultConnection = FALSE;
        pA->pNext = gpState->passocList;
        pA->uiTransactionId = pConn->uiCurrentTransaction;
        gpState->passocList = pA;
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x (created new association)\n", szName, uiCId));
    }


    //
    //    Execute real request...
    //
    PREFAST_ASSERT ((uiCId != OBEX_INVALID_CID) && pConn && pParser);
    SVSUTIL_ASSERT (pConn->uiCurrentTransaction);
    SVSUTIL_ASSERT (pConn->cBufSize >= 3);
    SVSUTIL_ASSERT (pConn->cFilled == pConn->cBufSize);
    o.ObexAuthRequest = obex_Auth;
    o.ObexExecute = obex_Execute;
    o.ObexEncryptionRequest = obex_EncryptionRequest;
    o.ObexComputeHash = obex_MD5_Helper;
    o.ObexGetRemoteInfo = obex_GetBtRemoteInfo;
    o.uiConnectionId = uiCId;
    o.uiTransactionId = pConn->uiCurrentTransaction;
    o.uiOp = OBEX_REQ_REQUEST;

    ObexCommand oc;

    memset (&oc, 0, sizeof(oc));
    oc.uiOp = pParser->Op () & OBEX_OP_OPMASK;
    oc.fFinal = pParser->Op () & OBEX_OP_ISFINAL;

    if (! pParser->GetCommandData (&oc.sPktData)) {
        DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::DispatchCall - packet is broken\n"));
        gpState->SendTrivialResponse (pConn, OBEX_STAT_BADREQUEST | OBEX_OP_ISFINAL);
        return FALSE;
    }

    o.pObex = &oc;

    //    Below code assumes the stricter or equal alignment requirements on ObexVariant than on int.
    SVSUTIL_ASSERT (sizeof(ObexVariant) >= sizeof(unsigned int));

    int iBufferSize = 0;
    while (! pParser->__EOF ()) {
        switch (pParser->Type ()) {
        case OBEX_TYPE_UNICODE:
            iBufferSize = (iBufferSize + 1) & (~1);
            iBufferSize += (pParser->Length () ? pParser->Length () : 2);
            break;

        case OBEX_TYPE_BYTESEQ:
            iBufferSize = (iBufferSize + 7) & (~7);
            iBufferSize += pParser->Length ();
            break;
        case OBEX_TYPE_BYTE:
        case OBEX_TYPE_DWORD:
            break;
        default:
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - packet is broken\n"));
            gpState->SendTrivialResponse (pConn, OBEX_STAT_BADREQUEST | OBEX_OP_ISFINAL);
            return FALSE;
        }

        ++oc.cProp;

        if (! pParser->Next ())
            break;
    }

    void *pAllocBuffer = NULL;

    if (oc.cProp > 0) {
        int iArraySize = (sizeof (ObexVariant) + sizeof(unsigned int)) * oc.cProp;
        iArraySize = (iArraySize + 7) & (~7);

        UINT uiAllocSize = 0;
        if(FAILED(UIntAdd(iArraySize, iBufferSize, &uiAllocSize))) {
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - overflow allocating buffer\n"));
            gpState->SendTrivialResponse (pConn, OBEX_STAT_INTERNALERROR | OBEX_OP_ISFINAL);
            return FALSE;
        }

        pAllocBuffer = obex_Alloc (uiAllocSize);

        if(!pAllocBuffer) {
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - OOM allocating buffer\n"));
            gpState->SendTrivialResponse (pConn, OBEX_STAT_INTERNALERROR | OBEX_OP_ISFINAL);
            return FALSE;
        }

        oc.aPropVar = (ObexVariant *)pAllocBuffer;
        oc.aPropID = (unsigned int *)&oc.aPropVar[oc.cProp];

        unsigned char *p = ((unsigned char *)oc.aPropVar) + iArraySize;


        SVSUTIL_ASSERT ((unsigned long)p >= (unsigned long)&oc.aPropID[oc.cProp]);

        pParser->ObexParser::ObexParser (pConn->pBuffer, pConn->cBufSize); // recharge

        int iProp = 0;

        int fParseError = FALSE;
        int iBufferSize2 = 0;

        while (! pParser->__EOF ()) {
            oc.aPropID[iProp] = pParser->Code ();

            switch (pParser->Type ()) {
            case OBEX_TYPE_UNICODE:
                {
                iBufferSize2 = (iBufferSize2 + 1) & (~1);
                int cChars = pParser->Length() / sizeof(WCHAR);
                SVSUTIL_ASSERT (cChars >= 0);

                if (cChars == 0)
                    cChars = 1;

                if (! pParser->GetString ((WCHAR *)(p + iBufferSize2), cChars)) {
                    fParseError = TRUE;
                    break;
                }
                oc.aPropVar[iProp].pwsz = (WCHAR *)(p + iBufferSize2);
                iBufferSize2 += cChars * sizeof(WCHAR);

                //now convert the string to native byte ordering
                for(unsigned int i=0; i<wcslen(oc.aPropVar[iProp].pwsz); i++)
                    oc.aPropVar[iProp].pwsz[i] = ntohs(oc.aPropVar[iProp].pwsz[i]);
                }

                break;

            case OBEX_TYPE_BYTESEQ:
                iBufferSize2 = (iBufferSize2 + 7) & (~7);
                if (! pParser->GetBytes (p + iBufferSize2, pParser->Length())) {
                    fParseError = TRUE;
                    break;
                }
                oc.aPropVar[iProp].caub.cuc = pParser->Length ();
                oc.aPropVar[iProp].caub.puc = p + iBufferSize2;
                iBufferSize2 += pParser->Length ();
                break;

            case OBEX_TYPE_BYTE:
                if (! pParser->GetBYTE (&oc.aPropVar[iProp].uc))
                    fParseError = TRUE;
                break;

            case OBEX_TYPE_DWORD:
                if (! pParser->GetDWORD ((DWORD *)&oc.aPropVar[iProp].ui))
                    fParseError = TRUE;
                break;

            default:
                SVSUTIL_ASSERT (0);
            }

            if (fParseError) {
                if (pAllocBuffer)
                    obex_Free (pAllocBuffer);

                DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::DispatchCall - packet is broken on get\n"));
                gpState->SendTrivialResponse (pConn, OBEX_STAT_BADREQUEST | OBEX_OP_ISFINAL);
                return FALSE;
            }

            ++iProp;

            if (! pParser->Next ())
                break;
        }

        SVSUTIL_ASSERT (iBufferSize2 == iBufferSize);
        SVSUTIL_ASSERT (iProp == (int)oc.cProp);
    }

    unsigned int uiTrid = pConn->uiCurrentTransaction;
    if (fServerStatus == NOT_INITED) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x : OBEX_REQ_INIT\n", szName, uiCId));
        ObexTransaction oInit;
        memset (&oInit, 0, sizeof(oInit));
        oInit.ObexAlloc = obex_Alloc;
        oInit.ObexFree = obex_Free;
        oInit.uiOp = OBEX_REQ_INIT;
        ++iCallRef;
        int iRet = 0;

        __try {
            if(pServiceCallback) {
                iRet = pServiceCallback (&oInit);
            }
        } __except (1) {
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - server %s excepted in initialization\n", szName));
            iRet = FALSE;
        }

        --iCallRef;

        if (iRet)
            fServerStatus = OPERATING;
        else
            fServerStatus = FAILED;
    }

    if ((gpState->fState == RUNNING) && (fServerStatus == OPERATING)) {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] Server::DispatchCall : Server %s CId 0x%08x : OBEX_REQ_REQUEST\n", szName, uiCId));

        ++iCallRef;

        __try {
            if(pServiceCallback) {
                pServiceCallback (&o);
            }
        } __except (1) {
            DEBUGMSG(ZONE_WARN, (L"Server::DispatchCall - server %s excepted in servicing request\n", szName));
        }
        --iCallRef;
    }

    if (pAllocBuffer)
        obex_Free (pAllocBuffer);

    if (gpState->fState != RUNNING) {
        gpState->Unlock ();
        return FALSE;
    }

    pConn = gpState->pconnList;
    while (pConn) {
        if (pConn->uiCurrentTransaction == uiTrid)
            break;

        pConn = pConn->pNext;
    }

    if (pConn) {    // Failed to clear up transaction...
        fServerStatus = FAILED;
        DEBUGMSG(ZONE_WARN, (L"[OBEX] Server::DispatchCall - request not handled by the server\n"));
        gpState->SendTrivialResponse (pConn, OBEX_STAT_INTERNALERROR | OBEX_OP_ISFINAL);
        return FALSE;
    }

    gpState->Unlock ();
    return TRUE;
}


//
//  Server authentication callback
//
static HRESULT obex_Auth(unsigned int uiTransactionId, UINT *uiRetCode)
{
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth \n"));

    HRESULT hr = E_FAIL;
    Connection *pConn = NULL;


    ASSERT(uiRetCode);

    if(!uiRetCode)
        return E_INVALIDARG;

    *uiRetCode = 0xFFFFFFFF;


    gpState->Lock ();
    if (gpState->fState != RUNNING) {
        gpState->Unlock ();
        goto Cleanup;
    }

    pConn = gpState->pconnList;
    while (pConn) {
        if (pConn->uiCurrentTransaction == uiTransactionId)
            break;
        pConn = pConn->pNext;
    }

    if(!pConn) {
        gpState->Unlock ();
        goto Cleanup;
    }


    if (pConn->addFam == AF_BT) {
#ifndef SDK_BUILD
        char ret[4];
        int len = 4;
        int iSockRet = setsockopt (pConn->s,
                        SOL_RFCOMM,
                        SO_BTH_AUTHENTICATE,
                        ret,
                        len);
#else //<-- PPC doesnt support the setsockopt() function
        typedef int (WINAPI *PFN_BTHAUTH) (BT_ADDR *pba);


        SOCKADDR_BTH    btAddr;
        int iSockRet;
        INT size = sizeof(btAddr);
        if(-1 == getpeername(pConn->s, (sockaddr *)&btAddr, &size))
        {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - request for BT socket %d FAILED with %x\n", pConn->s, GetLastError()));
            gpState->Unlock ();
            goto Cleanup;
        }

        HINSTANCE hLib = LoadLibrary(L"btdrt.dll");
        if (hLib)
        {
            PFN_BTHAUTH pBthAuth =
            (PFN_BTHAUTH) GetProcAddressW(hLib, TEXT("BthAuthenticate"));

            iSockRet = pBthAuth(&btAddr.btAddr);
            FreeLibrary(hLib);
        }
        else
        {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - error loading btdrt.dll\n"));
            gpState->Unlock ();
            goto Cleanup;
        }
#endif


        if(iSockRet != 0) {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - request for BT socket %d FAILED with %x\n", pConn->s, GetLastError()));
            gpState->Unlock ();
            hr = S_OK;
            *uiRetCode = 0;
            goto Cleanup;
        }
        else {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - request for BT socket %d SUCCEEDED\n", pConn->s));
            gpState->Unlock ();
            hr = S_OK;
            *uiRetCode = 1;
            goto Cleanup;
        }
    }
    else {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - requested AF that doesn't support authentication..\n"));
        gpState->Unlock ();
        hr = S_OK;
        *uiRetCode = 2;
        goto Cleanup;
    }

Cleanup:
    return hr;
}


//
//  Server authentication callback
//
static HRESULT obex_EncryptionRequest(unsigned int uiTransactionId, UINT *uiRetCode)
{
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_EncryptionRequest \n"));

    HRESULT hr = E_FAIL;
    Connection *pConn = NULL;


    ASSERT(uiRetCode);

    if(!uiRetCode)
        return E_INVALIDARG;

    *uiRetCode = 0xFFFFFFFF;


    gpState->Lock ();
    if (gpState->fState != RUNNING) {
        gpState->Unlock ();
        goto Cleanup;
    }

    pConn = gpState->pconnList;
    while (pConn) {
        if (pConn->uiCurrentTransaction == uiTransactionId)
            break;
        pConn = pConn->pNext;
    }

    if(!pConn) {
        gpState->Unlock ();
        goto Cleanup;
    }


    if (pConn->addFam == AF_BT) {
#ifndef SDK_BUILD
        int fVal = 1;
        int len = 4;
        int iSockRet = setsockopt (pConn->s,
                        SOL_RFCOMM,
                        SO_BTH_ENCRYPT,
                        (const char *)&fVal,
                        len);

        if(iSockRet != 0) {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - request for BT socket %d FAILED with %x\n", pConn->s, GetLastError()));
            gpState->Unlock ();
            hr = S_OK;
            *uiRetCode = 0;
            goto Cleanup;
        }
        else {
            DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - request for BT socket %d SUCCEEDED\n", pConn->s));
            gpState->Unlock ();
            hr = S_OK;
            *uiRetCode = 1;
            goto Cleanup;
        }
#else
    *uiRetCode = 2;
    hr = S_OK;
    goto Cleanup;
#endif

    }
    else {
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Auth - requested AF that doesnt support authentication..\n"));
        gpState->Unlock ();
        hr = S_OK;
        *uiRetCode = 2;
        goto Cleanup;
    }

Cleanup:
    return hr;
}



//
//    Server Callback
//
static int obex_Execute   (unsigned int uiOp, unsigned int uiCId, struct _obex_command *pCommand) {
    DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : op %s CId 0x%08x\n",
          (uiOp == OBEX_RESP_RESPOND ? L"OBEX_RESP_RESPOND" :
          (uiOp == OBEX_RESP_OK ? L"OBEX_RESP_OK" :
          (uiOp == OBEX_RESP_ACCEPT) ? L"OBEX_RESP_ACCEPT" :
          (uiOp == OBEX_RESP_REJECT ? L"OBEX_RESP_REJECT" :
          (uiOp == OBEX_RESP_DENY ? L"OBEX_RESP_DENY" :
          (uiOp == OBEX_RESP_CONTINUE ? L"OBEX_RESP_CONTINUE" :
          (uiOp == OBEX_RESP_ABORT ? L"OBEX_RESP_ABORT" :
          (uiOp == OBEX_RESP_DISCONNECT ? L"OBEX_RESP_DISCONNECT" :
          (uiOp == OBEX_RESP_HANGUP ? L"OBEX_RESP_HANGUP" : 
          (uiOp == OBEX_RESP_NOTFOUND ? L"OBEX_RESP_NOTFOUND" :
          (uiOp == OBEX_RESP_UNAUTHORIZED ? L"OBEX_RESP_UNAUTHORIZED" : L"UNKNOWN")))))))))),
          uiCId));

    gpState->Lock ();
    if (gpState->fState != RUNNING) {
        gpState->Unlock ();
        return FALSE;
    }

    Connection *pConn = NULL;

    if ((uiOp == OBEX_RESP_HANGUP) || (uiOp == OBEX_RESP_DISCONNECT)) {
        Association *pAssoc = gpState->passocList;
        Association *pParent = NULL;
        while (pAssoc) {
            if (pAssoc->uiConnectionId == uiCId)
                break;

            pParent = pAssoc;
            pAssoc = pAssoc->pNext;
        }

        if (! pAssoc) {
            gpState->Unlock ();
            DEBUGMSG(ZONE_WARN, (L"[OBEX] obex_Execute : hangup or disconnect - association does not exist for 0x%08x\n", uiCId));
            return FALSE;
        }

        pConn = pAssoc->pConn;

        if (! pParent)
            gpState->passocList = gpState->passocList->pNext;
        else
            pParent->pNext = pAssoc->pNext;

        svsutil_FreeFixed (pAssoc, gpState->pfmdAssociations);
        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : destroyed association for 0x%08x\n", uiCId));
    } else {
        pConn = gpState->pconnList;
        while (pConn) {
            if (pConn->uiCurrentTransaction == uiCId)
                break;
            pConn = pConn->pNext;
        }

        if (! pConn) {
            gpState->Unlock ();
            return FALSE;
        }
    }


    //no matter the response do a check to see if this is a CONNECT / (PUT/GET/SETPATH)
    // if its a CONNECT *AND* they dont specify a connection ID here make the connection
    // a default
    //
    //if its a (PUT/GET/SETPATH) *AND* no CONNECT has been received for this connection
    // make the connection a default
    if(pConn && pConn->pBuffer && pConn->cBufSize && uiOp != OBEX_RESP_HANGUP && uiOp != OBEX_RESP_DISCONNECT)
    {
        //start by finding an association
        Association *pA = gpState->passocList;
        while (pA)
        {
            if (pA->uiTransactionId == uiCId)
                break;
            pA = pA->pNext;
        }
        ASSERT(pA);
        pA->uiTransactionId = 0;


        ObexParser p(pConn->pBuffer, pConn->cBufSize);

        if(pCommand &&
           p.Op () == OBEX_OP_CONNECT &&
           ((pCommand->cProp && pCommand->aPropID[0] != OBEX_HID_CONNECTIONID) || 0 == pCommand->cProp))
        {
            ASSERT(pA);
            if(pA && pCommand->uiResp == OBEX_STAT_OK)
            {
               //cant have 2 connections over the same socket
               // when the service doesnt support connection ID's
               if(pA->fDefaultConnection)
               {
                    ASSERT(FALSE);
                    uiOp = OBEX_RESP_REJECT;
               }
               else
               {
                   pA->fDefaultConnection = TRUE;
               }
            }
        }
        else if(pCommand && p.Op () == OBEX_OP_DISCONNECT)
        {
            ASSERT(pA);
            if(pA)
            {
               //if its a disconnect, drop the default connection
               //  this is required so that multiple connections
               //  can occur from one socket even when connection
               //  ID's arent being used (CONNECT <stuff> DISCONNECT CONNECT <stuff> DISCONNECT)
               //  otherwise, due to the prevention of 2 CONNECTS w/o connID the second
               //  would be rejected (see previous if statment)
               pA->fDefaultConnection = FALSE;
            }
        }
        else if(pCommand &&
               (
                 ((p.Op () & OBEX_OP_OPMASK) == OBEX_OP_PUT) ||
                 ((p.Op () & OBEX_OP_OPMASK) == OBEX_OP_GET) ||
                  (p.Op () == OBEX_OP_SETPATH)
               ))
         {
                if (!p.IsA (OBEX_HID_CONNECTIONID))
                {
                        pA->fDefaultConnection = TRUE;
                        DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : marking association/connection as default (put has no connectionID) : 0x%08x\n", uiCId));
                }
         }
     }



    switch (uiOp) {
        //    Fall through...
    case OBEX_RESP_HANGUP:
    case OBEX_RESP_DISCONNECT:
    case OBEX_RESP_RESPOND:
        if (! pCommand) {
            if (uiOp == OBEX_RESP_HANGUP) {
                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : hangup : will remove physical connection 0x%08x from the list\n", uiCId));

                Connection *pParent = NULL;
                Connection *pRun = gpState->pconnList;
                while (pRun) {
                    if (pConn == pRun)
                        break;

                    pParent = pRun;
                    pRun = pRun->pNext;
                }

                if (!pRun) {
                    SVSUTIL_ASSERT (0);
                } else {
                    if (!pParent)
                        gpState->pconnList = gpState->pconnList->pNext;
                    else
                        pParent->pNext = pRun->pNext;
                }

                closesocket(pConn->s);
                // Cleanup the physical connection we took out from the list just before send()
                svsutil_FreeFixed (pConn, gpState->pfmdConnections);
            } else {
                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : disconnect or respond : nothing to process for 0x%08x\n", uiCId));
            }
            gpState->Unlock ();
            return uiOp == OBEX_RESP_RESPOND ? FALSE : TRUE;
        }

        {
            unsigned char *pBuffer = NULL;
            int iSize;
            __try {
                iSize = 3;
                if (pCommand->uiOp == (OBEX_OP_CONNECT & OBEX_OP_OPMASK))
                    iSize += 4;

                for (int i = 0 ; i < (int)pCommand->cProp ; ++i) {
                    switch (pCommand->aPropID[i] & OBEX_TYPE_MASK) {
                    case OBEX_TYPE_UNICODE:
                        iSize += 3 + (wcslen (pCommand->aPropVar[i].pwsz) + 1) * sizeof(WCHAR);
                        break;
                    case OBEX_TYPE_BYTESEQ:
                        iSize += 3 + pCommand->aPropVar[i].caub.cuc;
                        break;
                    case OBEX_TYPE_BYTE:
                        iSize += 2;
                        break;
                    case OBEX_TYPE_DWORD:
                        iSize += 5;
                        break;
                    default:
                        gpState->Unlock ();

                        return FALSE;
                    }
                }

                DEBUGMSG(ZONE_PROTOCOL, (L"[OBEX] obex_Execute : packet size %d for 0x%08x\n", iSize, uiCId));
                pBuffer = (unsigned char *)obex_Alloc (iSize);
                if (! pBuffer) {
                    DEBUGMSG(ZONE_ERROR, (L"[OBEX] obex_Execute : packet size %d too big / OOM for 0x%08x\n", iSize, uiCId));
                    gpState->Unlock ();
                    return FALSE;
                }

                unsigned char *p = pBuffer;

                *p++ = (unsigned char)(pCommand->uiResp | (pCommand->fFinal ? OBEX_OP_ISFINAL : 0));
                *p++ = (iSize >> 8) & 0xff;
                *p++ = iSize & 0xff;

                if (pCommand->uiOp == (OBEX_OP_CONNECT & OBEX_OP_OPMASK)) {
                    *p++ = pCommand->sPktData.ConnectRequestResponse.version;
                    *p++ = pCommand->sPktData.ConnectRequestResponse.flags;
                    *p++ = (pCommand->sPktData.ConnectRequestResponse.maxlen >> 8) & 0xff;
                    *p++ = (pCommand->sPktData.ConnectRequestResponse.maxlen) & 0xff;
                }

                for (i = 0 ; i < (int)pCommand->cProp ; ++i) {
                    *p++ = (unsigned char)(pCommand->aPropID[i]);
                    switch (pCommand->aPropID[i] & OBEX_TYPE_MASK) {
                    case OBEX_TYPE_UNICODE:
                        {
                            int iLen = (wcslen (pCommand->aPropVar[i].pwsz) + 1) * sizeof(WCHAR) + 3;
                            *p++ = (iLen >> 8) & 0xff;
                            *p++ = iLen & 0xff;
                            iLen -= 3;
                            memcpy (p, pCommand->aPropVar[i].pwsz, iLen);
                            p += iLen;
                        }
                        break;
                    case OBEX_TYPE_BYTESEQ:
                        {
                            int iLen = pCommand->aPropVar[i].caub.cuc + 3;
                            *p++ = (iLen >> 8) & 0xff;
                            *p++ = iLen & 0xff;
                            iLen -= 3;
                            memcpy (p, pCommand->aPropVar[i].caub.puc, iLen);
                            p += iLen;
                        }
                        break;
                    case OBEX_TYPE_BYTE:
                        *p++ = pCommand->aPropVar[i].uc;
                        break;
                    case OBEX_TYPE_DWORD:
                        *p++ = (pCommand->aPropVar[i].ui >> 24) & 0xff;
                        *p++ = (pCommand->aPropVar[i].ui >> 16) & 0xff;
                        *p++ = (pCommand->aPropVar[i].ui >> 8) & 0xff;
                        *p++ = (pCommand->aPropVar[i].ui) & 0xff;
                        break;
                    default:
                        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obex_Execute : unknown property 0x%02x for 0x%08x\n", pCommand->aPropID[i] & OBEX_TYPE_MASK, uiCId));
                        gpState->Unlock ();

                        return FALSE;
                    }
                    SVSUTIL_ASSERT (p - pBuffer <= iSize);
                }

            } __except (1) {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] obex_Execute : exception processing 0x%08x\n", uiCId));
                gpState->Unlock ();
                return FALSE;
            }

            //
            //    Clean up - we've responded...
            //
            PREFAST_ASSERT(pConn);
            SVSUTIL_ASSERT (pConn->uiCurrentTransaction);
            SVSUTIL_ASSERT (pConn->pBuffer);
            SVSUTIL_ASSERT (pConn->cBufSize >= 3);
            SVSUTIL_ASSERT (pConn->cFilled == pConn->cBufSize);

            SOCKET s = pConn->s;

            pConn->uiCurrentTransaction = 0;    // We are responding...
            pConn->cBufSize = pConn->cFilled = 0;
            obex_Free (pConn->pBuffer);
            pConn->pBuffer = NULL;

            DEBUGMSG(ZONE_PACKET, (L"[OBEX] obex_Execute : sending packet (%d bytes) for 0x%08x\n", iSize, uiCId));
            DbgPrintBytes(pBuffer, iSize);

            //
            // If this is a hangup, remove it from the physical connection list.
            //
            if (uiOp == OBEX_RESP_HANGUP) {
                Connection *pParent = NULL;
                Connection *pRun = gpState->pconnList;
                while (pRun) {
                    if (pConn == pRun)
                        break;

                    pParent = pRun;
                    pRun = pRun->pNext;
                }

                if (!pRun) {
                    SVSUTIL_ASSERT (0);
                } else {
                    if (!pParent)
                        gpState->pconnList = gpState->pconnList->pNext;
                    else
                        pParent->pNext = pRun->pNext;
                }

                svsutil_FreeFixed (pConn, gpState->pfmdConnections);
            }

            gpState->Unlock ();

            int iSentSoFar = 0;
            while (iSentSoFar < iSize) {
                int iRes = send(s, (char *)&pBuffer[iSentSoFar], iSize - iSentSoFar, 0);

                if (iRes <= 0 || iRes == SOCKET_ERROR )
                    break;
                iSentSoFar += iRes;
            }

            obex_Free (pBuffer);

            if (uiOp == OBEX_RESP_HANGUP)
                closesocket (s);

            return iSentSoFar == iSize;
        }

        case OBEX_RESP_OK:
        case OBEX_RESP_ACCEPT:
        case OBEX_RESP_REJECT:
        case OBEX_RESP_DENY:
        case OBEX_RESP_CONTINUE:
        case OBEX_RESP_ABORT:
        case OBEX_RESP_NOTFOUND:
        case OBEX_RESP_UNAUTHORIZED:
            {
                if (pCommand) {
                    gpState->Unlock ();
                    return FALSE;
                }

                unsigned int op = (uiOp == OBEX_RESP_OK) ? OBEX_STAT_OK :
                                  (uiOp == OBEX_RESP_ACCEPT) ? OBEX_STAT_ACCEPTED :
                                  (uiOp == OBEX_RESP_REJECT) ? OBEX_STAT_FORBIDDEN :
                                  (uiOp == OBEX_RESP_DENY) ? OBEX_STAT_NOTALLOWED :
                                  (uiOp == OBEX_RESP_NOTFOUND) ? OBEX_STAT_NOTFOUND :
                                  (uiOp == OBEX_RESP_UNAUTHORIZED) ? OBEX_STAT_UNAUTHORIZED :
                                  (uiOp == OBEX_RESP_CONTINUE) ? OBEX_STAT_CONTINUE : 0xff;

                return gpState->SendTrivialResponse (pConn, op | OBEX_OP_ISFINAL);
            }
            break;
    }

    gpState->Unlock ();

    return FALSE;
}

//
//    Auxiliary functions
//
CRITICAL_SECTION g_csMemory;

static void *obex_Alloc (int cSize) {
    EnterCriticalSection (&g_csMemory);

    void *pRes = g_funcAlloc (cSize, g_pvAllocData);

    LeaveCriticalSection (&g_csMemory);
    return pRes;
}

static void obex_Free (void *p) {
    EnterCriticalSection (&g_csMemory);

    g_funcFree (p, g_pvFreeData);

    LeaveCriticalSection (&g_csMemory);
}

#ifndef DEVICE_DRIVER
static HANDLE hMasterSwitch = NULL;

static DWORD WINAPI ConsoleThread (LPVOID pNull) {
    for ( ; ; ) {
        SVSUTIL_ASSERT (! gpState->IsLocked ());

        wprintf (L"OBEX> ");
        WCHAR buffer[200];
        _getws (buffer);

        if (wcsicmp (buffer, L"exit") == 0) {
            SetEvent (hMasterSwitch);
            break;
        }

        if (! gpState) {
            wprintf (L"Not initialized\n");
            continue;
        }

        gpState->Lock ();
        if (wcsicmp (buffer, L"dump assoc") == 0) {
            Association *pA = gpState->passocList;
            while (pA) {
                wprintf (L"Id:         0x%08x\n", pA->uiConnectionId);
                wprintf (L"Connection: %d\n", pA->pConn->s);
                wprintf (L"Server:     %s\n\n", pA->pServer->szName);
                pA = pA->pNext;
            }
        } else if (wcsicmp (buffer, L"dump servers") == 0) {
            Server *pServ = gpState->pservList;
            while (pServ) {
                wprintf (L"Name:       %s\n", pServ->szName);
                wprintf (L"Handle:     0x%08x\n", pServ->hDll);
                wprintf (L"Callback:   0x%08x\n", pServ->pServiceCallback);
                wprintf (L"Call ref:   %d\n", pServ->iCallRef);
                wprintf (L"Time stamp: %08x%\n", pServ->tickLastActive);
                wprintf (L"Prot seq:   ");
                for (int i = 0 ; i < pServ->cProtSeq ; ++i)
                    wprintf (L"%d ", pServ->aProtSeq[i]);
                wprintf (L"\n");
                wprintf (L"Server Id:  ");
                for (i = 0 ; i < (int)pServ->cServerId ; ++i)
                    wprintf (L"%02x", pServ->ucServerId[i]);
                wprintf (L"\n\n");

                pServ = pServ->pNext;
            }
        } else if (wcsicmp (buffer, L"dump af") == 0) {
            AddressFamily *pAF = gpState->pafList;
            while (pAF) {
                wprintf (L"Family:            %d\n", pAF->af);
                wprintf (L"Thread Handle:     0x%08x\n", pAF->hThread);
                wprintf (L"Accepting sockets: ");
                for (int i = 0 ; i < pAF->cSockets ; ++i)
                    wprintf (L"%d ", pAF->aSocket[i]);
                wprintf (L"\n\n");

                pAF = pAF->pNext;
            }
        } else if (wcsicmp (buffer, L"dump conn") == 0) {
            Connection *pConn = gpState->pconnList;
            while (pConn) {
                wprintf (L"Socket:      %d\n", pConn->s);
                wprintf (L"Family:      %d\n", pConn->paf->af);
                wprintf (L"Current Id:  0x%08x\n", pConn->uiCurrentTransaction);
                wprintf (L"Current CId: 0x%08x\n", pConn->uiConnectionId);
                wprintf (L"Buffer:      0x%08x\n", pConn->pBuffer);
                wprintf (L"Size:        %d\n", pConn->cBufSize);
                wprintf (L"Filled:      %d\n\n", pConn->cFilled);
                wprintf (L"PeekFilled:  %d\n\n", pConn->cPeekFilled);
                wprintf (L"Time stamp:  0x%08x\n", pConn->tickLastActive);
                pConn = pConn->pNext;
            }
        } else if (wcsicmp (buffer, L"dump") == 0) {
            wprintf (L"State          : %d\n", gpState->fState);
            wprintf (L"Maintenance on : %s\n", gpState->fMaintain ? L"yes" : L"no");
            wprintf (L"ConnectionId   : 0x%08x\n", gpState->uiConnectionId);
            wprintf (L"TransactionId  : 0x%08x\n", gpState->uiTransactionId);
            wprintf (L"Connection t/o : %d\n", gpState->uiOBEX_CONNECTION_TIMEOUT);
            wprintf (L"Server t/o     : %d\n", gpState->uiOBEX_SERVER_TIMEOUT);
            wprintf (L"Maintenance pd : %d\n", gpState->uiOBEX_MAINT_PERIOD);
        } else {
            wprintf (L"Use:\n\tdump\t\tto dump global parameters\n");
            wprintf (L"\tdump conn\tto dump connection parameters\n");
            wprintf (L"\tdump af\tto dump address families\n");
            wprintf (L"\tdump servers\tto dump server cache\n");
            wprintf (L"\tdump assoc\tto dump current associations\n");
        }

        gpState->Unlock ();
    }

    return 0;
}

int wmain (int argc, WCHAR **argv) {
    svsutil_Initialize();
    svslog_DebugInitialize();

    WSADATA wsd;

    int ierr = WSAStartup (MAKEWORD(1,1), &wsd);

    if (ierr) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] Could not initialize WinSock (error=%d)\n", ierr));
        return FALSE;
    }

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] OBEX server CoInitializeEx failed, error=0x%08x\n",GetLastError()));
        return FALSE;
    }

    hMasterSwitch = CreateEvent (NULL, FALSE, FALSE, NULL);

    if (! hMasterSwitch) {
        IFDBG (svslog_DebugOut (ZONE_ERROR, L"Unexpected error %d...\n", GetLastError ()));
        return 1;
    }

    InitializeCriticalSection (&g_csMemory);

    gpState = new GlobalData;
    if (! gpState) {
        IFDBG (svslog_DebugOut (ZONE_ERROR, L"Unexpected error %d...\n", GetLastError ()));
        return 2;
    }

    gpState->Lock ();

    if (! gpState->Start ()) {
        gpState->Stop ();
        return 3;
    }

    gpState->Unlock ();
    SVSUTIL_ASSERT (! gpState->IsLocked ());

    //    CloseHandle (CreateThread (NULL, 0, ConsoleThread, NULL, 0, NULL));

    WaitForSingleObject (hMasterSwitch, INFINITE);
    gpState->Lock ();
    gpState->Stop ();

    delete gpState;
    CloseHandle (hMasterSwitch);

    DeleteCriticalSection (&g_csMemory);

    CoUninitialize();
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd){
    return wmain (0, NULL);
}

#endif // DEVICE_DRIVER

//required for CE

#ifdef _WIN32_WINCE
#include <service.h>
#include <bldver.h>

#if (CE_MAJOR_VER < 4)

extern "C" LONG
WINAPI
ObexInterlockedCompareExchange(
    IN OUT LPLONG Destination,
    LONG Exchange,
    LONG Comperand
    ) {
    return InterlockedTestExchange (Destination, Comperand, Exchange);
}
#else
#define ObexInterlockedCompareExchange(dest,exchange,comperand) \
            InterlockedCompareExchange(dest,exchange,comperand)
#endif



HANDLE g_hAdminThread = 0;
static long g_ServiceState = SERVICE_STATE_UNINITIALIZED;

enum StartServerThreadReturnCode
{
    Success = 0,
    UnableToInitWinsock,
    OutOfMemory,
    UnableToStartServer,
    UnableToInitCOM
};

static DWORD WINAPI StartServerThread (LPVOID pNull) {

    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_STARTING_UP);
    SVSUTIL_ASSERT(!gpState);
    StartServerThreadReturnCode returnCode = Success;

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] StartServerThread: OBEX server CoInitializeEx failed, error=0x%08x\n", GetLastError()));
        returnCode = UnableToInitCOM;
        goto Cleanup;
    }

    WSADATA wsd;
    int ierr = WSAStartup (MAKEWORD(1,1), &wsd);

    if (ierr) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] Could not initialize WinSock (error=%d)\n", ierr));
        g_ServiceState = SERVICE_STATE_OFF;
        g_hAdminThread = 0;
        returnCode = UnableToInitWinsock;
        goto Cleanup;
    }

    gpState = new GlobalData;

    if (! gpState) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] Unexpected error %d...\n", GetLastError ()));
        g_ServiceState = SERVICE_STATE_OFF;
        g_hAdminThread = 0;
        returnCode = UnableToStartServer;
        goto Cleanup;
    }

    gpState->Lock ();

    if (! gpState->Start ()) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] Server -- could not start!\n"));
        gpState->Stop ();
        g_ServiceState = SERVICE_STATE_OFF;
        delete gpState;
        gpState = NULL;
        g_hAdminThread = 0;
        returnCode = UnableToStartServer;
        goto Cleanup;
    }

    gpState->Unlock ();
    SVSUTIL_ASSERT (! gpState->IsLocked ());
    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_STARTING_UP);

    g_ServiceState = SERVICE_STATE_ON;
    g_hAdminThread = 0;
Cleanup:
    if (returnCode != UnableToInitCOM)
        CoUninitialize();
    return returnCode;

}


// Helper function to unload service.  This is blocking.
static void StopServer(void) {
    gpState->Lock ();
    gpState->Stop ();
    SVSUTIL_ASSERT (gpState && !gpState->IsLocked());

    //delete the global data
    delete gpState;
    gpState = NULL;
}

static DWORD WINAPI StopServerThread(LPVOID lpv) {
    BOOL fRefresh = (BOOL) lpv;
    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_SHUTTING_DOWN);
    StopServer();
    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_SHUTTING_DOWN);

    if (fRefresh) {
        g_ServiceState = SERVICE_STATE_STARTING_UP;
        StartServerThread(0);
    }
    else {
        g_ServiceState = SERVICE_STATE_OFF;
        g_hAdminThread = 0;
    }
    return 0;
}



/*****************************************************************************/
/*   DllMain                                                                 */
/*     disable thread library calls                                          */
/*****************************************************************************/
BOOL APIENTRY DllMain(HANDLE hInst, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls((HMODULE)hInst);
        DEBUGREGISTER((HINSTANCE)hInst);
        svsutil_Initialize();
        InitializeCriticalSection (&g_csMemory);
    } else if (dwReason == DLL_PROCESS_DETACH) {
        DeleteCriticalSection (&g_csMemory);
        svsutil_DeInitialize();
    }
    return TRUE;
}


#ifdef DEVICE_DRIVER

//    @func PVOID | OBX_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc    Returns a DWORD which will be passed to Open & Deinit or NULL if
//            unable to initialize the device.
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//            in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for
// anything otherwise we run a risk of deadlocking
extern "C" DWORD OBX_Init (DWORD Index) {
    if (ObexInterlockedCompareExchange(&g_ServiceState,SERVICE_STATE_STARTING_UP,SERVICE_STATE_UNINITIALIZED) == SERVICE_STATE_UNINITIALIZED) {
        if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] OBEX server CoInitializeEx failed, error=0x%08x\n",GetLastError()));
            return FALSE;
        }

        g_hAdminThread = CreateThread(NULL, 0, StartServerThread, 0, 0, 0);
        if (g_hAdminThread) {
            CloseHandle (g_hAdminThread);
            return TRUE;
        }
        else {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] OBEX server cannot create thread, error=0x%08x\n",GetLastError()));
            g_ServiceState = SERVICE_STATE_OFF;
            return FALSE;
        }
    }
    DEBUGMSG(ZONE_ERROR, (L"OBX_Init: service already initailized, cannot initialize again\n"));
    SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
    return FALSE;
}


//    @func PVOID | OBX_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from OBX_Init call
//  @rdesc    Returns TRUE for success, FALSE for failure.
//    @remark    Routine exported by a device driver.  "PRF" is the string
//            passed in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for
// anything otherwise we run a risk of deadlocking
extern "C" BOOL OBX_Deinit(DWORD dwData) {
    long fOldState;

    if (g_hAdminThread)
        WaitForSingleObject(g_hAdminThread,INFINITE);
    Sleep(100);

    // Since obex service is unloading we MUST block until all threads are done.
#pragma warning(push)
#pragma warning(disable:4127)
    while (1) {
#pragma warning(pop)
        if ((fOldState = (long)ObexInterlockedCompareExchange(&g_ServiceState,SERVICE_STATE_UNLOADING,SERVICE_STATE_ON)) == SERVICE_STATE_ON ||
            (fOldState = (long)ObexInterlockedCompareExchange(&g_ServiceState,SERVICE_STATE_UNLOADING,SERVICE_STATE_OFF)) == SERVICE_STATE_OFF)
        {
            break;
        }
        DEBUGMSG(ZONE_INIT, (L"OBEX_Deinit: cannot unload in present state (=%d), sleeping 2 seconds and trying again\n",g_ServiceState));
        Sleep(2000);
    }
    SVSUTIL_ASSERT(!g_hAdminThread);
    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_UNLOADING);

    if (fOldState == SERVICE_STATE_ON) {
        StopServer();
    }
    SVSUTIL_ASSERT(!gpState);
    SVSUTIL_ASSERT(g_ServiceState == SERVICE_STATE_UNLOADING);
    // once we're set to SERVICE_STATE_UNLOADING nothing can ever load obex again.

    CoUninitialize();

    DEBUGMSG(ZONE_INIT, (L"OBEX_Deinit: (%d) memory objects not closed.. \n", svsutil_TotalAlloc()));
    SVSUTIL_ASSERT(0 == svsutil_TotalAlloc());
    return TRUE;
}

//    @func BOOL | OBX_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc    Returns TRUE for success, FALSE for failure
//    @remark    Routine exported by a device driver.  "PRF" is the string passed
//        in as lpszType in RegisterDevice

extern "C" BOOL
OBX_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
    HKEY hKey;
    DWORD dwValue;
    BOOL fStopCalled = FALSE;

    switch (dwCode) {
    case IOCTL_SERVICE_START:
        dwValue = 1;

        if (ObexInterlockedCompareExchange(&g_ServiceState,SERVICE_STATE_STARTING_UP,SERVICE_STATE_OFF) == SERVICE_STATE_OFF) {
            SVSUTIL_ASSERT(!g_hAdminThread);
            SVSUTIL_ASSERT(!gpState);

            g_hAdminThread = CreateThread(NULL, 0, StartServerThread, 0, 0, 0);
            if (g_hAdminThread) {
                CloseHandle (g_hAdminThread);
                if (ERROR_SUCCESS==RegOpenKeyEx(HKEY_LOCAL_MACHINE, OBEX_KEY_BASE, 0, KEY_ALL_ACCESS, &hKey)) {
                    RegSetValueEx(hKey, L"IsEnabled", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
                    RegCloseKey(hKey);
                }
                return TRUE;
            }
            else {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] OBEX server cannot create thread, error=0x%08x\n",GetLastError()));
                g_ServiceState = SERVICE_STATE_OFF;
                //not calling SetLastError() because CreateThread did it for me and its more descriptive
                return FALSE;
            }
        }
        DEBUGMSG(ZONE_ERROR, (L"Service cannot be started because it is not in SERVICE_STATE_OFF, current state = %d\n",g_ServiceState));
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    break;

    case IOCTL_SERVICE_STOP:
        dwValue = 0;
        fStopCalled = TRUE;

        // fall through...
    case IOCTL_SERVICE_REFRESH:
        if (ObexInterlockedCompareExchange(&g_ServiceState,SERVICE_STATE_SHUTTING_DOWN,SERVICE_STATE_ON) == SERVICE_STATE_ON) {
            SVSUTIL_ASSERT(!g_hAdminThread);
            SVSUTIL_ASSERT(gpState);



            g_hAdminThread = CreateThread(NULL,0,StopServerThread, (LPVOID)(IOCTL_SERVICE_REFRESH==dwCode),0,0);
            if (g_hAdminThread) {
                CloseHandle (g_hAdminThread);

                if (fStopCalled && ERROR_SUCCESS==RegOpenKeyEx(HKEY_LOCAL_MACHINE, OBEX_KEY_BASE, 0, KEY_ALL_ACCESS, &hKey)) {
                    RegSetValueEx(hKey, L"IsEnabled", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
                    RegCloseKey(hKey);
                }
                return TRUE;
            }
            else {
                DEBUGMSG(ZONE_ERROR, (L"[OBEX] OBEX server cannot create thread, error=0x%08x\n",GetLastError()));
                g_ServiceState = SERVICE_STATE_ON;
                //not calling SetLastError() because CreateThread did it for me and its more descriptive
                return FALSE;
            }
        }
        DEBUGMSG(ZONE_ERROR, (L"Service cannot be shutdown because it is not in SERVICE_STATE_ON, current state = %d\n",g_ServiceState));
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    break;

    case IOCTL_SERVICE_STATUS:
        __try {
            //check to make sure the pointer is safe
            if (!pBufOut || dwLenOut < sizeof(DWORD)) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            *(DWORD *)pBufOut = g_ServiceState;
            if (pdwActualOut)
                *pdwActualOut = sizeof(DWORD);
        } __except(1) {
            SetLastError (ERROR_INVALID_USER_BUFFER);
            return FALSE;
        }
        return TRUE;
    break;
    }

    DEBUGMSG(ZONE_ERROR, (L"OBX_IOControl received unknown IOCTL 0x%08x\n",dwCode));
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}




#endif

#endif



