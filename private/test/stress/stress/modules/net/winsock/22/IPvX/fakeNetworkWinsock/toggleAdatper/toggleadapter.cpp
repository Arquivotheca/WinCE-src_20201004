//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include <strsafe.h>
#include <mstcpip.h>
#include <xft.h>
#include "FnFakeNetwork.h"
#include "FnAddressGenerator.h"
#include "FnNetworkConfig.h"
#include "FnFakeDnsServer.h"
#include "FnUtils.h"
#include "FnLogging.h"
#include "vector"
#include "cmnet.h"
#include "util.h"

using namespace ce;
using namespace ce::qa::xft;
#define NUMADAPTERS             12
#define ADAPTERNAMEPREFIX       _T("ADAPTER")
#define VANameLength            256
#define FAKESERVER              _T("fnFakeServer")
#define MINSLEEPTIME            1000
#define MAXSLEEPTIME            2000
#define DEFAULT_THREADS         3

ce::vector<smart_ptr<FnFakeAdapter>>        g_fnAdapters;
ce::vector<smart_ptr<FnFakeIpAddress>>      g_FnFakeIpAddresses;
smart_ptr<FnFakeNetwork>                    g_fnNetwork=NULL;
smart_ptr<FnFakeDnsServer>                  g_fnDnsServer=NULL;
FnNetworkConfig                             g_fnConfig;
CRITICAL_SECTION                            g_CSAdapter;
int                 g_iAdapters=NUMADAPTERS;
DWORD               g_dwMinSleep=MINSLEEPTIME;
DWORD               g_dwMaxSleep=MAXSLEEPTIME;
BOOL                g_bCmAvailable=FALSE;
BOOL                g_bSetupNetwork=FALSE;
TCHAR               g_szServer[NI_MAXHOST]={0};
HANDLE              g_hInst = NULL;
HMODULE             g_hCmnet=NULL;

typedef CM_RESULT (*PFnCmGetToUpdatePolicyConfig)(
    __in_bcount(cbKey) CM_POLICY_CONFIG_KEY* pKey,
    __in DWORD cbKey,
    __out_bcount(*pcbData) CM_POLICY_CONFIG_DATA* pData,
    __inout DWORD* pcbData,
    __out CM_CONFIG_CHANGE_HANDLE* phConfig);

typedef CM_RESULT (*PFnCmUpdatePolicyConfig)(
    __in CM_CONFIG_CHANGE_HANDLE hConfig,
    __in_bcount(cbData) CM_POLICY_CONFIG_DATA* pData,
    __in DWORD cbData);

PFnCmGetToUpdatePolicyConfig    g_pFnCmGetToUpdatePolicyConfig=NULL;
PFnCmUpdatePolicyConfig         g_pFnCmUpdatePolicyConfig=NULL;


BOOL GetCurrentAccountName (ACCTID accIDReturned, TCHAR *szAccountNameReturned, DWORD cchAccountNameReturned);
BOOL SetupNetWork();
void DeletePolicy();

BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{
    
    *pnThreads=DEFAULT_THREADS;
    DWORD   dwThreads=DEFAULT_THREADS;
    LPTSTR  szServer=FAKESERVER;
    StringCchCopy(g_szServer, _countof(g_szServer), FAKESERVER);
    //Must call this to initialize stress module
    InitializeStressUtils (
                            _T("Toggle Adapter Stress"),                      // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, SLOG_FONT | SLOG_RGN),    // Logging zones used by default
                            pmp                                               // Forward the Module params passed on the cmd line
                            );
    LONG count = GetRunningModuleCount(g_hInst);
    //only one module can setup network
    if(count>1)
    {
        LogComment(TEXT("Detected another instance of the stress module in memory, only one may be run at a time."));
        return FALSE;
    }
    //this adapter only work on images with connection manager.
    g_bCmAvailable=IsCmAvailable();
    if(!g_bCmAvailable)
    {
        LogComment(TEXT("Connection manager is not avaialbe in this image."));     
        return FALSE;
    }

    InitUserCmdLineUtils(pmp->tszUser, NULL);
    User_GetOptionAsDWORD(_T("t"), &dwThreads);
    User_GetOption(_T("s"), &szServer);

    if(szServer&&_tcsicmp(szServer, TEXT("localhost")))
    {        
        StringCchCopy(g_szServer, _countof(g_szServer), szServer); 
    }

    *pnThreads = dwThreads;
    
    g_hCmnet = LoadLibrary(_T("CmNet.dll"));
    
    if(g_hCmnet)
    {
        g_pFnCmGetToUpdatePolicyConfig=(PFnCmGetToUpdatePolicyConfig) GetProcAddress(g_hCmnet, L"CmGetToUpdatePolicyConfig");
        g_pFnCmUpdatePolicyConfig=(PFnCmUpdatePolicyConfig) GetProcAddress(g_hCmnet, L"CmUpdatePolicyConfig");
    }
    if(!g_pFnCmGetToUpdatePolicyConfig ||!g_pFnCmUpdatePolicyConfig)
    {
        if(g_hCmnet)
        {
            FreeLibrary(g_hCmnet);
        }
        return FALSE;
    }
    g_bSetupNetwork=SetupNetWork();

    if(!g_bSetupNetwork)
    {
        DeletePolicy();
        FreeLibrary(g_hCmnet);
        return FALSE;
    }
    InitializeCriticalSection(&g_CSAdapter);

    return TRUE;
}

UINT InitializeTestThread (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in]*/ int index)
{
    LogVerbose(_T("InitializeTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
                    hThread, 
                    dwThreadId, 
                    index 
                    );

    return CESTRESS_PASS;
}

UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in]*/ LPVOID pv)
{
    ITERATION_INFO* pIter = (ITERATION_INFO*) pv;

    LogVerbose(_T("Thread %i, iteration %i"), pIter->index, pIter->iteration);

    if(!g_bCmAvailable)
    {
        return CESTRESS_PASS;
    }
    if(!g_bSetupNetwork)
    {
        return CESTRESS_ABORT;
    }
    
    TCHAR           szAdapterName[VANameLength];
    //ramdomly choose a adapter to make adapter name
    int             VACount=GetRandomRange(2,g_iAdapters-1);
    TCHAR           szCount[4]; 
    int             iSleepTime=GetRandomRange(g_dwMinSleep,g_dwMaxSleep);
    UINT            uRet = CESTRESS_PASS;
    memset(szAdapterName,0,sizeof(szAdapterName));
    memset(szCount,0,sizeof(szCount));

    _itot(VACount,szCount,10);
    //make adapter name
    StringCchCopy(szAdapterName, _countof(szAdapterName), ADAPTERNAMEPREFIX);
    StringCchCat(szAdapterName, _countof(szAdapterName), szCount); 
    
    //only one thread can toggle adapter each time.
    EnterCriticalSection(&g_CSAdapter);
    if(!g_fnConfig.IsAdapterConnected(szAdapterName))
    {
        //connect adapter
        if(!g_fnConfig.ConnectAdapter(szAdapterName))
        {
            LogFail(TEXT("Connect adapter %s failed"), szAdapterName);
            uRet= CESTRESS_FAIL;
        }
    }
    else
    {
        //disconnect adapter
        if(!g_fnConfig.DisconnectAdapter(szAdapterName))
        {
            LogFail(TEXT("Disconnect adapter %s failed"), szAdapterName);
            uRet= CESTRESS_FAIL;
        }
    }
    Sleep(iSleepTime);
    LeaveCriticalSection(&g_CSAdapter);

    // You must return one of these values:
    return uRet;
    //return CESTRESS_PASS;
    //return CESTRESS_FAIL;
    //return CESTRESS_WARN1;
    //return CESTRESS_WARN2;
    //return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
}

UINT CleanupTestThread (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in]*/ int index)
{
    LogComment(_T("CleanupTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
                    hThread, 
                    dwThreadId, 
                    index 
                    );

    return CESTRESS_PASS;
}

DWORD TerminateStressModule (void)
{

    DeleteCriticalSection(&g_CSAdapter);
    if(g_hCmnet)
    {
        DeletePolicy();
        FreeLibrary(g_hCmnet);
    }

    return CESTRESS_PASS;

}

///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance, 
                    ULONG dwReason, 
                    LPVOID lpReserved
                    )
{
    g_hInst = hInstance;
    return TRUE;
}

BOOL SetupNetWork()
{

    TCHAR           szAdapterName[VANameLength];
    int             iVACount=0;
    TCHAR           szCount[4];
    BOOL            bSetupNetwork=FALSE;

    CM_POLICY_CONFIG_DATA   * newPolicyConfigData = NULL;
    CM_POLICY_CONFIG_DATA   * pData = NULL;


    FnAddressGenerator AddrGen;

     //create fake adapter for both client and fake DNS server and fake Server
    for(;iVACount<g_iAdapters;iVACount++)
    { 
        memset(szAdapterName,0,sizeof(szAdapterName));
        memset(szCount,0,sizeof(szCount));
        _itot(iVACount,szCount,10);
        StringCchCopy(szAdapterName, _countof(szAdapterName), ADAPTERNAMEPREFIX); 
        StringCchCat(szAdapterName, _countof(szAdapterName), szCount); 
        g_fnAdapters.push_back(new FnFakeAdapter( szAdapterName));
    }
    
     //create fake IP address and gateway IP address
    for(int j=0;j<g_iAdapters;j++)
    {
        g_FnFakeIpAddresses.push_back(AddrGen.GenerateIPv4Address());
    }

    g_fnNetwork=new FnFakeNetwork( );
    g_fnDnsServer= new FnFakeDnsServer();
     
     //specify DNS server for the network
    g_fnNetwork->SetDnsServer( g_fnDnsServer );
     //set DNS server IP address
    g_fnDnsServer->AddServerAddress( g_FnFakeIpAddresses[0] );

    g_fnDnsServer->AddDNSEntry( NULL, NULL, g_szServer, g_FnFakeIpAddresses[1], 1 ); 
     
    for(int j=0;j<g_iAdapters;j++)
    {
        memset(szAdapterName,0,sizeof(szAdapterName));
        memset(szCount,0,sizeof(szCount));
        _itot(j,szCount,10);
        StringCchCopy(szAdapterName, _countof(szAdapterName), ADAPTERNAMEPREFIX); 
        StringCchCat(szAdapterName, _countof(szAdapterName), szCount); 
        //add adapter to fake network              
        g_fnNetwork->AddAdapter(g_fnAdapters[j]);
        //add IP address to fake adapter
        g_fnAdapters[j]->AddAddress( g_FnFakeIpAddresses[j]);
        //add DNS server to adapter
        g_fnAdapters[j]->AddDnsServer( g_FnFakeIpAddresses[0]);
        
        if(j>1)
        {
            g_fnAdapters[j]->AddConnection(szAdapterName);
        }
        else
        {
            g_fnAdapters[j]->AddConnection(szAdapterName, FnCmConnection::InfrastructureConnection );
        }
     }

    //display network info
    g_fnNetwork->PrintDetails( 0 );
   
    //create network
    if ( !g_fnConfig.CreateNetwork( g_fnNetwork ) ) 
    {
        goto Exit;
    } 
   
    ACCTID accIDReturned = 0;
    TCHAR szAccountName[128] = {0};
    CM_POLICY_CONFIG_KEY  Key;
    DWORD cbkey = sizeof(CM_POLICY_CONFIG_KEY);
    DWORD pcbActualData = sizeof(CM_POLICY_CONFIG_DATA);
    CM_CONFIG_CHANGE_HANDLE pHandle = NULL;
    CM_RESULT actualReturn;
    BOOL bAccount=FALSE;
    DWORD cchAccountNameReturned=_countof(szAccountName);

    //get account name to setup policy mapping
    bAccount= GetCurrentAccountName (accIDReturned, szAccountName, cchAccountNameReturned);

    Key.Version = CM_CURRENT_VERSION;
    Key.Type = CMPT_CONNECTION_MAPPINGS;
    StringCchCopyW(Key.ConnectionMappings.szAccount, _countof(Key.ConnectionMappings.szAccount), szAccountName);
    StringCchCopyW(Key.ConnectionMappings.szHost, _countof(Key.ConnectionMappings.szHost), _T("*"));

    DWORD cbData = sizeof(*pData);
    for (int attemp = 0; attemp < 3; attemp++)
    {
        pData = reinterpret_cast<CM_POLICY_CONFIG_DATA*>(new BYTE[cbData]);
        ASSERT(pData != NULL);
        actualReturn = g_pFnCmGetToUpdatePolicyConfig(&Key, sizeof(Key), pData, &cbData,
            &pHandle);
        if (actualReturn != CMRE_INSUFFICIENT_BUFFER)
            break;

        delete [] pData;
        pData = NULL;
    }
    DWORD dwconnCount=g_iAdapters-2;
    if(dwconnCount<=0)
    {
        goto Exit;
    }
    DWORD totSize = sizeof (CM_POLICY_CONFIG_DATA) + (sizeof (CM_CONNECTION_SELECTION) * (dwconnCount-1));
    newPolicyConfigData = reinterpret_cast<CM_POLICY_CONFIG_DATA*>(new BYTE[totSize]);
    if(newPolicyConfigData==NULL)
    {
        goto Exit;
    }
    DWORD dwconnIndex=0;
    DWORD dwConnection=1;
    //add connection to policy mapping
    for(int j=2;j<g_iAdapters;j++)
    {
        memset(szAdapterName,0,sizeof(szAdapterName));
        memset(szCount,0,sizeof(szCount));
        _itot(j,szCount,10);
        StringCchCopy(szAdapterName, _countof(szAdapterName), ADAPTERNAMEPREFIX); 
        StringCchCat(szAdapterName, _countof(szAdapterName), szCount); 
        newPolicyConfigData->ConnectionMappings.Connection[dwconnIndex].Selection = CMST_CONNECTION_NAME;
        StringCchCopyW(newPolicyConfigData->ConnectionMappings.Connection[dwconnIndex].szName,
            _countof(newPolicyConfigData->ConnectionMappings.Connection[dwconnIndex].szName), 
            szAdapterName);
        newPolicyConfigData->ConnectionMappings.cConnection = dwConnection++;
        dwconnIndex=dwconnIndex+1;
    }
    newPolicyConfigData->Version =CM_CURRENT_VERSION;
    newPolicyConfigData->Type = CMPT_CONNECTION_MAPPINGS;
    newPolicyConfigData->ConnectionMappings.fUseConnectionsInOrder = false;
    //update policy mapping with new configuration
    actualReturn = g_pFnCmUpdatePolicyConfig(pHandle, newPolicyConfigData, totSize);
    if(actualReturn==CMRE_SUCCESS)
    {
        bSetupNetwork=TRUE;
    }

Exit:
    if(pData)
    {
        delete [] pData;
    }
    
    if (newPolicyConfigData)
    {
        delete [] newPolicyConfigData;
    }
    if(!bSetupNetwork)
    {
        LogFail(TEXT("Failed to setup fake network"));
    }
    if(pHandle!=NULL && pHandle!=INVALID_HANDLE_VALUE)
    {
        CloseHandle(pHandle);
    }
    return bSetupNetwork;
}

BOOL GetCurrentAccountName (ACCTID accIDReturned, TCHAR *szAccountNameReturned, DWORD cchAccountNameReturned)
{
    *szAccountNameReturned = 0;
    return TRUE;
}

void DeletePolicy()
{
    CM_POLICY_CONFIG_DATA   * pData = NULL;
    ACCTID accIDReturned = 0;
    TCHAR szAccountName[128] = {0};
    CM_POLICY_CONFIG_KEY  Key;
    DWORD cbkey = sizeof(CM_POLICY_CONFIG_KEY);
    CM_CONFIG_CHANGE_HANDLE pHandle = NULL;
    CM_RESULT actualReturn=CMRE_SUCCESS;
    BOOL bAccount=FALSE;
    DWORD cchAccountNameReturned=_countof(szAccountName);

    //get account name to setup policy mapping
    bAccount= GetCurrentAccountName (accIDReturned, szAccountName, cchAccountNameReturned);

    Key.Version = CM_CURRENT_VERSION;
    Key.Type = CMPT_CONNECTION_MAPPINGS;
    StringCchCopyW(Key.ConnectionMappings.szAccount, _countof(Key.ConnectionMappings.szAccount), szAccountName);
    StringCchCopyW(Key.ConnectionMappings.szHost, _countof(Key.ConnectionMappings.szHost), _T("*"));

    DWORD cbData = sizeof(*pData);
    for (int attemp = 0; attemp < 3; attemp++)
    {
        pData = reinterpret_cast<CM_POLICY_CONFIG_DATA*>(new BYTE[cbData]);
        ASSERT(pData != NULL);
        actualReturn = g_pFnCmGetToUpdatePolicyConfig(&Key, sizeof(Key), pData, &cbData,
            &pHandle);
        if (actualReturn != CMRE_INSUFFICIENT_BUFFER)
            break;

        delete [] pData;
        pData = NULL;
    }
    DWORD dwconnCount=pData->ConnectionMappings.cConnection;
    if(dwconnCount<=0)
    {
        goto Exit;
    }
    DWORD totSize = sizeof (CM_POLICY_CONFIG_DATA) + (sizeof (CM_CONNECTION_SELECTION) * (dwconnCount-1));
    
    for(DWORD dwconnIndex=0;dwconnIndex<dwconnCount;dwconnIndex++)
    {
        pData->ConnectionMappings.Connection[dwconnIndex++].Selection = CMST_CONNECTION_ALL;
    }
    pData->ConnectionMappings.fUseConnectionsInOrder = false;
    //update policy mapping with new configuration
    actualReturn = g_pFnCmUpdatePolicyConfig(pHandle, pData, totSize);
Exit:
    if(pData)
    {
        delete [] pData;
    }
    if(pHandle!=NULL && pHandle!=INVALID_HANDLE_VALUE)
    {
        CloseHandle(pHandle);
    }
}
