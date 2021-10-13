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

#ifndef __AVCTP_USER_HPP__
#define __AVCTP_USER_HPP__

#include <bt_avctp.h>


// these really belong in bt_ddi.h
enum ConnResponse {
    ConnSuccess_e,
    ConnPending_e,
    ConnRefusedPSMNotSupp_e,
    ConnRefusedSecurityBlock_e,
    ConnRefusedNoResources_e
};

enum ConnStatus {
    NoInfo_e,
    AuthenPending_e,
    AuthorPending_e,
};



const int AvctpPacketHdrSize = 3;
const int AvctpFragStartHdrSize = 4;
const int AvctpFragContHdrSize = 1;

#pragma pack(push)
#pragma pack(1)

typedef struct _AvctpCommonPacketHdr {
    unsigned char   IPID : 1;
    unsigned char   CR : 1;
    unsigned char   PacketType : 2;
    unsigned char   Transaction : 4;
} AvctpCommonPacketHdr;


typedef struct _AvctpPacketHdr {
    AvctpCommonPacketHdr    Common;
    UNALIGNED unsigned short          ProfileID;
} AvctpPacketHdr;


typedef struct _AvctpFragStartHdr {
    AvctpCommonPacketHdr    Common;
    unsigned char       cPackets;
    unsigned short      ProfileID;
} AvctpFragStartHdr;


typedef struct _AvctpFragContHdr {
    AvctpCommonPacketHdr    Common;
} AvctpFragContHdr;

#pragma pack(pop)


enum ChannelState {
    ChannelOpen_e,
    ChannelConnecting_e,
    ChannelConnectInd_e,
    ChannelConfig_e,
    ChannelConfigLocalDone_e,
    ChannelConfigRemoteDone_e,
    ChannelEstablished_e,
    ChannelClosing_e

};


class AvctpChannel {
    friend class Avctp;
    friend class AvctpUser;
    AvctpChannel        *m_pNext;
    BD_ADDR             m_BdAddr;
    int                 m_cUsers;
    int                 m_cPendingInd;  // Pending indications
    ChannelState        m_ChannelState;
    class AvctpUser     *m_pUser;
    unsigned short      m_ProfileID;
    unsigned short      m_cid;          // channel id
    unsigned short      m_inMTU;
    unsigned short      m_outMTU;
    unsigned short      m_FlushTO;
    unsigned char       m_Identifier;
    bool                m_fIncoming;

};


enum UserChannelState {
    UChannelOpen_e,
    UChannelConnecting_e,
    UChannelConnectInd_e,
    UChannelConnectIndNotified_e,
    UChannelConfig_e,
    UChannelEstablished_e,
    UChannelClosing_e

};


class AvctpUserChannel {
    friend class Avctp;
    friend class AvctpUser;
    AvctpUserChannel    *m_pNext;
    AvctpChannel        *m_pChannel;    // ptr to the real channel
    UserChannelState    m_UChannelState;
    unsigned int        m_fNotified;
    unsigned char       m_Identifier;

    AvctpUserChannel(AvctpChannel *pChannel);

};


enum AvctpReqType {
    ConnectReq_t = 0x20,
    DisconnectReq_t,
    SendReq_t
};


class AvctpRequest {
    AvctpRequest    *m_pNext;
    AvctpReqType    m_What;
    AvctpUser       *m_pUser;

    BD_ADDR         m_BdAddr;

};


enum AvctpUserState {
    StateOpen_e,
    StateConnected_e,
    StateClosed_e

};


//
// The AvctpUser class describes an upper layer that is sitting
// on top of AVCTP.  An instance of this struct is created when the
// upper layer calls EstablishDeviceContext to register itself.
//
class AvctpUser : public SVSRefObj {
public:
    AvctpUser               *m_pNext;
    AVCT_EVENT_INDICATION   m_IndicateFns;         // Upper layer events
    AVCT_CALLBACKS          m_CallbackFns;          // Upper layer callbacks

    HANDLE                  m_hContext;   // Unique handle to an particular owner context

    void                    *m_pUserContext;

    AvctpUserChannel        *m_pUserChannels;
    ushort                  m_ProfileId;
    //AvctpUserState          m_State;

    AvctpUserChannel **_FindUserChannel(AvctpChannel *pChannel);
    AvctpUserChannel *FindUserChannel(AvctpChannel *pChannel);
    void DeleteUserChannel(AvctpChannel *pChannel);
    void InsertUserChannel(AvctpUserChannel *pChannel);
    void SetChannelState(AvctpChannel *pChannel, ChannelState NewChState);


    int ConnectReqIn (BD_ADDR *pAddr, ushort PID, LocalSideConfig *pConfig);
    int ConnectRspIn(BD_ADDR *pAddr, ushort ConnectResult, ushort Status, 
        LocalSideConfig *pConfig);
    int DisconnectReqIn(BD_ADDR *pAddr, ushort PID);

    ushort CalculatePackets(ushort cData, ushort Mtu);
    int SendFragPkt(AvctpChannel *pChannel, BD_ADDR * pAddr, 
        uchar Transaction, uchar Type, ushort ProfileId, BD_BUFFER *pBdSendBuf);

    
    int SendMsgIn(BD_ADDR *pAddr, uchar Transaction, uchar Type, ushort PID,
        BD_BUFFER *pBdBuf);
    
};



#endif  // __AVCTP_USER_HPP__


