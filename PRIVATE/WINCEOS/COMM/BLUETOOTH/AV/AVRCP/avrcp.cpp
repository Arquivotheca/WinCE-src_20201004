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

#include <svsutil.hxx>

#include <winsock2.h>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>
#include <bthapi.h>

#include <cxport.h>
#include "avrcp.hpp"


#define DEBUG_LAYER_TRACE                       0x00000040

DECLARE_DEBUG_VARS();



class Avrcp    *gpAVRCP;
CTELock         gAvrcpLock;


__inline void GetAvrcpLock() {
    CTEGetLock(&gAvrcpLock, 0);
}

__inline void FreeAvrcpLock() {
    CTEFreeLock(&gAvrcpLock, 0);
}


//
// AVCTP interface functions
//

static int AVCT_AvrcpConnectInd(BD_ADDR *pAddr) {

    GetAvrcpLock();
    if (gpAVRCP) {
        return gpAVRCP->AVCT_ConnectInd(pAddr);
    }

    FreeAvrcpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int AVCT_AvrcpDisconnectInd(BD_ADDR *pAddr) {

    GetAvrcpLock();
    if (gpAVRCP) {
        return gpAVRCP->AVCT_DisconnectInd(pAddr);
    }

    FreeAvrcpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int AVCT_AvrcpMessageRecInd(BD_ADDR *pAddr, 
	uchar Transaction, uchar Type, uchar *pData, ushort cData) {

    GetAvrcpLock();
    if (gpAVRCP) {
        return gpAVRCP->AVCT_MsgRecInd(pAddr, Transaction, Type, pData, cData);
    }

    FreeAvrcpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int AVCT_AvrcpStackEvent(void *pUserContext, int iEvent, void *pEventContext) {

    return ERROR_SUCCESS;
}



static int AVCT_AvrcpConnectOut(BD_ADDR *pAddr, 
	AVCT_RSP ConnectResult, ushort ConfigResult, ushort Status) {

    // we never call connect so why are we getting this!?!
    ASSERT(0);
    return ERROR_SUCCESS;

}


static int AVCT_AvrcpDisconnectOut(BD_ADDR *pAddr, 
	ushort DisconnectResult) {

    // we never call disconnect so why are we getting this!?!
    ASSERT(0);
    return ERROR_SUCCESS;

}


static int AVCT_AvrcpCallAborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_CallAborted call 0x%08x error %d\n", pCallContext, iError));

    // not sure we need to do anything here
    ASSERT(0);
    return ERROR_SUCCESS;
}


//
// AVRCP Implementation
//


Avrcp::Avrcp() {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"AVRCP: new AVRCP\n"));

    m_hMsgQueue = NULL;
    
    m_hAvctp = INVALID_HANDLE_VALUE;
    m_pConnections = NULL;
    memset (&m_AvctIntf, 0, sizeof(m_AvctIntf));

    AVCT_EVENT_INDICATION   AvctpInd;
    AVCT_CALLBACKS          AvctpCallback;

    AvctpInd.AvctConnectInd = AVCT_AvrcpConnectInd;
    AvctpInd.AvctDisconnectInd = AVCT_AvrcpDisconnectInd;
    AvctpInd.AvctMessageRecInd = AVCT_AvrcpMessageRecInd;
    AvctpInd.AvctStackEvent = AVCT_AvrcpStackEvent;

    AvctpCallback.AvctConnectOut = AVCT_AvrcpConnectOut;
    AvctpCallback.AvctDisconnectOut = AVCT_AvrcpDisconnectOut;
    AvctpCallback.AvctCallAborted = AVCT_AvrcpCallAborted;

    uint   Version = 0x00010000;

    if (ERROR_SUCCESS != AVCT_EstablishDeviceContext (Version, AvrcpPID, this, 
        &AvctpInd, &AvctpCallback, &m_AvctIntf, &m_cHeaders, 
        &m_cTrailers, &m_hAvctp))
        return;

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"AVRCP: new Avctp successful\n"));
   
}


Avrcp::~Avrcp() {

    if (INVALID_HANDLE_VALUE != m_hAvctp) {
        AVCT_CloseDeviceContext(m_hAvctp);
    }

}


AvrcpConn **Avrcp::_FindConnection(BD_ADDR *pAddr) {
    AvrcpConn   **ppConn, *pConn;

    ppConn = &m_pConnections;

    while (pConn = *ppConn) {
        __try {
            if (pConn->m_BdAddr == *pAddr) {
                break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {

        }
        ppConn = &pConn->m_pNext;
    }

    return ppConn;
    
}



AvrcpConn *Avrcp::FindConnection(BD_ADDR *pAddr) {

    return *_FindConnection(pAddr);
}


int Avrcp::InsertConnection(BD_ADDR *pAddr, AvrcpConn **ppUserConn) {
    int         Ret;
    AvrcpConn   **ppConn, *pConn;

    ppConn = _FindConnection(pAddr);
    if (*ppConn) {
        Ret = ERROR_ALREADY_EXISTS;
        *ppUserConn = *ppConn;
    } else {
        pConn = new AvrcpConn;
        if (pConn) {
            *ppConn = pConn;
            pConn->m_pNext = NULL;    
            CeSafeCopyMemory(&pConn->m_BdAddr, pAddr, sizeof(BD_ADDR));
            Ret = ERROR_SUCCESS;
            *ppUserConn = pConn;
        } else {
            Ret = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    return Ret;
}


int Avrcp::AVCT_ConnectInd(BD_ADDR * pAddr) {

    int         Ret;
    AvrcpConn   *pConn;


    Ret = InsertConnection(pAddr, &pConn);

    if (ERROR_ALREADY_EXISTS == Ret) {
        // hmm we already have this conn so why another indication?
        ASSERT(0);
        FreeAvrcpLock();
        Ret = ERROR_SUCCESS;

    } else if (ERROR_SUCCESS == Ret) {
        // call lower layer's ConnectCfm
        HANDLE              hAvctp = m_hAvctp;
        AVCT_ConnectRsp_In  pfnAVCT_ConnRspIn = m_AvctIntf.AvctConnectRspIn;

        FreeAvrcpLock();

        int AvctpErr = pfnAVCT_ConnRspIn(hAvctp, pAddr, 0, 0, NULL);

        ASSERT(ERROR_SUCCESS == AvctpErr);
        // should we delete the connection if AVCT_ConnRspIn fails?
    }

    return Ret;
}


int Avrcp::AVCT_DisconnectInd(BD_ADDR * pAddr) {
    int         Ret;
    AvrcpConn   *pConn, **ppConn;

    ppConn = gpAVRCP->_FindConnection(pAddr);
    if (pConn = *ppConn) {
        *ppConn = pConn->m_pNext;
        delete pConn;
        Ret = ERROR_SUCCESS;
    } else {
        // why did we get DiscInd on a non-existing connection?
        ASSERT(0);
        Ret = ERROR_NOT_FOUND;
    }

    FreeAvrcpLock();
    return Ret;
}

int Avrcp::HandleUnitReq(AvrcpPkt *pPkt, ushort cData, BD_BUFFER **ppRspBuf,
    ushort *pcRspBuf) {
    const ushort    cPkt = cMinAvrcpPkt + cUnitInfoData;
    int             Ret = ERROR_INVALID_PARAMETER;
    uchar           aData[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    if ((Status_t != pPkt->Ctype) ||
        (0x1F != pPkt->SubunitType) || 
        (0x7 != pPkt->SubunitId)) {
        ;   //Ret = ERROR_INVALID_PACKET;

    } else if (cData != cPkt) {
        Ret = ERROR_INVALID_PARAMETER;
        
    } else if (memcmp(&pPkt->aOpd[1], aData, 4)) {
        ;   //Ret = ERROR_INVALID_PACKET;
    } else {

        // ok formulate a response
        BD_BUFFER *pBdBuf = BufferAlloc(cPkt + m_cHeaders + m_cTrailers);

        if (pBdBuf) {
            pBdBuf->cStart += m_cHeaders;
            pBdBuf->cEnd -= m_cTrailers;

            UNALIGNED AvrcpPkt *pRspPkt = 
                (AvrcpPkt *)(pBdBuf->pBuffer + pBdBuf->cStart);

            memcpy(pBdBuf->pBuffer + pBdBuf->cStart, (uchar *)pPkt, cPkt);

            pRspPkt->Ctype = Stable_t;

            // pRspPkt->OpCode stays as is

            pRspPkt->aOpd[0] = 0x07;
            pRspPkt->aOpd[1] = 0x48;

            *ppRspBuf = pBdBuf;
            *pcRspBuf = cData;
            Ret = ERROR_SUCCESS;
        } else {
            Ret = ERROR_NOT_ENOUGH_MEMORY;
        }

    }

    return Ret;

}


int Avrcp::CreateAvrcpMsgQ() {
    int             Ret;
    MSGQUEUEOPTIONS Options;

    Options.dwSize = sizeof(Options);
    Options.dwFlags = 0;
    Options.dwMaxMessages = 5;
    Options.cbMaxMessage = 100;
    Options.bReadAccess = FALSE;

    m_hMsgQueue = CreateMsgQueue(szAvrcpMsgQName, &Options);
    ASSERT(m_hMsgQueue);

    Ret = GetLastError();
    RETAILMSG(1,
        (L"Avrcp: CreateMsgQ returned %d\r\n", Ret));

    if (m_hMsgQueue && (Ret == ERROR_ALREADY_EXISTS))
        Ret = ERROR_SUCCESS;
    

    return Ret;
}


int Avrcp::SendMsg(uint Cmd) {
    int         Ret;
    AvrcpMsg    Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.OpCode = PassThru_t;
    Msg.OpId = Cmd;

    if (WriteMsgQueue(m_hMsgQueue, &Msg, sizeof(Msg), 50, 0)) {
        Ret = ERROR_SUCCESS;
    } else {
        Ret = GetLastError();
        RETAILMSG(1, (L"!Avrcp: SendMsg: WriteMsgQueue failed with %d / %X\r\n",
            Ret, Ret));

        if (6 == Ret) {
            CloseMsgQueue(m_hMsgQueue);
            CreateAvrcpMsgQ();
        }
        
        Ret = ERROR_WRITE_FAULT;
    }

    return Ret;
}


int Avrcp::SendToWMP(uint Cmd) {
    int Ret = ERROR_SUCCESS;

    if (NULL == m_hMsgQueue) {
        CreateAvrcpMsgQ();
    }

    if (m_hMsgQueue) {

        Ret = SendMsg(Cmd);

        switch (Cmd) {
          case Play_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Play\r\n"));
            break;

          case Stop_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Stop\r\n"));
            break;

          case Pause_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Pause\r\n"));
            break;

          case Rewind_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Rewind\r\n"));
            break;

          case FastFwd_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Fast Forward\r\n"));
            break;

          case Forward_t:
            RETAILMSG(1, (L"Avrcp: SendToWMP: Next\r\n"));
            break;

          case Backward_t:
            RETAILMSG(1, (L"Avrcp: SendTpoWMP: Previous\r\n"));
            break;

          default:
            RETAILMSG(1, (L"Avrcp: SendToWMP: unrecognized command\r\n"));
            Ret = ERROR_INVALID_PARAMETER;
            break;

        }
        
    } else {
        Ret = ERROR_WRITE_FAULT;
    }

    return Ret;

}


int Avrcp::HandlePassThru(AvrcpPkt *pPkt, ushort cData, BD_BUFFER **ppRspBuf,
    ushort *pcRspBuf) {
    int         Ret;
    int         fSendRsp;
    AVCTypes    Response;

    if (PanelType != pPkt->SubunitType || 
        0 != pPkt->SubunitId) {
        fSendRsp = TRUE;
        Response = NotImpl_t;
        Ret = ERROR_CALL_NOT_IMPLEMENTED;
        
    } else if (Control_t != pPkt->Ctype) {
        fSendRsp = FALSE;
        Ret = ERROR_CALL_NOT_IMPLEMENTED;
        
    } else {
        int SendStatus = SendToWMP(pPkt->aOpd[0]);

        if (ERROR_SUCCESS == SendStatus) {
            Response = Accepted_t;
        } else if (ERROR_WRITE_FAULT == SendStatus) {
            Response = Rejected_t;
        } else {
            Response = NotImpl_t;
        }

        fSendRsp = TRUE;
    }

    if (fSendRsp) {

        // ok formulate a response
        BD_BUFFER *pBdBuf = BufferAlloc(cData + m_cHeaders + m_cTrailers);

        if (pBdBuf) {
            pBdBuf->cStart += m_cHeaders;
            pBdBuf->cEnd -= m_cTrailers;

            UNALIGNED AvrcpPkt *pRspPkt = 
                (AvrcpPkt *)(pBdBuf->pBuffer + pBdBuf->cStart);

            memcpy(pBdBuf->pBuffer + pBdBuf->cStart, (uchar *)pPkt, cData);

            pRspPkt->Ctype = Response;

            *ppRspBuf = pBdBuf;
            *pcRspBuf = cData;
            Ret = ERROR_SUCCESS;
        } else {
            Ret = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    return Ret;

}


int Avrcp::AVCT_MsgRecInd(BD_ADDR * pAddr, uchar Transaction, uchar Type, 
    uchar *pData, ushort cData) {
    int         Ret = ERROR_SUCCESS;
    BD_BUFFER   *pRspBuf = NULL;
    ushort      cRspBuf = 0;
    AvrcpPkt *pPkt = (AvrcpPkt *)pData;


    if (cData < cMinAvrcpPkt) {
        Ret = ERROR_INSUFFICIENT_BUFFER;
    } else if (0 != pPkt->Blank) {
        Ret = ERROR_INVALID_DATA;
    } else {

        switch (pPkt->OpCode) {
          case UnitInfo_t:
          case SubunitInfo_t:

            Ret = HandleUnitReq(pPkt, cData, &pRspBuf, &cRspBuf);
            break;

          case PassThru_t:
            Ret = HandlePassThru(pPkt, cData, &pRspBuf, &cRspBuf);
            break;


          default:
            // unrecognized command
            break;
        }
    }

    if (pRspBuf) {
        // we have a response to send!
        HANDLE              hAvctp = m_hAvctp;
        AVCT_SendMessage_In pfnAvctpSendMsgIn = m_AvctIntf.AvctSendMessageIn;

        FreeAvrcpLock();

        ASSERT((pRspBuf->cEnd - pRspBuf->cStart) == cRspBuf);

        pfnAvctpSendMsgIn(hAvctp, pAddr, Transaction, avct_rsp_msg_e, 
            AvrcpPID, pRspBuf);
    } else {
        FreeAvrcpLock();
    }

    return Ret;
}


//
//      Init stuff
//


AddSdpRecord(void
    )
{
 
    DWORD Ret  = ERROR_SUCCESS;
    DWORD SizeOut = 0;
    const int cSdpRecord = 0x00000059;
    BYTE rgbSdpRecord[] = {
        0x35, 0x57, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19,
        0x11, 0x0c, 0x09, 0x00, 0x04, 0x35, 0x10, 0x35,
        0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x17, 0x35,
        0x06, 0x19, 0x00, 0x17, 0x09, 0x01, 0x00, 0x09,
        0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x11,
        0x0e, 0x09, 0x01, 0x00, 0x09, 0x01, 0x00, 0x25,
        0x22, 0x41, 0x75, 0x64, 0x69, 0x6f, 0x20, 0x56,
        0x69, 0x64, 0x65, 0x6f, 0x20, 0x52, 0x65, 0x6d,
        0x6f, 0x74, 0x65, 0x20, 0x43, 0x6f, 0x6e, 0x74,
        0x72, 0x6f, 0x6c, 0x20, 0x50, 0x72, 0x6f, 0x66,
        0x69, 0x6c, 0x65, 0x09, 0x03, 0x11, 0x09, 0x00,
        0x01
    };


    struct {
        BTHNS_SETBLOB   b;
        unsigned char   uca[cSdpRecord];
    } bigBlob;
    
    ULONG SdpVersion = BTH_SDP_VERSION;
    ULONG   SdpRecord = 0;

    bigBlob.b.pRecordHandle   = &(SdpRecord);
    bigBlob.b.pSdpVersion     = &SdpVersion;
    bigBlob.b.fSecurity       = 0;
    bigBlob.b.fOptions        = 0;
    bigBlob.b.ulRecordLength  = cSdpRecord;


    memcpy(bigBlob.b.pRecord, rgbSdpRecord, cSdpRecord);

    BLOB blob;
    blob.cbSize    = sizeof(BTHNS_SETBLOB) + cSdpRecord - 1;
    blob.pBlobData = (PBYTE) &bigBlob;

    WSAQUERYSET Service;
    memset(&Service, 0, sizeof(Service));
    Service.dwSize = sizeof(Service);
    Service.lpBlob = &blob;
    Service.dwNameSpace = NS_BTH;

    Ret = BthNsSetService (&Service, RNRSERVICE_REGISTER,0);
    if (ERROR_SUCCESS == Ret) 
        {
        }
    else
        {
        ASSERT(0);
        }

    return Ret;
}



int avrcp_InitializeOnce(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"++avrcp_InitializeOnce\n"));

    CTEInitLock(&gAvrcpLock);

    IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"--avrcp_InitializeOnce:: ERROR_SUCCESS\n"));    

//exit:    
    return dwRetVal;
}


int avrcp_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+avctp_CreateDriverInstance\r\n"));
    int Ret;

    GetAvrcpLock();

    if (gpAVRCP) {
        Ret = ERROR_ALREADY_INITIALIZED;
    } else {

        gpAVRCP = new Avrcp;
        if (gpAVRCP)
            Ret = ERROR_SUCCESS;
        else
            Ret = ERROR_OUTOFMEMORY;
    }

    AddSdpRecord();
    
    FreeAvrcpLock();

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-avctp_CreateDriverInstance : Ret %d\n", Ret));
    return Ret;
}


int avrcp_CloseDriverInstance (void) {
    int Ret;
    
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+avrcp_CloseDriverInstance\n"));


    GetAvrcpLock();
    if (! gpAVRCP) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else {
        Ret = ERROR_SUCCESS;
        delete gpAVRCP;
        gpAVRCP = NULL;
    }
    FreeAvrcpLock();

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-avrcp_CloseDriverInstance : Ret %d\n", Ret));

    return Ret;
}



