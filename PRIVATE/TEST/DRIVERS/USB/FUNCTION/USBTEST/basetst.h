//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++

Module Name:  
    BaseTst.h

Abstract:  
    USB driver Default function Test.
    
Notes: 

--*/


#ifndef __BASETST_H_
#define __BASETST_H_
#include <windows.h>
#include <usbtypes.h>
#include "SyncObj.h"
#include "usbserv.h"

class UsbFuncTest  {
public:
	UsbFuncTest(UsbClientDrv * pDriverPtr,BOOL Async=FALSE):
			usbDriver(*pDriverPtr),
			fAsync(Async)
	{ cEvent.ResetEvent();hTransfer=0;errorCode=0;};
	virtual DWORD FunctionComplete() { cEvent.SetEvent(); return 0;};
	BOOL WaitForComplete(DWORD dwTicks=INFINITE) {
		if (fAsync) {
			if (!hTransfer) {
				g_pKato->Log(LOG_FAIL, TEXT("Fail! Call back before returning handle at Transfer"));
				SetErrorCode(ERROR_USB_TRANSFER);
			};
			return cEvent.Lock(dwTicks);
		}
		else
			return TRUE;
	};
	virtual BOOL CheckResult(BOOL bSuccess,USB_HANDLE usbTransfer);
	DWORD SetErrorCode(DWORD aError) { return errorCode=aError; };
	DWORD GetErrorCode() { return errorCode;};


	UsbClientDrv& usbDriver;
	const BOOL fAsync;
	static DWORD WINAPI TransferNotify(LPVOID lpvNotifyParameter) ;

	USB_TRANSFER hTransfer; // just for reference
private:
	DWORD errorCode;
	CEvent cEvent;
};

#define USB_TIME_OUT 5000

BOOL GetDespscriptorTest(UsbClientDrv *,BOOL aSync);
BOOL GetInterfaceTest_A(UsbClientDrv *,BOOL Async);
BOOL BaseCase1Command(UsbClientDrv *pDriverPtr,BOOL Async);
BOOL DeviceRequest(UsbClientDrv *pDriverPtr,BOOL aSync);
#endif
