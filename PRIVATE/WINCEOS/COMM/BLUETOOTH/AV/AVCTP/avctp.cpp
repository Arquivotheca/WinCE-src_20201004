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
/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Abstract:
        Windows CE Bluetooth Audio/Video Control Transport Protocol

**/

#include <windows.h>

#include <svsutil.hxx>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>

#include <cxport.h>
#include "avctp.hpp"

#if defined (UNDER_CE)
#include <pkfuncs.h>
#endif

#include "creg.hxx"


#define DEBUG_LAYER_TRACE                       0x00000040


class Avctp    *gpAVCTP;
CTELock         gAvctpLock;


DECLARE_DEBUG_VARS();



#define NONE                    0x00
#define CONNECTED               0x01
#define CONFIG_REQ_DONE 0x02
#define CONFIG_IND_DONE 0x04
#define UP              0x07
#define LINK_ERROR              0x80

__inline void GetAvctpLock() {
    CTEGetLock(&gAvctpLock, 0);
}

__inline void FreeAvctpLock() {
    CTEFreeLock(&gAvctpLock, 0);
    ASSERT((HANDLE )GetCurrentThreadId() != gAvctpLock.OwnerThread);
}




//
//      L2CAP Indication interface functions
//

static int L2CA_AvctpConnectInd(void *pUserContext, BD_ADDR *pRemoteAddr, 
    ushort ChannelId, uchar Id, ushort PSM) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_ConnectInd(pRemoteAddr, ChannelId, Id, PSM);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int L2CA_AvctpConfigInd(void *pUserContext, uchar id, ushort cid, 
    ushort usOutMTU, ushort usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, 
        struct btCONFIGEXTENSION **pExtendedOptions) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_ConfigInd(id, cid, usOutMTU, usInFlushTO, pInFlow, 
            cOptNum, pExtendedOptions);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int L2CA_AvctpDisconnectInd (void *pUserContext, ushort cid, int iError) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_DisconnectInd(cid, iError);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int L2CA_AvctpDataUpInd(void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_DataUpInd(cid, pBuffer);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int L2CA_AvctpStackEvent(void *pUserContext, int iEvent, void *pEventContext) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_StackEvent(iEvent, pEventContext);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}

//
//      L2CAP Callback interface functions
//

static int L2CA_AvctpConnectReqOut (void *pCallContext, ushort cid, 
    ushort Result, ushort Status) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_ConnectReqOut((AvctpChannel *)pCallContext, cid, 
            Result, Status);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


static int L2CA_AvctpConnectRspOut (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Shell: ConnectResponse out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}


static int L2CA_AvctpConfigReqOut (void *pCallContext, ushort Result, 
    ushort InMTU, ushort OutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, 
    struct btCONFIGEXTENSION **ppExtendedOptions) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_ConfigReqOut((AvctpChannel *)pCallContext, 
            Result, InMTU, OutFlushTO, pOutFlow, cOptNum, ppExtendedOptions);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


// These are just stubs - they do nothing
static int L2CA_AvctpConfigRspOut (void *pCallContext, unsigned short result) {

    RETAILMSG(1, (L"+L2CA_AvctpConfigRspOut\r\n"));

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Shell: ConfigResponse out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}


// this is called by L2Cap in response to an Diconnect request
static int L2CA_AvctpDisconnectOut (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Shell: disconnect out call 0x%08x, result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}


// this function is called after a call L2Cap's DataDownIn
static int L2CA_AvctpDataDownOut (void *pCallContext, unsigned short result) {
    // we're going to do nothing here since we don't have to indicate the send
    // compelete to our user

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Shell: Data down call 0x%08x result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}


static int L2CA_AvctpCallAborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_CallAborted call 0x%08x error %d\n", pCallContext, iError));

    // handle aborted connects

    RETAILMSG(1, (L"+Avctp: L2CA_AvctpCallAborted\r\n"));

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->L2CA_CallAborted((AvctpChannel *)pCallContext, iError);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}

static void DispatchStackEvent(int iEvent)
{
    if (gpAVCTP) {
        for (AvctpUser* pUser = gpAVCTP->m_pUsers; pUser; pUser = pUser->m_pNext) {
            BT_LAYER_STACK_EVENT_IND pCallback = pUser->m_IndicateFns.AvctStackEvent;
            if (pCallback) {
                void *pUserContext = pUser->m_pUserContext;

                FreeAvctpLock();

                IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"AVCTP: going into StackEvent notification\n"));

                __try {
                    pCallback(pUserContext, iEvent, NULL);
                } __except(1) {
                    IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"[AVCTP] DispatchStackEvent: exception in AvctpStackEvent!\n"));
                }

                IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"AVCTP: Came back from StackEvent notification\n"));

                GetAvctpLock();
            }
        }
    }
}



Avctp::Avctp() {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"AVCTP: new AVCTP\n"));

    m_fIsRunning = FALSE;
    m_fConnected = FALSE;

    m_hL2CAP = NULL;

    m_cHeaders = 0;
    m_cTrailers = 0;
    m_pChannels = NULL;
    m_pUsers = NULL;


    memset (&m_L2CapIntf, 0, sizeof(m_L2CapIntf));

    L2CAP_EVENT_INDICATION L2CapEventInd;
    memset (&L2CapEventInd, 0, sizeof(L2CapEventInd));

    L2CapEventInd.l2ca_ConnectInd = L2CA_AvctpConnectInd;
    L2CapEventInd.l2ca_ConfigInd = L2CA_AvctpConfigInd;
    L2CapEventInd.l2ca_DisconnectInd = L2CA_AvctpDisconnectInd;
    L2CapEventInd.l2ca_DataUpInd = L2CA_AvctpDataUpInd;
    L2CapEventInd.l2ca_StackEvent = L2CA_AvctpStackEvent;

    L2CAP_CALLBACKS L2CapCallback;
    memset (&L2CapCallback, 0, sizeof(L2CapCallback));

    L2CapCallback.l2ca_ConnectReq_Out = L2CA_AvctpConnectReqOut;
    L2CapCallback.l2ca_ConnectResponse_Out = L2CA_AvctpConnectRspOut;
    L2CapCallback.l2ca_ConfigReq_Out = L2CA_AvctpConfigReqOut;
    L2CapCallback.l2ca_ConfigResponse_Out = L2CA_AvctpConfigRspOut;
    L2CapCallback.l2ca_Disconnect_Out = L2CA_AvctpDisconnectOut;
    L2CapCallback.l2ca_DataDown_Out = L2CA_AvctpDataDownOut;
    L2CapCallback.l2ca_CallAborted = L2CA_AvctpCallAborted;

    if (ERROR_SUCCESS != L2CAP_EstablishDeviceContext (this, AVCTP_PSM, 
        &L2CapEventInd, &L2CapCallback, &m_L2CapIntf, &m_cHeaders, 
        &m_cTrailers, &m_hL2CAP))
        return;

    m_fIsRunning = TRUE;

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"AVCTP: new Avctp successful\n"));
   
}


AvctpChannel ** Avctp::_FindChannel(AvctpChannel *pRequestedChannel) {
    AvctpChannel **ppChannel, *pChannel;

    ppChannel = & m_pChannels;
    while (pChannel = *ppChannel) {
        if (pRequestedChannel == pChannel)
            break;
        ppChannel = & (pChannel->m_pNext);
    }

    return ppChannel;
}


AvctpChannel * Avctp::FindChannel(AvctpChannel *pRequestedChannel) {
    AvctpChannel *pChannel;

    for (pChannel = m_pChannels; pChannel; pChannel = pChannel->m_pNext) {
        if (pRequestedChannel == pChannel)
            break;
    }

    return pChannel;
}


AvctpChannel * Avctp::FindChannel(BD_ADDR *pAddr) {
    AvctpChannel *pChannel;

    for (pChannel = m_pChannels; pChannel; pChannel = pChannel->m_pNext) {
        if (*pAddr == pChannel->m_BdAddr)
            break;
    }

    return pChannel;
}


AvctpChannel * Avctp::FindChannel(ushort ChannelId) {
    AvctpChannel *pChannel;

    for (pChannel = m_pChannels; pChannel; pChannel = pChannel->m_pNext) {
        if (ChannelId == pChannel->m_cid)
            break;
    }

    return pChannel;
}


int Avctp::DisconnectChannel(AvctpChannel *pChannel) {
    int                 Ret = 0;
    AvctpUser           *pUser = m_pUsers;
    AvctpUserChannel    *pUserChannel = NULL;
    
    while (pUser && (! pUserChannel)) {
        pUserChannel = pUser->FindUserChannel(pChannel);
        pUser = pUser->m_pNext;
    }

    if (! pUserChannel) {
        // there is no one using this channel get rid of it!
        pChannel->m_ChannelState = ChannelClosing_e;

        AvctpChannel **ppChannel = _FindChannel(pChannel);
        if (*ppChannel == pChannel) {
            *ppChannel = pChannel->m_pNext;
        }

        HANDLE hL2Cap = m_hL2CAP;
        L2CA_Disconnect_In pfnL2CapDiscIn = m_L2CapIntf.l2ca_Disconnect_In;
        FreeAvctpLock();

        // we shouldn't need to do anything on a disconnect out
        __try {
            Ret = pfnL2CapDiscIn(hL2Cap, NULL, pChannel->m_cid);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
        }

        if (0 == Ret) {
            delete (pChannel);
        }

        GetAvctpLock();
    }


    return Ret;

}


void Avctp::DeleteChannel(AvctpChannel *pChannel) {

    pChannel->m_ChannelState = ChannelClosing_e;

    // tell each of the users!
    AvctpUser *pUser = m_pUsers;
    while (pUser) {
        pUser->DeleteUserChannel(pChannel);
        pUser = pUser->m_pNext;
    }

    AvctpChannel **ppChannel = _FindChannel(pChannel);
    if (*ppChannel == pChannel) {
        *ppChannel = pChannel->m_pNext;

        delete pChannel;
    }
}


AvctpUser * Avctp::FindUser(ushort ProfileId) {
    AvctpUser *pUser;

    for (pUser = m_pUsers; pUser; pUser = pUser->m_pNext) {
        if (ProfileId == pUser->m_ProfileId)
            break;
    }

    return pUser;
}


//      Auxiliary code

static Avctp *CreateNewState (void) {
    return new Avctp;
}



void Avctp::StackDown () {

    // let use assume that L2Cap will deal with clearing all the Channels

    m_fIsRunning = FALSE;

    DispatchStackEvent(BTH_STACK_DOWN);

    IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"AVCTP:StackDown\n"));
}

void Avctp::StackUp () {

    m_fIsRunning = TRUE;

    DispatchStackEvent(BTH_STACK_UP);
	
    IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"AVCTP:StackUp\n"));
}

int Avctp::L2CA_DataUpInd (unsigned short cid, BD_BUFFER *pBuffer) {
    int         Ret = ERROR_INSUFFICIENT_BUFFER;
    ushort      ProfileId;
    uchar       cFragments;
    int         fFreeData = FALSE;
    int         fFreeLock = TRUE;
    int         fInvalidPID = FALSE;
    uchar       *pData = NULL;
    uint        cData;
    AvctpUser   *pUser;
    AvctpChannel    *pChannel;

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"Shell: Data up on channel 0x%04x %d bytes\n", cid, BufferTotal (pBuffer)));

    AvctpCommonPacketHdr CommonHdr;

    if (! BufferGetByte(pBuffer, (uchar *)&CommonHdr)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVCTP:ProcessPacket : Buffer is too small\n"));
        goto exit;
    }

    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
        goto exit;
    }

    uint PacketType = CommonHdr.PacketType;

    if (PacketType <= 1) {
        // non-frag or start packet

        if (1 == PacketType) {
            if (! BufferGetByte(pBuffer, &cFragments)) {
                goto exit;
            }
        }

        if (! BufferGetShort(pBuffer, &ProfileId)) {
            goto exit;
        }

        Ret = ERROR_SERVICE_DOES_NOT_EXIST;

        pUser = FindUser(ProfileId);
        if (! pUser) {
            fInvalidPID = TRUE;
            goto exit;
        }

        pChannel = FindChannel(cid);
        AvctpUserChannel    *pUserChannel;

        if (pChannel && (ChannelEstablished_e == pChannel->m_ChannelState)) {
            pUserChannel = pUser->FindUserChannel(pChannel);
        } else {
            pUserChannel = NULL;
        }
        
        if (pUserChannel && 
            (UChannelEstablished_e == pUserChannel->m_UChannelState)) {

            if (0 == PacketType) {
                pData = &pBuffer->pBuffer[pBuffer->cStart];
                cData = pBuffer->cEnd- pBuffer->cStart;
                Ret = ERROR_SUCCESS;
            } else {
                Ret = m_FragList.AddNewPkt(cid, CommonHdr, cFragments, 
                    ProfileId, pBuffer, pChannel->m_inMTU);
            }
        }
    } else {
        Ret = m_FragList.AddContinuePkt(cid, CommonHdr, pBuffer, &pData, 
            &cData, &ProfileId);
        if ((ERROR_SUCCESS == Ret) && pData) {
            ASSERT(cData);
            fFreeData = TRUE;
            pUser = FindUser(ProfileId);
            pChannel = FindChannel(cid);

            if ((! pUser) ||  (! pChannel))
                Ret = ERROR_SERVICE_NOT_FOUND;
        }
    }

    if ((ERROR_SUCCESS == Ret) && (pData)) {
        // OK deliver it!
        AVCT_MessageRec_Ind pfnAvctMsgRecInd = 
            pUser->m_IndicateFns.AvctMessageRecInd;

        if (pfnAvctMsgRecInd) {
            BD_ADDR Addr = pChannel->m_BdAddr;
            uchar   Transaction = CommonHdr.Transaction;
            uchar   Type = CommonHdr.CR ? 2 : 1;

            FreeAvctpLock();
            fFreeLock = FALSE;
            Ret = ERROR_SUCCESS;
            __try {
                pfnAvctMsgRecInd(&Addr, Transaction, Type, pData, cData);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }


    }


exit:

    if (fInvalidPID) {
        // we should send a response back
        HANDLE  hL2Cap = gpAVCTP->m_hL2CAP;
        BD_BUFFER   *pBdBuf = BufferAlloc(3 + 
            gpAVCTP->m_cHeaders + gpAVCTP->m_cTrailers);


        if (pBdBuf) {
            pBdBuf->cStart = gpAVCTP->m_cHeaders;
            pBdBuf->cEnd = pBdBuf->cSize - gpAVCTP->m_cTrailers;
            //pBdBuf->fMustCopy = TRUE;

            L2CA_DataDown_In pfnL2CapDataDownIn = 
                gpAVCTP->m_L2CapIntf.l2ca_DataDown_In;

            FreeAvctpLock();
            fFreeLock = FALSE;

            // we last looked at the PID field so let us set the end of the buffer
            //pBuffer->cEnd = pBuffer->cStart;
            // now move the start back 3 octets
            //pBuffer->cStart -= 3;
            // now put in the Hdr
            CommonHdr.IPID = 1;

    		uchar *pc = (uchar *)& CommonHdr;
            pBdBuf->pBuffer[pBdBuf->cStart] = *pc;
            memcpy(&pBdBuf->pBuffer[pBdBuf->cStart + 1], &ProfileId, 2);

            // OK send it
            // we shouldn't need to do anything on a DataDownOut
            pfnL2CapDataDownIn(hL2Cap, NULL, cid, pBdBuf);

        }

    }

    if (fFreeData)
        delete [] (pData);

    BufferFree(pBuffer);

    if (fFreeLock)
        FreeAvctpLock();
    
    return Ret;
}


int Avctp::L2CA_DisconnectInd (unsigned short cid, int iError) {
    int             Ret;
    AvctpChannel    *pChannel;

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_DisconnectInd channel 0x%04x\n", cid));

    if (! (pChannel = FindChannel(cid))) {
        Ret = ERROR_NOT_FOUND;

    } else {
        DeleteChannel(pChannel);
        Ret = ERROR_SUCCESS;
    }
    FreeAvctpLock();

    return Ret;
    
}

int Avctp::L2CA_CallAborted (AvctpChannel *pContext, int iError) {
    int             Ret;
    AvctpChannel    *pChannel;

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_CallAborted channel 0x%04x\n", pContext));

    if (! (pChannel = FindChannel(pContext))) {
        Ret = ERROR_NOT_FOUND;
    } else {
        ASSERT(pChannel == pContext);
        DeleteChannel(pChannel);
        Ret = ERROR_SUCCESS;
    }
    FreeAvctpLock();

    return Ret;
    
}


int Avctp::L2CA_StackEvent (int iEvent, void *pEventContext) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_StackEvent %d\n", iEvent));

    if (iEvent == BTH_STACK_DISCONNECT)
        ;
        //AvctpCloseDriverInstance ();
    else if (iEvent == BTH_STACK_DOWN)
        StackDown();
    else if (iEvent == BTH_STACK_UP)
        StackUp();

    FreeAvctpLock();
    return ERROR_SUCCESS;

}


int Avctp::L2CA_ConfigInd (unsigned char id, 
    unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, 
    struct btFLOWSPEC *pInFlow, int cOptNum, 
    struct btCONFIGEXTENSION **pExtendedOptions) {

    int Ret = ERROR_SUCCESS;

    RETAILMSG(1, (L"+Avctp: L2CA_ConfigInd\r\n"));

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"AVCTP: L2CA_ConfigInd: ch 0x%04x id %d MTU %d flush 0x%04x, flow: %s\n", cid, id, usOutMTU, usInFlushTO, pInFlow ? L"yes" : L"no"));


    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else {

        AvctpChannel *pChannel = FindChannel(cid);
        if (! pChannel) {
            Ret = ERROR_NOT_FOUND;
        } else {

            int fAccept = FALSE;

            if ((usInFlushTO == 0xffff) && 
                (! pInFlow || (pInFlow->service_type == 0x01))) {
                if (usOutMTU)
                    pChannel->m_outMTU = usOutMTU;
                else
                    pChannel->m_outMTU = L2CAP_MTU;
                
                fAccept = TRUE;
            }

            int         L2CapErr = ERROR_INTERNAL_ERROR;
            HANDLE      hL2Cap = m_hL2CAP;
            L2CA_ConfigResponse_In pfnL2CA_ConfigRspIn = 
                gpAVCTP->m_L2CapIntf.l2ca_ConfigResponse_In;
            FreeAvctpLock();

            __try {
                L2CapErr = pfnL2CA_ConfigRspIn (hL2Cap, NULL, id, cid, 
                    fAccept ? 0 : 2, usOutMTU, 0xffff, NULL, 0, NULL);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }

            if (ERROR_SUCCESS != L2CapErr) {
                fAccept = FALSE;
                if (ERROR_SUCCESS == Ret)
                    Ret = L2CapErr;
            }

            GetAvctpLock();
            if (gpAVCTP && FindChannel(pChannel)) {
                if (fAccept) {
                    if (ChannelConfig_e == pChannel->m_ChannelState) {
                        pChannel->m_ChannelState = ChannelConfigRemoteDone_e;
                    } else if (ChannelConfigLocalDone_e == pChannel->m_ChannelState) {
                        pChannel->m_ChannelState = ChannelEstablished_e;
                        
                        AvctpUser *pUser;
                        for (pUser = gpAVCTP->m_pUsers; pUser; pUser = pUser->m_pNext) {
                            pUser->SetChannelState(pChannel, ChannelEstablished_e);
                        }
                    } else {
                        ASSERT(ChannelClosing_e == pChannel->m_ChannelState);
                    }

                } else {
                    DeleteChannel(pChannel);
                }
            } else {
                Ret = ERROR_NOT_FOUND;
            }

        }
    }

    FreeAvctpLock();
    return Ret;
}


int Avctp::L2CA_ConfigReqOut (AvctpChannel *pChannel, ushort Result, 
    ushort InMTU, ushort OutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, 
    struct btCONFIGEXTENSION **pExtendedOptions) {

    int Ret = ERROR_SUCCESS;

    RETAILMSG(1, (L"+Avctp: L2CA_ConfigReqOut\r\n"));

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_ConfigReqOut for call result 0x%04x mtu %d flush 0x%04x, flow %s\n", 
        Result, InMTU, OutFlushTO, pOutFlow ? L"yes" : L"no" ));

    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else if (! FindChannel(pChannel)) {
        Ret = ERROR_NOT_FOUND;
    } else {

        if (Result) {
            // if Result is one it just means pending
            if (Result != 1) {
                DeleteChannel(pChannel);
            }

        } else {

            if (ChannelConfig_e == pChannel->m_ChannelState) {
                pChannel->m_ChannelState = ChannelConfigLocalDone_e;
            } else if (ChannelConfigRemoteDone_e == pChannel->m_ChannelState) {
                pChannel->m_ChannelState = ChannelEstablished_e;

                AvctpUser *pUser;
                for (pUser = gpAVCTP->m_pUsers; pUser; pUser = pUser->m_pNext) {
                    pUser->SetChannelState(pChannel, ChannelEstablished_e);
                }

            } else {
                ASSERT(0);
            }

        }
    }

    FreeAvctpLock();

    return Ret;

}


DWORD WINAPI AvctpAuthThread(LPVOID lpv) {

    GetAvctpLock();
    if (gpAVCTP)
        return gpAVCTP->AuthThread((AvctpChannel *)lpv);

    FreeAvctpLock();
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


int Avctp::AuthThread (AvctpChannel *pChannel) {
    int     Ret = ERROR_SUCCESS;
    int     fFreeLock = TRUE;
    ushort  Result = ConnRefusedNoResources_e;
    BD_ADDR RemoteAddr;
    HANDLE  hL2Cap;
    ushort  ChannelId;
    uchar   Id;
    bool    fIncoming = false;

    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else if (! FindChannel(pChannel)) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else {

        Result = ConnSuccess_e;

        // PSM is correct & we have the channel
        // send pending connect resp, authenticate, notify the upper layer
        
        RemoteAddr = pChannel->m_BdAddr;
        fIncoming = pChannel->m_fIncoming;

        if (fIncoming) {
            RemoteAddr = pChannel->m_BdAddr;

            hL2Cap = m_hL2CAP;
            ChannelId = pChannel->m_cid;
            Id = pChannel->m_Identifier;
            
            L2CA_ConnectResponse_In pfnL2CA_ConnectRspIn = 
                m_L2CapIntf.l2ca_ConnectResponse_In;
            FreeAvctpLock();
            pfnL2CA_ConnectRspIn(hL2Cap, NULL, &RemoteAddr, Id, ChannelId, 1, 1);
            GetAvctpLock();
        }

        BT_ADDR TempAddr = SET_NAP_SAP(RemoteAddr.NAP, RemoteAddr.SAP);

        FreeAvctpLock();

        int SecErr = BthAuthenticate(&TempAddr);
        if (ERROR_SUCCESS == SecErr) {
            SecErr = BthSetEncryption(&TempAddr, TRUE);
        }
        
        GetAvctpLock();

        if (SecErr)
            Result = ConnRefusedSecurityBlock_e;

        if (! FindChannel(pChannel)) {
            Result = ConnRefusedSecurityBlock_e;
            Ret = ERROR_NOT_FOUND;
        }

        if ((ConnSuccess_e == Result) && fIncoming) {

            AvctpUser *pUser;

            // for each user create a User channel
            for (pUser = m_pUsers; pUser; pUser = pUser->m_pNext) {

                AvctpUserChannel *pUserChannel = new AvctpUserChannel(pChannel);
                if (! pUserChannel) {
                    Result = ConnRefusedNoResources_e;
                    break;
                }
                pUserChannel->m_pChannel = pChannel;
                pUserChannel->m_UChannelState = UChannelConnectInd_e;
                pUserChannel->m_Identifier = Id;

                pUserChannel->m_pNext = pUser->m_pUserChannels;
                pUser->m_pUserChannels = pUserChannel;

                pChannel->m_cPendingInd++;
            }
            // if we ran out of resources delete all of the User channels
            if (ConnRefusedNoResources_e == Result) {
                AvctpUser *pLastUser = pUser;
                
                for (pUser = m_pUsers; pUser && (pUser != pLastUser); 
                    pUser = pUser->m_pNext) {

                    AvctpUserChannel *pUserChannel = pUser->m_pUserChannels;

                    if (pUserChannel && (pUserChannel->m_pChannel == pChannel)) {
                        // take it out of the list and delete
                        pUser->m_pUserChannels = pUserChannel->m_pNext;
                        delete (pUserChannel);
                    } else {
                        ASSERT(0);
                    }
                }
                // now delete the channel itself
                ASSERT(m_pChannels == pChannel);
                m_pChannels = pChannel->m_pNext;
                delete pChannel;
                pChannel = NULL;
                
            }
        }

        if ((ConnSuccess_e == Result) && fIncoming) {

            // now notify each user
            AvctpUser *pUser = m_pUsers;

            int UserErr = 1;
            int fAccepted = 0;
            while (pUser) {

                // if we have notified this user or the channel state is not what
                // we expect then skip to the next one
                AvctpUserChannel *pUserChannel = pUser->FindUserChannel(pChannel);
                if ((! pUserChannel) || 
                    (UChannelConnectInd_e != pUserChannel->m_UChannelState)) {
                    pUser = pUser->m_pNext;
                    continue;
                }
                
                AVCT_Connect_Ind    pfnAVCT_ConnectInd = pUser->m_IndicateFns.AvctConnectInd;

                pUserChannel->m_UChannelState = UChannelConnectIndNotified_e;

                FreeAvctpLock();
                UserErr = pfnAVCT_ConnectInd(&RemoteAddr);
                GetAvctpLock();

                pChannel = FindChannel(ChannelId);
                if (! pChannel) {
                    ASSERT(0);
                    Result = ConnRefusedNoResources_e;
                    break;
                }
                if (UserErr)
                    pChannel->m_cPendingInd--;
                else
                    fAccepted++;
                
                // start from first user as we left the lock
                pUser = m_pUsers;
            }

            if (! fAccepted) {
                // this means no upper layer accepted the indication--fail it ourselves
                Result = ConnRefusedSecurityBlock_e;
            }
        }
        
        // For outgoing connections send a config request
        if ((ERROR_SUCCESS == Ret) && !fIncoming) {
            if (ConnSuccess_e != Result) {
                DeleteChannel(pChannel);
            } else {
                int L2CapErr;

                ushort cid = pChannel->m_cid;
                unsigned short  Mtu = pChannel->m_inMTU;
                HANDLE hL2C = m_hL2CAP;
                L2CA_ConfigReq_In pfnL2CA_ConfigReqIn = 
                    m_L2CapIntf.l2ca_ConfigReq_In;

                FreeAvctpLock();

                __try {
                    L2CapErr = pfnL2CA_ConfigReqIn (hL2C, pChannel, cid, Mtu, 
                        0xffff, NULL, 0, NULL);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    L2CapErr = ERROR_INTERNAL_ERROR;
                }

                GetAvctpLock();
                if (L2CapErr) {
                    DeleteChannel(pChannel);
                }
            }
        }
    }

    if (ERROR_SERVICE_NOT_ACTIVE != Ret) {
        if (ConnSuccess_e != Result && fIncoming) {
            // there was a failure we need to fail to L2Cap ourselves
            //HANDLE hL2Cap = m_hL2CAP;
            ASSERT(hL2Cap);
            L2CA_ConnectResponse_In pfnL2CA_ConnectRspIn = 
                m_L2CapIntf.l2ca_ConnectResponse_In;
            FreeAvctpLock();
            fFreeLock = FALSE;
            pfnL2CA_ConnectRspIn(hL2Cap, NULL, &RemoteAddr, Id, ChannelId, Result, 0);
        }
    }

    if (fFreeLock)
        FreeAvctpLock();

    return ERROR_SUCCESS;    
}


int Avctp::L2CA_ConnectInd (BD_ADDR *pRemoteAddr,
    unsigned short ChannelId, unsigned char Id, unsigned short PSM) {

    int                 Ret = ERROR_SUCCESS;
    int                 fFreeLock = TRUE;
    unsigned short      Result;

    RETAILMSG(1, (L"+Avctp: L2CA_ConnectInd\r\n"));

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_ConnectInd: from %04x%08x ch 0x%04x id %d psm 0x%04x\n", 
        pRemoteAddr->NAP, pRemoteAddr->SAP, ChannelId, Id, PSM));
    
    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else if (AVCTP_PSM == PSM) {
        Result = ConnSuccess_e;
        // the PSM is correct, notify the upper layer

        // Search for this channel among existing ones.
        AvctpChannel *pChannel = FindChannel(ChannelId);

        if (pChannel) {
            // why did we get an indication of an already existing channel
            ASSERT(0);
            Result = ConnRefusedNoResources_e;
        } else {
            pChannel = new AvctpChannel();
            if (! pChannel) {
                Result = ConnRefusedNoResources_e;
            } else {
                memset(pChannel, 0, sizeof(AvctpChannel));
                pChannel->m_BdAddr = *pRemoteAddr;
                pChannel->m_cid = ChannelId;
                pChannel->m_ChannelState = ChannelConnectInd_e;
                pChannel->m_Identifier = Id;
                pChannel->m_fIncoming = true;

                pChannel->m_pNext = m_pChannels;
                m_pChannels = pChannel;
            }
        }

        if (ConnSuccess_e == Result) {
            btutil_ScheduleEvent(AvctpAuthThread, pChannel, 0);
        }

    } else {
        Result = ConnRefusedPSMNotSupp_e;
    }

    if (ERROR_SERVICE_NOT_ACTIVE != Ret) {
        if (ConnSuccess_e != Result) {
            // there was a failure we need to fail to L2Cap ourselves
            HANDLE hL2Cap = m_hL2CAP;
            L2CA_ConnectResponse_In pfnL2CA_ConnectRspIn = 
                m_L2CapIntf.l2ca_ConnectResponse_In;
            FreeAvctpLock();
            fFreeLock = FALSE;
            pfnL2CA_ConnectRspIn(hL2Cap, NULL, pRemoteAddr, Id, ChannelId, Result, 0);
        }
    }

    if (fFreeLock)
        FreeAvctpLock();
    
    return Ret;
}


// this routine should get called after a successful AvctConnectReqIn
// it calls the users Connect confirmation routine
int Avctp::L2CA_ConnectReqOut (AvctpChannel *pChannel, ushort cid, 
    ushort Result, ushort Status) {

    int             Ret;

    RETAILMSG(1, (L"+Avctp: L2CA_ConnectReqOut\r\n"));
    
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"Avctp: L2CA_ConnectReqOut pChannel 0x%08x ch 0x%04x Result = 0x%04x Status 0x%04x\n",
        L2CA_ConnectReqOut, cid, Result, Status));

    ASSERT(! m_fIsRunning || (pChannel == FindChannel(pChannel)));

    if (! m_fIsRunning) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else if (! FindChannel(pChannel)) {
        Ret = ERROR_NOT_FOUND;
    } else {
        Ret = ERROR_SUCCESS;

        if (Result) {
            // if Result is one it just means pending
            if (Result != 1) {
                DeleteChannel(pChannel);
            }
        } else {
            pChannel->m_ChannelState = ChannelConfig_e;  
            pChannel->m_cid = cid;            
            
            // This will authenticate and then send a config request
            btutil_ScheduleEvent(AvctpAuthThread, pChannel, 0);
        }
    }

    FreeAvctpLock();

    return Ret;
}


//
//      Init stuff
//

int avctp_InitializeOnce(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"++avctp_InitializeOnce\n"));

    CTEInitLock(&gAvctpLock);

    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"--avctp_InitializeOnce:: ERROR_SUCCESS\n"));

//exit:    
    return dwRetVal;
}


int avctp_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+avctp_CreateDriverInstance\r\n"));
    int Ret;

    GetAvctpLock();

    if (gpAVCTP) {
        Ret = ERROR_ALREADY_INITIALIZED;
    } else {
        BOOL fEnabled = TRUE;

        CReg reg(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Bluetooth\\avctp");
        if (reg.IsOK()) {
            fEnabled = reg.ValueDW(L"Enabled", TRUE);
        }

        if (fEnabled) {
            gpAVCTP = new Avctp;
            if (gpAVCTP) {
                Ret = ERROR_SUCCESS;
            } else {
                Ret = ERROR_OUTOFMEMORY;
            }
        } else {
            Ret = ERROR_SUCCESS; // Return success so stack keeps loading
        }
    }
    
    FreeAvctpLock();

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-avctp_CreateDriverInstance : Ret %d\n", Ret));
    return Ret;
}


int avctp_CloseDriverInstance (void) {
    int Ret;
    
    IFDBG(DebugOut (DEBUG_LAYER_TRACE, L"+avctp_CloseDriverInstance\n"));


    GetAvctpLock();
    if (! gpAVCTP) {
        Ret = ERROR_SERVICE_NOT_ACTIVE;
    } else {
        Ret = ERROR_SUCCESS;
        delete gpAVCTP;
        gpAVCTP = NULL;
    }
    FreeAvctpLock();

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-avctp_CloseDriverInstance : Ret %d\n", Ret));

    return Ret;
}

