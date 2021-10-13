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
#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"
#include <Pkfuncs.h>
#include <Pm.h>
#include <nkintr.h>
#include <ndis.h>

extern BOOL  g_bWirelessMiniport;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestSusRes)
{
   TEST_ENTRY;
   
   HRESULT hr = S_OK;
   DWORD rc = ERROR_NOT_SUPPORTED;
   ULONG ulConversationId = 0;
   DWORD dwAlarmResolution = 0;
   HANDLE ahAdapter = NULL;
   BYTE* pucRandomAddr = NULL;

   NDTLogMsg(
      _T("Start 1c_SusRes (Suspend/Resume) test on the adapter %s"), g_szTestAdapter
   );

   if (rand_s((unsigned int*)&ulConversationId) != 0)   {
      hr = E_FAIL;
      NDTLogErr(g_szFailGetConversationId, 0);
      goto cleanUp;
   }

   tePlatType ePlatform;
   if ((GetPlatformType(&ePlatform)) && (ePlatform==NDT_SP))
   {
       //It is Smartphone, so we can NOT run this test.
       NDTLogMsg(
           _T("%s is on Smartphone, so skipping this test."), g_szTestAdapter
           );
       return TPR_SKIP;
   }

   hr = NDTOpen(g_szTestAdapter, &ahAdapter);
   if (FAILED(hr)) {
       NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
       goto cleanUp;
   }

   NDTLogMsg(_T("Enabling the Wake up source : SYSINTR_RTC_ALARM"));

   hr = NDTHalSetupWake(ahAdapter, NULL, FLAG_NDT_ENABLE_WAKE_UP, 0);
   if (FAILED(hr)) {
       NDTLogWrn(_T("Enabling WAKE UP using IOCTL_HAL_ENABLE_WAKE failed."));
   }

   hr = NDTClose(&ahAdapter);

   dwAlarmResolution = NDTGetWakeUpResolution();
   
   // 2 suspend/resume the system for say 10 times.
   for (INT i=0;i<5;++i)
   {
       // Now setup RTC timer to wake up after next 30 sec.
       HANDLE hNotify, hWaitEvent;
       NDTLogMsg(_T("Setting alarm to %d sec"), ((dwAlarmResolution/1000)*2));
       hNotify = NDTEnableWakeUp(_T("MyNDTEvent"), ((dwAlarmResolution/1000)*2), &hWaitEvent);

       NDTLogMsg(_T("Suspending the system %d time"),i+1);
       rc = SetSystemPowerState(NULL,POWER_STATE_SUSPEND, 0);
       rc = SetSystemPowerState(NULL, POWER_STATE_ON, 0);
       NDTLogMsg(_T("Resumed ....."));
       NDTLogMsg(_T("Sleeping %d sec ....."), (i+1)*5);
       Sleep((i+1) * 5000);

       NDTDisableWakeUp(hNotify, hWaitEvent);
   }

   // 3 Now test if the given miniport adapter is working fine by sending data through it.
   NDTLogMsg(_T("Testing if the adapter %s is fine after suspend/resume"), g_szTestAdapter);

   //This is required for wireless card to cath up with its association.
   if (g_bWirelessMiniport)
   {
       NDTLogMsg(_T("Since %s is Wireless-LAN, Sleeping for 2 min to let it catch up with its Association"), g_szTestAdapter);
       Sleep(2000 * 60);
   }

   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   HANDLE ahSend = NULL;
   
   ULONG aulPacketsSent = 0;
   ULONG aulPacketsCompleted = 0;
   ULONG aulPacketsCanceled = 0; 
   ULONG aulPacketsUncanceled = 0;
   ULONG aulPacketsReplied = 0;
#ifndef NDT6   
   ULONG aulTime = 0;
#else
   ULONGLONG aulTime = 0;
#endif
   ULONG aulBytesSend = 0;
   ULONG aulBytesRecv = 0;

   UINT  cbAddr = 0;
   UINT  cbHeader = 0;

   BYTE* apucDestAddr[8];

   UINT  uiMaximumFrameSize = 0;
   UINT  uiMaximumTotalSize = 0;
   UINT  uiMinimumPacketSize = 64;
   UINT  uiMaximumPacketSize = 0;

   UINT  uiPacketSize = 64;
   UINT  uiPacketsToSend = 100;

   BOOL  bFixedSize = FALSE;
   
   // Zero structures
   memset(apucDestAddr, 0, sizeof(apucDestAddr));

   hr = NDTOpen(g_szTestAdapter, &ahAdapter);
   if (FAILED(hr)) {
       NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
       goto cleanUp;
   }

   if (FAILED(hr)) goto cleanUp;

   NDTLogMsg(_T("Getting basic info about the adapter"));

   // Get some information about a media
   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

   BOOL fAdapterIsNotUp;
   DWORD dwCount = 0;
   // Bind
   do
   {
       hr = NDTBind(ahAdapter, bForce30, ndisMedium);
       if (FAILED(hr)) {
           HRESULT hrErr = NDIS_FROM_HRESULT(hr);
           if ( (hrErr == NDIS_STATUS_ADAPTER_NOT_FOUND) && (dwCount < 2) )
           {
               //This indicates (in the context of this thread) that
               //the adapter is not up yet, as adapter may get unloaded & reloaded.
               fAdapterIsNotUp = TRUE;
               NDTLogWrn(_T("%s adapter is not up yet, waiting 30 sec"), g_szTestAdapter);
               Sleep(30000);
               ++dwCount;
           }
           else
           {
               fAdapterIsNotUp = FALSE;
               NDTLogErr(g_szFailBind, hrErr);
               goto cleanUp;
           }
       }
       else
           fAdapterIsNotUp = FALSE;
   } while (fAdapterIsNotUp);

   // Get maximum frame size
   hr = NDTGetMaximumFrameSize(ahAdapter, &uiMaximumFrameSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumFrameSize, hr);
      goto cleanUp;
   }

   // Get maximum total size
   hr = NDTGetMaximumTotalSize(ahAdapter, &uiMaximumTotalSize);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailGetMaximumTotalSize, hr);
      goto cleanUp;
   }
   
   NDTLogMsg(_T("Get OID_GEN_MAXIMUM_TOTAL_SIZE = %d"), uiMaximumTotalSize);
   NDTLogMsg(_T("Get OID_GEN_MAXIMUM_FRAME_SIZE = %d"), uiMaximumFrameSize);
   
   if (uiMaximumTotalSize - uiMaximumFrameSize != cbHeader) {
      hr = E_FAIL;
      NDTLogErr(
         _T("Difference between values above should be %d"), cbHeader
      );
      goto cleanUp;
   }

   uiMaximumPacketSize = uiMaximumFrameSize;
   
   // Generate address used later
   pucRandomAddr = NDTGetRandomAddr(ndisMedium);

   NDTLogMsg(_T("Testing sending to a (random) directed address"));
   

   apucDestAddr[0] = pucRandomAddr;
   
   uiPacketSize = uiMinimumPacketSize;
   bFixedSize = FALSE;

   while (uiPacketSize <= uiMaximumPacketSize) {
       
       NDTLogMsg(_T("Sending %d packets each of %d size to a random address"),uiPacketsToSend,uiPacketSize);

       // Send on first instance
       hr = NDTSend(
           ahAdapter, cbAddr, NULL, 1, apucDestAddr, NDT_RESPONSE_NONE, 
           NDT_PACKET_TYPE_FIXED, uiPacketSize, uiPacketsToSend, ulConversationId, 0, 0, 
           &ahSend
           );
       if (FAILED(hr)) {
           NDTLogErr(g_szFailSend, hr);
           goto cleanUp;
       }
       
       // Wait to finish on first
       hr = NDTSendWait(
           ahAdapter, ahSend, INFINITE, &aulPacketsSent,
           &aulPacketsCompleted, &aulPacketsCanceled, 
           &aulPacketsUncanceled, &aulPacketsReplied, &aulTime, 
           &aulBytesSend, &aulBytesRecv
           );

       if (FAILED(hr)) {
           NDTLogErr(g_szFailSendWait, hr);
           goto cleanUp;
       }

       //60% packets should be sent
       if (  aulPacketsCompleted < ULONG((uiPacketsToSend * 60)/100)  )
       {
           hr = NDT_STATUS_FAILURE;
           NDTLogErr(_T("%d packets were SendCompleted instead of %d"),aulPacketsCompleted, uiPacketsToSend);
           break;
       }
       else
           NDTLogMsg(_T("%d packets were SendCompleted"),aulPacketsCompleted);

       // Update the packet size
       if (bFixedSize) {
           uiPacketSize--;
           bFixedSize = FALSE;
       }

       uiPacketSize += (uiMaximumPacketSize - uiMinimumPacketSize)/2;
       if (uiPacketSize < uiMaximumPacketSize && (uiPacketSize & 0x1) == 0) {
           bFixedSize = TRUE;
           uiPacketSize++;
       }
   }
      

cleanUp:
   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   // Unbind
   hr = NDTUnbind(ahAdapter);
   if (FAILED(hr)) {
       NDTLogErr(g_szFailUnbind, hr);
   }

   hr = NDTClose(&ahAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

   delete pucRandomAddr;

   return rc;
}
