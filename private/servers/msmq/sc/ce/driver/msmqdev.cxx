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

/*--
Module Name: MSMQD.CXX
Abstract: MSMQ service
--*/


#include <windows.h>
#include <stdio.h>

#include <mq.h>

#include <sc.hxx>
#include <scapi.h>
#include <scsrmp.hxx>

#include <service.h>
#include <mqmgmt.h>
#include <psl_marshaler.hxx>


extern "C" DWORD WINAPI scmain_StartDLL (LPVOID lpParm);
extern "C" HRESULT      scmain_StopDLL (void);

static long                 gs_fApiReady        = FALSE;
// static HANDLE       gs_hDevice               = NULL;
static const WCHAR *gs_MachineToken = MO_MACHINE_TOKEN;
unsigned long       glServiceState  = SERVICE_STATE_OFF;

#if defined (SC_VERBOSE)
extern unsigned int g_bCurrentMask;
extern unsigned int g_bOutputChannels;
#endif

extern "C" BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:        {
            DisableThreadLibraryCalls((HMODULE)hInstDll);

            svsutil_Initialize ();
            gMem = new GlobalMemory;

            if ((! gMem) || (! gMem->fInitialized))
                return FALSE;
        }
        break;

        case DLL_PROCESS_DETACH:
            delete gMem;
            svsutil_DeInitialize();
            break;
    }
    return TRUE;
}

extern "C" int MSMQDInitialize(TCHAR *szRegPath)
{
    return scmain_StartDLL (NULL) ? TRUE : FALSE;
}

extern "C" int MSMQDUnInitialize(void)
{
    return scmain_StopDLL ();
}

int scce_RegisterDLL (void) {
    if (InterlockedExchange (&gs_fApiReady, TRUE))
        return FALSE;

    return gs_fApiReady;
}

int scce_UnregisterDLL (void) {
    gs_fApiReady = FALSE;

    return TRUE;
}

// Changes whether MSMQ is started or stopped at boot time
void SetRegistryStartup(DWORD dwValue) {
    HKEY  hKey;

    // if registry key doesn't exist, don't create it.  "msmqadm -register" needs
    // to create it to fill in all the other relevant registry settings.
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey)) {
        RegSetValueEx(hKey, L"CEStartAtBoot", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// Logic to marshall pointers from user-mode services into services.exe, and vice-versa.



//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//              EXECUTION THREAD: Client-application!
//                      These functions are only executed on the caller's thread
//                      i.e. the thread belongs to the client application
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      @func PVOID | MMQ_Init | Device initialization routine
//  @parm DWORD | dwInfo | Info passed to RegisterDevice
//  @rdesc      Returns a DWORD which will be passed to Open & Deinit or NULL if
//                      unable to initialize the device.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for
// anything otherwise we run a risk of deadlocking
extern "C" DWORD MMQ_Init (DWORD Index)
{
    return MSMQDInitialize(NULL);
}

//      @func PVOID | MMQ_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from CON_Init call
//  @rdesc      Returns TRUE for success, FALSE for failure.
//      @remark Routine exported by a device driver.  "PRF" is the string
//                      passed in as lpszType in RegisterDevice

// NOTE: Since this function can be called in DllMain it cannot block for
// anything otherwise we run a risk of deadlocking
extern "C" BOOL MMQ_Deinit(DWORD dwData)
{
    return scmain_ForceExit();
}

//      @func PVOID | MMQ_Open          | Device open routine
//  @parm DWORD | dwData                | value returned from CON_Init call
//  @parm DWORD | dwAccess              | requested access (combination of GENERIC_READ
//                                                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc      Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                      unable to open device.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
extern "C" DWORD MMQ_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
    HANDLE *hCallerProc = (HANDLE *)g_funcAlloc (sizeof(HANDLE), g_pvAllocData);
    if (hCallerProc == NULL)
        return 0;

    *hCallerProc = NULL;

	gMem->Lock();
	// If this call fails, not a fatal error.  Just means that those
	// few MSMQ API's that require RemoteHeap allocation will fail.
	gMem->remoteAlloc.InitForProcess();
	gMem->Unlock();

    return (DWORD)hCallerProc;
}

//      @func BOOL | MMQ_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from MMQ_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
extern "C" BOOL MMQ_Close (DWORD dwData)
{
    // Security note: We're safe dereferencing dwData because it can only be supplied
    // by services.exe.  No means for a calling app to modify this.
    HANDLE *hCallerProc = (HANDLE *)dwData;

    if (hCallerProc && (*hCallerProc))
        scapi_ProcExit (*hCallerProc);

    if (hCallerProc)
        g_funcFree (hCallerProc, g_pvFreeData);

    return TRUE;
}

//      @func DWORD | MMQ_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc      Returns -1 for error, otherwise the number of bytes written.  The
//                      length returned is guaranteed to be the length requested unless an
//                      error condition occurs.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
//
extern "C" DWORD MMQ_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen)
{
    return -1;
}

//      @func DWORD | MMQ_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//                      bytes read.  The length returned is guaranteed to be the length
//                      requested unless end of file or an error condition occurs.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
//
extern "C" DWORD MMQ_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
    return -1;
}

//      @func DWORD | MMQ_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//               in as lpszType in RegisterDevice

extern "C" DWORD MMQ_Seek (DWORD dwData, long pos, DWORD type)
{
    return (DWORD)-1;
}


// Wrapper for StringCchCopy
BOOL MySafeStrcpy(wchar_t* pszDest, size_t cchDest, const wchar_t* pszSrc) {
    HRESULT hr;

    if (NULL == pszSrc)
        return FALSE;

    __try {
        hr = StringCchCopy(pszDest,cchDest,pszSrc);
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        hr = STRSAFE_E_INVALID_PARAMETER;
    }

    return SUCCEEDED(hr);
}

//      @func BOOL | MMQ_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from CON_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//              in as lpszType in RegisterDevice

#ifdef _PREFAST_
#pragma prefast(disable:214,"BOOL + HRESULT are both currently 4 byte values on CE.  All the return values cast around to them are safe.")
#endif

extern "C" BOOL MMQ_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
    HRESULT hr;
    
    if (dwCode == IOCTL_PSL_NOTIFY) {
        DEVICE_PSL_NOTIFY pslPacket;
        if ((pBufIn==NULL) || (0 == CeSafeCopyMemory(&pslPacket,pBufIn,sizeof(pslPacket)))) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        if ((pslPacket.dwSize == sizeof(DEVICE_PSL_NOTIFY)) && (pslPacket.dwFlags == DLL_PROCESS_EXITING)){
            SVSUTIL_ASSERT (*(HANDLE *)dwData == pslPacket.hProc);
            scapi_ProcExit ((HANDLE)pslPacket.hProc);
        }

        return STATUS_SUCCESS;
    }

    if (dwData != TRUE) {
        // Security note: We're safe dereferencing dwData because it can only be supplied
        // by services.exe.  No means for a calling app to modify this.
        HANDLE *hCallerProc = (HANDLE *)dwData;
        SVSUTIL_ASSERT (hCallerProc);

        if (! *hCallerProc)
            *hCallerProc = GetCallerProcess ();

        SVSUTIL_ASSERT (*hCallerProc == GetCallerProcess());
    }

    switch (dwCode) {
    case IOCTL_MSMQ_INVOKE:
    {
        // PSL marshaller logic.  This is what processes calls from user mode 
        // apps like MQCreateQueue.
        ce::psl_stub<> stub(pBufIn, dwLenIn);

        switch(stub.function()) {
        case MQAPI_CODE_MQCreateQueue:
            hr = stub.call(scapi_MQCreateQueuePSL);
            break;

        case MQAPI_CODE_MQDeleteQueue:
            hr = stub.call(scapi_MQDeleteQueuePSL);
            break;

        case MQAPI_CODE_MQGetQueueProperties:
            hr = stub.call(scapi_MQGetQueuePropertiesPSL);
            break;

        case MQAPI_CODE_MQSetQueueProperties:
            hr = stub.call(scapi_MQSetQueuePropertiesPSL);
            break;

        case MQAPI_CODE_MQOpenQueue:
            hr = stub.call(scapi_MQOpenQueuePSL);
            break;

        case MQAPI_CODE_MQCloseQueue:
            hr = stub.call(scapi_MQCloseQueuePSL);
            break;


        case MQAPI_CODE_MQCreateCursor:
            hr = stub.call(scapi_MQCreateCursorPSL);
            break;

        case MQAPI_CODE_MQCloseCursor:
            hr = stub.call(scapi_MQCloseCursorPSL);
            break;

        case MQAPI_CODE_MQHandleToFormatName:
            hr = stub.call(scapi_MQHandleToFormatNamePSL);
            break;

        case MQAPI_CODE_MQPathNameToFormatName:
            hr = stub.call(scapi_MQPathNameToFormatNamePSL);
            break;

        case MQAPI_CODE_MQSendMessage:
            hr = stub.call(scapi_MQSendMessagePSL);
            break;

        case MQAPI_CODE_MQReceiveMessage:
            hr = stub.call(scapi_MQReceiveMessagePSL);
            break;

        case MQAPI_CODE_MQFreeMemory:
            hr = stub.call(scapi_MQFreeMemoryPSL);
            break;

        case MQAPI_CODE_MQGetMachineProperties:
            hr = stub.call(scapi_MQGetMachinePropertiesPSL);
            break;

        case MQAPI_CODE_MQMgmtGetInfo2:
            hr = stub.call(scmgmt_MQMgmtGetInfo2PSL);
            break;

        case MQAPI_CODE_MQMgmtAction:
            hr = stub.call(scmgmt_MQMgmtActionPSL);
            break;

        default:
            hr = STATUS_INTERNAL_ERROR;
            break;
        }
    } // End case IOCTL_MSMQ_INVOKE
    return svsutil_DWORDtoBOOLErrCode(hr);
    break;

    // Process HTTP request for MSMQ
    case MQAPI_Code_SRMPControl: {
		if (GetCallerVMProcessId() != GetCurrentProcessId()) {
            // This call can only be made from the web server (in particular
            // srmpIsapi.dll) running in servicesd.exe space into MSMQ.
			SetLastError(ERROR_ACCESS_DENIED);
			return FALSE;
		}

        if (!g_fHaveSRMP || !pBufIn || dwLenIn != sizeof(SrmpIOCTLPacket) || !pBufOut || dwLenOut != sizeof(DWORD)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

		// Even though there are embedded pointers in this structure which we
		// arguably should be marshalling, because only web server in servicesd.exe
		// calls us (in same process space) don't bother.
        SrmpAcceptHttpRequest((SrmpIOCTLPacket *) pBufIn,(DWORD*) pBufOut);
        break;
    }

    //
    // Services IOControl codes.
    //
    case IOCTL_SERVICE_START:
        // only perform registry operations on SERVICE IOControls because other
        // IOControls are called by existing MSMQ admin utilities which are responsible for setting.

        SetRegistryStartup(1);
        hr = scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_STARTUP);
        break;

    case IOCTL_SERVICE_STOP:
        SetRegistryStartup(0);
        hr = scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_SHUTDOWN);
        break;

    case IOCTL_SERVICE_CONSOLE:
        hr = scmgmt_MQMgmtAction (NULL, gs_MachineToken, MACHINE_ACTION_CONSOLE);
        break;

#if defined (SC_VERBOSE)
    case IOCTL_SERVICE_DEBUG:
        if ((dwLenIn == sizeof(L"console")) && (wcsicmp ((WCHAR *)pBufIn, L"console") == 0))
            g_bOutputChannels = VERBOSE_OUTPUT_CONSOLE;
        else if ((dwLenIn == sizeof(L"serial")) && (wcsicmp ((WCHAR *)pBufIn, L"serial") == 0))
            g_bOutputChannels = VERBOSE_OUTPUT_DEBUGMSG;
        else if ((dwLenIn == sizeof(L"file")) && (wcsicmp ((WCHAR *)pBufIn, L"file") == 0))
            g_bOutputChannels = VERBOSE_OUTPUT_LOGFILE;
        else if (dwLenIn == sizeof(DWORD))
            g_bCurrentMask = *(unsigned int *)pBufIn;
        else {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        hr = STATUS_SUCCESS;
    break;
#endif

    case IOCTL_SERVICE_STATUS:
        __try {
            if (!pBufOut || dwLenOut < sizeof(DWORD)) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            *(DWORD *)pBufOut = glServiceState;
            if (pdwActualOut)
                *pdwActualOut = sizeof(DWORD);
        }
        __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        return TRUE;
    break;
 
    default:
#if defined (SC_VERBOSE)
        scerror_DebugOut (VERBOSE_MASK_FATAL, L"Unknown control code %d\n", dwCode);
#endif
        hr = STATUS_INVALID_PARAMETER;
    }

    return svsutil_DWORDtoBOOLErrCode(hr);
}

#ifdef _PREFAST_
#pragma prefast(enable:214,"")
#endif

//      @func void | MMQ_PowerUp | Device powerup routine
//      @comm   Called to restore device from suspend mode.  You cannot call any
//                      routines aside from those in your dll in this call.
extern "C" void MMQ_PowerUp(void)
{
    return;
}
//      @func void | MMQ_PowerDown | Device powerdown routine
//      @comm   Called to suspend device.  You cannot call any routines aside from
//                      those in your dll in this call.
extern "C" void MMQ_PowerDown(void)
{
    return;
}

//
//      These do nothing in DLL
//
void scce_Listen (void) {
}

HRESULT scce_Shutdown (void) {
    return MQ_OK;
}
