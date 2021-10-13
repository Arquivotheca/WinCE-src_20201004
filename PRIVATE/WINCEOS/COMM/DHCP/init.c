//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	init.c

  DESCRIPTION:
	initialization routines for Dhcp

*/

#include "dhcpp.h"
#include "protocol.h"
#include "ntddip.h"
#include "linklist.h"

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("DHCP"), {
    TEXT("Init"),	TEXT("Timer"),		TEXT("AutoIP"),		TEXT("Unused"),
    TEXT("Recv"),   TEXT("Send"),		TEXT("Request"),	TEXT("Media Sense"),
    TEXT("Notify"),	TEXT("Buffer"),		TEXT("Interface"),  TEXT("Misc"),
    TEXT("Alloc"),  TEXT("Function"),   TEXT("Warning"),    TEXT("Error") },
    0xc000
}; 
#endif

/*
NTSTATUS
IPDispatchDeviceControl(
                        IN PIRP Irp,
                        IN PIO_STACK_LOCATION IrpSp
                        )
*/

typedef DWORD (*PFNDWORD2)(void *, void *);

PFNDWORD2 v_pfnIPDispatchDeviceControl;

PFNVOID *v_apSocketFns, *v_apAfdFns;

CRITICAL_SECTION		v_ProtocolListLock;
PDHCP_PROTOCOL_CONTEXT	v_ProtocolList;
HANDLE					v_TcpipDriver;
int						v_DhcpInitDelay;

extern CTEEvent v_DhcpEvent;
void DhcpEventWorker(CTEEvent * Event, void * Context);


STATUS HandleMediaConnect(unsigned Context, PTSTR pAdapter);
STATUS HandleMediaDisconnect(unsigned Context, PTSTR pAdapter);


//	Find the protocol context with the given name
//	in the global list.
PDHCP_PROTOCOL_CONTEXT
FindProtocolContextByName(
	PWCHAR				wszProtocolName) {
	PDHCP_PROTOCOL_CONTEXT	pContext;

	// ASSERT(lockheld)

	for (pContext = v_ProtocolList; pContext; pContext = pContext->pNext) {
		if (wcsncmp(wszProtocolName, pContext->wszProtocolName, 
			MAX_DHCP_PROTOCOL_NAME_LEN) == 0) {
			break;
		}
	}

	return pContext;
}


// Handle by which to notify applications of changes to IP data structures.
PDHCP_PROTOCOL_CONTEXT
DhcpRegister(
	PWCHAR				wszProtocolName,
	PFNSetDHCPNTE		pfnSetNTE,
	PFNIPSetNTEAddr		pfnSetAddr, 
	PFN_DHCP_NOTIFY		*ppDhcpNotify)
//
//	If successful, returns a pointer to a DHCP protocol context
//	which the caller will pass to subsequent calls to DhcpNotify.
//
{
	PDHCP_PROTOCOL_CONTEXT pContext;

	DEBUGMSG(ZONE_INIT | ZONE_WARN, (TEXT("+DhcpRegister:\n")));

	EnterCriticalSection(&v_ProtocolListLock);

	pContext = FindProtocolContextByName(wszProtocolName);
	if (pContext == NULL) {
		pContext = LocalAlloc(LPTR, sizeof(*pContext));
		if (pContext) {
			pContext->pNext = v_ProtocolList;
			v_ProtocolList = pContext;
			wcsncpy(pContext->wszProtocolName, wszProtocolName, MAX_DHCP_PROTOCOL_NAME_LEN);
		}
	}

	if (pContext) {
		pContext->pfnSetNTE = pfnSetNTE;
		pContext->pfnSetAddr = pfnSetAddr;
		*ppDhcpNotify = DhcpNotify;
	}

	LeaveCriticalSection(&v_ProtocolListLock);

	DEBUGMSG(ZONE_INIT | ZONE_WARN,  (TEXT("-DhcpRegister: Ret = %x\n"), pContext));

	return pContext;
}	// DhcpRegister()


BOOL DllEntry (HANDLE hinstDLL, DWORD Op, PVOID lpvReserved) {
	BOOL Status = TRUE;

	switch (Op) {
	case DLL_PROCESS_ATTACH:
	    DEBUGREGISTER(hinstDLL);
		DEBUGMSG (ZONE_INIT|ZONE_WARN, 
			(TEXT("Dhcp: dllentry() %d\r\n"), hinstDLL));

		InitializeCriticalSection(&v_ProtocolListLock);
        CTEInitEvent(&v_DhcpEvent, DhcpEventWorker);
		DisableThreadLibraryCalls ((HMODULE)hinstDLL);
		break;

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_DETACH :
		break;

	case DLL_THREAD_ATTACH :
		break;
	
	default :
		break;
	}
	return Status;
}	// dllentry()


void CheckMediaSense() {
	PIRP						pIrp;
	PIO_STACK_LOCATION			pIrpSp;
	IP_GET_IP_EVENT_RESPONSE	*pResp;
	IP_GET_IP_EVENT_REQUEST		Req;
	ULONG						cResp;
	HANDLE						hEvent;
	DWORD						Status;
	USHORT						cBuf;

	pIrp = LocalAlloc(LPTR, sizeof(*pIrp));
	pIrpSp = LocalAlloc(LPTR, sizeof(*pIrpSp));
	cResp = sizeof(*pResp) + (MAX_ADAPTER_NAME * sizeof(WCHAR)); // space for adapter name too
	pResp = LocalAlloc(LPTR, cResp);
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);


	Req.SequenceNo = 0;
	while (pIrp && pIrpSp && pResp) {

		pIrpSp->Parameters.DeviceIoControl.IoControlCode = 
			IOCTL_IP_GET_IP_EVENT;
		pIrpSp->Parameters.DeviceIoControl.OutputBufferLength = cResp;
		pIrpSp->Parameters.DeviceIoControl.InputBufferLength = sizeof(Req);

		pIrp->AssociatedIrp.SystemBuffer = pResp;
		pIrp->Tail.Overlay.CurrentStackLocation = pIrpSp;
		pIrp->UserEvent = hEvent;

		Status = (*v_pfnIPDispatchDeviceControl)(pIrp, pIrpSp);
		DEBUGMSG(1, (TEXT("IPDispatchDeviceControl returned Status %x\r\n"),
			Status));

		WaitForSingleObject(hEvent, INFINITE);

		DEBUGMSG(ZONE_WARN, 
			(TEXT("************DHCP MEDIA STATUS************\r\n")));
		
		DEBUGMSG(ZONE_WARN, (TEXT("SeqNo:	%d\r\n"), pResp->SequenceNo));
		DEBUGMSG(ZONE_WARN, (TEXT("MediaStatus:	%d\r\n"), pResp->MediaStatus));
		DEBUGMSG(ZONE_WARN, (TEXT("Context:	%d - %d\r\n"), pResp->ContextStart, 
			pResp->ContextEnd));

        //
        // Zero terminate UNICODE_STRING since it is not guaranteed to be zero terminated.
        //
        cBuf = pResp->AdapterName.Length/sizeof(WCHAR);
        if (cBuf < MAX_ADAPTER_NAME) {
            pResp->AdapterName.Buffer[cBuf] = 0;
        } else {
            pResp->AdapterName.Buffer[MAX_ADAPTER_NAME-1] = 0;
        }

		DEBUGMSG(ZONE_WARN, 
			(TEXT("AdapterName:	%s\r\n"), pResp->AdapterName.Buffer));

		switch(pResp->MediaStatus) {
		case IP_MEDIA_CONNECT:
			DEBUGMSG(ZONE_WARN, 
				(TEXT("Media Status is: IP_MEDIA_CONNECT\r\n")));
			HandleMediaConnect(pResp->ContextStart, pResp->AdapterName.Buffer);
			break;
		case IP_BIND_ADAPTER:
			DEBUGMSG(ZONE_WARN, 
				(TEXT("Media Status is: IP_BIND_ADAPTER\r\n")));
			break;
		case IP_UNBIND_ADAPTER:
			DEBUGMSG(ZONE_WARN, 
				(TEXT("Media Status is: IP_UNBIND_ADAPTER\r\n")));
			break;
		case IP_MEDIA_DISCONNECT:
			DEBUGMSG(ZONE_WARN, 
				(TEXT("Media Status is: IP_MEDIA_DISCONNECT\r\n")));
			HandleMediaDisconnect(pResp->ContextStart, 
				pResp->AdapterName.Buffer);
			break;
		case IP_INTERFACE_METRIC_CHANGE:
			DEBUGMSG(ZONE_WARN, 
				(TEXT("Media Status is: IP_INTERFACE_METRIC_CHANGE\r\n")));
			break;
		default:
			DEBUGMSG(1, (TEXT("Media Status is: UNKNOWN\r\n")));
			break;
		}

	}

	if (pIrp)
		LocalFree(pIrp);
	if (pIrpSp)
		LocalFree(pIrpSp);
	if (pResp)
		LocalFree(pResp);
	if (hEvent)
		CloseHandle(hEvent);

}	// CheckMediaSense()


DWORD InitMediaSense() {
	HANDLE	hTcpstk, hThrd;
	DWORD	Status = FALSE;

    if (hTcpstk = LoadLibrary(L"tcpstk.dll")) {

	    if (v_pfnIPDispatchDeviceControl = (PFNDWORD2)
	    	GetProcAddress(hTcpstk, L"IPDispatchDeviceControl")) {

			if (hThrd = CreateThread(NULL, 0, 
				(LPTHREAD_START_ROUTINE)CheckMediaSense, NULL, 0, NULL)) {

				Status = TRUE;
				CloseHandle(hThrd);
			}
	    }
    }

#ifdef DEBUG
	if (! Status) {
		DEBUGMSG(ZONE_WARN, 
			(TEXT("!Dhcp: Cannot initialize Media Sense\r\n")));
	}
#endif

	return Status;
	
}	// InitMediaSense()


//	IMPORTANT: The name of this fn must be the same as the HelperName
//	note: the parameters for registration is different: Unused, OpCode, 
//			pVTable, cVTable, pMTable, cMTable, pIndexNum
extern unsigned int v_cXid;

BOOL Dhcp(DWORD Unused, DWORD OpCode, 
			 PBYTE pName, DWORD cBuf1,
			 PBYTE pBuf1, DWORD cBuf2,
			 PDWORD pBuf2) {
	STATUS	Status = DHCP_SUCCESS;
	BOOL	fStatus = TRUE;
	PWSTR	pAdapterName = (PWSTR)pName;
    unsigned int    cRand, cRand2;
    FILETIME	CurTime;
    __int64 cBigRand;
	HKEY	hKey;
	LONG	hRes;

	switch(OpCode) {
	case DHCP_REGISTER:
		v_apAfdFns = (PFNVOID *)pName;
		v_apSocketFns = (PFNVOID *)pBuf1;
		*pBuf2 = AFD_DHCP_IDX;
		CTEInitLock(&v_GlobalListLock);
        InitializeListHead(&v_EstablishedList);

		v_ProtocolList = NULL;

        GetCurrentFT(&CurTime);
        cRand = (CurTime.dwHighDateTime & 0x788) | (CurTime.dwLowDateTime & 0xf800);
        cBigRand = CeGetRandomSeed();
		cRand2 = (uint)(cBigRand >> 32);
		cRand2 ^= (uint)(cBigRand & 0xffFFffFF);
        v_cXid = cRand ^ cRand2;
        InitMediaSense();

        hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("Comm\\Tcpip\\Parms"), 
        	0, 0, &hKey);
        if (ERROR_SUCCESS == hRes) {
            GetRegDWORDValue(hKey, TEXT("DhcpGlobalInitDelayInterval"), 
                &v_DhcpInitDelay);
            RegCloseKey(hKey);
        }

        DEBUGMSG(ZONE_INIT, (TEXT("\tDhcp:Register: FT %d, Xid %d\r\n"),
            cRand, v_cXid));
		return fStatus;

	case DHCP_PROBE:
		return TRUE;
		break;

	default:
		fStatus = FALSE;
		break;
	}
	
	if (Status != DHCP_SUCCESS) {
		SetLastError(Status);
		fStatus = FALSE;
	}

	return fStatus;
}	// Dhcp()


