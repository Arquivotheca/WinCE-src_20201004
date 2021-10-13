//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef NETBIOS_TRANSPORT_H
#define NETBIOS_TRANSPORT_H

#include <netbios.h>

#include "SMB_globals.h"
#include "cracker.h"


/******************************************************************************/
/*  PUBLIC FUNCTIONS (used by anyone)                                         */
/******************************************************************************/

//starts the NETBIOS transport layer
HRESULT StartNetbiosTransport();

//stops the NETBIOS transport layer
HRESULT StopNetbiosTransport();

HRESULT QueueNBPacketForSend(SMB_PACKET *pPacket, BOOL fDestruct); 

DWORD NETbiosThunk(DWORD x1, DWORD dwOpCode, PVOID pNCB, DWORD cBuf1,
      PBYTE pBuf1, DWORD cBuf2, PDWORD pBuf2);

class ncb;

/******************************************************************************/
/*  PRIVATE FUNCTIONS (used by NetbiosTransport.cpp)                          */
/*       NOT to be used by ANYONE else!!  just here for readability of .cpp   */
/******************************************************************************/
namespace NETBIOS_TRANSPORT {
    extern LONG g_lNumInterfaces;
    extern ce::list<SMB_PACKET *>        g_PacketsToSend;
    extern LONG g_fIsRunning;
    extern LONG g_fIsAccepting;    
    BYTE Netbios(ncb * pncb);

    //map error code to string
    LPTSTR NCBError(UCHAR ncb_err);

class NetBIOSAdapter 
    {
        public:
            NetBIOSAdapter(BYTE bIDX);
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
                return bIDX;
            }
        private: 
            BYTE nameNum;
            BYTE bIDX;    
            HANDLE hListenThread;
            BYTE bRegisteredCName[16];
            LONG fStopped;
    };


    DWORD SMBSRVR_NetbiosListenThread(LPVOID netnum);
    DWORD SMBSRVR_NetbiosSendThread(LPVOID netnum);
    DWORD SMBSRVR_NetbiosRecvThread(LPVOID netnum);
    DWORD SMBSRVR_NameNotifyThread(LPVOID unused);
}


#endif
