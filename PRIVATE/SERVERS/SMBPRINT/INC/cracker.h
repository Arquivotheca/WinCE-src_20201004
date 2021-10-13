//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef CRACKER_H
#define CRACKER_H

#include <list.hxx>

#include "SMB_Globals.h"
#include "FixedAlloc.h"

#define CRACKER_PACKETS_ALLOC          pool_allocator<10, SMB_PACKET *>


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


#define SMB_NORMAL_PACKET       1
#define SMB_CONNECTION_DROPPED  2


struct SMB_PACKET
{

//
// In debug do logging of creation time for perf monitoring
#ifdef DEBUG
    SMB_PACKET() {
        dwStartTime = GetTickCount();
    }  
    ~SMB_PACKET() {
        TRACEMSG(ZONE_STATS, (L"SMB(0x%3x -- %10s) packet %d -- took %d to process!", SMB, GetCMDName(SMB), uiPacketNumber, TimeProcessing()));
        TRACEMSG(ZONE_STATS, (L"--------------------------------------------------"));
    }
    DWORD PerfStopTimer(CHAR _SMB) {
       SMB = _SMB; 
       return (dwStopTime = GetTickCount());
    }
    DWORD TimeProcessing() {
       return dwStopTime - dwStartTime;
    }

    private:
        CHAR SMB;
        DWORD dwStopTime;
        DWORD dwStartTime;
    public: //must be last to get everything under me 
        UINT uiPacketNumber;
#endif

    //
    //  These variables are used to get a packet back to 
    //    the transport once its processed by the cracker
    //    (this way, the Cracker doesnt have to know 
    //    about the transport AT ALL)
    QueuePacketToBeSent pfnQueueFunction;
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


HRESULT StartCracker();
HRESULT StopCracker();


// Add a packet to the Cracker Queue -- this function simply enters
//  a critical section and adds your packet to the list to be cracked (to be called by
//  a transport)
HRESULT CrackPacket(SMB_PACKET *pNewPacket); 

namespace Cracker{

    //main cracking thread -- this thread simply loops waiting for packets to 
    //  come in from transport
    DWORD SMBSRVR_CrackerThread(LPVOID netnum);
    DWORD SMBSRVR_CrackerDelayThread(LPVOID netnum);

    // Linked list of packets to crack, critical section to govern its use,
    //  and semaphore to represent its length all used by CrackerThread
    extern ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC>        g_PacketsToCrack;
    extern ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC>        g_PacketsToDelayCrack;
    extern CRITICAL_SECTION              g_csCrackLock;
    extern HANDLE                        g_ToCrackQueueSem;
    extern HANDLE                        g_hCrackDelaySem;
    extern HANDLE                        g_hStopEvent;
    extern HANDLE                        g_hCrackerThread;
    extern HANDLE                        g_hCrackerDelayThread;
    extern const UINT                    g_uiMaxPacketsInQueue;
    extern BOOL                          g_fIsRunning;  

    //
    // HACKHACK:  I dont like having a global variable for this, but it keeps 
    //    code cleaner except in a few special places
    //
    // g_dwDelayPacket represents the amount of time a packet will be delayed
    //    set this and before a packet is sent out to the client it will be stalled for that number of ms
    extern DWORD						 g_dwDelayPacket; 

#ifdef DEBUG    
    extern CRITICAL_SECTION              g_csPerfLock;
    extern double                        g_dblPerfAvePacketTime[0xFF];
    extern DWORD                         g_dwPerfPacketsProcessed[0xFF];
    extern BOOL                          g_fAlreadyStarted;
#endif

}


#endif
