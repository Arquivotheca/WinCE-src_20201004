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

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>

#include <cxport.h>

#include "FragmentList.hpp"


Fragment **FragmentList::_FindPkt(ushort ChannelId, uint Transaction) {

    Fragment *pFrag, **ppFrag;

    ppFrag = &m_pFragments;

    while (pFrag = *ppFrag) {
        if ((ChannelId == pFrag->ChannelId) && 
            (Transaction == pFrag->Transaction))
            break;
        
        ppFrag = &pFrag->pNext;
    }

    return ppFrag;
    
}


void FragmentList::DeletePkt(Fragment **ppFrag) {

    Fragment *pFrag = *ppFrag;

    // take it out of the list
    *ppFrag = pFrag->pNext;

    if (pFrag->pData)
        delete [] (pFrag->pData);
    
    delete (pFrag);
}


int FragmentList::AddNewPkt(ushort ChannelId, AvctpCommonPacketHdr Hdr, 
    uchar cFrags, ushort ProfileId, BD_BUFFER *pBuf, int Mtu) {

    int         Ret = ERROR_INVALID_PARAMETER;
    Fragment    *pFrag, **ppFrag;

    if ((cFrags > 1) && (Mtu > 0)) {

        ppFrag = _FindPkt(ChannelId, Hdr.Transaction);
        if (pFrag = *ppFrag) {
            DeletePkt(ppFrag);
        }

        int cDataSize = cFrags * Mtu;

        if (cDataSize <= 128000) {

            uchar   *pData = new uchar[cDataSize];
            if (pData)
                pFrag = new Fragment;
            
            if (pFrag) {
                pFrag->ChannelId = ChannelId;
                pFrag->ProfileId = ProfileId;
                pFrag->Transaction = Hdr.Transaction;
                pFrag->cTotalFrags = cFrags;
                pFrag->cFrags = 1;
                pFrag->cMaxSize = cDataSize;
                pFrag->pData = pData;

                int cSize = BufferTotal(pBuf);

                if (BufferGetChunk(pBuf, cSize, pFrag->pData)) {
                    pFrag->cRcvd = cSize;
                    Ret = ERROR_SUCCESS;

                    pFrag->pNext = m_pFragments;
                    m_pFragments = pFrag;
                } else {
                    delete [] pData;
                    delete pFrag;
                }
            } else {
                if (pData)
                    delete [] pData;
                
                Ret = ERROR_OUTOFMEMORY;
            }
        }

    }

    return Ret;

}


int FragmentList::AddContinuePkt(ushort ChannelId, AvctpCommonPacketHdr Hdr, 
    BD_BUFFER *pBuf, uchar **ppData, uint *pcData, ushort *pProfileId) {

    int         Ret = ERROR_INTERNAL_ERROR;
    Fragment    *pFrag, **ppFrag = _FindPkt(ChannelId, Hdr.Transaction);


    if (pFrag = *ppFrag) {

        int cSize = BufferTotal(pBuf);
        int fDeletePkt = TRUE;

        if ((cSize + pFrag->cRcvd > cSize) &&   // check overflow
            (cSize + pFrag->cRcvd < pFrag->cMaxSize) &&
            (pFrag->cFrags < pFrag->cTotalFrags)) {

            if (BufferGetChunk(pBuf, cSize, pFrag->pData + pFrag->cRcvd)) {
                pFrag->cRcvd += cSize;
                pFrag->cFrags++;

                if (3 == Hdr.PacketType) {
                    if (pFrag->cFrags == pFrag->cTotalFrags) {
                        *ppData = pFrag->pData;

                        // make sure data portion not deleted!
                        pFrag->pData = NULL;
                        *pcData = pFrag->cRcvd;
                        *pProfileId = pFrag->ProfileId;

                        Ret = ERROR_SUCCESS;
                    }
                
                } else if (2 == Hdr.PacketType) {
                    fDeletePkt = FALSE;
                    Ret = ERROR_SUCCESS;

                } else {
                    ASSERT(0);
                }

            }

        }


        // only a type 2 (cont) packet with no errors will not call delete pkt
        if (fDeletePkt)
            DeletePkt(ppFrag);

    } else {
        Ret = ERROR_SERVICE_NOT_FOUND;
    }

    return Ret;

}



int FragmentList::CleanOld() {

    return 0;
}


