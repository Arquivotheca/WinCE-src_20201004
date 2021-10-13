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
#include "AvctpUser.hpp"
#include "Avctp.hpp"

AvctpUserChannel::AvctpUserChannel(AvctpChannel *pChannel) {
    m_pNext = NULL;
    m_UChannelState = UChannelOpen_e;
    m_fNotified = 0;
    m_Identifier = 0;
    m_pChannel = pChannel;
}


AvctpUserChannel ** AvctpUser::_FindUserChannel(AvctpChannel *pChannel) {
    AvctpUserChannel **ppUserChannel, *pUserChannel;

    ppUserChannel = &m_pUserChannels;

    while (pUserChannel = *ppUserChannel) {
        if (pUserChannel->m_pChannel == pChannel)
            break;
        ppUserChannel = &(pUserChannel->m_pNext);
    }

    return ppUserChannel;
}


AvctpUserChannel * AvctpUser::FindUserChannel(AvctpChannel *pChannel) {
    
    return *_FindUserChannel(pChannel);
}


void AvctpUser::InsertUserChannel(AvctpUserChannel *pChannel) {

    pChannel->m_pNext = m_pUserChannels;
    m_pUserChannels = pChannel;
}


void AvctpUser::SetChannelState(AvctpChannel *pChannel, ChannelState NewChState) {
    AvctpUserChannel *pUserChannel = FindUserChannel(pChannel);

    if (pUserChannel) {
        switch (NewChState) {

          case ChannelEstablished_e:
            
            if (UChannelEstablished_e != pUserChannel->m_UChannelState) {
                if (UChannelConnecting_e == pUserChannel->m_UChannelState) {
                    AVCT_Connect_Out    pfnConnectOut = m_CallbackFns.AvctConnectOut;
                    BD_ADDR             BdAddr = pChannel->m_BdAddr;

                    __try {
                        pfnConnectOut(&BdAddr, avct_connect_success_e, 0, 0);
                    }
                    __except(EXCEPTION_EXECUTE_HANDLER) {
                    }
                }

                pUserChannel->m_UChannelState = UChannelEstablished_e;

            }

            break;

          default:
            break;
        }
    }


}


void AvctpUser::DeleteUserChannel(AvctpChannel *pChannel) {

    AvctpUserChannel **ppUserChannel = _FindUserChannel(pChannel);
    AvctpUserChannel *pUserChannel = *ppUserChannel;

    if (pUserChannel) {
        *ppUserChannel = pUserChannel->m_pNext;

        // notify user if needed!

        AVCT_Disconnect_Ind pfnAvctDiscInd = m_IndicateFns.AvctDisconnectInd;
        if (pfnAvctDiscInd) {
            BD_ADDR Addr = pChannel->m_BdAddr;

            FreeAvctpLock();
            __try {
                pfnAvctDiscInd(&Addr);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
            GetAvctpLock();
        }

        delete (pUserChannel);
    }

}


int AvctpUser::ConnectReqIn(BD_ADDR *pAddr, ushort PID, 
    LocalSideConfig *pLocalConfig) {

    int             Ret;

    RETAILMSG(1, (L"+AvctpUser: ConnectReqIn\r\n"));

    if (! pAddr) {
        Ret = ERROR_INVALID_PARAMETER;
    } else {

        AvctpChannel        *pChannel;
        AvctpUserChannel    *pUserChannel;
        AVCT_Connect_Out    pfnConnectOut = m_CallbackFns.AvctConnectOut;

        if (pChannel = gpAVCTP->FindChannel(pAddr)) {
            int fCallConnectOut = 0;
            
            if (pUserChannel = FindUserChannel(pChannel)) {

                RETAILMSG(1, 
                    (L"AvctpUser: ConnectReqIn: using pre-existing connection\r\n"));

                // great we already have a user channel if it is established
                // we're done otherwise we get notified later
                if (ChannelEstablished_e == pChannel->m_ChannelState) {
                    fCallConnectOut = 1;
                    pUserChannel->m_UChannelState = UChannelEstablished_e;
                } else {
                    // shouldn't have to do anything here!
                }
                Ret = REQUEST_ACCEPTED;
            } else {
                pUserChannel = new AvctpUserChannel(pChannel);

                if (pUserChannel) {
                    pUserChannel->m_pChannel = pChannel;
                    pUserChannel->m_UChannelState = UChannelEstablished_e;
                    Ret = REQUEST_ACCEPTED;
                    fCallConnectOut = 1;
                } else {
                    Ret = ERROR_NOT_ENOUGH_MEMORY;
                }
            }

            if (fCallConnectOut) {
                __try {
                    pfnConnectOut(pAddr, avct_connect_success_e, 0, 0);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                }
            }
        } else {
            pChannel = new AvctpChannel;
            if (pChannel) {
                pUserChannel = new AvctpUserChannel(pChannel);
                if (! pUserChannel) {
                    delete pChannel;
                }
            }

            if (pChannel && pUserChannel) {
                memset(pChannel, 0, sizeof(*pChannel));
                pChannel->m_BdAddr = *pAddr;
                pChannel->m_ChannelState = ChannelConnecting_e;
                //pChannel->m_cid = 0;
                pChannel->m_ProfileID = PID;
                pChannel->m_inMTU = pLocalConfig->MTU;
                //pChannel->m_outMTU = 0;
                pChannel->m_FlushTO= pLocalConfig->FlushTO;

                pChannel->m_pNext = gpAVCTP->m_pChannels;
                gpAVCTP->m_pChannels = pChannel;

                pUserChannel->m_UChannelState = UChannelConnecting_e;
                InsertUserChannel(pUserChannel);
            
                HANDLE hL2Cap = gpAVCTP->m_hL2CAP;
                L2CA_ConnectReq_In pfnL2CappConnReqIn = 
                    gpAVCTP->m_L2CapIntf.l2ca_ConnectReq_In;
                int L2CapErr;
                
                FreeAvctpLock();
                L2CapErr = pfnL2CappConnReqIn(hL2Cap, pChannel, 
                    AVCTP_PSM, pAddr);

                GetAvctpLock();
                if (ERROR_SUCCESS != L2CapErr) {                    
                    // first we should find it again!
                    gpAVCTP->DeleteChannel(pChannel);

                    Ret = L2CapErr;

                } else {
                    Ret = REQUEST_ACCEPTED;
                }
            } else {
                Ret = ERROR_NOT_ENOUGH_MEMORY;
            }
        }

    }

    FreeAvctpLock();

    return Ret;

}


int AvctpUser::ConnectRspIn(BD_ADDR *pAddr, ushort ConnectResult, ushort Status, 
        LocalSideConfig *pLocalConfig) {

    int                 Ret;
    AvctpChannel        *pChannel = gpAVCTP->FindChannel(pAddr);;
    AvctpUserChannel    **ppUserChannel = _FindUserChannel(pChannel);
    AvctpUserChannel    *pUserChannel = *ppUserChannel;
    int                 fNotifyL2Cap = 0;
    int                 fDeleteUserChannel = 0;

    RETAILMSG(1, (L"+AvctpUser: ConnectRspIn\r\n"));

    if (pUserChannel && 
        (UChannelConnectIndNotified_e == pUserChannel->m_UChannelState)) {
        pChannel = pUserChannel->m_pChannel;

        Ret = 0;

        if (ChannelEstablished_e == pChannel->m_ChannelState) {
            // someone else already accepted it
            if (ConnSuccess_e != ConnectResult) {
                fDeleteUserChannel = 1;
            }
        } else if (ChannelConnectInd_e == pChannel->m_ChannelState) {
            if (ConnSuccess_e == ConnectResult) {
                // establish conn
                fNotifyL2Cap = 1;

            } else {
                if (0 == --(pChannel->m_cPendingInd)) {
                    fNotifyL2Cap = 2;
                }
            }
        }

    } else {
        Ret = 1; // failure to match any outstanding connection request
        fDeleteUserChannel = 1;
    }

    if (fNotifyL2Cap) {
        HANDLE          hL2Cap = gpAVCTP->m_hL2CAP;
        unsigned char   Id = pUserChannel->m_Identifier;
        unsigned short  ChannelId = pChannel->m_cid;
        int             L2CapErr;
        
        L2CA_ConnectResponse_In pfnL2CA_ConnectRspIn = 
            gpAVCTP->m_L2CapIntf.l2ca_ConnectResponse_In;
        L2CA_ConfigReq_In pfnL2CA_ConfigReqIn = 
            gpAVCTP->m_L2CapIntf.l2ca_ConfigReq_In;

        if (1 == fNotifyL2Cap) {
            pChannel->m_ChannelState = ChannelConfig_e;
            pUserChannel->m_UChannelState = UChannelConfig_e;
        } else {
            if (pUserChannel) {
                *ppUserChannel = pUserChannel->m_pNext;
                delete (pUserChannel);
                pUserChannel = NULL;
            }
        }

        FreeAvctpLock();
        L2CapErr = pfnL2CA_ConnectRspIn(hL2Cap, pChannel, pAddr, Id, 
            ChannelId, ConnectResult, 0);

        // if we were trying to establish the conn then configure it
        if ((ConnSuccess_e == ConnectResult) && (! L2CapErr)) {
            ushort Mtu;
            ushort FlushTO;

            if (pLocalConfig) {
                Mtu = pLocalConfig->MTU;
                FlushTO = pLocalConfig->FlushTO;
                pChannel->m_inMTU = Mtu;
            } else {
                Mtu = 0;
                FlushTO = 0xffff;
                pChannel->m_inMTU = L2CAP_MTU;
            }
            
            L2CapErr = pfnL2CA_ConfigReqIn(hL2Cap, pChannel, ChannelId, 
                Mtu, FlushTO, NULL, 0, NULL);
        }

        GetAvctpLock();
        
    }

     if (fDeleteUserChannel && pUserChannel) {
        *ppUserChannel = pUserChannel->m_pNext;
        delete (pUserChannel);
        pUserChannel = NULL;
    }

    FreeAvctpLock();

    return Ret;

}


int AvctpUser::DisconnectReqIn(BD_ADDR *pAddr, ushort ProfileId) {
    int     Ret = ERROR_SUCCESS;

    if (ProfileId != m_ProfileId) {
        Ret = ERROR_INVALID_PARAMETER;
    } else {

        AvctpChannel *pChannel = gpAVCTP->FindChannel(pAddr);
        if (pChannel && (pChannel->m_ChannelState >= ChannelEstablished_e)) {
            AvctpUserChannel **ppUserChannel = _FindUserChannel(pChannel);
            AvctpUserChannel *pUserChannel = *ppUserChannel;

            if (pUserChannel) {
                pUserChannel->m_UChannelState = UChannelClosing_e;
                *ppUserChannel = pUserChannel->m_pNext;
                delete pUserChannel;
            }

            Ret = gpAVCTP->DisconnectChannel(pChannel);
            
        } else {
            Ret = ERROR_REQUEST_REFUSED;
        }

        if (ERROR_SUCCESS == Ret) {

            AvctpUser *pUser = gpAVCTP->FindUser(ProfileId);

            if (pUser == this) {

                AVCT_Disconnect_Out pfnDiscOut = 
                    m_CallbackFns.AvctDisconnectOut;

                FreeAvctpLock();

                // we shouldn't need to do anything on a disconnect out
                __try {
                    pfnDiscOut(pAddr, 0);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                }
                GetAvctpLock();
            }

        }
        
    }

    FreeAvctpLock();
    return Ret;

}


ushort AvctpUser::CalculatePackets(ushort cData, ushort Mtu) {

    ushort  cPackets;

    ASSERT(Mtu >= 48);

    if (cData + (uint)AvctpPacketHdrSize <= (uint)Mtu) {
        cPackets = 1;
    } else {
        cPackets = 1;
        cData -= (Mtu - AvctpFragStartHdrSize);

        // our effective Mtu is the following
        Mtu -= AvctpFragContHdrSize;

        cPackets += (cData + Mtu - 1) / (Mtu);
    }

    return cPackets;

}


int AvctpUser::SendFragPkt(AvctpChannel *pChannel, BD_ADDR * pAddr, 
    uchar Transaction, uchar Type, ushort ProfileId, BD_BUFFER *pBdSendBuf) {
    int     Ret = ERROR_SUCCESS;
    HANDLE  hL2Cap = gpAVCTP->m_hL2CAP;
    ushort  ChannelId = pChannel->m_cid;
    uint    Mtu = pChannel->m_outMTU;
    uint    cData = BufferTotal(pBdSendBuf);
    uint    cSent = 0;
    L2CA_DataDown_In pfnL2CapDataDownIn = 
        gpAVCTP->m_L2CapIntf.l2ca_DataDown_In;

    FreeAvctpLock();

    BD_BUFFER   *pBdBuf = BufferAlloc(Mtu + 
        gpAVCTP->m_cHeaders + gpAVCTP->m_cTrailers);

    if (pBdBuf) {
        ushort      cPackets = CalculatePackets(cData, Mtu);
        ushort      cSend;
        uint        cHdr;

        pBdBuf->fMustCopy = TRUE;

        for (ushort cPacket = 1; cPacket <= cPackets; cPacket++) {
            pBdBuf->cStart = gpAVCTP->m_cHeaders;
            pBdBuf->cEnd = pBdBuf->cSize - gpAVCTP->m_cTrailers;
            
            UNALIGNED AvctpCommonPacketHdr *pCommonHdr = 
                (UNALIGNED AvctpCommonPacketHdr *)
                &pBdBuf->pBuffer[pBdBuf->cStart];

            pCommonHdr->Transaction = Transaction;
            pCommonHdr->PacketType = 0;
            if (avct_rsp_msg_e == Type)
                pCommonHdr->CR = 1;
            else 
                pCommonHdr->CR = 0;
            pCommonHdr->IPID = 0;
            
            if (1 == cPacket) {
                cHdr = AvctpFragStartHdrSize;
                cSend = Mtu - cHdr;
                UNALIGNED AvctpFragStartHdr *pHdr = 
                    (UNALIGNED AvctpFragStartHdr *)(pBdBuf->pBuffer + pBdBuf->cStart);

                pHdr->Common.PacketType = 1;
                pHdr->cPackets = (uchar)cPackets;
                pHdr->ProfileID = ProfileId;
            } else {
                UNALIGNED AvctpFragContHdr *pHdr = 
                    (UNALIGNED AvctpFragContHdr *)(pBdBuf->pBuffer + pBdBuf->cStart);

                cHdr = AvctpFragContHdrSize;
                cSend = Mtu - cHdr;

                if (cPacket == cPackets)
                    pHdr->Common.PacketType = 3;
                else
                    pHdr->Common.PacketType = 2;

            }

            if (cSend > BufferTotal(pBdSendBuf)) {
                ASSERT(cPacket == cPackets);
                cSend = BufferTotal(pBdSendBuf);
                pBdBuf->cEnd = pBdBuf->cStart + cSend + cHdr;
                ASSERT(pBdBuf->cEnd <= pBdBuf->cSize);
            }

            cSent += cSend;
            ASSERT(cSent <= cData);

            if (! BufferGetChunk(pBdSendBuf, cSend, 
                pBdBuf->pBuffer + pBdBuf->cStart + cHdr)) {
                ASSERT(0);
                break;
            }

            ASSERT(pBdBuf->cEnd - pBdBuf->cStart == cSend + cHdr);

            Ret = pfnL2CapDataDownIn(hL2Cap, NULL, ChannelId, pBdBuf);
            if (Ret) {
                break;
            }

        }
        
        BufferFree(pBdBuf);
    } else {
        Ret = ERROR_NOT_ENOUGH_MEMORY;
    }

    return Ret;

}


AvctpUser::SendMsgIn(BD_ADDR * pAddr, uchar Transaction, uchar Type, 
    ushort ProfileId, BD_BUFFER *pBdSendBuf) {

    int Ret;
    int fReleaseLock = 1;

    RETAILMSG(1, (L"+AvctpUser: SendMsgIn\r\n"));


    if (ProfileId != m_ProfileId) {
        Ret = ERROR_INVALID_PARAMETER;
    } else if (! pAddr) {
        Ret = ERROR_INVALID_PARAMETER;
    } else if (pBdSendBuf->cStart < (
        AvctpFragStartHdrSize + gpAVCTP->m_cHeaders)) {
        ASSERT(0);
        Ret = ERROR_INVALID_PARAMETER;
    } else {
        AvctpChannel *pChannel = gpAVCTP->FindChannel(pAddr);

        Ret = ERROR_CONNECTION_INVALID;

        if (pChannel && (ChannelEstablished_e == pChannel->m_ChannelState)) {
            AvctpUserChannel *pUserChannel = FindUserChannel(pChannel);

            if (pUserChannel) {
                HANDLE  hL2Cap = gpAVCTP->m_hL2CAP;
                ushort  ChannelId = pChannel->m_cid;
                uint    Mtu = pChannel->m_outMTU;
                uint    cData = BufferTotal(pBdSendBuf);

                L2CA_DataDown_In pfnL2CapDataDownIn = 
                    gpAVCTP->m_L2CapIntf.l2ca_DataDown_In;


                if (cData + AvctpPacketHdrSize > Mtu) {

                    Ret = SendFragPkt(pChannel, pAddr, Transaction, Type,
                        ProfileId, pBdSendBuf); 
                    fReleaseLock = 0;

                } else {

                    FreeAvctpLock();
                    fReleaseLock = 0;

                    pBdSendBuf->cStart -= AvctpPacketHdrSize;
                    UNALIGNED AvctpPacketHdr *pHdr = (UNALIGNED AvctpPacketHdr *)
                        (pBdSendBuf->pBuffer + pBdSendBuf->cStart);

                    pHdr->Common.Transaction = Transaction;
                    pHdr->Common.PacketType = 0;
                    if (avct_rsp_msg_e == Type)
                        pHdr->Common.CR = 1;
                    else 
                        pHdr->Common.CR = 0;
                    
                    pHdr->Common.IPID = 0;

                    pHdr->ProfileID = ProfileId;

                    // we shouldn't need to do anything on a DataDownOut
                    Ret = pfnL2CapDataDownIn(hL2Cap, NULL, ChannelId, pBdSendBuf);

                }
                
            } 
        }

    }

    if (fReleaseLock)
        FreeAvctpLock();

    return Ret;
    
}


