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

#ifndef __AVCTP_HPP__
#define __AVCTP_HPP__

#include <bt_avctp.h>

#include "AvctpUser.hpp"
#include "FragmentList.hpp"

void GetAvctpLock();
void FreeAvctpLock();


class Avctp : public SVSSynch, public SVSRefObj {
    friend class AvctpChannel;
public:

    unsigned int    m_fIsRunning;
    unsigned int    m_fConnected;

    HANDLE          m_hL2CAP;
    L2CAP_INTERFACE m_L2CapIntf;    // L2CAP Interface

    int             m_cHeaders;
    int             m_cTrailers;

    AvctpChannel    *m_pChannels;
    AvctpUser       *m_pUsers;

    Avctp ();

    ~Avctp (void) {

        if (m_hL2CAP)
            L2CAP_CloseDeviceContext (m_hL2CAP);

    }


    int L2CA_ConfigInd (uchar id, ushort cid, ushort usOutMTU, 
        ushort usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, 
        struct btCONFIGEXTENSION **pExtendedOptions);
    int L2CA_ConnectInd (BD_ADDR *pRemoteAddr, ushort ChannelId, uchar Id, 
        ushort PSM);
    int L2CA_DataUpInd(unsigned short cid, BD_BUFFER *pBuffer);
    int L2CA_DisconnectInd (unsigned short cid, int iError);
    int L2CA_CallAborted (AvctpChannel *pContext, int iError);
    int L2CA_StackEvent (int iEvent, void *pEventContext);


    int L2CA_ConfigReqOut (AvctpChannel *pChannel, ushort Result, 
        ushort InMTU, ushort OutFlushTO, struct btFLOWSPEC *pOutFlow, 
        int cOptNum, struct btCONFIGEXTENSION **pExtendedOptions);

    int L2CA_ConnectReqOut (AvctpChannel *pChannel, ushort cid, 
        ushort Result, ushort Status);

    int             AuthThread (AvctpChannel *pChannel);
    int             DisconnectChannel(AvctpChannel *pChannel);
    void            DeleteChannel(AvctpChannel *pChannel);
    AvctpChannel    **_FindChannel(AvctpChannel *pChannel);
    AvctpChannel    *FindChannel(AvctpChannel *pChannel);
    AvctpChannel    *FindChannel(BD_ADDR *pAddr);
    AvctpChannel    *FindChannel(ushort ChannelId);
    AvctpUser       *FindUser(ushort ProfileId);


private:
    void            StackUp();
    void            StackDown();
    FragmentList    m_FragList;
    

};

extern Avctp    *gpAVCTP;

#endif  // __AVCTP_HPP__

