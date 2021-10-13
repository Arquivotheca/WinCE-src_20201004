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
#ifndef NETBIOS_TRANSPORT_H
#define NETBIOS_TRANSPORT_H

#include <netbios.h>
#include <winsock2.h>

#include "SMB_globals.h"
#include "PktHandler.h"


/******************************************************************************/
/*  PUBLIC FUNCTIONS (used by anyone)                                         */
/******************************************************************************/

//starts the NETBIOS transport layer
HRESULT StartNetbiosTransport();

//stops the NETBIOS transport layer
HRESULT StopNetbiosTransport();

HRESULT InitNetbiosTransport();
HRESULT DestroyNetbiosTransport();


HRESULT NB_GetSocketName(SMB_PACKET *pPacket, struct sockaddr *pSockAddr, int *pNameLen);
HRESULT NB_TerminateSession(ULONG ulConnectionID);



DWORD NETbiosThunk(DWORD x1, DWORD dwOpCode, PVOID pNCB, DWORD cBuf1,
      PBYTE pBuf1, DWORD cBuf2, PDWORD pBuf2);

class ncb;



/******************************************************************************/
/*  PRIVATE FUNCTIONS (used by NetbiosTransport.cpp)                          */
/*       NOT to be used by ANYONE else!!  just here for readability of .cpp   */
/******************************************************************************/
namespace NETBIOS_TRANSPORT {
    class NetBIOSAdapter;
    
    struct RecvNode {
       NetBIOSAdapter *pAdapter;
       HANDLE MyHandle;
       USHORT usLSN;
       CHAR LANA;
       ce::list<ULONG> OutStandingConnectionIDs;
    };
    
    
    #define DELETE_LANA_FL               0x01
    #define NETBIOS_DEV_NAME             L"NBT1:"

#ifndef NO_POOL_ALLOC
    #define NETBIOS_CONNECTION_ALLOC     ce::singleton_allocator< ce::fixed_block_allocator<5>, sizeof(RecvNode) >
#else
    #define NETBIOS_CONNECTION_ALLOC     ce::allocator
#endif
    
    namespace AddressChangeNotification {
        extern USHORT usID;
        extern SOCKET s;
        extern WSAOVERLAPPED ov;
    }

    namespace NameChangeNotification {
        extern HANDLE h;   
        extern USHORT usID;
    }
    
    
    extern HANDLE g_hHaltNetbiosTransport;

    extern LONG g_lNumInterfaces;
    extern ce::list<SMB_PACKET *>        g_PacketsToSend;
    extern LONG g_fIsRunning;
    extern LONG g_fIsAccepting;  
    extern LONG g_fIsInited;

    extern ce::list<NetBIOSAdapter *> NBAdapterDeleteStack;
    extern ce::list<NetBIOSAdapter *> NBAdapterStack;
    extern ce::list<RecvNode, NETBIOS_CONNECTION_ALLOC > ActiveRecvList;

    extern ce::fixed_block_allocator<10>          g_NCBAllocator;

    extern CRITICAL_SECTION csAdapterStackList;
    extern CRITICAL_SECTION csSendLock;
    extern CRITICAL_SECTION csActiveRecvListLock;
    extern CRITICAL_SECTION csNCBLock;

    BYTE Netbios(ncb * pncb);

    //map error code to string
    LPTSTR NCBError(UCHAR ncb_err);

    class NetBIOSAdapter 
    {
        public:
            NetBIOSAdapter(BYTE bLana);
            ~NetBIOSAdapter();        
            
            HRESULT HaltAdapter();
            BOOL DuringShutDown();
                      
            HRESULT SetNameNum(BYTE _nameNum) {
                    nameNum = _nameNum; 
                    return S_OK;
            }
            
            HRESULT GetNameNum(BYTE *_nameNum) {
                    if(0xFF == nameNum) {
                        ASSERT(FALSE);
                        return E_UNEXPECTED;
                    }
                    *_nameNum = nameNum; 
                    return S_OK;
            }            
           
            HRESULT SetCName(BYTE _CName[16]) {
                memcpy(bRegisteredCName, _CName, 16);
                return S_OK;
            }
           
            HRESULT GetCName(BYTE **_CName) {
                if(NULL == _CName)
                    return E_INVALIDARG;
                *_CName = bRegisteredCName;
                return S_OK;
            }   

            BYTE GetLANA() {
                return bLana;
            }
            
            UINT GetNTE() {
                return dwNTE;
            }
            VOID SetNTE(DWORD _dwNTE) {
                dwNTE = _dwNTE;
            }

            //
            // Marking is used to mark/sweep to know what adapters have come up/down
            VOID SetMark(BOOL fOn) {
                bMark = fOn;
            }
            BOOL GetMark() {
                return bMark;
            }
        private: 
            BYTE nameNum;
            BYTE bLana;    
            HANDLE hListenThread;
            BYTE bRegisteredCName[16];
            DWORD dwNTE;
            LONG fStopped;            
            BOOL bMark;
    };


    DWORD SMBSRVR_NetbiosListenThread(LPVOID netnum);
    DWORD SMBSRVR_NetbiosSendThread(LPVOID netnum);
    DWORD SMBSRVR_NetbiosRecvThread(LPVOID netnum);
    VOID SMBSRVR_HostNameChanged(USHORT usID, VOID *pToken);
    HRESULT QueueNBPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct); 
    HRESULT CopyNBTransportToken(VOID *pToken, VOID **pNewToken);
    HRESULT DeleteNBTransportToken(VOID *pToken);
}

#endif
