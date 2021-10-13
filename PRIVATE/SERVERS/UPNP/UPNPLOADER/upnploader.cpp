//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <upnphost.h>
#include <linklist.h>
#include <regentry.h>
#include <ncdefine.h>
#include <ncdebug.h>
#include <service.h>
#include "upnphostkeys.h"

#define ttidLoader      1
#define LOADER_SIGNAL_STOP 1      // registry value that tells the loader to exit

IUPnPRegistrar *g_upnpRegistrar = NULL;
PCWSTR g_pszHTMLDeviceListPath = L"\\windows\\upnp\\loader.htm";
HANDLE g_hUPnPHostEvent = NULL;

BOOL fShutdown;
LONG  g_fState;

static void UpdateRunningDeviceList();

class HostedDeviceInfo
{
    public:
    HostedDeviceInfo(
        PCWSTR pszDeviceId
        ) :
            newState(UPNPREG_DISABLED)
        {
            link.Flink = link.Blink = 0;
            bstrDeviceId = SysAllocString(pszDeviceId);
        }
    HRESULT Run(RegEntry *pDeviceKey);
    HRESULT Stop();
    ~HostedDeviceInfo();
    static void InitializeRunningDeviceList();
    static void UpdateRunningDeviceList();
    static void CleanupRunningDeviceList();
    static BOOL CreateHTMLDeviceList();
    static HostedDeviceInfo *FindRunningDevice(PCWSTR pszDeviceId);   
    private:
    static LIST_ENTRY runningDeviceList;
    LIST_ENTRY link;
    BSTR bstrDeviceId;
    DWORD newState;
};

LIST_ENTRY HostedDeviceInfo::runningDeviceList;

HRESULT
HostedDeviceInfo::Run(
        RegEntry *pupnpDeviceKey
        )
{
    HRESULT hr;
    PWSTR pszProgId;
    PWSTR pszInitString;
    PWSTR pszXMLDesc;
    PWSTR pszResourcePath;
    DWORD dwLifetime = 0;
	IUPnPDeviceControl *pdev = NULL;
	IUPnPReregistrar *pupnpReregistrar = NULL;
	CLSID clsid;
	//need to make copies of the registry values because RegEntry only holds onto the last string value
	pszProgId = _wcsdup(pupnpDeviceKey->GetString(UPNPPROGIDVALUE));
    pszInitString = _wcsdup(pupnpDeviceKey->GetString(UPNPINITSTRINGVALUE));
    pszXMLDesc = _wcsdup(pupnpDeviceKey->GetString(UPNPXMLDESCVALUE));
    pszResourcePath = _wcsdup(pupnpDeviceKey->GetString(UPNPRESOURCEPATHVALUE));
    dwLifetime = pupnpDeviceKey->GetNumber(UPNPLIFETIME);

    if (pszProgId == NULL || *pszProgId == NULL || pszXMLDesc == NULL || *pszXMLDesc == NULL)
    {
        TraceTag(ttidError, "Run: Invalid parameters\n");
        hr =  E_INVALIDARG;
    }
    else if (!g_upnpRegistrar 
        || FAILED(g_upnpRegistrar->QueryInterface(__uuidof(IUPnPReregistrar), (void **)&pupnpReregistrar)))
    {
        hr = E_FAIL;
    }
    else
    {
    	hr = CLSIDFromProgID(pszProgId, &clsid);
    	if (FAILED(hr))
    	{
    		TraceTag(ttidError, "Failed to get clsid for %ls\n", pszProgId);
    	}
    	else
    	    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER,
    							__uuidof(IUPnPDeviceControl), (LPVOID *)&pdev);

    	if (FAILED(hr))
    	{
    		TraceTag(ttidError, "Failed to create %ls\n", pszProgId);
    	}
        else
        {
            TraceTag(ttidLoader,"Loading device %ls with ProgId %ls\n",bstrDeviceId,pszProgId);
            hr = pupnpReregistrar->ReregisterRunningDevice(
                    bstrDeviceId,
                    (BSTR) pszXMLDesc,
                    pdev,
                    (BSTR)pszInitString,
                    (BSTR)pszResourcePath,
                    dwLifetime
                    );
            if (SUCCEEDED(hr))
            {   // add to running device list
                Assert(link.Flink == 0);
                InsertHeadList(&runningDeviceList,&link);
            }
        }
    }
    if (pdev)
        pdev->Release();
    if (pupnpReregistrar)
		pupnpReregistrar->Release();
    if (pszProgId)
        free (pszProgId);
    if (pszInitString)
        free (pszInitString);
    if (pszXMLDesc)
        free (pszXMLDesc);
    if (pszResourcePath)
        free (pszResourcePath);
    return hr;
}

HRESULT HostedDeviceInfo::Stop()
{
    HRESULT hr;
    // we should be on the running device list
    if (!link.Flink || !link.Blink)
        return E_FAIL;
    // need to use IUPnPRegistrar 
    if (!g_upnpRegistrar)
        return E_FAIL;

    TraceTag(ttidLoader, "Stopping device %ls\n", bstrDeviceId);
    
    hr = g_upnpRegistrar->UnregisterDevice(bstrDeviceId, FALSE);

    RemoveEntryList(&link);
    link.Flink = link.Blink = 0;
    return hr;
}

HostedDeviceInfo::~HostedDeviceInfo()
{
    Assert(link.Flink == 0 && link.Blink == 0);
    if (bstrDeviceId)
        SysFreeString(bstrDeviceId);
}

void HostedDeviceInfo::InitializeRunningDeviceList()
{
    InitializeListHead(&runningDeviceList);
}

void 
HostedDeviceInfo::UpdateRunningDeviceList()
{
    HRESULT hr;
    LIST_ENTRY *pListEntry;
    HostedDeviceInfo *pDev;
    RegEntry upnpDevicesKey(UPNPDEVICESKEY,HKEY_LOCAL_MACHINE,FALSE);
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; pListEntry = pListEntry->Flink)
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        pDev->newState = UPNPREG_DISABLED;   // until proven otherwise
    }
    if (upnpDevicesKey.Success())
    {
        // enumerate subkeys
        RegEnumSubKeys enumDevices(&upnpDevicesKey);
        while (enumDevices.Next() == 0)
        {
            RegEntry upnpDeviceKey(enumDevices.GetName(), upnpDevicesKey.GetKey());
            if (upnpDeviceKey.Success())
            {
                UPNPREG_STATE state = (UPNPREG_STATE) upnpDeviceKey.GetNumber(L"State", UPNPREG_DISABLED);
                pDev = FindRunningDevice(enumDevices.GetName());
                // Now compare the registry state with our list and resolve the differences
                //  Registry:           RunningDeviceList:
                //  DISABLED          absent            => OK
                //  STARTING          absent            => new device, write STARTED
                //  STARTED           absent            => new device, write STARTED
                //  DISABLED          present           => remove device
                //  STARTING          present           => restart device, write STARTED
                //  STARTED           present           => OK
                if (!pDev)
                {
                    switch (state)
                    {
                        case UPNPREG_DISABLED:
                            // nothing to do
                            break;
                        case UPNPREG_STARTING:
                        case UPNPREG_STARTED:
                            // new device
                            pDev = new HostedDeviceInfo(enumDevices.GetName());

                            if (pDev)
                            {
                                hr = pDev->Run(&upnpDeviceKey);
                                if (FAILED(hr))
                                {
                                    TraceTag(ttidError, "Could not start device %ls hr=%x\n",pDev->bstrDeviceId, hr);
                                    delete pDev;
                                    pDev = NULL;
                                }
                            }
                            if (!pDev)
                            {
                                // failed to start the device
                                upnpDeviceKey.SetValue(L"State",UPNPREG_DISABLED);
                            }
                            else
                            {
                                pDev->newState = UPNPREG_STARTED;
                                upnpDeviceKey.SetValue(L"State",pDev->newState);
                            }
                            break;
                    }
                }
                else
                {
                    // device is already in the list
                    pDev->newState = state;
                }
            }
        }

        // now go through the running devices and see if we need to stop any
        for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; )
        {
            pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
            pListEntry = pListEntry->Flink;
            switch (pDev->newState)
            {
                case UPNPREG_STARTING:
                {
                    TraceTag(ttidLoader,"stopping and restarting %ls\n", pDev->bstrDeviceId);
                    // anomalous. Handle by stopping and then restarting
                    RegEntry upnpDeviceKey(pDev->bstrDeviceId, upnpDevicesKey.GetKey());
                    pDev->Stop();   // removes the device from the running list
                    // run will re-add the device to the list we are traversing but this should not 
                    // be a problem.
                    hr = pDev->Run(&upnpDeviceKey);
                    if (FAILED(hr))
                        delete pDev;
                    break;
                }
                case UPNPREG_STARTED:
                    // nothing to do
                    break;
                
                case UPNPREG_DISABLED:
                default:
                    // stop the device. The unregister is gratuitous since somebody has probably already
                    // called Unregister to cause the registry key to go away.
                    pDev->Stop();
                    delete pDev;
                    break;
            }
        }
        
    }
    CreateHTMLDeviceList();
}

void
HostedDeviceInfo::CleanupRunningDeviceList()
{
    HRESULT hr;
    LIST_ENTRY *pListEntry;
    HostedDeviceInfo *pDev;
    // now go through the running devices and see if we need to stop any
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; )
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        hr = pDev->Stop();
        TraceTag(ttidInit, "Stop() returned %x\n", hr);
        delete pDev;
        pListEntry = runningDeviceList.Flink;
    }
}

BOOL
HostedDeviceInfo::CreateHTMLDeviceList()
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LIST_ENTRY *pListEntry;
    HostedDeviceInfo *pDev;
    BOOL fRet = FALSE;
    DWORD dwWritten;
    PCSTR pszHTMLHeader = "<html>\r\n"
                         "<head><title>Registered UPnP Devices</title></head>\r\n"
                         "<h2>Hosted Devices</h2>\r\n";
    PCSTR pszHTMLTrailer = "</html>\r\n";
    CHAR szBuffer[1024];
    PWSTR pszTmpPath = new WCHAR [wcslen(g_pszHTMLDeviceListPath)+5];
    if (!pszTmpPath)
        return FALSE;
    wcscpy(pszTmpPath,g_pszHTMLDeviceListPath);
    wcscat(pszTmpPath,L".bak");  // append .bak to create the temp file name
    hFile = CreateFileW(pszTmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto Cleanup;
    fRet = WriteFile(hFile, pszHTMLHeader, strlen(pszHTMLHeader), &dwWritten,NULL);
    if (!fRet)
        goto Cleanup;
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; pListEntry = pListEntry->Flink)
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        sprintf(szBuffer,"<p>%ls",pDev->bstrDeviceId);
        fRet = WriteFile(hFile, szBuffer,strlen(szBuffer),&dwWritten, NULL);
        if (!fRet)
            break;
    }
    fRet = WriteFile(hFile, pszHTMLTrailer, strlen(pszHTMLTrailer), &dwWritten, NULL);
    if (!fRet)
        goto Cleanup;

    CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;
    DeleteFileW(g_pszHTMLDeviceListPath);   // the fail may not exist
    fRet = MoveFileW(pszTmpPath,g_pszHTMLDeviceListPath); 
    
Cleanup:
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
    if (pszTmpPath)
        delete [] pszTmpPath;
    return fRet;
}

HostedDeviceInfo *
HostedDeviceInfo::FindRunningDevice(PCWSTR pszDeviceId)
{
    LIST_ENTRY *pListEntry;
    HostedDeviceInfo *pDev;
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; pListEntry = pListEntry->Flink )
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        if (wcscmp(pszDeviceId, pDev->bstrDeviceId) == 0)
            break;
    }
    return (pListEntry != &runningDeviceList ? pDev : NULL);
}

DWORD WINAPI
DeviceLoaderProc( PVOID)
{
    HRESULT hr;
    RegEntry upnpDevicesKey(UPNPDEVICESKEY);

    HostedDeviceInfo::InitializeRunningDeviceList();
    
    // Initialize COM
    hr = CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        TraceError("Cannot initialize COM\n", hr);
        return hr;
    }
    
    // Create the UPnPRegistrar object
    hr = CoCreateInstance(__uuidof(UPnPRegistrar),NULL,CLSCTX_INPROC_SERVER,__uuidof(IUPnPRegistrar), (LPVOID *)&g_upnpRegistrar);
    if (FAILED(hr))
    {
        TraceError("Cannot instantiate UPnPRegistrar\n", hr);
        goto Cleanup;
    }
    
    // Create the named event that will be used to signal us
    g_hUPnPHostEvent = CreateEvent(NULL,FALSE,FALSE,UPNPLOADEREVENTNAME);
    if (g_hUPnPHostEvent == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        TraceTag(ttidError,"DeviceLoaderProc: %s event creation error %d\n",UPNPLOADEREVENTNAME, GetLastError());
        goto Cleanup;
    }
    
    while (TRUE)
    {
        DWORD dwWait;
        
        // start registered devices
        HostedDeviceInfo::UpdateRunningDeviceList();

        // Wait on the named event for someone to signal us that a device has been registered or unregistered
        TraceTag(ttidLoader, "DeviceLoaderProc: waiting for registration events ...\n");
        dwWait = WaitForSingleObject(g_hUPnPHostEvent,INFINITE);
        TraceTag(ttidLoader, "DeviceLoaderProc: wait returned %d\n", dwWait);
        fShutdown = upnpDevicesKey.GetNumber(UPNPLOADERSIGNALVALUE,0);
        if (dwWait != WAIT_OBJECT_0 || fShutdown)
        {
            TraceTag(ttidLoader, "DeviceLoaderProc: exiting\n");
            break;
        }
    }
    
Cleanup:
    upnpDevicesKey.DeleteValue(UPNPLOADERSIGNALVALUE);  // signal is transient. Delete after reading
    
    HostedDeviceInfo::CleanupRunningDeviceList();
    if (g_hUPnPHostEvent)
    {
        CloseHandle(g_hUPnPHostEvent);
        g_hUPnPHostEvent = NULL;
    }
    if (g_upnpRegistrar != NULL)
    {
        g_upnpRegistrar->Release();
        g_upnpRegistrar = NULL;
    }
    CoUninitialize();
    return (DWORD) hr;
}

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("UPNPLOADER"), {
    TEXT("Misc"), TEXT("Loader"), TEXT(""), TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT("Trace"),TEXT("Error") },
    0x00008003
};
#endif
PWCHAR g_pszFile;


#ifdef UPNPLOADER_PROGRAM
// standalone program 
void Usage()
{
    printf("Usage: upnploader <options>\n"
          "\t-b     Debug Break\n"
          "\t-c     clean registry\n"
          "\t-k     kill previous instance\n"
          );
    exit(1);
}

void wmain(int argc, WCHAR *argv[])
{
    int  i;
    BOOL bClean = FALSE;
    BOOL bDebugBreak = FALSE;
    BOOL bKill = FALSE;

    // Process args
    for (i=1;i<argc;i++) {
        if ((argv[i][0] == TEXT('-')) || (argv[i][0] == TEXT('/'))) {
            switch (argv[i][1]) {
#ifdef DEBUG                
                case TEXT('b'):
                case TEXT('B'):
                    bDebugBreak = TRUE;
                    break;
#endif                    
                case TEXT('c'):
                case TEXT('C'):
                    bClean = TRUE;  // clean all existing upnp device registry keys
                    break;
                    
                case TEXT('k'):
                case TEXT('K'):
                    bKill = TRUE;   // kill a previous instance of the device loader
                    break;
                    
                case TEXT('f'):
                case TEXT('F'):
                    
                    if (argv[i][2] == TEXT(':'))
                        g_pszFile = _wcsdup(argv[i]+3);
                    //else
                    //    Usage();
                    break;
                default:
                    Usage();
                    break;
            }
        }
    }
#ifdef DEBUG
    DEBUGREGISTER(NULL);
    if (bDebugBreak)
        DebugBreak();
#endif
    if (bClean)
    {   // delete all existing upnp device keys in the registry before we start
        RegEntry upnpDevicesKey(UPNPDEVICESKEY,HKEY_LOCAL_MACHINE,FALSE);
        RegEnumSubKeys enumDevices(&upnpDevicesKey);
        while (enumDevices.Next() == 0)
        {
            TraceTag(ttidLoader,"Deleting key %ls\n",enumDevices.GetName());
            RegDeleteKey(upnpDevicesKey.GetKey(),enumDevices.GetName());
        }
    }
    if (bKill)
    {
        // kill a previous instance of this program by setting a special key in the registry
        HANDLE hUPnPHostEvent = CreateEvent(NULL,FALSE,FALSE,UPNPLOADEREVENTNAME);
        if (hUPnPHostEvent != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            RegEntry upnpDevicesKey(UPNPDEVICESKEY,HKEY_LOCAL_MACHINE);
            upnpDevicesKey.SetValue(UPNPLOADERSIGNALVALUE, LOADER_SIGNAL_STOP );
            SetEvent(hUPnPHostEvent);
            CloseHandle(hUPnPHostEvent);
            TraceTag(ttidLoader,"Setting event to kill previous instance\n");
        }
        else
            TraceTag(ttidError,"Event open error: No previous instance? \n");
        return;
    }
    DeviceLoaderProc();
}
        
#else
// DLL stream driver 
extern "C" {
DWORD WINAPI UPL_Init(DWORD dwInfo );
BOOL WINAPI UPL_Deinit( DWORD dwData );
HANDLE WINAPI UPL_Open( DWORD dwData, DWORD dwAccessCode, DWORD dwShareMode);
BOOL WINAPI UPL_Close(DWORD dwOpenData);
DWORD WINAPI UPL_Read(DWORD dwOpenData, LPVOID pBuf, DWORD len);
DWORD WINAPI UPL_Write( DWORD dwOpenData, LPCVOID pBuf, DWORD len);
DWORD WINAPI UPL_Seek(DWORD dwOpenData, long pos, DWORD type);
void WINAPI UPL_PowerUp(DWORD dwData);
void WINAPI UPL_PowerDown( DWORD dwData);
BOOL WINAPI UPL_IOControl( DWORD  dwOpenData, DWORD  dwCode, PBYTE  pBufIn, 
DWORD  dwLenIn,
							PBYTE  pBufOut,DWORD  dwLenOut, PDWORD pdwActualOut);

};

HANDLE g_hUPnPLoaderThread;


DWORD Startup()
{
    if(!g_hUPnPLoaderThread)
        g_hUPnPLoaderThread = CreateThread(NULL, 0, DeviceLoaderProc, NULL, 0, NULL);
    
    if(g_hUPnPLoaderThread)
    {
        g_fState = SERVICE_STATE_ON;
        return TRUE;
    }
    else
        return FALSE;
}


BOOL Shutdown()
{
    if (g_hUPnPHostEvent && g_hUPnPLoaderThread)
	{
	    // set a particular value in the registry to tell the DeviceLoaderProc thread to stop.
	    // Why not use a global variable ? Only because we want the code to build as a program or a DLL service.
        RegEntry upnpDevicesKey(UPNPDEVICESKEY,HKEY_LOCAL_MACHINE);
        upnpDevicesKey.SetValue(UPNPLOADERSIGNALVALUE, LOADER_SIGNAL_STOP );
	    SetEvent(g_hUPnPHostEvent);
	    WaitForSingleObject(g_hUPnPLoaderThread, INFINITE);
	    CloseHandle(g_hUPnPLoaderThread);
	    g_hUPnPLoaderThread = NULL;
	}

    g_fState = SERVICE_STATE_OFF;

    return TRUE;
}


DWORD WINAPI
UPL_Init(
    DWORD dwInfo
    )
{
	TraceTag(ttidInit, "UPL_Init called\n");
	
    return Startup();    
}


BOOL WINAPI
UPL_Deinit(
    DWORD dwData
    )
{
	TraceTag(ttidInit, "UPL_Deinit called\n");
	
    return Shutdown();
}


HANDLE WINAPI
UPL_Open(
    DWORD dwData,
    DWORD dwAccessCode,
    DWORD dwShareMode
    )
{
    return (HANDLE) NULL;
}

BOOL WINAPI
UPL_Close(
    DWORD dwOpenData
    )
{
	return FALSE;
}

DWORD WINAPI
UPL_Read(
    DWORD dwOpenData, 
    LPVOID pBuf, 
    DWORD len
    )
{
    return (ULONG) -1;
}

DWORD WINAPI
UPL_Write(
    DWORD dwOpenData, 
    LPCVOID pBuf, 
    DWORD len
    )
{
    return (ULONG) -1;
}

DWORD WINAPI
UPL_Seek(
    DWORD dwOpenData, 
    long pos, 
    DWORD type
    )
{
    return (ULONG) -1;
}

void WINAPI
UPL_PowerUp(
    DWORD dwData
    )
{
}

void WINAPI
UPL_PowerDown(
    DWORD dwData
    )
{
}

BOOL WINAPI UPL_IOControl( DWORD  dwOpenData, DWORD  dwCode,
    PBYTE  pBufIn, DWORD  dwLenIn,
    PBYTE  pBufOut,DWORD  dwLenOut, PDWORD pdwActualOut)
{
    switch (dwCode)
    {
        case IOCTL_SERVICE_START:
		    
            return Startup();

        case IOCTL_SERVICE_STOP:
		    
            return Shutdown();

	    case IOCTL_SERVICE_STATUS:

            if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) 
            {
			    pBufOut = (PBYTE) MapCallerPtr(pBufOut,sizeof(DWORD));
			    pdwActualOut = (PDWORD) MapCallerPtr(pdwActualOut,sizeof(DWORD));
		    }

		    if (!pBufOut || dwLenOut < sizeof(DWORD))
            {
			    SetLastError(ERROR_INVALID_PARAMETER);
			    return FALSE;
		    }

		    *(DWORD *)pBufOut = g_fState;
		    if (pdwActualOut)
			    *pdwActualOut = sizeof(DWORD);
		    return TRUE;
    }

    return FALSE;
}

extern "C"
BOOL
WINAPI
DllMain(IN HANDLE DllHandle,
        IN ULONG Reason,
        IN PVOID Context OPTIONAL)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:

        // Initialize debugging
		DEBUGREGISTER((HMODULE)DllHandle);
        // We don't need to receive thread attach and detach
        // notifications, so disable them to help application
        // performance.
        DisableThreadLibraryCalls((HMODULE)DllHandle);

        g_fState = SERVICE_STATE_UNINITIALIZED;

        break; 

    case DLL_PROCESS_DETACH:

        break;

    }

    return TRUE;
}

#endif

