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
#include "avctp.hpp"
#include <bt_debug.h>

#define DEBUG_LAYER_TRACE                       0x00000040

AvctpUser *VerifyOwner(HANDLE hContext)
{
    AvctpUser * pUser = (AvctpUser*) btutil_TranslateHandle((SVSHandle)hContext);
    if (! pUser) {
        IFDBG(DebugOut(DEBUG_LAYER_TRACE, L"VerifyOwner : ERROR_NOT_FOUND\n"));
        return NULL;
    }

#if 0
    //
    // Ensure the an owner is still in the list that has this context
    //
    BOOL fFound = FALSE;
    for (ContextList::iterator it = gpAVCTP->listContexts.begin(), itEnd = gpAVCTP->listContexts.end(); it != itEnd; ++it) {
        if ((*it)->hContext == hContext) {
            fFound = TRUE;
            break;
        }
    }
    SVSUTIL_ASSERT(fFound);
#endif // DEBUG

    return pUser;
}


int GetUserFromContext(HANDLE hDeviceContext, AvctpUser **ppUser) {
    int Ret;

    GetAvctpLock();
    if (! gpAVCTP)
        Ret = ERROR_SERVICE_DOES_NOT_EXIST;
    else {
        *ppUser = VerifyOwner(hDeviceContext);
        if (*ppUser)
            Ret = ERROR_SUCCESS;
        else
            Ret = ERROR_INVALID_HANDLE;
    }

    return Ret;

}


int AVCT_ConnectReqIn (HANDLE hDeviceContext, BD_ADDR *pAddr, ushort ProfileId, 
    LocalSideConfig *pLocalConfig) {

    AvctpUser *pUser;

    int Ret = GetUserFromContext(hDeviceContext, &pUser);

    if (ERROR_SUCCESS == Ret) {
        Ret = pUser->ConnectReqIn(pAddr, ProfileId, pLocalConfig);
    } else {
        FreeAvctpLock();
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-AVCTP: AVCT_ConnectReqIn: ret %X\n", Ret));

    return Ret;
}

int AVCT_ConnectRspIn(HANDLE hDeviceContext, BD_ADDR *pAddr, 
    ushort ConnectResult, ushort Status, LocalSideConfig *pLocalConfig) {

    AvctpUser *pUser;

    int Ret = GetUserFromContext(hDeviceContext, &pUser);

    if (ERROR_SUCCESS == Ret) {
        Ret = pUser->ConnectRspIn(pAddr, ConnectResult, Status, pLocalConfig);
    } else {
        FreeAvctpLock();
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-AVCTP: AVCT_ConnectRspIn: ret %X\n", Ret));

    return Ret;
}

int AVCT_DisconnectReqIn(HANDLE hDeviceContext, BD_ADDR *pAddr, 
    ushort ProfileId) {

    AvctpUser *pUser;

    int Ret = GetUserFromContext(hDeviceContext, &pUser);

    if (ERROR_SUCCESS == Ret) {
        Ret = pUser->DisconnectReqIn(pAddr, ProfileId);
    } else {
        FreeAvctpLock();
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-AVCTP: AVCT_DisconnectReqIn: ret %X\n", Ret));

    return Ret;

}


int AVCT_SendMessageIn(HANDLE hDeviceContext, BD_ADDR *pAddr, 
	uchar Transaction, uchar Type, ushort ProfileId, BD_BUFFER *pBdSendBuf) {

    AvctpUser *pUser;

    int Ret = GetUserFromContext(hDeviceContext, &pUser);

    if (ERROR_SUCCESS == Ret) {
        Ret = pUser->SendMsgIn(pAddr, Transaction, Type, ProfileId, 
            pBdSendBuf);
    } else {
        FreeAvctpLock();
    }

    IFDBG(DebugOut (DEBUG_LAYER_TRACE, 
        L"-AVCTP: AVCT_DisconnectReqIn: ret %X\n", Ret));

    return Ret;

}

int AVCT_EstablishDeviceContext
(
    uint                    Version,            // IN
    ushort                  ProfileID,          // IN
    void*                   pUserContext,       /* IN */
    PAVCT_EVENT_INDICATION  pInd,               /* IN */
    PAVCT_CALLBACKS         pCall,              /* IN */
    PAVCT_INTERFACE         pInt,               /* OUT */
    int*                    pcDataHeaders,      /* OUT */
    int*                    pcDataTrailers,     /* OUT */
    HANDLE*                 phDeviceContext)    /* OUT */
{
    int Ret;

    IFDBG(DebugOut (DEBUG_AVDTP_INIT, L"++AVCTP_EstablishDeviceContext\n"));

    GetAvctpLock();
    if (! gpAVCTP) {
        Ret = ERROR_SERVICE_DOES_NOT_EXIST;
        
/*    } else if (! gpAVCTP->m_fIsRunning) {     // let him register
        Ret = ERROR_SERVICE_NOT_ACTIVE;     */
    } else {

        Ret = ERROR_NOT_ENOUGH_MEMORY;
        AvctpUser *pUser = new AvctpUser;
        if (pUser) {
            
            pUser->m_hContext = (HANDLE) btutil_AllocHandle(pUser);
            if (SVSUTIL_HANDLE_INVALID == pUser->m_hContext) {
                delete pUser;
                
            } else {
                *pcDataHeaders = gpAVCTP->m_cHeaders + AvctpFragStartHdrSize;
                *pcDataTrailers = gpAVCTP->m_cTrailers;
                *phDeviceContext = pUser->m_hContext;

                pInt->AvctConnectReqIn = AVCT_ConnectReqIn;
                pInt->AvctConnectRspIn = AVCT_ConnectRspIn;
                pInt->AvctDisconnectReqIn = AVCT_DisconnectReqIn;
                pInt->AvctSendMessageIn = AVCT_SendMessageIn;

                //pInt->AvctIoctl = AVCT_Ioctl;
                //pInt->AvctAbortCall = AVCT_AbortCall;

                pUser->m_IndicateFns = *pInd;
                pUser->m_CallbackFns = *pCall;
                pUser->m_pUserContext = pUserContext;
                pUser->m_pUserChannels = NULL;
                pUser->m_ProfileId = ProfileID;

                pUser->m_pNext = gpAVCTP->m_pUsers;
                gpAVCTP->m_pUsers = pUser;

                Ret = ERROR_SUCCESS;
            }
        }
    }

    FreeAvctpLock();

    IFDBG(DebugOut (DEBUG_AVDTP_INIT, L"--AVCTP_EstablishDeviceContext:: Ret %d\n", Ret));
    
    return Ret;
}


int AVCT_CloseDeviceContext(HANDLE hDeviceContext) {
    int Ret;

    IFDBG(DebugOut (DEBUG_AVDTP_INIT, L"++AVCT_CloseDeviceContext\n"));

    AvctpUser **ppUser, *pUser;
    Ret = GetUserFromContext(hDeviceContext, &pUser);
    if (ERROR_SUCCESS == Ret) {
        ppUser = &gpAVCTP->m_pUsers;

        while (*ppUser && (*ppUser != pUser)) {
            ppUser = & (*ppUser)->m_pNext;
        }

        if (*ppUser == pUser) {
            *ppUser = pUser->m_pNext;

            delete pUser;
        } else {
            Ret = ERROR_NOT_FOUND;
            ASSERT(0);
        }
    }
        

    FreeAvctpLock();

    return Ret;

}

