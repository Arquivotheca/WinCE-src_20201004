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
#ifndef PktHandler_H
#define PktHandler_H

#include <list.hxx>
#include <block_allocator.hxx>

#include "SMB_Globals.h"

#ifndef NO_POOL_ALLOC
#define PktHandler_PACKETS_ALLOC          ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(SMB_PACKET *) >
#else 
#define PktHandler_PACKETS_ALLOC          ce::allocator
#endif

// 
// Values used in the SMB packet (flag 1 -- its the one byte flag
#define SMB_FLAG1_OBSOLSCENT_REQUEST 0x1
#define SMB_FLAG1_RESERVED           0x2
#define SMB_FLAG1_RESERVED2          0x4
#define SMB_FLAG1_CASELESS           0x8
#define SMB_FLAG1_RESERVED3          0x10
#define SMB_FLAG1_OBSOLESCENT2       0x20
#define SMB_FLAG1_OBSOLESCENT3       0x40
#define SMB_FLAG1_SERVER_TO_REDIR    0x80


struct SMB_PACKET;
typedef HRESULT (*QueuePacketToBeSent) (SMB_PACKET *pPacket, BOOL fDestruct);
typedef HRESULT (*CopyTransportToken) (VOID *pToken, VOID **pNewToken);
typedef HRESULT (*DeleteTransportToken) (VOID *pToken);
typedef HRESULT (*TransGetSocketName)  (SMB_PACKET *pPacket, struct sockaddr *pAddr, int *pNameLen);



//
// Please note on these (you might get confused if you search the code for these values)
//   only a "NORMAL_PACKET" will get sent out the wire.  so if you want to push a packet through
//   the stack w/o actually sending, or if you need to monitor somethat about the packet set this
//   (see LockX when an OpLock break occurs) just set it to something else (like SMB_DISCARD_PACKET)
#define SMB_NORMAL_PACKET       1
#define SMB_CONNECTION_DROPPED  2
#define SMB_CLIENT_IDLE_TIMEOUT 3
#define SMB_DISCARD_PACKET      4


struct SMB_PACKET
{

//
// In debug do logging of creation time for perf monitoring
#ifdef DEBUG
    SMB_PACKET() {
        dwStartTime = GetTickCount()-1000;
        lpszTransport = NULL;		
    }  
    ~SMB_PACKET() {
        TRACEMSG(ZONE_STATS, (L"SMB(0x%3x -- %10s) packet %d -- took %d to process!", SMB, GetCMDName(SMB), uiPacketNumber, TimeProcessing()));
        TRACEMSG(ZONE_STATS, (L"--------------------------------------------------"));
    }
    DWORD PerfStopTimer(CHAR _SMB) {
       SMB = _SMB; 
       return (dwStopTime = GetTickCount());
    }

    VOID PerfPrintStatus(WCHAR *pMsg = L"") {
        DWORD dwProcessing = GetTickCount() - dwStartTime;
        TRACEMSG(ZONE_STATS, (L"   %s Current Status of %d: %d ms", pMsg, uiPacketNumber, dwProcessing));
    }
    VOID PerfStartTimer() {
        dwStartTime = GetTickCount();
    }
    DWORD TimeProcessing() {
       return dwStopTime - dwStartTime;
    }
    DWORD GetProcessingTime() {
	return GetTickCount() - dwStartTime;
    }

    private:
        CHAR SMB;
        DWORD dwStopTime;
        DWORD dwStartTime;
    public: //must be last to get everything under me 
        UINT uiPacketNumber;
	 LPCWSTR lpszTransport;	
#endif

    //
    //  These variables are used to get a packet back to 
    //    the transport once its processed by the PktHandler
    //    (this way, the PktHandler doesnt have to know 
    //    about the transport AT ALL)
    QueuePacketToBeSent pfnQueueFunction;
    CopyTransportToken  pfnCopyTranportToken;
    DeleteTransportToken pfnDeleteTransportToken;
    //TransGetSocketName  pfnGetSocketName;

    void *pToken; 

    //
    //  The connectionID is a # made up by the transport
    //     that ID's the packet
    ULONG ulConnectionID;    
    
    //
    // Pointers to the in and out SMB packets
    BYTE InSMB[SMB_Globals::MAX_PACKET_SIZE];
    SMB_HEADER *pInSMB;
    
    UINT uiInSize;

    BYTE OutSMB[SMB_Globals::MAX_PACKET_SIZE];
    struct SMB_HEADER *pOutSMB;
    UINT uiOutSize;

    //
    //  This ID's the packet TYPE... this
    //    is basically just to ID a regular SMB or a 'special' event -- like the 
    //    connection to the transport went down
    UINT  uiPacketType;

    //
    // Send this packet at this time -- 0 is a special value meaning send it now
    //    use it like this:
    DWORD dwDelayBeforeSending;		 //put the delay here (in ms) -- note this ISNT EXACT!
};


#include <svsutil.hxx>


HRESULT StartPktHandler();
HRESULT StopPktHandler();


// Add a packet to the PktHandler Queue -- this function simply enters
//  a critical section and adds your packet to the list to be PktHandleed (to be called by
//  a transport).
HRESULT SMBHandlePacket(SMB_PACKET *pNewPacket); 

namespace PktHandler{

    //main PktHandling thread -- this thread simply loops waiting for packets to 
    //  come in from transport
    DWORD SMBSRVR_PktHandlerThread(LPVOID netnum);
    DWORD SMBSRVR_PktHandlerDelayThread(LPVOID netnum);
    VOID  LockPktHandler();
    VOID  UnlockPktHandler();

    // Linked list of packets to PktHandle, critical section to govern its use,
    //  and semaphore to represent its length all used by PktHandlerThread
    extern ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC>        g_PacketsToDelayPktHandle;
    extern CRITICAL_SECTION              g_csPktHandleLock;
    extern HANDLE                        g_hPktHandleDelaySem;
    extern HANDLE                        g_hStopEvent;
    extern HANDLE                        g_hPktHandlerDelayThread;
    extern const UINT                    g_uiMaxPacketsInQueue;
    extern BOOL                          g_fIsRunning; 


#ifdef DEBUG    
    extern CRITICAL_SECTION              g_csPerfLock;
    extern double                        g_dblPerfAvePacketTime[0xFF];
    extern DWORD                         g_dwPerfPacketsProcessed[0xFF];
#endif

}


#endif
