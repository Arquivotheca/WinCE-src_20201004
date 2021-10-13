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
#include <winsock2.h>
#include <upnphost.h>
#include <linklist.h>
#include <regentry.h>
#include <ncdefine.h>
#include <ncdebug.h>
#include <service.h>
#include <upnphostkeys.h>
#include <upnpmem.h>
#include <cetls.h>

#include <auto_xxx.hxx>
#include <string.hxx>

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
    HRESULT                 hr;
    PWSTR                   pszProgId;
    ce::auto_bstr           bstrInitString;
    ce::auto_bstr           bstrXMLDesc;
    ce::auto_bstr           bstrResourcePath;
    DWORD                   dwLifetime = 0;
    IUPnPDeviceControl*     pdev = NULL;
    IUPnPReregistrar*       pupnpReregistrar = NULL;
    CLSID                   clsid;
    ce::wistring            wstrXMLDesc;
    ce::wistring::size_type n;
    char                    pszHostName[256];
    ce::wstring             wstrHostName;

    //need to make copies of the registry values because RegEntry only holds onto the last string value
    pszProgId = _wcsdup(pupnpDeviceKey->GetString(UPNPPROGIDVALUE));
    bstrInitString = SysAllocString(pupnpDeviceKey->GetString(UPNPINITSTRINGVALUE));
    wstrXMLDesc = pupnpDeviceKey->GetString(UPNPXMLDESCVALUE);
    bstrResourcePath = SysAllocString(pupnpDeviceKey->GetString(UPNPRESOURCEPATHVALUE));
    dwLifetime = pupnpDeviceKey->GetNumber(UPNPLIFETIME);
    bstrDeviceId = SysAllocString(pupnpDeviceKey->GetString(UPNPUDN));

    if(wstrXMLDesc.empty())
    {
        ce::auto_hfile hFile;

        hFile = CreateFile(pupnpDeviceKey->GetString(UPNPXMLDESCFILE), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if(hFile.valid())
        {
            DWORD       cbFile = GetFileSize(hFile, NULL);
            ce::string  strXMLDesc;

            strXMLDesc.reserve(cbFile);

            if(ReadFile(hFile, strXMLDesc.get_buffer(), strXMLDesc.capacity(), &cbFile, NULL))
            {
                ce::MultiByteToWideChar(CP_UTF8, strXMLDesc, cbFile, &wstrXMLDesc);
            }
        }
    }

    // Get device host name
    gethostname(pszHostName, sizeof(pszHostName));
    ce::MultiByteToWideChar(CP_ACP, pszHostName, sizeof(pszHostName), &wstrHostName);

    //
    // Replace every instance of %!HOST_NAME! in the device description with the host name of the device
    //
    for(n = 0; ce::wstring::npos != (n = wstrXMLDesc.find(L"%!HOST_NAME!", n)); )
    {
        wstrXMLDesc.replace(n, sizeof("%!HOST_NAME!") - 1, wstrHostName);
    }

    bstrXMLDesc = SysAllocString(wstrXMLDesc);

    if (pszProgId == NULL || *pszProgId == NULL || bstrXMLDesc == NULL || *bstrXMLDesc == NULL)
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
        {
            hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IUPnPDeviceControl), (LPVOID *)&pdev);
        }

        if (FAILED(hr))
        {
            TraceTag(ttidError, "Failed to create %ls\n", pszProgId);
        }
        else
        {
            if(SysStringLen(bstrDeviceId))
            {
                TraceTag(ttidLoader,"Loading device %ls with ProgId %ls\n",bstrDeviceId,pszProgId);
                hr = pupnpReregistrar->ReregisterRunningDevice(
                        bstrDeviceId,
                        bstrXMLDesc,
                        pdev,
                        bstrInitString,
                        bstrResourcePath,
                        dwLifetime);
            }
            else
            {
                TraceTag(ttidLoader,"Loading device with ProgId %ls for the first time\n", pszProgId);
                hr = g_upnpRegistrar->RegisterRunningDevice(
                        bstrXMLDesc, 
                        pdev, 
                        bstrInitString, 
                        bstrResourcePath, 
                        dwLifetime, 
                        &bstrDeviceId);
                if (SUCCEEDED(hr))
                {
                    pupnpDeviceKey->SetValue(UPNPUDN, bstrDeviceId);
                }
            }

            if (SUCCEEDED(hr))
            {   // add to running device list
                Assert(link.Flink == 0);
                InsertHeadList(&runningDeviceList,&link);
            }
        }
    }
    if (pdev)
    {
        pdev->Release();
    }
    if (pupnpReregistrar)
    {
        pupnpReregistrar->Release();
    }
    if (pszProgId)
    {
        free (pszProgId);
    }
    return hr;
}

HRESULT HostedDeviceInfo::Stop()
{
    HRESULT hr;
    // we should be on the running device list
    if (!link.Flink || !link.Blink)
    {
        return E_FAIL;
    }
    // need to use IUPnPRegistrar 
    if (!g_upnpRegistrar)
    {
        return E_FAIL;
    }

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
    {
        SysFreeString(bstrDeviceId);
    }
}

void HostedDeviceInfo::InitializeRunningDeviceList()
{
    InitializeListHead(&runningDeviceList);
}


void HostedDeviceInfo::UpdateRunningDeviceList()
{
    HRESULT hr;
    LIST_ENTRY *pListEntry = NULL;
    HostedDeviceInfo *pDev = NULL;

    RegEntry upnpDevicesKey(UPNPDEVICESKEY,HKEY_LOCAL_MACHINE,FALSE);
    if(!upnpDevicesKey.Success())
    {
        TraceTag(ttidError, "%S: Unable to open registry entry [HKLM\\%S]\n", __FUNCTION__, UPNPDEVICESKEY);
        return;
    }

    TraceTag(ttidLoader, "%S: Marking devices disabled until we read registry settings\n", __FUNCTION__);
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; pListEntry = pListEntry->Flink)
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        pDev->newState = UPNPREG_DISABLED;   // until proven otherwise
    }

    // enumerate subkeys
    TraceTag(ttidLoader, "%S: Enumerating [HKLM\\%S] subkeys\n", __FUNCTION__, UPNPDEVICESKEY);
    RegEnumSubKeys enumDevices(&upnpDevicesKey);
    if(!enumDevices.Success())
    {
        TraceTag(ttidError, "%S: Error enumerating UpnpLoader subkeys\n", __FUNCTION__);
        return;
    }


    while (enumDevices.Next() == 0)
    {
        RegEntry upnpDeviceKey(enumDevices.GetName(), upnpDevicesKey.GetKey());
        if(!upnpDeviceKey.Success())
        {
            TraceTag(ttidError, "%S: Error reading %S from registry.  Continuing.\n", __FUNCTION__, enumDevices.GetName());
            continue;
        }

        UPNPREG_STATE state = (UPNPREG_STATE) upnpDeviceKey.GetNumber(L"State", UPNPREG_DISABLED);
        pDev = FindRunningDevice(upnpDeviceKey.GetString(UPNPUDN));
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
                    pDev = new HostedDeviceInfo(upnpDeviceKey.GetString(UPNPUDN));

                    if (pDev)
                    {
                        hr = pDev->Run(&upnpDeviceKey);
                        if (FAILED(hr))
                        {
                            TraceTag(ttidError, "%S:  Could not start device %ls hr=%x\n",__FUNCTION__, pDev->bstrDeviceId, hr);
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

    // now go through the running devices and see if we need to stop any
    TraceTag(ttidLoader, "%S: Determine whether any devices need to be stopped.\n", __FUNCTION__);
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; )
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        pListEntry = pListEntry->Flink;
        switch (pDev->newState)
        {
            case UPNPREG_STARTING:
            {
                TraceTag(ttidLoader,"%S: Stopping and restarting [%ls]\n", __FUNCTION__, pDev->bstrDeviceId);

                // anomalous. Handle by stopping and then restarting
                RegEntry upnpDeviceKey(pDev->bstrDeviceId, upnpDevicesKey.GetKey());
                pDev->Stop();   // removes the device from the running list
                
                // run will re-add the device to the list we are traversing but this should not be a problem
                hr = pDev->Run(&upnpDeviceKey);
                if(FAILED(hr))
                {
                    TraceTag(ttidError, "%S: Error restarting device [%ls]\n", __FUNCTION__, pDev->bstrDeviceId);
                    delete pDev;
                }
                break;
            }
            case UPNPREG_STARTED:
                // nothing to do
                break;

            case UPNPREG_DISABLED:
            default:
                // stop the device. The unregister is gratuitous since somebody has probably already
                // called Unregister to cause the registry key to go away.
                TraceTag(ttidLoader, "%S: Unregistering device [%ls]\n", __FUNCTION__, pDev->bstrDeviceId);
                hr = pDev->Stop();
                if(FAILED(hr))
                {
                    TraceTag(ttidError, "%S: Failed to unregister device [%ls].  May already be unregistered.\n", __FUNCTION__, pDev->bstrDeviceId);
                }
                delete pDev;
                break;
        }
    }

    CreateHTMLDeviceList();
    TraceTag(ttidLoader, "%S: Updated UpnpLoader running devices list\n", __FUNCTION__);
}


void HostedDeviceInfo::CleanupRunningDeviceList()
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
    {
        return FALSE;
    }
    wcscpy(pszTmpPath,g_pszHTMLDeviceListPath);
    wcscat(pszTmpPath,L".bak");  // append .bak to create the temp file name
    hFile = CreateFileW(pszTmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        goto Cleanup;
    }
    fRet = WriteFile(hFile, pszHTMLHeader, strlen(pszHTMLHeader), &dwWritten,NULL);
    if (!fRet)
    {
        goto Cleanup;
    }
    for (pListEntry = runningDeviceList.Flink; pListEntry != &runningDeviceList; pListEntry = pListEntry->Flink)
    {
        pDev = CONTAINING_RECORD(pListEntry,HostedDeviceInfo,link);
        sprintf(szBuffer,"<p>%ls",pDev->bstrDeviceId);
        fRet = WriteFile(hFile, szBuffer,strlen(szBuffer),&dwWritten, NULL);
        if (!fRet)
        {
            break;
        }
    }
    fRet = WriteFile(hFile, pszHTMLTrailer, strlen(pszHTMLTrailer), &dwWritten, NULL);
    if (!fRet)
    {
        goto Cleanup;
    }

    CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;
    DeleteFileW(g_pszHTMLDeviceListPath);   // the fail may not exist
    fRet = MoveFileW(pszTmpPath,g_pszHTMLDeviceListPath); 

Cleanup:
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
    }
    if (pszTmpPath)
    {
        delete [] pszTmpPath;
    }
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
        {
            break;
        }
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
    0x00008000
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
    for (i=1;i<argc;i++) 
    {
        if ((argv[i][0] == TEXT('-')) || (argv[i][0] == TEXT('/'))) 
        {
            switch (argv[i][1]) 
            {
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
                    {
                        g_pszFile = _wcsdup(argv[i]+3);
                    }
                    //else
                    //{
                    //    Usage();
                    //}
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
    {
        DebugBreak();
    }
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
        {
            TraceTag(ttidError,"Event open error: No previous instance? \n");
        }
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
    {
        g_hUPnPLoaderThread = CreateThread(NULL, 0, DeviceLoaderProc, NULL, 0, NULL);
    }

    if(g_hUPnPLoaderThread)
    {
        g_fState = SERVICE_STATE_ON;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
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

            __try
            {
                if (!pBufOut || dwLenOut < sizeof(DWORD))
                {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
                }

                *(DWORD *)pBufOut = g_fState;
                if (pdwActualOut)
                {
                    *pdwActualOut = sizeof(DWORD);
                }
                return TRUE;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
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

        // initialize debugging
        DEBUGREGISTER((HMODULE)DllHandle);

        if(!UpnpHeapCreate())
        {
            return FALSE;
        }

        if (!CeTlsProcessAttach())
        {
            UpnpHeapDestroy();
            return FALSE;
        }

        g_fState = SERVICE_STATE_UNINITIALIZED;
        break;

    case DLL_PROCESS_DETACH:

        CeTlsProcessDetach();
        UpnpHeapDestroy();
        break;

    case DLL_THREAD_ATTACH:
        CeTlsThreadAttach();
        break;

    case DLL_THREAD_DETACH:
        CeTlsThreadDetach();
        break;

    }

    return TRUE;
}

#endif

