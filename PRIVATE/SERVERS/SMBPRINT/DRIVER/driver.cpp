//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// Windows CE SMB file server driver componet
//
#include <CReg.hxx>
#include <String.hxx>
#include <service.h>
#include <GUIDGen.h>
#include <svsutil.hxx>

#include "SMB_Globals.h"
#include "utils.h"
#include "NetbiosTransport.h"
#include "TCPTransport.h"
#include "ShareInfo.h"
#include "PrintQueue.h"
#include "Cracker.h"
#include "FileServer.h"
#include "ConnectionManager.h"
#include "SMBIOCTL.h"
#include <psl_marshaler.hxx>
#include "wdm.h"
#include "pc_net_prog.h"

#ifndef  SECURITY_WIN32
#define  SECURITY_WIN32
#endif
#include <security.h>
#include <sspi.h>
#include "serverAdmin.h"

extern SERVERNAME_HARDENING_LEVEL SmbServerNameHardeningLevel;

class InitClass {
public:
    InitClass() {
        static BOOL fInited = FALSE;
        if(FALSE == fInited) {
            #ifdef DEBUG
                SMB_Globals::g_lMemoryCurrentlyUsed = 0;
            #endif
            
            svsutil_Initialize();
            fInited = TRUE;
        } else {
            ASSERT(FALSE);
        }        
    }
    ~InitClass() {
        svsutil_DeInitialize();
    }
};


// Stream interface functions
#define SMB_CONTEXT  0xABCDEF01

//
// Global that will call into SVS Utils init etc (any construction that must happen first)
InitClass g_InitClass;
ThreadSafePool<10, SMB_PACKET>   SMB_Globals::g_SMB_Pool;
BOOL      g_fServerRunning = FALSE;

CRITICAL_SECTION g_csDriverLock;
extern HRESULT StartListenOnNameChange();
extern VOID SMB_RestartServer();

HRESULT CreateShare(const WCHAR *pName, 
                    DWORD dwType, 
                    const WCHAR *pPath, 
                    const WCHAR *pACL,
                    const WCHAR *pROACL,
                    const WCHAR *pDriver, 
                    const WCHAR *pComment);


#define TOASCII(x, dest) \
    { \
        dest = NULL; \
        UINT uiSize; \
        if(0 != (uiSize = WideCharToMultiByte(CP_ACP, 0, x, -1, NULL, 0,NULL,NULL))) \
        { \
            __try { \
                dest = (char *)_alloca(uiSize); \
                \
                WideCharToMultiByte(CP_ACP, 0, x, -1, dest, uiSize, NULL, NULL); \
            } __except(1) { \
                dest = NULL; \
            } \
        } \
    }



        
HRESULT AddFileShare(const WCHAR *_pName, 
                 const WCHAR *_pPath, 
                 const WCHAR *_pACL,
                 const WCHAR *_pROACL) 
{
    ASSERT(TRUE == g_fFileServer);    
    HRESULT hr = E_FAIL;
    Share *pFileShare = NULL;   

    //
    // Verify incoming params
    if(NULL == _pName || 
       NULL == _pPath) {
       TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - Invalid Params")));
       ASSERT(FALSE);
       goto Done;
    }

    
    //add file as a share
    if(NULL == (pFileShare = new Share())) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }
    
    pFileShare->SetDirectoryName(_pPath);
    pFileShare->SetShareName(_pName);
    pFileShare->SetShareType(STYPE_DISKTREE);
    pFileShare->SetServiceName(L"A:");
    pFileShare->SetACL(_pACL, _pROACL);
    pFileShare->SetRequireAuth(TRUE); 
        
    
    if(FAILED(hr = SMB_Globals::g_pShareManager->AddShare(pFileShare))) {  
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add default share!")));  
        delete pFileShare;
        goto Done;
    }

    hr = S_OK;
    Done:
        return hr;
}

       

HRESULT AddPrintSpool(const  WCHAR *_pName, 
                   const WCHAR *_pDriverName, 
                   const WCHAR *_pProc,
                   const WCHAR *_pComment, 
                   const WCHAR *_pDirectoryName,
                   const WCHAR *_pShareName,
                   const WCHAR *_pServiceName,
                   const WCHAR *_pRemark,
                   const WCHAR *_pPortName,
                   const WCHAR *_pACL)
                   
{
    SMBPrintQueue *pPrintQueue;
    Share *pPrinterShare;   
    HRESULT hr = E_FAIL;

    CHAR *pName;
    CHAR *pDriverName;
    CHAR *pProc;
    CHAR *pComment;
    CHAR *pRemark;

    TOASCII(_pName, pName);
    TOASCII(_pDriverName, pDriverName);
    TOASCII(_pProc, pProc);
    TOASCII(_pComment, pComment);
    TOASCII(_pRemark, pRemark);
    
    if(NULL == _pName || 
       NULL == _pDriverName || 
       NULL == _pProc || 
       NULL == _pComment || 
       NULL == _pRemark) {
       TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - Error doing ASCII conversion"))); 
       goto Done;
    }

    //
    
    if(NULL == (pPrintQueue = new SMBPrintQueue())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create printqueue")));     
        hr = E_OUTOFMEMORY;
        goto Done;
    }
      
    //
    // Add all printers connected to the mix
    pPrintQueue->SetStartTime(SecSinceJan1970_0_0_0());
    pPrintQueue->SetName(pName);
    pPrintQueue->SetDriverName(pDriverName);
    pPrintQueue->SetPrProc(pProc);    
    pPrintQueue->SetSepFile("");
    pPrintQueue->SetParams("");
    pPrintQueue->SetComment(pComment);
    pPrintQueue->SetPriority(1);  
    pPrintQueue->SetPortName(_pPortName);    
        
    
    //add \printer as a share
    if(NULL == (pPrinterShare = new Share())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create shares")));  
        hr = E_OUTOFMEMORY;
        goto Done;
    }
    
    pPrinterShare->SetDirectoryName(_pDirectoryName);
    pPrinterShare->SetShareName(_pShareName);
    pPrinterShare->SetServiceName(_pServiceName);
    pPrinterShare->SetRemark(pRemark);
    pPrinterShare->SetShareType(STYPE_PRINTQ);
    pPrinterShare->SetPrintQueue(pPrintQueue); 
    pPrinterShare->SetRequireAuth(TRUE); 
    pPrinterShare->SetACL(_pACL, L"");
        
    if(FAILED(hr = SMB_Globals::g_pShareManager->AddShare(pPrinterShare))) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add default share!")));       
    }

    hr = S_OK;
    Done:
        return hr;
}


HRESULT AddShare(const WCHAR *ShareName) 
{
    HRESULT hr = E_FAIL;
    CReg ShareKey;
    WCHAR Path[MAX_PATH];
    TRACEMSG(ZONE_INIT, (L"Loading Share: %s", ShareName));
    WCHAR RegPath[MAX_PATH];
    WCHAR RegDriver[MAX_PATH];
    WCHAR RegComment[MAX_PATH];       
    WCHAR ACL[MAX_PATH];
    WCHAR ROACL[MAX_PATH];
    DWORD dwType;
    DWORD dwRegType;
    DWORD dwLen = sizeof(DWORD);
   
    
    
    swprintf(Path, L"Services\\SMBServer\\Shares\\%s", ShareName);
    RETAILMSG(1, (L"SMB_PRINT: Loading Registry: %s\n", Path));
    if(FALSE == ShareKey.Open(HKEY_LOCAL_MACHINE, Path)) {
        RETAILMSG(1, (L"   -- Cant Open KEY %s\n", Path));
        goto Done;
    }        
    
    //
    // Get the type 
    if(ERROR_SUCCESS != RegQueryValueEx(ShareKey, L"Type", 0, &dwRegType, (LPBYTE)&dwType, &dwLen) ||
       REG_DWORD != dwRegType ||
       sizeof(DWORD) != dwLen) {
        RETAILMSG(1, (L"   -- Cant Open TYPE on %s\n", Path));
        goto Done;
    }        
    RETAILMSG(1, (L"   -- Type: %d\n", dwType));
    
    
    //
    // Only let file shares in if we are linked up with the file server
    //    code -- otherwise just do print
    if(FALSE == (g_fFileServer && dwType == STYPE_DISKTREE) && 
       dwType != STYPE_PRINTQ) {
        RETAILMSG(1, (L"   -- unknown type!! -- we only support printers"));
        goto Done;
    }
    
    //
    // Get the Path
    if(FALSE == ShareKey.ValueSZ(L"Path", RegPath, MAX_PATH/sizeof(WCHAR))) {
        RETAILMSG(1, (L"   -- Cant get PATH\n"));
        goto Done;
    }
    RETAILMSG(1, (L"   -- Path: %s\n", RegPath));
           
    //
    // Get the ACL
    if(FALSE == ShareKey.ValueSZ(L"UserList", ACL, MAX_PATH/sizeof(WCHAR))) {
        RETAILMSG(1, (L"    --  NOT USING ACL's!\n"));
        ACL[0] = NULL;
    }
    RETAILMSG(1, (L"   -- ACL: %s\n", ACL));       

    if(FALSE == ShareKey.ValueSZ(L"ROUserList", ROACL, MAX_PATH/sizeof(WCHAR))) {
        RETAILMSG(1, (L"    --  NOT USING READ-ONLY ACL's!\n"));
        ROACL[0] = NULL;
    }
    RETAILMSG(1, (L"   -- Read-ONLYACL: %s\n", ROACL));      

   
    // 
    // Get printer specific information
    if(dwType == STYPE_PRINTQ)
    {
        //
        // Get the Driver (printer specific)
        if(FALSE == ShareKey.ValueSZ(L"Driver", RegDriver, MAX_PATH/sizeof(WCHAR))) {
            RETAILMSG(1, (L"   -- Cant get Driver\n"));
            RegDriver[0] = NULL;
        }
        RETAILMSG(1, (L"   -- Driver: %s\n", RegDriver));
        
        //
        // Get the Comment (printer specific)
        if(FALSE == ShareKey.ValueSZ(L"Comment", RegComment, MAX_PATH/sizeof(WCHAR))) {
            RETAILMSG(1, (L"   -- Cant get Comment\n"));
            RegComment[0] = 0;
        }
        RETAILMSG(1, (L"   -- Comment: %s\n", RegComment));            
         
         
        //
        // Load up the driver
        AddPrintSpool(ShareName, 
                       RegDriver,
                       L"WinPrint",
                       RegComment, 
                       L"PRINTER_DIR",
                       ShareName,
                       L"LPT1:",
                       RegComment,
                       RegPath,
                       ACL);
        
    }else if (dwType == STYPE_DISKTREE) {
       //
       //  Add the fileshare
       ASSERT(TRUE == g_fFileServer);
       AddFileShare(ShareName, RegPath, ACL, ROACL);       
    }

    hr = S_OK;
    Done:
        return hr;
}

HRESULT AddShares()
{
    CReg RegShares;
    WCHAR ShareName[MAX_PATH];
    BOOL fFoundSomething = FALSE;
    Share *pIPCShare = NULL;

    //
    // Add IPC Share
    if(NULL == (pIPCShare = new Share())) {
        return E_OUTOFMEMORY;
    }
    pIPCShare->SetDirectoryName(L"IPC$");
    pIPCShare->SetShareName(L"IPC$");
    pIPCShare->SetServiceName(L"IPC");
    pIPCShare->SetRequireAuth(FALSE); //let anyone in
    pIPCShare->SetRemark("Remote Inter Process Communication");
    pIPCShare->SetShareType(STYPE_IPC);
    pIPCShare->SetPrintQueue(NULL);
    
    if(FAILED(SMB_Globals::g_pShareManager->AddShare(pIPCShare))) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add IPC$ share!")));
        goto Done;
    }

    
    //
    // Load the CName from the registry
    if(FALSE == RegShares.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
        RETAILMSG(1, (L"No registy key under Services\\SMBServer\\Shares"));
    }
    
    while(TRUE == RegShares.EnumKey(ShareName, MAX_PATH)) {
        if(SUCCEEDED(AddShare(ShareName))) {
            fFoundSomething = TRUE;
        }
    }

    Done:
    if(fFoundSomething)
        return S_OK;    
    else  
        return E_FAIL;
}

DWORD
SMB_Init(
    DWORD dwContext
    )
{
    DWORD dwRet = 0;
    CReg CRCName; 
    CReg CRSettings;
    CReg RegAllowAll;    
    DWORD dwType;
    DWORD dwLen;
    DWORD dwSetting;
    WCHAR MyCName[16];
    WCHAR OrigName[16];
    WCHAR wszWorkGroup[MAX_PATH];
    BOOL fAllowAll = FALSE;
    
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Init\n")));
    UINT i;

   
    //
    // Print a RETAIL message if passwords are disabled
    if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
        fAllowAll = !(RegAllowAll.ValueDW(L"UseAuthentication", TRUE));
        if(TRUE == fAllowAll) {
           RETAILMSG(1, (L"SMB_SRV: **PASSWORDS DISABLED**"));          
        }
    }   
       
    //    
    // Initialize any globals
    //
    // First get the server GUID from registry (if it doesnt exist, create it)
    if(FALSE == CRSettings.OpenOrCreateRegKey(HKEY_LOCAL_MACHINE, 
                                              L"Services\\SMBServer",0)) {
        RETAILMSG(1, (L"SMB_SRVR: Error opening up SMBPrint reg key!\n"));
        dwRet = 0;       
        goto Done;
    }

    if(FALSE == CRSettings.ValueSZ(L"AdapterList", SMB_Globals::g_AdapterAllowList, MAX_PATH / sizeof(WCHAR))) {
        RETAILMSG(1, (L"Name value under HKLM\\Services\\AdapterList is not there\n"));
        SMB_Globals::g_AdapterAllowList[0] =  NULL;        
    }
    
    dwLen = sizeof(SMB_Globals::g_ServerGUID);
    if(ERROR_SUCCESS != RegQueryValueEx(CRSettings, 
                                         L"GUID", 
                                         NULL,
                                         &dwType, 
                                         SMB_Globals::g_ServerGUID, 
                                         &dwLen) || (16 != dwLen) || (REG_BINARY != dwType)) 
    {
        GUID srvrGUID;                             
        TRACEMSG(ZONE_INIT, (L"SMB_SRV: Server GUID not specified in registry -- creating a new one"));
        GenerateGUID(&srvrGUID);

        memcpy(&(SMB_Globals::g_ServerGUID[0]), (BYTE *)(&srvrGUID.Data1), 4);
        memcpy(&(SMB_Globals::g_ServerGUID[4]), (BYTE *)(&srvrGUID.Data2), 2);
        memcpy(&(SMB_Globals::g_ServerGUID[6]), (BYTE *)(&srvrGUID.Data3), 2);
        memcpy(&(SMB_Globals::g_ServerGUID[8]),  srvrGUID.Data4, 8);

        if(TRUE != CRSettings.SetBinary(L"GUID", SMB_Globals::g_ServerGUID, sizeof(SMB_Globals::g_ServerGUID))) {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV: Cant write GUID to registry -- we will continue, but this isnt good"));
            ASSERT(FALSE);            
        }
    }

    if (ERROR_SUCCESS != RegQueryValueEx(
                                      CRSettings,
                                      L"SmbServerNameHardeningLevel",
                                      NULL,
                                      &dwType,
                                      (LPBYTE)&dwSetting,
                                      &dwLen))
    {
        TRACEMSG(ZONE_INIT, (L"SMB_SRV: Name value under HKLM\\Services\\SMBServer\\SmbServerNameHardeningLevel is not there."));
    TRACEMSG(ZONE_INIT, (L"SMB_SRV: Server hardening level is not specified in registry -- using NO HARDENED."));

    SmbServerNameHardeningLevel = SmbServerNameNoCheck;
    
    }
    else
    {
         if ((dwSetting == SmbServerNamePartialCheck) ||
        (dwSetting == SmbServerNameFullCheck))
         {
            SmbServerNameHardeningLevel = (SERVERNAME_HARDENING_LEVEL)dwSetting;
         }
    }

    InitializeCriticalSection(&SMB_Globals::SrvAdminNameLock);
    
    if(FALSE == CRSettings.ValueSZ(L"workgroup", wszWorkGroup, sizeof(wszWorkGroup)/sizeof(WCHAR))) {
        RETAILMSG(1, (L"SMB_SRV: Workgroup reg value not set -- using WORKGROUP\n"));
        sprintf(SMB_Globals::g_szWorkGroup, "WORKGROUP");        
    } else {
        BYTE *pWorkGroup = NULL;
        UINT uiSize;
        StringConverter SCWorkGroup;
        SCWorkGroup.append(wszWorkGroup);
        pWorkGroup = SCWorkGroup.NewSTRING(&uiSize, FALSE);
        if(NULL == pWorkGroup) {
            dwRet = 0;
            ASSERT(FALSE);
            goto Done;                 
        }
        strcpy(SMB_Globals::g_szWorkGroup, (CHAR *)pWorkGroup);
        LocalFree(pWorkGroup);              
    }


    //
    // Set MaxConnections
    SMB_Globals::g_uiMaxConnections = CRSettings.ValueDW(L"MaxConnections", 5);
               
    
#ifdef DEBUG
    SMB_Globals::g_PacketID = 0;    
#endif

    // 
    // Make sure the netbios transport is initialized (because we may start listening for name changes etc)
    InitNetbiosTransport();


    //
    // Load the CName from the registry
    if(FALSE == CRCName.Open(HKEY_LOCAL_MACHINE, L"Ident")) {        
        RETAILMSG(1, (L"No registy key under HKLM\\Ident"));
        dwRet = SMB_CONTEXT;
        goto Done;
    }
    if(FALSE == CRCName.ValueSZ(L"Name", MyCName, sizeof(MyCName)/sizeof(WCHAR))) {
        RETAILMSG(1, (L"Name value under HKLM\\Ident is either too large (15 chars) or isnt present -- stopping server"));
        dwRet = SMB_CONTEXT;
        goto Done;
    }
    if(FALSE == CRCName.ValueSZ(L"OrigName", OrigName, sizeof(OrigName)/sizeof(WCHAR))) {
        RETAILMSG(1, (L"OrigName value under HKLM\\Ident is either too large (15 chars) or isnt present -- stopping server"));
        dwRet = SMB_CONTEXT;
        goto Done;        
    }
    if(0 == wcscmp(MyCName, OrigName)) {
        RETAILMSG(1, (L"OrigName = Name value under HKLM\\Ident -- stopping server to prevent multiple machine with the same name"));
        dwRet = SMB_CONTEXT;     
        
        // 
        // In the case where names arent the same start up the name listener so if they become okay we will start again    
        StartListenOnNameChange();
        goto Done;
    }   
    
    if(0 == WideCharToMultiByte(CP_ACP, 0, MyCName, -1, SMB_Globals::CName, sizeof(SMB_Globals::CName),NULL,NULL)) {
        TRACEMSG(ZONE_ERROR, (L"Conversion of CName (%s) failed!!!", MyCName));
        ASSERT(FALSE);
        dwRet = 0;
        goto Done;
    }
    //make sure its upper case
    for(i=0; i<sizeof(SMB_Globals::CName); i++) {
        SMB_Globals::CName[i] = toupper(SMB_Globals::CName[i]);
    }
    
    RETAILMSG(1, (L"SMB_SRV: Registering CName %s", MyCName));

    //
    // Create a abstract filesystem (since CE doesnt have locking)
    SMB_Globals::g_pAbstractFileSystem = new AbstractFileSystem();
    if(NULL == SMB_Globals::g_pAbstractFileSystem) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create abstract file system")));
        dwRet = 0;
        goto Done;
    }
    
    //
    // Start the master share list
    SMB_Globals::g_pShareManager = new ShareManager();
    if(NULL == SMB_Globals::g_pShareManager) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create shares")));
        dwRet = 0;
        goto Done;
    }
    
    //
    // Start the TID Manager
    SMB_Globals::g_pConnectionManager = new ConnectionManager();
    if(NULL == SMB_Globals::g_pConnectionManager) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create connection manager")));
        dwRet = 0;
        goto Done;
    }

    //
    // Load (and add) all shares from the registry
    if(FAILED(AddShares())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - adding shares failed!")));
        dwRet = SMB_CONTEXT;
        goto NoShares;
    }       
    
    //
    // Start up the packet cracker
    if(FAILED(StartCracker())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed")));
        dwRet = 0;
        goto Done;
    }

    //
    // Start up the print spool
    if(FAILED(StartPrintSpool())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartPrintSpool failed")));
        dwRet = 0;
        goto Done;
    }

    //
    // Start up protocol transports 
    if(FAILED(StartTCPTransport())) {  //TCP must be started BEFORE Netbios!
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer on TCP failed")));
        ASSERT(FALSE);
        dwRet = 0;
        goto Done;
    }
    if(FAILED(StartNetbiosTransport())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer on NETBsdIOS failed")));
        ASSERT(FALSE);
        dwRet = 0;
        goto Done;
    }

    dwRet = SMB_CONTEXT;
    g_fServerRunning = TRUE;
    Done:
        if(0 == dwRet) {
    NoShares:    
                TRACEMSG(ZONE_ERROR, (TEXT("SMB_Init -- an error occured during init... cleaning up")));

                //
                // Stop transports (NOTE: this is okay to do even if they were not inited!!
                if(FAILED(StopTCPTransport())) {
                    TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init -- couldnt stop TCP transport!!")));
                    ASSERT(FALSE);
                }
                if(FAILED(StopNetbiosTransport())) {
                    TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init -- couldnt stop NETBIOS transport!!")));
                    ASSERT(FALSE);
                }
                
                //
                // Stop print spool
                if(FAILED(StopPrintSpool())) {
                    TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init -- couldnt stop print spool!!")));
                    ASSERT(FALSE);
                }
                
                //
                // Stop the cracker
                if(FAILED(StopCracker())) {
                    TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init -- couldnt stop cracker!!")));
                    ASSERT(FALSE);
                }
                
                //
                // Clean up memory
                if(NULL != SMB_Globals::g_pShareManager) {
                    delete SMB_Globals::g_pShareManager;
                    SMB_Globals::g_pShareManager = NULL;
                }  
                if(NULL != SMB_Globals::g_pConnectionManager) {
                    delete SMB_Globals::g_pConnectionManager;
                    SMB_Globals::g_pConnectionManager = NULL;
                }
                if(NULL != SMB_Globals::g_pWakeUpOnEvent) {
                    delete SMB_Globals::g_pWakeUpOnEvent;
                    SMB_Globals::g_pWakeUpOnEvent = NULL;
                }
                if(NULL != SMB_Globals::g_pAbstractFileSystem) {
                    delete SMB_Globals::g_pAbstractFileSystem;
                    SMB_Globals::g_pAbstractFileSystem = NULL;
                }
        }
        
        TRACEMSG(ZONE_INIT,(TEXT("-SMB_Init")));
        return dwRet;
}


DWORD
SMB_Deinit(
    DWORD dwClientContext
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Deinit")));   
    BOOL fSuccess = TRUE;
       
    //
    // Halt all existing TCP connections (and dont allow for new ones)
    if(FAILED(HaltTCPIncomingConnections())) {
        TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Closing TCP transport incoming connections failed")));
        fSuccess = FALSE;
        ASSERT(FALSE);
    }
    
    //
    // Halt all existing NETBIOS connections (and dont allow for new ones)
    if(FAILED(StopNetbiosTransport())) {
        TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Closing NETBIOS transport failed")));
        fSuccess = FALSE;
        ASSERT(FALSE);
    }
      
    //
    // Stop the cracker -- this will flush all existing packets
    TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Halt SMB Cracker")));
    if(FAILED(StopCracker())) {
        TRACEMSG(ZONE_INIT, (L"-SMB_Deinit -- error stopping cracker!!"));
        fSuccess = FALSE;
        ASSERT(FALSE);
    }         
              
    //
    // Now shut off the transport threads  
    TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Halting TCP transport")));
    if(FAILED(StopTCPTransport())) {
        TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Closing TCP transport failed")));
        fSuccess = FALSE;
        ASSERT(FALSE);
    }
           
      
    
    TRACEMSG(ZONE_INIT, (TEXT("-SMB_Deinit - Deleting memory")));
    
    //
    // Delete all connection information
    //   The ShareManager needs to be around still!  
    if(SMB_Globals::g_pConnectionManager) {
        delete SMB_Globals::g_pConnectionManager;
        SMB_Globals::g_pConnectionManager = NULL;
    } 
        
    //
    // Stop print spool
    if(FAILED(StopPrintSpool())) {
       TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init -- couldnt stop print spool!!")));
       ASSERT(FALSE);
    }
    
    //
    // Delete all share information (print spool must be down by now
    //   because the share binds to the print spool and blocks on its termination)
    if(SMB_Globals::g_pShareManager) {
       delete SMB_Globals::g_pShareManager;
       SMB_Globals::g_pShareManager = NULL;
    } 
    
    //
    // Delete wakeup event object iff the namechange notification handle isnt alive
    if(SMB_Globals::g_pWakeUpOnEvent && !NETBIOS_TRANSPORT::NameChangeNotification::h) {
        delete SMB_Globals::g_pWakeUpOnEvent;
        SMB_Globals::g_pWakeUpOnEvent = NULL;
    }
    
    //
    // Delete the abstract file system
    if(NULL != SMB_Globals::g_pAbstractFileSystem) {
        delete SMB_Globals::g_pAbstractFileSystem;
        SMB_Globals::g_pAbstractFileSystem = NULL;
    }
            
    // 
    // Delete the printers memmapped buffers
    if(FAILED(SMB_Globals::g_PrinterMemMapBuffers.Close())) {
        TRACEMSG(ZONE_ERROR, (L"SMB_PRINT: Error closing swap buffer!"));
        ASSERT(FALSE);
    }   

    // 
    // Destroy any netbios memory
    DestroyNetbiosTransport();

    DeleteCriticalSection(&SMB_Globals::SrvAdminNameLock);
    
    g_fServerRunning = FALSE;
    TRACEMSG(ZONE_INIT,(TEXT("-SMB_Deinit")));
    return fSuccess;
}


DWORD
SMB_Open(
    DWORD dwClientContext,
    DWORD dwAccess,
    DWORD dwShareMode              
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Open")));
    return SMB_CONTEXT;
}


BOOL
SMB_Close(
    DWORD dwOpenContext
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Close")));
    return 0;
}


DWORD
SMB_Read(
    DWORD dwOpenContext,
    LPVOID pBuffer,
    DWORD dwNumBytes
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Read")));
    return 0;
}


DWORD
SMB_Write(
    DWORD dwOpenContext,
    LPCVOID pBuffer,
    DWORD dwNumBytes
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Write")));
    return 0;
}


DWORD
SMB_Seek(
    DWORD dwOpenContext,
    long lDistance,
    DWORD dwMoveMethod
    )
{
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Seek")));
    return 0;
}

void SMB_PowerUp(void)  {return;}
void SMB_PowerDown(void){return;}

HRESULT IOCTL_Change_ACL(const WCHAR *pShareName, const WCHAR *pACL, const WCHAR *pROACL)
{
    CReg ShareKey;
    HRESULT hr = E_FAIL;
    
    StringConverter ShareName;
    ShareName.append(L"Services\\SMBServer\\Shares\\");
    ShareName.append(pShareName); 
    
    if(FALSE == ShareKey.Open(HKEY_LOCAL_MACHINE, ShareName.GetString())) {            
        RETAILMSG(1, (L"SMBSERVER: Error -- cant change ACL because %s isnt a valid share", pShareName));
        goto Done;
    }      
    if(pACL && FALSE == ShareKey.SetSZ(L"UserList", pACL, wcslen(pACL))) {
        RETAILMSG(1, (L"SMBSERVER: Error -- cant change ACL because %s userlist cant be set", pShareName));      
        goto Done;
    }
    if(pROACL && FALSE == ShareKey.SetSZ(L"ROUserList", pROACL, wcslen(pROACL))) {   
        RETAILMSG(1, (L"SMBSERVER: Error -- cant change ACL because %s ROUserList cant be set", pShareName));  
        goto Done;
    }
    
    if(TRUE == g_fServerRunning && NULL != SMB_Globals::g_pShareManager) {
        SMB_Globals::g_pShareManager->ReloadACLS();
    }

    hr = S_OK;
    Done:
           return hr;
}

HRESULT IOCTL_Add_Share(const WCHAR *pName, DWORD dwType, const WCHAR *pPath, const WCHAR *pACL, const WCHAR *pROACL, const WCHAR *pDriver, const WCHAR *pComment)
{
    HRESULT hr;
    CReg regPaths;

    if(0 != wcsstr(pPath, L"..")) {       
        hr = E_FAIL;
        goto Done;
    }
   
    if(regPaths.OpenOrCreateRegKey(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\ExcludePaths")) {
        WCHAR wcName[100];
        WCHAR wcPath[500];
        
        while(regPaths.EnumValue(wcName, sizeof(wcName)/sizeof(WCHAR), wcPath, sizeof(wcPath))) {
            if(0 == _memicmp(pPath, wcPath, wcslen(wcPath))) {
                RETAILMSG(1, (L"SMBSERVER: Error adding share with path name %s because that path is blocked", pPath));
                hr = E_FAIL;
                goto Done;
            }
        }
    }
    
    if(SUCCEEDED(hr = CreateShare(pName, dwType, pPath, pACL, pROACL, pDriver, pComment))) {  
        if(FALSE == g_fServerRunning) {                   
            SMB_RestartServer();
        } else {
            AddShare(pName);
        }
    }   

    Done:
        return hr;
}

HRESULT IOCTL_Del_Share(const WCHAR *pName)
{
    // NOTE: its safe to search for this share and use it w/o CritSection b/c we are the
    //   only person that ever deletes shares
    Share *pMyShare = NULL;

    if(g_fServerRunning) {
        pMyShare = SMB_Globals::g_pShareManager->SearchForShare((WCHAR *)pName);
    }
    
    ce::wstring RegPath = L"Services\\SMBServer\\Shares\\";
    RegPath.append((WCHAR *)pName);
    HRESULT hr = E_FAIL;
   
    if(g_fServerRunning && NULL != pMyShare) {                  
            if(SUCCEEDED(SMB_Globals::g_pConnectionManager->TerminateTIDsOnShare(pMyShare))) {
               SMB_Globals::g_pShareManager->DeleteShare(pMyShare);      
               if(ERROR_SUCCESS != RegDeleteKey(HKEY_LOCAL_MACHINE, RegPath)) {
                   RETAILMSG(1, (L"SMBSRV:  Share %s doesnt exist in the registry!", (WCHAR *)(RegPath.get_buffer())));
               } 
               hr = S_OK;
            }                    
    } else if(!g_fServerRunning) { 
      if(ERROR_SUCCESS != RegDeleteKey(HKEY_LOCAL_MACHINE, RegPath)) {
          RETAILMSG(1, (L"SMBSRV:  Share %s doesnt exist in the registry!", (WCHAR *)(RegPath.get_buffer())));
      } else {
          SMB_RestartServer();
          hr = S_OK;
      }
    }        
    return hr;
}

HRESULT IOCTL_List_Connected_Users(WCHAR *pBuffer, UINT *puiLen)
{
  ce::wstring WhosOn = L"";
  
  if(g_fServerRunning && SMB_Globals::g_pConnectionManager) {
      SMB_Globals::g_pConnectionManager->ListConnectedUsers(WhosOn);
  }
  if(*puiLen < (sizeof(WCHAR) * (WhosOn.length()+2))) {
      *puiLen = sizeof(WCHAR) * (WhosOn.length()+2);
      return E_OUTOFMEMORY;
  }
  memcpy(pBuffer, WhosOn.get_buffer(), sizeof(WCHAR) * (WhosOn.length()+2));
  return S_OK;  
}

HRESULT IOCTL_QueryAmountTransfered(LARGE_INTEGER *pRead, LARGE_INTEGER *pWritten)
{
  CCritSection csLock(&SMB_Globals::g_Bookeeping_CS);
  csLock.Lock();
  pRead->QuadPart = SMB_Globals::g_Bookeeping_TotalRead.QuadPart;
  pWritten->QuadPart = SMB_Globals::g_Bookeeping_TotalWritten.QuadPart;
  return S_OK;  
}

BOOL IOCTL_NetUpdateAliases(const WCHAR *pBuffer,
                                    DWORD uiLen)
                                    
/*++

   Routine Description:

       used to update server administration aliases list.

   Arguments:

          pBuffer -names need to be saved in server administration aliases list. it should be formated as name1\0name2\0name3
          uiLen - length of pBuffer in byte.

   Return Values:

          TRUE/FALSE.

   Notes:

       None.

--*/

{
    UNICODE_STRING Aliases = {0};
    UNICODE_STRING localHost = {18 ,18, L"LocalHost"};
    NTSTATUS status;

    if (uiLen == 0)
    {
        return FALSE;
    }

    Aliases.Buffer = (PWCHAR)(const WCHAR*)pBuffer;
    Aliases.Length = Aliases.MaximumLength = uiLen;

    status = SrvAdminAllocateNameList(
                    &Aliases,
                    &localHost,
                    &SMB_Globals::SrvAdminNameLock,
                    &SMB_Globals::SrvAdminServerNameList
                    );

    if (STATUS_SUCCESS == status)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL IOCTL_NetUpdateIpAddress(const WCHAR *pBuffer,
                                        DWORD uiLen)
/*++

   Routine Description:

      Used to update server administration aliases list.

   Arguments:

      pBuffer - names need to be saved in server administration Ip address list. It should be formated as name1\0name2\0name3
      uiLen - length of pBuffer in byte.

   Return Values:

      TRUE/FALSE

   Notes:

      None.

 --*/
{
    UNICODE_STRING IPAddr = {0};
    NTSTATUS status;

    if (uiLen == 0)
    {
        return FALSE;
    }

    IPAddr.Buffer = (PWCHAR)(const WCHAR*)pBuffer;
    IPAddr.Length = IPAddr.MaximumLength = uiLen;

    status = SrvAdminAllocateNameList(
                                &IPAddr,
                                NULL,
                                &SMB_Globals::SrvAdminNameLock,
                                &SMB_Globals::SrvAdminIpAddressList
                                );

    if (STATUS_SUCCESS == status)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}



BOOL
SMB_IOControl(
    DWORD dwOpenContext,
    DWORD dwIoControlCode,
    PBYTE pInBuf,
    DWORD nInBufSize,
    PBYTE pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned
    )
{
    BOOL fRet = 1;
    CCritSection csLock(&g_csDriverLock);
    csLock.Lock();

    
    switch(dwIoControlCode) { 
        case IOCTL_SERVICE_REFRESH:
            SMB_RestartServer();
            break;
        case SMB_IOCTL_INVOKE: {
            ce::psl_stub<> stub(pInBuf, nInBufSize);

            switch(stub.function()) {
                case IOCTL_CHANGE_ACL:
                    SetLastError(stub.call(IOCTL_Change_ACL));                    
                    break;
                case IOCTL_ADD_SHARE:
                    SetLastError(stub.call(IOCTL_Add_Share));
                    break;
                case IOCTL_DEL_SHARE:
                    SetLastError(stub.call(IOCTL_Del_Share));
                    break;
                case IOCTL_LIST_USERS_CONNECTED:
                    SetLastError(stub.call(IOCTL_List_Connected_Users));
                    break;
                case IOCTL_QUERY_AMOUNT_TRANSFERED:
                    SetLastError(stub.call(IOCTL_QueryAmountTransfered));
                    break;
              case IOCTL_NET_UPDATE_ALIASES:
                fRet = stub.call(IOCTL_NetUpdateAliases);
                break;
               case IOCTL_NET_UPDATE_IP_ADDRESS:
                fRet = stub.call(IOCTL_NetUpdateIpAddress);
                break;                  
            }
            break;
        }
      /*  case IOCTL_SET_MAX_CONNS:
            if(sizeof(UINT) == nInBufSize) { 
                CReg reg;               
                if(reg.OpenOrCreateRegKey(HKEY_LOCAL_MACHINE, L"Services\\SMBServer")) {
                   SMB_Globals::g_uiMaxConnections = *((UINT *)pInBuf);
                   reg.SetDW(L"MaxConnections", SMB_Globals::g_uiMaxConnections);
                   TRACEMSG(ZONE_INIT, (L"SMB_SRV: Set Max Connections to: %d", SMB_Globals::g_uiMaxConnections));
                } else {
                    TRACEMSG(ZONE_ERROR, (L"SMB_SRV: SMB registry not present!"));
                    ASSERT(FALSE);
                }                
            }
            break;
        */       
         
        case IOCTL_SERVICE_STOP:
            TRACEMSG(ZONE_INIT,(L"+SMB_IOControl, STOP IOCTL"));
            SMB_Deinit(0);
            break;
        case IOCTL_SERVICE_START:
            TRACEMSG(ZONE_INIT,(L"+SMB_IOControl, START IOCTL"));
            SMB_Init(0);
            break;  
        #ifdef DEBUG
        case IOCTL_DEBUG_PRINT:
            SMB_Globals::g_pConnectionManager->DebugPrint();
            break;
        #endif
        default:
            TRACEMSG(ZONE_INIT,(TEXT("+SMB_IOControl, unknown code 0x%X"),dwIoControlCode));
            break;
    }
    return fRet; //0=failure, nonzero=success
}


DWORD SMBSRVR_RestartServerThread(LPVOID netnum)
{
    CCritSection csLock(&g_csDriverLock);
    csLock.Lock();
    SMB_Deinit(0);
    SMB_Init(0); 
    return 0;
}


VOID SMB_RestartServer()
{
    HANDLE h;
    if(NULL == (h = CreateThread(NULL, 0, SMBSRVR_RestartServerThread, 0, 0, NULL))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: cant make restart thread"));
        ASSERT(FALSE);
        return;
    }
    CloseHandle(h);
}

extern "C"
BOOL WINAPI DllMain(IN HANDLE DllHandle,
                    IN ULONG Reason,
                    IN PVOID Context OPTIONAL)
{
    switch(Reason) {

        case DLL_PROCESS_ATTACH:
            TRACEMSG(ZONE_INIT,(TEXT("SMBSRV: DLL_PROCESS_ATTACH, hInst:0x%X"), DllHandle));
            InitializeCriticalSection(&g_csDriverLock);
            DEBUGREGISTER((HINSTANCE)DllHandle);  
            SMB_Globals::g_Bookeeping_TotalRead.QuadPart = 0;
            SMB_Globals::g_Bookeeping_TotalWritten.QuadPart = 0;  
            InitializeCriticalSection(&SMB_Globals::g_Bookeeping_CS);
            return TRUE;

        case DLL_PROCESS_DETACH:
            TRACEMSG(ZONE_INIT,(TEXT("SMBSRV: DLL_PROCESS_DETACH")));
            DeleteCriticalSection(&g_csDriverLock);
            DeleteCriticalSection(&SMB_Globals::g_Bookeeping_CS);
            return TRUE;
        
        case DLL_THREAD_ATTACH:    // A new thread has been created
        case DLL_THREAD_DETACH:    // Thread has exited
            // No processing for these
            return TRUE;
            
        default:
            TRACEMSG(ZONE_ERROR,(TEXT("SMBSRV:DllEntry: Invalid reason #%d"), Reason));
            return FALSE;
            break;
    }
    return TRUE;
}


HRESULT CreateShare(const WCHAR *pName, 
                    DWORD dwType, 
                    const WCHAR *pPath, 
                    const WCHAR *pACL,
                    const WCHAR *pROACL,
                    const WCHAR *pDriver, 
                    const WCHAR *pComment)
{    
    ce::wstring RegPath = L"Services\\SMBServer\\Shares\\";    
    HRESULT hr = E_FAIL;
    CReg reg;

    if(NULL == pName) {
        goto Done;
    }

    RegPath += pName;
    if(FALSE == reg.OpenOrCreateRegKey(HKEY_LOCAL_MACHINE, RegPath)) {
        goto Done;
    }

    //
    // These values are for all share typessd 
    if(FALSE == reg.SetDW(L"Type", dwType)) {
        goto Done;
    }
    if(NULL != pPath && FALSE == reg.SetSZ(L"Path", pPath)) {
        goto Done;
    }
    if(NULL != pACL && FALSE == reg.SetSZ(L"UserList", pACL)) {
       goto Done;
    }
    if(NULL != pROACL && FALSE == reg.SetSZ(L"ROUserList", pROACL)) {
       goto Done;
    }

    if(dwType == STYPE_PRINTQ) {
        if(NULL != pDriver && FALSE == reg.SetSZ(L"Driver", pDriver)) {
            goto Done;
        }
        if(NULL != pComment && FALSE == reg.SetSZ(L"Comment", pComment)) {
           goto Done;
        }
    } else if(dwType == STYPE_DISKTREE) {
    } else {
        RETAILMSG(1, (L"SMBSRV: Error Creating Share! unknown type!"));
        ASSERT(FALSE);
        goto Done;
    }
    
    hr = S_OK;
    Done:
        return hr;
}

