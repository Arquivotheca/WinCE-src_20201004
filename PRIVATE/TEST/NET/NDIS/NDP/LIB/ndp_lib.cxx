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
#include <tchar.h>
#include <winioctl.h>
#include <ntddndis.h>
#include "ndp_ioctl.h"
#include "ndp_lib.h"

//------------------------------------------------------------------------------

static HANDLE g_hDriver = NULL;
static LONG g_driverRefCount = 0;

//------------------------------------------------------------------------------

#ifdef UNDER_CE

DWORD NDISIOControl(
   DWORD dwCommand, LPVOID pInBuffer, DWORD cbInBuffer, LPVOID pOutBuffer,
   DWORD *pcbOutBuffer
)
{
   HANDLE hNDIS;
   DWORD rc = ERROR_SUCCESS;
   DWORD cbOutBuffer;

   hNDIS = CreateFile(
      DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL
   );

   if (hNDIS == INVALID_HANDLE_VALUE) {
      rc = GetLastError();
      goto cleanUp;
   }

   cbOutBuffer = 0;
   if (pcbOutBuffer) cbOutBuffer = *pcbOutBuffer;

   if (!DeviceIoControl(
      hNDIS, dwCommand, pInBuffer, cbInBuffer, pOutBuffer, cbOutBuffer,
      &cbOutBuffer, NULL
   )) {
      rc = GetLastError();
   }

   if (pcbOutBuffer) *pcbOutBuffer = cbOutBuffer;
   CloseHandle(hNDIS);
   
cleanUp:
   return rc;
}

//------------------------------------------------------------------------------

DWORD LoadAdapter(LPCTSTR szAdapter,LPCTSTR szDriver)
{
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

  // First driver name
   lstrcpy(pc, szDriver);
   pc += lstrlen(szDriver) + 1;
   cbBuffer += lstrlen(szDriver) + 1;
   // Now full adapter name
   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);

   return NDISIOControl(
      IOCTL_NDIS_REGISTER_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
}

//------------------------------------------------------------------------------

DWORD UnloadAdapter(LPCTSTR szAdapter)
{
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   // Full adapter name   
   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);

   return NDISIOControl(
      IOCTL_NDIS_DEREGISTER_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
}

//------------------------------------------------------------------------------

DWORD QueryAdapters(LPTSTR mszAdapters, DWORD dwSize)
{
   DWORD cbAdapters = dwSize;

   return NDISIOControl(
      IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, (LPVOID)mszAdapters, &cbAdapters
   );
}

//------------------------------------------------------------------------------

DWORD QueryProtocols(LPTSTR mszProtocols, DWORD dwSize)
{
   DWORD cbProtocols = dwSize;

   return NDISIOControl(
      IOCTL_NDIS_GET_PROTOCOL_NAMES, NULL, 0, (LPVOID)mszProtocols, &cbProtocols
   );
}

//------------------------------------------------------------------------------

DWORD QueryBindings(LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize)
{
   DWORD cbProtocols = dwSize;

   return NDISIOControl(
      IOCTL_NDIS_GET_ADAPTER_BINDINGS, (LPVOID)szAdapter, 
      (lstrlen(szAdapter) + 1) * sizeof(TCHAR), (LPVOID)mszProtocols, 
      &cbProtocols
   );
}

//------------------------------------------------------------------------------

DWORD BindProtocol(LPCTSTR szAdapterName, LPCTSTR szProtocol)
{
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   lstrcpy(pc, szAdapterName);
   pc += lstrlen(szAdapterName) + 1;
   cbBuffer += lstrlen(szAdapterName) + 1;
   if (szProtocol != NULL) {
      lstrcpy(pc, szProtocol);
      pc += lstrlen(szProtocol) + 1;
      cbBuffer += lstrlen(szProtocol) + 1;
   }
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);
  
   return NDISIOControl(
      IOCTL_NDIS_BIND_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
}

//------------------------------------------------------------------------------

DWORD UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   if (szProtocol != NULL) {
      lstrcpy(pc, szProtocol);
      pc += lstrlen(szProtocol) + 1;
      cbBuffer += lstrlen(szProtocol) + 1;
   }
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);
  
   return NDISIOControl(
      IOCTL_NDIS_UNBIND_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
}

//------------------------------------------------------------------------------

DWORD ReBindProtocol(LPCTSTR szAdapterName, LPCTSTR szProtocol)
{
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   lstrcpy(pc, szAdapterName);
   pc += lstrlen(szAdapterName) + 1;
   cbBuffer += lstrlen(szAdapterName) + 1;
   if (szProtocol != NULL) {
      lstrcpy(pc, szProtocol);
      pc += lstrlen(szProtocol) + 1;
      cbBuffer += lstrlen(szProtocol) + 1;
   }
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);
  
   return NDISIOControl(
      IOCTL_NDIS_REBIND_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
}

#include <nkintr.h>
////////////////////////////////////////////////////////
// Very simplified interfaces to enable or disable wakeup.
// Input:
// dwWakeSrc= Wake up source Eg. SYSINTR_RTC_ALARM
//            If you enter 0 then SYSINTR_RTC_ALARM is assumed.
// dwSeconds= Time in sec for which you want device to be suspended.
// Output:
// HANDLE= This is "CreateFile" handle to NDIS Protocol driver
//         use this same handle while calling ClearDeviceWakeUp.
// This interface basically uses NDIS Protocol driver which runs in kernel mode
// to enable wake up and/or setup RTCTimer, as in Yamazaki KernelIoControl can NOT be 
// called from user mode.
HANDLE SetDeviceWakeUp(DWORD dwWakeSrc, DWORD dwSeconds)
{
	DWORD actualLengthOut = 0;
	tsDevWakeUp sDevWakeUp = {0};
	HANDLE hAdapter = NULL;

	hAdapter = OpenProtocol();

	if (!hAdapter)
		return NULL;

	sDevWakeUp.dwActionFlags = FLAG_NDP_ENABLE_WAKE_UP|FLAG_NDP_ENABLE_RTC_TIMER;
	if (dwWakeSrc)
		sDevWakeUp.dwWakeUpSource = dwWakeSrc;
	else
		sDevWakeUp.dwWakeUpSource = SYSINTR_RTC_ALARM;
	sDevWakeUp.dwRTCAlarmTimeInSec = dwSeconds;

	BOOL flag = DeviceIoControl(
		hAdapter, IOCTL_NDP_DEVICE_WAKE,&sDevWakeUp,sizeof(sDevWakeUp),NULL,0,&actualLengthOut,NULL);

	if (!flag)
	{
		CloseProtocol(hAdapter);
		hAdapter = NULL;
	}
	return hAdapter;
}

//////////////////////////////////////////////////////////////////////////////////
// Input:
// hAdapter= Use the same handle which is returned by SetDeviceWakeUp
// dwWakeSrc= You MUST use the same wakeup source which you used while calling SetDeviceWakeUp
BOOL ClearDeviceWakeUp(HANDLE hAdapter, DWORD dwWakeSrc)
{
	DWORD actualLengthOut = 0;
	tsDevWakeUp sDevWakeUp = {0};

	if (!hAdapter)
		return FALSE;

	sDevWakeUp.dwActionFlags = FLAG_NDP_DISABLE_WAKE_UP|FLAG_NDP_DISABLE_RTC_TIMER;
	if (dwWakeSrc)
		sDevWakeUp.dwWakeUpSource = dwWakeSrc;
	else
		sDevWakeUp.dwWakeUpSource = SYSINTR_RTC_ALARM;
	sDevWakeUp.dwRTCAlarmTimeInSec = 0;

	BOOL flag = DeviceIoControl(
		hAdapter, IOCTL_NDP_DEVICE_WAKE,&sDevWakeUp,sizeof(sDevWakeUp),NULL,0,&actualLengthOut,NULL);

	CloseProtocol(hAdapter);
	return flag;
}

//===============================================================================================
// These interfaces are supposed to be working but for now are NOT working, most likely an issue with
// CeSetUserNotificationEx(). So for now these functions are disabled.
# if 0
	#include <Notify.h>
	#include <utils.h>

	//------------------------------------------------------------------------------
	// Use this function to setup wake up event at specified interval.
	// Input Params
	// szEventName: Name of the event. This function will create this named event for you.
	// dwSeconds: Specify how many seconds later the current time this event should be set.
	// Output Params
	// Returned Value: Function returns handle to this notify process. This is different than
	//                 handle returned by pWaitEvent. Use this handle to clear off this wake up process.
	//                 If this function fails then it returns NULL.
	// pWaitEvent: Handle to named event which one can wait on
	// 
	HANDLE EnableWakeUp(LPCTSTR szEventName, DWORD dwSeconds, HANDLE * pWaitEvent)
	{
		HANDLE     hWaitEvent=NULL;
		HANDLE     hNotify=NULL;
		CE_NOTIFICATION_TRIGGER sTrigger;
		TCHAR szNotifyEventName[255]= _T("\\\\.\\Notifications\\NamedEvents\\");
		SYSTEMTIME curSysTime = {0};
		FILETIME fileTime;

		if ((!szEventName)||(!pWaitEvent))
		{
			RETAILMSG(1, (_T("invalid input params")));
			return hNotify;
		}

		//Ignoring buffer overflow.
		_tcscat(szNotifyEventName, szEventName);

		RETAILMSG(1, (_T("szNotifyEventName=%s"), szNotifyEventName));
		RETAILMSG(1, (_T("Seconds=%d"), dwSeconds));

		sTrigger.dwSize = sizeof(CE_NOTIFICATION_TRIGGER);
		//Time based notification
		sTrigger.dwType = CNT_TIME;     
		sTrigger.dwEvent = 0;  

		// this will be a named event
		sTrigger.lpszApplication = szNotifyEventName;  
		sTrigger.lpszArguments = NULL;

		// set start and end of notification period
		sTrigger.stEndTime = curSysTime;

		GetLocalTime(&curSysTime);
		SystemTimeToFileTime(&curSysTime, &fileTime);
		AddSecondsToFileTime(&fileTime, dwSeconds);
		FileTimeToSystemTime(&fileTime, &curSysTime);

		sTrigger.stStartTime = curSysTime;

		// create a named event
		hWaitEvent = CreateEvent(NULL, FALSE, FALSE, szEventName);

		// if named event was created
		if(hWaitEvent)
		{
			// create user notification
			hNotify = CeSetUserNotificationEx( 0, &sTrigger, NULL );
			if (!hNotify)
			{
				RETAILMSG(1, (_T("CeSetUserNotificationEx failed.")));
				CloseHandle(hNotify);
			}
		}
		else
			RETAILMSG(1, (_T("CreateEvent failed.")));

		*pWaitEvent = hWaitEvent;
		return hNotify;
	}

	//------------------------------------------------------------------------------
	// Use this function to disable wake up event which was setup earlier
	// Input Params
	// hNotify: Handle to "Notify process". OS will do cleanup based on this handle.
	// hWaitEvent: This is wait event on which user app is supposed to wait. This will be closed here.
	// Output Params
	// Returned Value: None.
	// 
	void DisableWakeUp(HANDLE hNotify, HANDLE hWaitEvent)
	{
		if (hNotify)
			CeClearUserNotification(hNotify);

		if (hWaitEvent)
			CloseHandle(hWaitEvent);
	}

#endif //end of #if 0

#else		//UNDER_CE

//This is for WINDOWS NT
#include <winsvc.h>
#include <assert.h>

const int   TIMEOUT_COUNT  = 15;

#define DriverStateNotInstalled 1
#define DriverStateInstalled    2
#define DriverStateRunning      3

#define NDP_SERVICE_NAME             _T("NDP")
#define NDP_SERVICE_NAME_L           L"NDP"

#define MapPtrToProcess(_pBuff, _pid)	(_pBuff)
#define UnMapPtr(_pBuff)	

DWORD NDPDriverCheckState(
	PDWORD pdwState
	)
/*++
Routine Description:
	 Check whether the driver is installed, removed, running etc.

Arguments:
	 pdwState    -   State of the driver

Return Value:
	 NO_ERROR    - If successful
	 Others      - failure
--*/
{
	SC_HANDLE       hSCMan   = NULL;
	SC_HANDLE       hDriver  = NULL;
	DWORD           dwStatus = NO_ERROR;
	SERVICE_STATUS  ServiceStatus;

	*pdwState = DriverStateNotInstalled;

	do
	{	// break off loop

		hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if(NULL == hSCMan)
		{

			dwStatus = GetLastError();
			//printf("Failed to OpenSCManager. Error %u\n", dwStatus);
			break;
		}

		//open service to see if it exists
		hDriver = OpenService(hSCMan, NDP_SERVICE_NAME, SERVICE_ALL_ACCESS);

		if(NULL != hDriver)
		{

			*pdwState = DriverStateInstalled;

			//service does exist
			if(QueryServiceStatus(hDriver, &ServiceStatus))
			{

				switch(ServiceStatus.dwCurrentState)
				{
					case SERVICE_STOPPED:
						//printf("AtmSmDrv current status -- STOPPED\n");
						break;

					case SERVICE_RUNNING:
						//printf("AtmSmDrv current status -- RUNNING\n");
						*pdwState = DriverStateRunning;
						break;

					default:
						break;
				}
			}

		}
		else
		{
			dwStatus = GetLastError();
			//printf("Failed to OpenService - Service not installed\n");
			// driver not installed.
			break;
		}

	} while(FALSE);

	if(NULL != hDriver)
		CloseServiceHandle(hDriver);

	if(NULL != hSCMan)
		CloseServiceHandle(hSCMan);

	return dwStatus;
}


HANDLE NTNDPDriverStart(
	)
/*++
Routine Description:
	 Start the AtmSmDrv.Sys.

Arguments:
					 -
Return Value:
	 NO_ERROR    - If successful
	 Others      - failure
--*/
{

	DWORD   dwState, dwStatus;

	if(NO_ERROR != (dwStatus = NDPDriverCheckState(&dwState)))
	{
		return HANDLE(NULL);
	}

	switch(dwState)
	{

		case DriverStateNotInstalled:
			dwStatus = ERROR_SERVICE_DOES_NOT_EXIST;	 // not installed
			break;

		case DriverStateRunning:
			break;

		case DriverStateInstalled: {

				// the service is installed, start it

				SC_HANDLE       hSCMan   = NULL;
				SC_HANDLE       hDriver  = NULL;
				SERVICE_STATUS  ServiceStatus;
				int             i;

				do
				{ // break off loop

					hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

					if(NULL == hSCMan)
					{
						dwStatus = GetLastError();
						//printf("Failed to OpenSCManager. Error - %u\n", dwStatus);
						break;
					}

					hDriver = OpenService(hSCMan, NDP_SERVICE_NAME, 
									 SERVICE_ALL_ACCESS);

					// start service
					if(!hDriver || !StartService(hDriver, 0, NULL))
					{
						dwStatus = GetLastError();
						//printf("Failed to StartService - Error %u\n", dwStatus);
						break;
					}

					dwStatus = ERROR_TIMEOUT;

					// query state of service
					for(i = 0; i < TIMEOUT_COUNT; i++)
					{

						Sleep(1000);

						if(QueryServiceStatus(hDriver, &ServiceStatus))
						{

							if(ServiceStatus.dwCurrentState == SERVICE_RUNNING)
							{
								dwStatus = NO_ERROR;
								break;
							}
						}
					}

				} while(FALSE);

				if(NULL != hDriver)
					CloseServiceHandle(hDriver);

				if(NULL != hSCMan)
					CloseServiceHandle(hSCMan);

				break;
			}
	}

	if (dwStatus == NO_ERROR)
		return HANDLE(1);
	else
		return HANDLE(NULL);
}


DWORD NTNDPDriverStop(
	)
/*++
Routine Description:
	 Stop the AtmSmDrv.Sys.

Arguments:
					 -
Return Value:
	 NO_ERROR    - If successful
	 Others      - failure
--*/
{

	DWORD   dwState, dwStatus;

	if(NO_ERROR != (dwStatus = NDPDriverCheckState(&dwState)))
	{
		return dwStatus;
	}

	switch(dwState)
	{

		case DriverStateNotInstalled:
		case DriverStateInstalled: 
			// not running
			break;

		case DriverStateRunning: {

				// the service is running, stop it

				SC_HANDLE       hSCMan   = NULL;
				SC_HANDLE       hDriver  = NULL;
				SERVICE_STATUS  ServiceStatus;
				int             i;

				do
				{	  // break off loop

					hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

					if(NULL == hSCMan)
					{
						dwStatus = GetLastError();
						//printf("Failed to OpenSCManager. Error %u\n", dwStatus);
						break;
					}


					hDriver = OpenService(hSCMan, NDP_SERVICE_NAME, 
									 SERVICE_ALL_ACCESS);

					// stop service
					if(!hDriver || !ControlService(hDriver, 
						SERVICE_CONTROL_STOP, 
						&ServiceStatus))
					{
						dwStatus = GetLastError();
						//printf("Failed to StopService. Error %u\n", dwStatus);
						break;
					}

					// query state of service
					i = 0;
					while((ServiceStatus.dwCurrentState != SERVICE_STOPPED) &&
						(i < TIMEOUT_COUNT))
					{
						if(!QueryServiceStatus(hDriver, &ServiceStatus))
							break;

						Sleep(1000);
						i++;
					}

					if(ServiceStatus.dwCurrentState != SERVICE_STOPPED)
						dwStatus = ERROR_TIMEOUT;

				} while(FALSE);

				if(NULL != hDriver)
					CloseServiceHandle(hDriver);

				if(NULL != hSCMan)
					CloseServiceHandle(hSCMan);

				break;
			}
	}

	return dwStatus;
}

#endif      //UNDER_CE

//------------------------------------------------------------------------------

HANDLE OpenProtocol()
{
   HANDLE hAdapter;
   
   do {

      // Try open device
      hAdapter = CreateFile(
         NDP_PROTOCOL_DOS_NAME, GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL
      );

      // We was able do open, so include number of references to driver
      if (hAdapter != INVALID_HANDLE_VALUE) {
         if (g_hDriver != NULL) InterlockedIncrement(&g_driverRefCount);
         break;
      }

      // Driver is loaded and there was failure, nothing to do
      if (g_hDriver != NULL) break;

      // Try load driver/device      
#ifdef UNDER_CE
      g_hDriver = RegisterDevice(_T("ndp"), 1, _T("ndp.dll"), 0);
#else
	  g_hDriver = NTNDPDriverStart();
#endif

   }while (g_hDriver != NULL);
   
   return hAdapter;
}

//------------------------------------------------------------------------------

VOID CloseProtocol(HANDLE hAdapter)
{
   if (hAdapter != NULL) {
      CloseHandle(hAdapter);
      if (g_hDriver != NULL) {
         if (InterlockedDecrement(&g_driverRefCount) == 0) {

#ifdef UNDER_CE
            DeregisterDevice(g_hDriver);
#else
			NTNDPDriverStop();
#endif
            g_hDriver = NULL;
         }
      }         
   }
}

//------------------------------------------------------------------------------

BOOL OpenAdapter(HANDLE hAdapter, LPCTSTR szAdapter)
{
   DWORD actualLengthOut = 0;
   DWORD lenIn = (lstrlen(szAdapter) + 1) * sizeof(szAdapter[0]);

   return DeviceIoControl(
      hAdapter, IOCTL_NDP_OPEN_MINIPORT, (PVOID)szAdapter, 
      lenIn, NULL, 0, &actualLengthOut, NULL
   );
}

//------------------------------------------------------------------------------

BOOL CloseAdapter(HANDLE hAdapter)
{
   DWORD actualLengthOut = 0;

   return DeviceIoControl(
      hAdapter, IOCTL_NDP_CLOSE_MINIPORT, NULL, 0, NULL, 0, &actualLengthOut, 
      NULL
   );
}

//------------------------------------------------------------------------------

BOOL QueryMPOID(HANDLE hAdapter,NDIS_OID oid, PBYTE puBuff, PDWORD pdwBuffSize, NDIS_STATUS * pstatus)
{
	NDP_OID_MP ndpOid;
	ndpOid.eType = QUERY;
	ndpOid.oid = oid;
	ndpOid.lpInOutBuffer = puBuff;
	ndpOid.pnInOutBufferSize = pdwBuffSize;
	DWORD actualLengthOut = 0;

	BOOL flag = DeviceIoControl(
		hAdapter, IOCTL_NDP_MP_OID,&ndpOid,sizeof(NDP_OID_MP),NULL,0,&actualLengthOut,NULL);

	*pstatus = ndpOid.status;
	return flag;
}

BOOL SetMPOID(HANDLE hAdapter,NDIS_OID oid, PBYTE puBuff, PDWORD pdwBuffSize, NDIS_STATUS * pstatus)
{
	NDP_OID_MP ndpOid;
	ndpOid.eType = SET;
	ndpOid.oid = oid;
	ndpOid.lpInOutBuffer = puBuff;
	ndpOid.pnInOutBufferSize = pdwBuffSize;
	DWORD actualLengthOut = 0;

	BOOL flag = DeviceIoControl(
		hAdapter, IOCTL_NDP_MP_OID,&ndpOid,sizeof(NDP_OID_MP),NULL,0,&actualLengthOut,NULL);

	*pstatus = ndpOid.status;
	return flag;
}

//------------------------------------------------------------------------------

BOOL SendPacket(
   HANDLE hAdapter, UCHAR dstAddr[], DWORD packetType, UCHAR *pData, 
   DWORD dataSize
) {
   BOOLEAN ok = FALSE;
   NDP_SEND_PACKET_INP *pInp = NULL;
   DWORD inpSize, actualOutSize = 0;

   inpSize = sizeof(NDP_SEND_PACKET_INP) - 1 + dataSize;
   pInp = (NDP_SEND_PACKET_INP*)LocalAlloc(LPTR, inpSize);
   if (pInp == NULL) goto cleanUp;
      
   memcpy(&pInp->destAddr, dstAddr, sizeof(pInp->destAddr));
   pInp->packetType = packetType;
   pInp->dataSize = dataSize;
   if (dataSize > 0) memcpy(pInp->data, pData, dataSize);
   
   ok = DeviceIoControl(
      hAdapter, IOCTL_NDP_SEND_PACKET, pInp, inpSize, NULL, 0, &actualOutSize, 
      NULL
   );
   
cleanUp:
   if (pInp != NULL) LocalFree(pInp);
   return ok;
}

//------------------------------------------------------------------------------

BOOL Listen(HANDLE hAdapter, DWORD poolSize, BOOL bDirect, BOOL bBroadcast)
{
   BOOLEAN ok = FALSE;
   NDP_LISTEN_INP inp;
   DWORD actualOutSize = 0;

   inp.fDirected = bDirect ? 1 : 0;
   inp.fBroadcast = bBroadcast ? 1 : 0;
   inp.poolSize = poolSize;

   return DeviceIoControl(
      hAdapter, IOCTL_NDP_LISTEN, &inp, sizeof(NDP_LISTEN_INP), NULL, 0, 
      &actualOutSize, NULL
   );
}

//------------------------------------------------------------------------------

BOOL ReceivePacket(
   HANDLE hAdapter, DWORD timeout, UCHAR srcAddr[], DWORD *pPacketType, 
   UCHAR *pData, DWORD *pDataSize
) {
   BOOLEAN ok = FALSE;
   NDP_RECV_PACKET_INP inp;
   NDP_RECV_PACKET_OUT *pOut = NULL;
   DWORD outSize, actualOutSize = 0;

   inp.timeout = timeout;
   outSize = sizeof(NDP_RECV_PACKET_OUT) - 1;
   if (pDataSize != NULL) outSize += *pDataSize;
   pOut = (NDP_RECV_PACKET_OUT*)LocalAlloc(LPTR, outSize);
   if (pOut == NULL) goto cleanUp;

   ok = DeviceIoControl(
      hAdapter, IOCTL_NDP_RECV_PACKET, &inp, sizeof(NDP_RECV_PACKET_INP), pOut,
      outSize, &actualOutSize, NULL
   );
   if (!ok) goto cleanUp;

   memcpy(srcAddr, pOut->srcAddr, sizeof(pOut->srcAddr));
   *pPacketType = pOut->packetType;
   if (pDataSize != NULL) {
      if (pOut->dataSize < *pDataSize) *pDataSize = pOut->dataSize;
      if (*pDataSize > 0) memcpy(pData, pOut->data, *pDataSize);
   }
   
cleanUp:
   if (pOut != NULL) LocalFree(pOut);
   return ok;
}

//------------------------------------------------------------------------------

BOOL StressSend(
   HANDLE hAdapter, BOOL bSend, UCHAR dstAddr[], DWORD packetType, 
   DWORD poolSize, DWORD packetSize, DWORD packetsSend,
   DWORD dwFlagStressControlled,DWORD dwDelayInABurst, DWORD dwPacketsInABurst,
   DWORD *pTime, DWORD *pIdleTime, DWORD *pPacketsSent, DWORD *pBytesSent
) {
   BOOLEAN ok;
   NDP_STRESS_SEND_INP inp = {0};
   NDP_STRESS_SEND_OUT out;
   DWORD actualLengthOut = 0;

   inp.fSendOnly = bSend;
   memcpy(&inp.dstAddr, dstAddr, sizeof(inp.dstAddr));
   inp.poolSize = poolSize;
   inp.packetSize = packetSize;
   inp.packetType = packetType;
   inp.packetsSend = packetsSend;
   inp.FlagControl = dwFlagStressControlled;
   inp.delayInABurst = dwDelayInABurst;
   inp.packetsInABurst = dwPacketsInABurst;
   
   ok = DeviceIoControl(
      hAdapter, IOCTL_NDP_STRESS_SEND, &inp, sizeof(inp), &out, sizeof(out), 
      &actualLengthOut, NULL
   );
   if (!ok) goto cleanUp;
   
   *pTime = out.time;
   *pIdleTime = out.idleTime;
   *pPacketsSent = out.packetsSent;
   *pBytesSent = out.bytesSent;

cleanUp:
   return ok;
}

//------------------------------------------------------------------------------

BOOL StressReceive(
   HANDLE hAdapter, DWORD poolSize, DWORD packetType, UCHAR srcAddr[], 
   DWORD *pPacketType, UCHAR *pData, DWORD *pDataSize, DWORD* pTime, 
   DWORD* pIdleTime, DWORD *pPacketsReceived, DWORD *pBytesReceived
) {
   BOOLEAN ok = FALSE;
   NDP_STRESS_RECV_INP inp;
   NDP_STRESS_RECV_OUT *pOut;
   DWORD outSize, actualOutSize = 0;

   inp.packetType = packetType;
   inp.poolSize = poolSize;

   outSize = sizeof(NDP_STRESS_RECV_OUT) - 1;
   if (pDataSize != NULL) outSize += *pDataSize;
   pOut = (NDP_STRESS_RECV_OUT*)LocalAlloc(LPTR, outSize);
   if (pOut == NULL) goto cleanUp;
   
   ok = DeviceIoControl(
      hAdapter, IOCTL_NDP_STRESS_RECV, &inp, sizeof(inp), pOut, outSize, 
      &actualOutSize, NULL
   );
   if (!ok) goto cleanUp;
   
   *pTime = pOut->time;
   *pIdleTime = pOut->idleTime;
   *pPacketsReceived = pOut->packetsReceived;
   *pBytesReceived = pOut->bytesReceived;
   *pPacketType = pOut->packetType;
   memcpy(srcAddr, pOut->srcAddr, sizeof(pOut->srcAddr));
   if (pDataSize != NULL) {
      if (pOut->dataSize < *pDataSize) *pDataSize = pOut->dataSize;
      if (*pDataSize > 0) memcpy(pData, pOut->data, *pDataSize);
   }

cleanUp:
   if (pOut != NULL) LocalFree(pOut);
   return ok;
}

//------------------------------------------------------------------------------
