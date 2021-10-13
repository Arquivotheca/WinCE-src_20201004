//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include <ndispwr.h>

HANDLE g_hEventStartStress = NULL;
HANDLE g_hEventStopStress = NULL;
DWORD WINAPI StressSendRecv(LPVOID pIgnored);
DWORD WINAPI StressOIDs(LPVOID pIgnored);
BOOL g_bPMAware = FALSE;

#define		_1_MiliSec  (1)
#define		_1_Sec		(1000 * _1_MiliSec)
#define		_1_Min		(60 * _1_Sec)

BOOL IsMiniportPowerManagabled(HANDLE hAdapter);
DWORD SetMiniportPower(CEDEVICE_POWER_STATE Dx);

//------------------------------------------------------------------------------

TEST_FUNCTION(TestStressSusRes)
{
   TEST_ENTRY;
   
   HRESULT hr = S_OK;
   DWORD rc = TPR_PASS;
   HANDLE hThOIDs = NULL; HANDLE hThSendRecv = NULL;
   DWORD dwThreadId1,dwThreadId2;
   HANDLE ahAdapter = NULL;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   BOOL bForce30 = FALSE;

   // If we ever controlled the power of device then in the end of test we have to let
   // Power manager to know that we are done with controlling & now on you handle it.
   BOOL bControlledPower = FALSE;

   NDTLogMsg(
      _T("Start 1c_StressSusRes (Stress in Suspend/Resume) test on the adapter %s"), g_szTestAdapter
   );

   g_hEventStartStress = CreateEvent(NULL, TRUE, FALSE, NULL);
   g_hEventStopStress = CreateEvent(NULL, TRUE, FALSE, NULL);

   if (  (g_hEventStartStress == NULL) || (g_hEventStopStress == NULL)  ) 
	   return TPR_FAIL;

   do 
   {
	   hThOIDs = CreateThread(NULL,								// lpThreadAttributes : must be NULL
		   0,								// dwStackSize : ignored
		   StressOIDs,						// lpStartAddress : pointer to function
		   0,								// lpParameter : pointer passed to thread
		   0,								// dwCreationFlags 
		   &dwThreadId1					// lpThreadId 
		   );
	   
	   if (!hThOIDs) break;
	   
	   hThSendRecv = CreateThread(NULL,							// lpThreadAttributes : must be NULL
		   0,							// dwStackSize : ignored
		   StressSendRecv,				// lpStartAddress : pointer to function
		   0,							// lpParameter : pointer passed to thread
		   0,							// dwCreationFlags 
		   &dwThreadId2				// lpThreadId 
		   );
	   
	   if (!hThSendRecv) break;

	   NDTLogVbs(_T("Thread StressOIDs : %d"),hThOIDs);
	   NDTLogVbs(_T("Thread StressSendRecv : %d"),hThSendRecv);

	   //
	   NDTLogMsg(_T("Opening adapter"));
	   hr = NDTOpen(g_szTestAdapter, &ahAdapter);
	   if (FAILED(hr)) {
		   ahAdapter = NULL;
		   NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
		   break;
	   }
	   
	   // Get some information about a media
	   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
	   
	   NDTLogMsg(_T("Binding to it"));
	   // Bind
	   hr = NDTBind(ahAdapter, bForce30, ndisMedium);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailBind, hr);
		   break;
	   }

	   //
	   // Find out if the miniport driver supports Power Management
	   //
	   g_bPMAware = IsMiniportPowerManagabled(ahAdapter);

	   // Let child threads run & initialize
	   Sleep(50);
	   SetEvent(g_hEventStartStress);
	   NDTLogVbs(_T("Stress threads signaled to Start"));
	   // Let child threads start stress
	   Sleep(50);
	   
	   DWORD dwWait = 1000;
	   // 2 suspend/resume the system for say 10 times.
	   for (INT i=0;i<20;++i)
	   {
		   //This is just to make wait time variable.
		   switch (i)
		   {
		   case 5: 
			   dwWait = 2000;
			   break;
		   case 10: 
			   dwWait = 4000;
			   break;
		   case 15: 
			   dwWait = 5000;
			   break;
		   }

		   if (g_bPMAware)
		   {
			   NDTLogMsg(_T("Transitioning the adapter to D4 state %d time"),i+1);
			   DWORD dw = SetMiniportPower(D4);
			   if (dw != ERROR_SUCCESS)
			   {
				   NDTLogErr(_T("Failed to Set Power D4 to %s adapter. Error=%d"), g_szTestAdapter, dw);
				   rc = TPR_FAIL;
			   }
			   else
				   bControlledPower = TRUE;

			   Sleep(1000);
			   NDTLogMsg(_T("Transitioning back to D0 state %d time"),i+1);

			   dw = SetMiniportPower(D0);
			   if (dw != ERROR_SUCCESS)
			   {
				   NDTLogErr(_T("Failed to Set Power D0 to %s adapter. Error=%d"), g_szTestAdapter, dw);
				   rc = TPR_FAIL;
			   }
			   else
				   bControlledPower = TRUE;
		   }
		   else
		   {
			   NDTLogMsg(_T("Resetting the adapter %d time"),i+1);

			   hr = NDTReset(ahAdapter);
			   if (FAILED(hr)) {
				   rc = TPR_FAIL;
				   NDTLogErr(_T("Failed to Reset %s adapter. Error=0x%x"), g_szTestAdapter, NDIS_FROM_HRESULT(hr));
			   }
		   }

		   NDTLogMsg(_T("Waiting %d mili sec"), dwWait);
		   Sleep(dwWait);
	   }
	   
	   //Stop the stress test.
	   SetEvent(g_hEventStopStress);
	   NDTLogVbs(_T("Stress threads signaled to Stop"));
	   
   } while (0);

   //If we have ever set the power of device then let PM know that we don't want to control
   //it any more.
   if (bControlledPower)
   {
	   DWORD dw = SetMiniportPower(PwrDeviceUnspecified);
	   if (dw != ERROR_SUCCESS)
	   {
		   NDTLogWrn(_T("Failed to Set Power 'PwrDeviceUnspecified' to %s adapter. Error=%d"), g_szTestAdapter, dw);
	   }
   }

   // If we have come here as a result of error then Set events for Start & Stop
   SetEvent(g_hEventStartStress);
   SetEvent(g_hEventStopStress);

   DWORD rc_OIDs = TPR_FAIL; DWORD rc_SendRecv = TPR_FAIL;
   NDTLogMsg(_T("Waiting for Stress OIDs thread to terminate"));
   
   //Wait for threads to terminate
   // Get the exit code from the threads to decide if the test has passed or not.
   if (WAIT_OBJECT_0 == WaitForSingleObject(hThOIDs, _1_Min))
   {
	   GetExitCodeThread(hThOIDs, &rc_OIDs);
	   NDTLogMsg(_T("Stress OIDs thread exited with exit value : %s"), 
		   (rc_OIDs == TPR_PASS)? _T("PASS"):_T("FAIL"));
   }
   else
   {
	   TerminateThread(hThOIDs,rc_OIDs);
	   NDTLogMsg(_T("Stress OIDs thread terminated...."));
   }

   NDTLogMsg(_T("Waiting for StressSendRecv thread to terminate"));
   //Wait for threads to terminate
   if (WAIT_OBJECT_0 == WaitForSingleObject(hThSendRecv, _1_Min))
   {
	   GetExitCodeThread(hThSendRecv, &rc_SendRecv);
	   NDTLogMsg(_T("Stress SendRecv thread exited with exit value : %s"), 
		   (rc_SendRecv == TPR_PASS)? _T("PASS"):_T("FAIL"));
   }
   else
   {
	   TerminateThread(hThSendRecv,rc_SendRecv);
	   NDTLogMsg(_T("Stress SendRecv thread terminated...."));
   }

   // Set the rc accordingly.
   if ( (rc_SendRecv == TPR_FAIL) || (rc_OIDs == TPR_FAIL) || (rc == TPR_FAIL) )
   {
	   NDTLogErr(_T("1c_StressSusRes Failed"));
	   rc = TPR_FAIL;
   }
   else
	   rc = TPR_PASS;

   //Clean up
   if (ahAdapter)
   {
	   NDTLogMsg(_T("Unbinding to it"));
	   // Unbind
	   hr = NDTUnbind(ahAdapter);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailUnbind, hr);
	   }
	   
	   NDTLogMsg(_T("Closing adapter"));
	   hr = NDTClose(&ahAdapter);
	   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   }

   return rc;
}

BOOL IsMiniportPowerManagabled(HANDLE hAdapter)
{
	HRESULT hr = S_OK;
	BOOL fRet = FALSE;

	NDTLogMsg(_T("Checking if miniport is PM aware."));
	
	NDIS_PNP_CAPABILITIES sPNP = {{0}};
	DWORD dwSize=sizeof(sPNP);
	UINT cbUsed = 0; UINT cbRequired = 0;
	
	NDTLogVbs(_T("Querying PNP Capabilities"));
	hr = NDTQueryInfo(
		hAdapter, OID_PNP_CAPABILITIES, PBYTE(&sPNP), dwSize,
		&cbUsed, &cbRequired
		);
	
	if (FAILED(hr)) {
		NDTLogMsg(_T("%s does not support Power Management, Status: 0x%x"), g_szTestAdapter, NDIS_FROM_HRESULT(hr));
	}
	else
	{
		fRet = TRUE;
		if (sPNP.Flags)
		{
			NDTLogMsg(_T("%s supports Power Management and")
				_T(" one or more wake-up capabilities"), g_szTestAdapter);
		}
		else
			NDTLogMsg(_T("%s supports only Power Management, but")
			_T(" does not have wake-up capabilities"), g_szTestAdapter);
	}

    return fRet;
}

// returns ERROR_SUCCESS when succeeds.
DWORD SetMiniportPower(CEDEVICE_POWER_STATE Dx)
{
	//CEDEVICE_POWER_STATE Dx = PwrDeviceUnspecified;
	TCHAR                szName[MAX_PATH];        
	int                  nChars;        
	PTCHAR pszAdapter = g_szTestAdapter;
	DWORD dwRet = ERROR_WRITE_FAULT;

	nChars = _sntprintf(
		szName, 
		MAX_PATH-1, 
		_T("%s\\%s"), 
		PMCLASS_NDIS_MINIPORT, 
		pszAdapter);
	
	szName[MAX_PATH-1]=0;
	
	if(nChars != (-1)) 
	{
		dwRet = SetDevicePower(szName, POWER_NAME, Dx);
	}

	return dwRet;
}


DWORD WINAPI StressSendRecv(LPVOID pIgnored)
{
   DWORD rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;

   UINT   nBindings = 1;
   HANDLE ahAdapter = NULL;
   HANDLE ahSend = NULL;
   UINT   ixBinding = 0;

   ULONG aulPacketsSent = 0;
   ULONG aulPacketsCompleted = 0;
   ULONG aulPacketsCanceled = 0; 
   ULONG aulPacketsUncanceled = 0;
   ULONG aulPacketsReplied = 0;
   ULONG aulTime = 0;
   ULONG aulBytesSend = 0;
   ULONG aulBytesRecv = 0;

   UINT  cbAddr = 0;
   UINT  cbHeader = 0;

   BYTE* pucRandomAddr = NULL;

   ULONG nDestAddrs = 0;
   BYTE* apucDestAddr[8];

   UINT  uiMaximumFrameSize = 0;
   UINT  uiMaximumTotalSize = 0;
   UINT  uiMinimumPacketSize = 64;
   UINT  uiMaximumPacketSize = 0;

   UINT  uiPacketSize = 64;
   UINT  uiPacketsToSend = 1000;

   BOOL  bFixedSize = FALSE;
   UINT  ui = 0;

   NDTLogVbs(_T("StressSendRecv: thread started"));

   do 
   {
	   // Zero structures
	   memset(apucDestAddr, 0, sizeof(apucDestAddr));
	   
	   hr = NDTOpen(g_szTestAdapter, &ahAdapter);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
		   break;
	   }
	   
	   // Get some information about a media
	   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

	   NDTLogVbs(_T("StressSendRecv: NDT Opened: cbAddr:%d cbHeader:%d"),cbAddr,cbHeader);
	   
	   // Bind
	   hr = NDTBind(ahAdapter, bForce30, ndisMedium);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailBind, hr);
		   break;
	   }
	   
	   // Get maximum frame size
	   hr = NDTGetMaximumFrameSize(ahAdapter, &uiMaximumFrameSize);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailGetMaximumFrameSize, hr);
		   break;
	   }
	   
	   // Get maximum total size
	   hr = NDTGetMaximumTotalSize(ahAdapter, &uiMaximumTotalSize);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailGetMaximumTotalSize, hr);
		   break;
	   }
	   
	   if (uiMaximumTotalSize - uiMaximumFrameSize != cbHeader) {
		   NDTLogErr(
			   _T("Difference between values above should be %d"), cbHeader
			   );
		   break;
	   }
	   
	   NDTLogVbs(_T("StressSendRecv: NDT bound MaximumFrameSize:%d MaximumTotalSize:%d"),uiMaximumFrameSize,uiMaximumTotalSize);
	   uiMaximumPacketSize = uiMaximumFrameSize;
	   
	   // Generate address used later
	   pucRandomAddr = NDTGetRandomAddr(ndisMedium);
	   apucDestAddr[0] = pucRandomAddr;
	   
	   NDTLogVbs(_T("StressSendRecv: waiting for stress to start"));

	   //Wait for 10 sec till StartStress is flaged.
	   if (WAIT_OBJECT_0 != WaitForSingleObject(g_hEventStartStress, 10000))
		   break;
	   
	   NDTLogMsg(_T("StressSendRecv: stress started"));
	   uiPacketSize = uiMinimumPacketSize;

	   do
	   {
		   HRESULT HrStress = S_OK;

		   DWORD dwRet = WaitForSingleObject(g_hEventStopStress, 1);
		   if ((WAIT_OBJECT_0==dwRet) || (dwRet == WAIT_FAILED))
		   {
			   NDTLogMsg(_T("StressSendRecv: signaled to stop"));
			   break;
		   }
		   
		   NDTLogMsg(_T("StressSendRecv: Sending %d packets each of %d size to a random address"),uiPacketsToSend,uiPacketSize);
		   
		   // Send on first instance
		   HrStress = NDTSend(
			   ahAdapter, cbAddr, NULL, 1, apucDestAddr, NDT_RESPONSE_NONE, 
			   NDT_PACKET_TYPE_FIXED, uiPacketSize, uiPacketsToSend, 0, 0, 
			   &ahSend
			   );
		   
		   if (FAILED(HrStress)) {
			   hr = HrStress;
			   NDTLogErr(g_szFailSend, hr);
			   break;
		   }
		   
		   // Wait to finish on first
		   HrStress = NDTSendWait(
			   ahAdapter, ahSend, INFINITE, &aulPacketsSent,
			   &aulPacketsCompleted, &aulPacketsCanceled, 
			   &aulPacketsUncanceled, &aulPacketsReplied, &aulTime, 
			   &aulBytesSend, &aulBytesRecv
			   );
		   
		   if (FAILED(HrStress)) {
			   hr = HrStress;
			   NDTLogErr(g_szFailSendWait, hr);
			   break;
		   }
		   
		   NDTLogMsg(_T("StressSendRecv: %d packets each of %d size were SendCompleted"),aulPacketsCompleted,uiPacketSize);
		   
		   // Update the packet size
		   uiPacketSize += 64;
		   
		   if (uiPacketSize > uiMaximumPacketSize)
			   uiPacketSize = uiMinimumPacketSize;
		   else
			   --uiPacketSize; //This just helps to have odd/even packet size alternately.
	   } while (1);
   } while (0);
      
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

   NDTLogMsg(_T("StressSendRecv: Cleaned up & exiting."));
   return rc;
}

DWORD WINAPI StressOIDs(LPVOID pIgnored)
{
   DWORD rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   UINT uiPhysicalMedium = 0;
   HANDLE hAdapter = NULL;
   HANDLE ahSend = NULL;
   UCHAR * pacBuffer = NULL;
   UINT  cbAddr = 0;
   UINT  cbHeader = 0;
   UINT cbUsed = 0;
   UINT cbRequired = 0;

   NDTLogVbs(_T("StressOIDs: thread started"));
   do
   {
	   hr = NDTOpen(g_szTestAdapter, &hAdapter);
	   
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
		   break;
	   }
	   
	   // Get some information about a media
	   NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

	   NDTLogVbs(_T("StressOIDs: NDT Opened: cbAddr:%d cbHeader:%d"),cbAddr,cbHeader);
	   
	   // Bind
	   hr = NDTBind(hAdapter, bForce30, ndisMedium, &ndisMedium);
	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailBind, hr);
		   break;		
	   }

	   hr = NDTGetPhysicalMedium(hAdapter, &uiPhysicalMedium);

	   NDTLogVbs(_T("StressOIDs: Get Supported OIDs List"));
	   
	   NDIS_OID aSupportedOids[256];
	   NDIS_OID aRequiredOids[256];
	   INT cRequiredOids = 0;

	   hr = NDTQueryInfo(
		   hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, sizeof(aSupportedOids),
		   &cbUsed, &cbRequired
		   );

	   if (FAILED(hr)) {
		   NDTLogErr(g_szFailQueryInfo, hr);
		   break;
	   }

	   INT cbSupported = cbUsed/sizeof(NDIS_OID);
	   NDTLogMsg(_T("StressOIDs: %s supports ** %d ** OIDs"),g_szTestAdapter,cbSupported);

	   aRequiredOids[cRequiredOids++] = OID_GEN_SUPPORTED_LIST;
	   aRequiredOids[cRequiredOids++] = OID_GEN_HARDWARE_STATUS;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_SUPPORTED;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_IN_USE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_LOOKAHEAD;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_FRAME_SIZE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_LINK_SPEED;
	   aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BUFFER_SPACE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BUFFER_SPACE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BLOCK_SIZE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BLOCK_SIZE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_ID;
	   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DESCRIPTION;
	   aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_PACKET_FILTER;
	   aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_LOOKAHEAD;
	   aRequiredOids[cRequiredOids++] = OID_GEN_DRIVER_VERSION;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_TOTAL_SIZE;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MAC_OPTIONS;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_CONNECT_STATUS;
	   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_SEND_PACKETS;
	   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DRIVER_VERSION;
	   
	   aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_OK;
	   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_OK;
	   aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_ERROR;
	   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_ERROR;
	   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_NO_BUFFER;
	   
	   aRequiredOids[cRequiredOids++] = OID_802_3_PERMANENT_ADDRESS;
	   aRequiredOids[cRequiredOids++] = OID_802_3_CURRENT_ADDRESS;
	   aRequiredOids[cRequiredOids++] = OID_802_3_MULTICAST_LIST;
	   aRequiredOids[cRequiredOids++] = OID_802_3_MAXIMUM_LIST_SIZE;
	   aRequiredOids[cRequiredOids++] = OID_802_3_RCV_ERROR_ALIGNMENT;
	   aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_ONE_COLLISION;
	   aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_MORE_COLLISIONS;

	   DWORD cbacBuffSize = 8192;
	   pacBuffer = (UCHAR *) new UCHAR[cbacBuffSize];
	   if (!pacBuffer)
	   {
		   hr = E_OUTOFMEMORY;
		   break;
	   }
	   
	   NDTLogVbs(_T("StressOIDs: waiting for stress to start"));
	   //Wait for 10 sec till StartStress is flaged.
	   if (WAIT_OBJECT_0 != WaitForSingleObject(g_hEventStartStress, 10000))
		   break;

	   NDTLogMsg(_T("StressOIDs: stress started"));

	   INT ix=0;
	   do
	   {
		   HRESULT HrStress = S_OK;
		   if ((ix % 10) == 0)
		   {
			   //Check for Stopstress event periodically after n numbers of Queries/Sets
			   DWORD dwRet = WaitForSingleObject(g_hEventStopStress, 1);
			   if ((WAIT_OBJECT_0==dwRet) || (dwRet == WAIT_FAILED)) 
			   {
				   NDTLogMsg(_T("StressOIDs: signaled to stop"));
				   break;
			   }
		   }
		   
		   HrStress = NDTQueryInfo(hAdapter, aRequiredOids[ix], pacBuffer, cbacBuffSize,
			   &cbUsed,&cbRequired);
		   
		   //Since we don't care to give right values while Querying OIDs we don't care the output
		   if (FAILED(HrStress) && HrStress != NDT_STATUS_BUFFER_TOO_SHORT) {
			   NDIS_STATUS status = NDIS_FROM_HRESULT(HrStress);
		   }
		   
		   ++ix;
		   if (ix >= cRequiredOids)
			   ix =0;

	   }while (1);

   } while (0);

   // We have deside about test pass/fail there
   rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

   if (pacBuffer)
	   delete [] pacBuffer;

   hr = NDTUnbind(hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailUnbind, hr);

   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);
   
   NDTLogMsg(_T("StressOIDs: Cleaned up & exiting."));
   return rc;
}

//------------------------------------------------------------------------------
