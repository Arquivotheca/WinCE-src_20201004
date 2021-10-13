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
#include <tchar.h>
#include "utils.h"
#include "log.h"
#include "ndp_lib.h"
#include "ndp_protocol.h"
#include "globals.h"

DWORD g_acceptTimeout = 0xFFFFFFFF;
extern HANDLE g_hAdapter;
extern BOOL g_fUseNdisSendOnly;
HANDLE g_hFinishEvent = NULL;
//------------------------------------------------------------------------------

#ifndef UNDER_CE

// To modify the Ctrl+c behaviour for windows XP
BOOL CtrlHandler(DWORD fdwCtrlType) 
{ 
       /*
    When a CTRL+C signal is received, the control handler returns TRUE, indicating 
    that it has handled the signal. Doing this prevents other control handlers from being called.

    When a CTRL_CLOSE_EVENT signal is received, the control handler returns TRUE, 
    causing the system to display a dialog box that gives the user the choice of terminating 
    the process and closing the console or allowing the process to continue execution. 
    If the user chooses not to terminate the process, the system closes the console when 
    the process finally terminates.

    When a CTRL+BREAK, CTRL_LOGOFF_EVENT, or CTRL_SHUTDOWN_EVENT signal is received, 
    the control handler returns FALSE. Doing this causes the signal to be passed to the 
    next control handler function. If no other control handlers have been registered or 
    none of the registered handlers returns TRUE, the default handler will be used, 
    resulting in the process being terminated. 
    */

    switch (fdwCtrlType) 
    { 
        // Handle the CTRL+C signal. 
 
        case CTRL_C_EVENT: 

            Beep(1000, 1000); 
            LogMsg(_T("Closing the perf_ndis instance and exiting."));
            SetEvent(g_hFinishEvent);
            return TRUE; 
 
        // CTRL+CLOSE: confirm that the user wants to exit. 
 
        case CTRL_CLOSE_EVENT: 
 
            return TRUE; 
 
        // Pass other signals to the next handler. 
 
        case CTRL_BREAK_EVENT: 
 
        case CTRL_LOGOFF_EVENT: 
 
        case CTRL_SHUTDOWN_EVENT: 
 
        default: 
 
            return FALSE; 
    } 
} 

#endif

BOOL AcceptPeer(HANDLE hAdapter, DWORD timeout, UCHAR peerAddr[])
{
   BOOL ok = FALSE;
   DWORD packetType = 0;
   UCHAR srcAddr[ETH_ADDR_SIZE];
   DWORD startTime;
   BOOL bListened = FALSE;

   startTime = GetTickCount();
   
   do {

      LogVbs(_T("Wait for broadcast 'LOOKFOR' packet"));

      // Start listen for broadcast 'LOOKFOR'
      if (!Listen(hAdapter, 8, FALSE, TRUE)) {
          LogErr(_T("Failed start listen for broadcast packets"));
          Listen(hAdapter, 0, FALSE, FALSE);
          goto cleanUp;
      }

      bListened = TRUE;

      // Wait for request
      do {
         packetType = 0;


         BOOL bWaitLoop = TRUE;
         while(bWaitLoop)
         {
             if ( (GetTickCount() - startTime) > timeout ) 
                 bWaitLoop = FALSE;

             if ( ReceivePacket(hAdapter, NDP_TIMEOUT_SHORT, 
                 peerAddr, &packetType, NULL, NULL) )
                 bWaitLoop = FALSE;

             //Check if somebody wants us to leave
             if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
             { 
                 //Somebody wants us to leave.
                 LogMsg(_T("user wants us to leave.OK."));
                 goto cleanUp;
             }
         }

         // We or get a packet or there is timeout
         if (packetType == 0) {
            goto cleanUp;
         }
         if (packetType != NDP_PACKET_LOOKFOR) {
            LogMsg(_T("Get unexpected packet type when wait for request"));
         }
      } while (packetType != NDP_PACKET_LOOKFOR);

      // Stop listen
      if (!Listen(hAdapter, 0, FALSE, FALSE)) {
         LogErr(_T("Failed stop listen for broadcast packets"));
         goto cleanUp;
      }

      bListened = FALSE;

      LogVbs(_T("Received 'LOOKFOR' packet"));

      // Start listen for directed packets
      if (!Listen(hAdapter, 8, TRUE, FALSE)) {
         LogErr(_T("Failed start listen for direct packets"));
         goto cleanUp;
      }

      bListened = TRUE;

      // Send offer
      if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_OFFER, NULL, 0)) {
         LogErr(_T("Failed send offer packet"));
         goto cleanUp;
      }

      LogVbs(_T("Sent 'OFFER' packet"));
      LogVbs(_T("Wait for 'ACCEPT' packet"));

      // Was our offer accepted?
      do {
         if (ReceivePacket(hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL)) {
            break;//got a packet
         }else{
            LogMsg(_T("Daemon: Timeout when wait for 'ACCEPT' packet"));
         }
        
        //Check if somebody wants us to leave
         if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
         { 
             //Somebody wants us to leave.
             LogMsg(_T("user wants us to leave.OK."));
             goto cleanUp;
         }
      } while (
         packetType != NDP_PACKET_ACCEPT ||
         memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
      );

      // Stop listen
      if (!Listen(hAdapter, 0, FALSE, FALSE)) {
         LogErr(_T("Failed stop listen for direct packets"));
         goto cleanUp;
      }

      bListened = FALSE;

      // We get there with different packet type on timeout
      if (packetType != NDP_PACKET_ACCEPT) break;

   }while (FALSE);

   LogVbs(_T("Received 'ACCEPT' packet"));

   ok = TRUE;

cleanUp:
   if (bListened)
   {
       bListened = FALSE;
       // Stop listen
       if (!Listen(hAdapter, 0, FALSE, FALSE)) {
           LogErr(_T("Failed stop listen for direct packets"));
       }
   }
   return ok;
}

//------------------------------------------------------------------------------

BOOL ReceiveIterationWithPeer(
   HANDLE hAdapter, UCHAR peerAddr[], DWORD *pPacketType
) {
   BOOL ok = FALSE;
   UCHAR srcAddr[ETH_ADDR_SIZE];
   DWORD size, packetType;
   NDP_STRESS_RESULT result;
   tsNDPParams sNDPParams;
   BOOL bListened = FALSE;
   DWORD dwSyncSteps = 0;

   // Just for case
   *pPacketType = NDP_PACKET_NONE;

   LogVbs(_T("Daemon: Start Listening"));

   // Start listen for directed packets
   if (!Listen(hAdapter, 8, TRUE, FALSE)) {
      LogErr(_T("Daemon: Failed start listen for direct packets"));
      goto cleanUp;
   }

   bListened = TRUE;

   // Send 'OK' packet
   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_OK, NULL, 0)) {
      LogErr(_T("Daemon: Failed send 'OK' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Daemon: Sent 'OK' packet"));
   LogVbs(_T("Daemon: Wait for 'PARAMS' packet"));

   // Wait for test parameters
   do {
      size = sizeof(sNDPParams);
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, (UCHAR*)&sNDPParams, 
         &size
      )) {
         LogMsg(_T("Daemon: Timeout when wait for 'PARAMS' packet"));
      }
      LogVbs(_T("Daemon: 'PARAMS' get %d"), packetType);
      
    //Check if somebody wants us to leave
     if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
     { 
         //Somebody wants us to leave.
         LogMsg(_T("user wants us to leave.OK."));
         goto cleanUp;
     }
   } while (
      packetType != NDP_PACKET_PARAMS ||
      memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
   );

   LogVbs(_T("Daemon: Received 'PARAMS' packet. PoolSize=%d, TestType=%d, PacketSize=%d, Packets =%d"),
                    sNDPParams.dwPoolSize, sNDPParams.dwNDPTestType, sNDPParams.dwPacketSize, sNDPParams.dwNosOfPacketsToSend);
   
   // Send 'OK' packet
   if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_OK, NULL, 0)) {
      LogErr(_T("Daemon: Failed send 'OK' packet"));
      goto cleanUp;
   }

   LogVbs(_T("Daemon: Sent 'OK' packet"));

   switch(sNDPParams.dwNDPTestType)
   {
   case SEND_THROUGHPUT:

       if (!Listen(hAdapter, 0, FALSE, FALSE)) {
           LogErr(_T("Daemon: Failed stop listen for direct packets"));
           goto cleanUp;
       }

       bListened = FALSE;

       // Send 'READY' packet
       if (!SendPacket(hAdapter, peerAddr, NDP_PACKET_READY, NULL, 0)) {
           LogErr(_T("Daemon: Failed send 'OK' packet"));
           goto cleanUp;
       }
       
       LogVbs(_T("Daemon: Sent 'READY' packet"));

       // Start stress receive
       if (!StressReceive(
           hAdapter, sNDPParams.dwPoolSize, NDP_PACKET_STRESS, peerAddr, &packetType, NULL,
           NULL, &result.time, &result.idleTime, &result.packetsReceived, &result.bytesReceived
           )) {
           LogErr(_T("Daemon: Failed start stress receive"));
           goto cleanUp;
       }
       
       LogVbs(_T("Daemon: Done with 'STRESS' packets"));
       
       // Start listen for results
       if (!Listen(hAdapter, 8, TRUE, FALSE)) {
           LogErr(_T("Daemon: Failed start listen for direct packets"));
           goto cleanUp;
       }

       bListened = TRUE;

       // Send DONE packet
       if (!SendPacket(g_hAdapter, peerAddr, NDP_PACKET_DONE, NULL, 0)) {
           LogErr(_T("Daemon: Failed send 'DONE' packet"));
           goto cleanUp;
       }

   break;

   case RECV_THROUGHPUT:

       //Wait for ready packet.
       do {
           if (!ReceivePacket(
               hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
               )) {
               LogErr(_T("Daemon: Timeout waiting for 'READY' packet"));
           }
        
            //Check if somebody wants us to leave
            if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
            { 
                //Somebody wants us to leave.
                LogMsg(_T("user wants us to leave.OK."));
                goto cleanUp;
            }
       } while (
           packetType != NDP_PACKET_READY || 
           memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
           );
       
       if (!Listen(hAdapter, 0, FALSE, FALSE)) {
           LogErr(_T("Daemon: Failed stop listen for direct packets"));
           goto cleanUp;
       }

       bListened = FALSE;
       
       LogVbs(_T("Daemon: Got 'READY' packet"));

       Sleep(NDP_TIMEOUT_DELAY);
       LogVbs(_T("Daemon: Start send 'STRESS' packets"));

       // Send packets   
       if (!StressSend(
           hAdapter, g_fUseNdisSendOnly, peerAddr, NDP_PACKET_STRESS, sNDPParams.dwPoolSize, sNDPParams.dwPacketSize, 
           sNDPParams.dwNosOfPacketsToSend, sNDPParams.dwFlagBurstControl,
           sNDPParams.dwDelayInABurst, sNDPParams.dwPacketsInABurst,
           &result.time, &result.idleTime, &result.packetsReceived, &result.bytesReceived
           )) {
           LogErr(_T("Daemon: Stress send failed"));
           goto cleanUp;
       }
       
       LogVbs(_T("Daemon: Sent 'STRESS' packets"));
       
       // Wait a while before we end stress iteration
       Sleep(NDP_TIMEOUT_SHORT);
       
       // Start listen for results
       if (!Listen(hAdapter, 8, TRUE, FALSE)) {
           LogErr(_T("Daemon: Failed start listen for direct packets"));
           goto cleanUp;
       }

       bListened = TRUE;

#define MAX_REPEAT_STEPS_FOR_PEER 4

       //Wait for Done packet from peer.
       do {
           // Send DONE packet
           if (!SendPacket(g_hAdapter, peerAddr, NDP_PACKET_DONE, NULL, 0)) {
               LogErr(_T("Daemon: Failed send 'DONE' packet"));
               goto cleanUp;
           }
           
           LogVbs(_T("Daemon: Sent 'DONE' packet % time"),++dwSyncSteps);

           if (!ReceivePacket(
               hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
               )) {
               if (dwSyncSteps >= MAX_REPEAT_STEPS_FOR_PEER)
               {
                   LogErr(_T("Daemon: Timeout waiting for 'READY' packet"));
                   goto cleanUp;
               }
               LogMsg(_T("Daemon: Waiting for 'READY' packet %d more time"),dwSyncSteps);
           }
        
            //Check if somebody wants us to leave
            if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
            { 
                //Somebody wants us to leave.
                LogMsg(_T("user wants us to leave.OK."));
                goto cleanUp;
            }
       } while (
           packetType != NDP_PACKET_DONE || 
           memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
           );

       // Wait a while before we end stress iteration
       //Sleep(NDP_TIMEOUT_DELAY);
       break;
   case PING_THROUGHPUT:
       break;
   }

   //Wait For GET ME RESULT Packet from Test.
   do {
       if (!ReceivePacket(
           hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
           )) {
           LogMsg(_T("Daemon: Timeout when wait for 'RESULT' packet"));
       }
    
     //Check if somebody wants us to leave
      if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
      { 
          //Somebody wants us to leave.
          LogMsg(_T("user wants us to leave.OK."));
          goto cleanUp;
      }
   } while (
       (packetType != NDP_PACKET_RESULT)||
       memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
       );

   
   if (!SendPacket(
       hAdapter, peerAddr, NDP_PACKET_RESULT, (UCHAR*)&result, sizeof(result)
       )) {
       LogErr(_T("Daemon: Failed send 'RESULT' packet"));
       goto cleanUp;
   }
   
   LogVbs(_T("Daemon: Sent 'RESULT' packet"));

   LogVbs(_T("Daemon: Wait for 'DONE/NEXT' packet"));

   do {
      if (!ReceivePacket(
         hAdapter, NDP_TIMEOUT_SHORT, srcAddr, &packetType, NULL, NULL
      )) {
         LogMsg(_T("Daemon: Timeout when wait for 'DONE/NEXT' packet"));
      }
    
     //Check if somebody wants us to leave
      if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
      { 
          //Somebody wants us to leave.
          LogMsg(_T("user wants us to leave.OK."));
          goto cleanUp;
      }
   } while (
      (packetType != NDP_PACKET_NEXT && packetType != NDP_PACKET_EXIT)||
      memcmp(peerAddr, srcAddr, ETH_ADDR_SIZE) != 0
   );

   LogVbs(_T("Daemon: Received 'DONE/NEXT' packets"));

   *pPacketType = packetType;
   ok = TRUE;

cleanUp:
   if (bListened)
   {
       bListened = FALSE;
       // Stop listen
       if (!Listen(hAdapter, 0, FALSE, FALSE)) {
           LogErr(_T("Daemon: Failed stop listen for offer packets"));
       }
   }
   return ok;
}

//------------------------------------------------------------------------------

int ndisd(void)
{
   int rc = TPR_FAIL;
   UCHAR peerAddr[ETH_ADDR_SIZE];
   DWORD packetType;
   DWORD dwConnections = 0;
 
   g_hFinishEvent = CreateEvent(NULL, FALSE, FALSE, _T("PERF_NDIS"));
   if (g_hFinishEvent == NULL) 
   {
       g_hFinishEvent = NULL;
       LogErr(_T("ndisd:: Failed in creating the event. Error = %d"),GetLastError());
       return FALSE;
   }

   // If event already exists then set it and mark the exit
   if (GetLastError() == ERROR_ALREADY_EXISTS) 
   {
       LogMsg(_T("First instance of Perf_ndis is found"));
       LogMsg(_T("Stopping the first instance and exiting this current instance"));
       SetEvent(g_hFinishEvent);
       CloseHandle(g_hFinishEvent);
       return FALSE;
   }

#ifndef UNDER_CE
   BOOL fSuccess = SetConsoleCtrlHandler(
        (PHANDLER_ROUTINE) CtrlHandler,  // handler function 
        TRUE);                           // add to list 

   if (!fSuccess)
        LogErr(_T("Could not set control handler"));
#endif

   do
   {
       LogMsg(_T("Waiting for new client connection...Start running perf_ndis test on CE device..."));

       // Accept connection from peer
       if (!AcceptPeer(g_hAdapter, g_acceptTimeout, peerAddr)) goto cleanUp;
       else
           LogMsg(_T("Accepted client: %d connection"), ++dwConnections);
       
       // Do iteration while we get next packets at end
       do {
           if (!ReceiveIterationWithPeer(g_hAdapter, peerAddr, &packetType)) {
               goto cleanUp;
           }
           
           //Check if somebody wants us to leave
           if (WAIT_OBJECT_0 == WaitForSingleObject(g_hFinishEvent,5))
           { 
               //Somebody wants us to leave.
               LogMsg(_T("user wants us to leave.OK."));
               goto cleanUp;
           }
       } while (packetType == NDP_PACKET_NEXT);

   } while (TRUE);
   
   rc = TPR_PASS;
cleanUp:
   return rc;
}

//------------------------------------------------------------------------------

