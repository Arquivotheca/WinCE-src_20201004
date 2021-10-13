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

#ifndef __AVRCP_HPP__
#define __AVRCP_HPP__

#include <bt_avctp.h>
#include <bt_avrcp.h>

const uint DefaultCompanyId = 0xffffff;

const uchar PanelType = 9;

enum AVCTypes {
    Control_t = 0x0,
    Status_t,
    SpecInquiry_t,
    Notify_t,
    GenInquiry_t,

    // Responses
    NotImpl_t = 0x8,
    Accepted_t,
    Rejected_t,
    InTransition_t,
    Stable_t,
    Changed_t,

    Interim_t = 0xf
};


typedef struct _OperationId {
    uchar   OpIdPress   : 1;
    uchar   OpId        : 7;
} OperationId;


const int cMinAvrcpPkt = 3;
const int cUnitInfoData = 5;
const int cMinSubunitInfoData = 5;

typedef struct _AvrcpPkt {
    uchar   Ctype       : 4;
    uchar   Blank       : 4;

    uchar   SubunitId   : 3;
    uchar   SubunitType : 5;

    uchar   OpCode;
    uchar   aOpd[1];
} AvrcpPkt;


class AvrcpConn {
    friend class Avrcp;
    
    AvrcpConn   *m_pNext;
    BD_ADDR     m_BdAddr;


};

const ushort AvrcpPID = 0x0e11;     // Avrcp's ProfileId

class Avrcp {

    AvrcpConn       *m_pConnections;
    HANDLE          m_hAvctp;
    int             m_cHeaders;
    int             m_cTrailers;


    HANDLE          m_hMsgQueue;


    AVCT_INTERFACE  m_AvctIntf;     // interface fn's to AVCTP

    AvrcpConn       **_FindConnection(BD_ADDR *pAddr);
    AvrcpConn       *FindConnection(BD_ADDR *pAddr);
    int             InsertConnection(BD_ADDR *pAddr, AvrcpConn **ppConn);


    int CreateAvrcpMsgQ();
    int SendMsg(uint Cmd);

    int SendToWMP(uint Cmd);

    int HandleUnitReq(AvrcpPkt *pPkt, ushort cData, BD_BUFFER **ppRspBuf,
        ushort *pcRspBuf);

    int HandlePassThru(AvrcpPkt *pPkt, ushort cData, BD_BUFFER **ppRspBuf,
        ushort *pcRspBuf);

public:
    Avrcp();
    ~Avrcp();

    int AVCT_ConnectInd(BD_ADDR *pAddr);
    int AVCT_DisconnectInd(BD_ADDR *pAddr);
    int AVCT_MsgRecInd(BD_ADDR *pAddr, 
	    uchar Transaction, uchar Type, uchar *pData, ushort cData);
    
};

#endif  // __AVRCP_HPP__

