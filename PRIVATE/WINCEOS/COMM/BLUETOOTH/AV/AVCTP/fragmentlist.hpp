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

#ifndef __FRAGMENTLIST_HPP__
#define __FRAGMENTLIST_HPP__

#include <bt_avctp.h>
#include "avctpuser.hpp"


typedef struct Fragment {
    struct Fragment *pNext;
    ushort          ChannelId;
    ushort          ProfileId;
    uint            Transaction;
    uint            cTotalFrags;
    uint            cFrags;     // recv'ed so far
    int             cMaxSize;
    int             cRcvd;
    uchar           *pData;
} Fragment;




class FragmentList {
public:
    FragmentList() {m_pFragments = NULL;}

    int AddNewPkt(ushort ChannelId, AvctpCommonPacketHdr Hdr, uchar cFrags, 
        ushort ProfileId, BD_BUFFER *pBuf, int Mtu);
    int AddContinuePkt(ushort ChannelId, AvctpCommonPacketHdr Hdr, 
        BD_BUFFER *pBuf, uchar **ppData, uint *pcData, ushort *pProfileId);
    int CleanOld();

private:
    Fragment **FragmentList::_FindPkt(ushort ChannelId, uint Transaction);
    void FragmentList::DeletePkt(Fragment **ppFrag);


    
    Fragment    *m_pFragments;
};

#endif  //__FRAGMENTLIST_HPP__

