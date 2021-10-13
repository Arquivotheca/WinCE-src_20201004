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
#include <service.h>
#include <GUIDGen.h>

#include "SMB_Globals.h"
#include "utils.h"
#include "NetbiosTransport.h"
#include "TCPTransport.h"
#include "ShareInfo.h"
#include "PrintQueue.h"
#include "Cracker.h"
#include "FileServer.h"
#include "ConnectionManager.h"
#include "FixedAlloc.h"


// Stream interface functions
#define SMB_CONTEXT  0xABCDEF01

//
// Global that will call into SVS Utils init etc (any construction that must happen first)
InitClass g_InitClass;
ThreadSafePool<10, SMB_PACKET>   SMB_Globals::g_SMB_Pool;

CRITICAL_SECTION g_csDriverLock;
extern HRESULT StartListenOnNameChange();

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
        
VOID AddFileShare(WCHAR *_pName, 
                 WCHAR *_pPath, 
                 WCHAR *_pACL) 
{
    ASSERT(TRUE == g_fFileServer);    
 
    Share *pFileShare = NULL;   

    //
    // Verify incoming params
    if(NULL == _pName || 
       NULL == _pPath) {
       TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - Error doing ASCII conversion"))); 
       return;
    }

    
    //add file as a share
    if(NULL == (pFileShare = new Share())) {
        goto Done;
    }
    
    pFileShare->SetDirectoryName(_pPath);
    pFileShare->SetShareName(_pName);
    pFileShare->SetShareType(STYPE_DISKTREE);
    pFileShare->SetServiceName(L"A:");
    pFileShare->SetACL(_pACL);
    pFileShare->SetRequireAuth(TRUE); 
        
    
    if(FAILED(SMB_Globals::g_pShareManager->AddShare(pFileShare))) {  
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add default share!")));       
    }
    
    Done:
    ;
}

       

VOID AddPrintSpool( WCHAR *_pName, 
                   WCHAR *_pDriverName, 
                   WCHAR *_pProc,
                   WCHAR *_pComment, 
                   WCHAR *_pDirectoryName,
                   WCHAR *_pShareName,
                   WCHAR *_pServiceName,
                   WCHAR *_pRemark,
                   WCHAR *_pPortName,
                   WCHAR *_pACL)
                   
{
    SMBPrintQueue *pPrintQueue;
    Share *pPrinterShare;   

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
       return;
    }

    //
    // Add a second print spool
    pPrintQueue = new SMBPrintQueue();
    if(NULL == pPrintQueue) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- couldnt create printqueue")));       
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
        goto Done;
    }
    
    pPrinterShare->SetDirectoryName(_pDirectoryName);
    pPrinterShare->SetShareName(_pShareName);
    pPrinterShare->SetServiceName(_pServiceName);
    pPrinterShare->SetRemark(pRemark);
    pPrinterShare->SetShareType(STYPE_PRINTQ);
    pPrinterShare->SetPrintQueue(pPrintQueue); 
    pPrinterShare->SetRequireAuth(TRUE); 
    pPrinterShare->SetACL(_pACL);
        
    if(FAILED(SMB_Globals::g_pShareManager->AddShare(pPrinterShare))) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add default share!")));       
    }
    
    Done:
    ;
}


HRESULT AddShares()
{
    CReg RegShares;
    WCHAR ShareName[MAX_PATH];
    BOOL fFoundSomething = FALSE;
    
    //
    // Load the CName from the registry
    if(FALSE == RegShares.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
        RETAILMSG(1, (L"No registy key under Services\\SMBServer\\Shares"));
    }
    
    while(TRUE == RegShares.EnumKey(ShareName, MAX_PATH)) {
        CReg ShareKey;
        WCHAR Path[MAX_PATH];
        TRACEMSG(ZONE_INIT, (L"Loading Share: %s", ShareName));
        WCHAR RegPath[MAX_PATH];
        WCHAR RegDriver[MAX_PATH];
        WCHAR RegComment[MAX_PATH];       
        WCHAR ACL[MAX_PATH];
        DWORD dwType;
        DWORD dwRegType;
        DWORD dwLen = sizeof(DWORD);
        DWORD dwEnabled = FALSE;
        
        
        
        swprintf(Path, L"Services\\SMBServer\\Shares\\%s", ShareName);
        RETAILMSG(1, (L"SMB_PRINT: Loading Registry: %s\n", Path));
        if(FALSE == ShareKey.Open(HKEY_LOCAL_MACHINE, Path)) {
            RETAILMSG(1, (L"   -- Cant Open KEY %s\n", Path));
            goto LoadLoop;
        }        
        
        //
        // Get the type 
        if(ERROR_SUCCESS != RegQueryValueEx(ShareKey, L"Type", 0, &dwRegType, (LPBYTE)&dwType, &dwLen) ||
           REG_DWORD != dwRegType ||
           sizeof(DWORD) != dwLen) {
            RETAILMSG(1, (L"   -- Cant Open TYPE on %s\n", Path));
            goto LoadLoop;
        }        
        RETAILMSG(1, (L"   -- Type: %d\n", dwType));
        
        
        //
        // Only let file shares in if we are linked up with the file server
        //    code -- otherwise just do print
        if(FALSE == (g_fFileServer && dwType == STYPE_DISKTREE) && 
           dwType != STYPE_PRINTQ) {
            RETAILMSG(1, (L"   -- unknown type!! -- we only support printers"));
            goto LoadLoop;
        }
        
        //
        // Get the Path
        if(FALSE == ShareKey.ValueSZ(L"Path", RegPath, MAX_PATH)) {
            RETAILMSG(1, (L"   -- Cant get PATH\n"));
            goto LoadLoop;
        }
        RETAILMSG(1, (L"   -- Path: %s\n", RegPath));
               
        //
        // Get the ACL
        if(FALSE == ShareKey.ValueSZ(L"UserList", ACL, MAX_PATH)) {
            RETAILMSG(1, (L"    --  NOT USING ACL's!\n"));
            ACL[0] = NULL;
        }
        RETAILMSG(1, (L"   -- ACL: %s\n", ACL));       

       
        // 
        // Get printer specific information
        if(dwType == STYPE_PRINTQ)
        {
            //
            // Get the Driver (printer specific)
            if(FALSE == ShareKey.ValueSZ(L"Driver", RegDriver, MAX_PATH)) {
                RETAILMSG(1, (L"   -- Cant get Driver\n"));
                goto LoadLoop;
            }
            RETAILMSG(1, (L"   -- Driver: %s\n", RegDriver));
            
            //
            // Get the Comment (printer specific)
            if(FALSE == ShareKey.ValueSZ(L"Comment", RegComment, MAX_PATH)) {
                RETAILMSG(1, (L"   -- Cant get Comment\n"));
                goto LoadLoop;
            }
            RETAILMSG(1, (L"   -- Comment: %s\n", RegComment));            
             
            //
            // See if the share has been enabled
            dwEnabled = ShareKey.ValueDW(L"Enabled", FALSE);
             
            //
            // Load up the driver
            if(TRUE == dwEnabled) {
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
                 
                fFoundSomething = TRUE;
            }
        }else if (dwType == STYPE_DISKTREE) {
           //
           //  Add the fileshare
           ASSERT(TRUE == g_fFileServer);
           AddFileShare(ShareName, RegPath, ACL);
           fFoundSomething = TRUE;
        }
        LoadLoop:
            ;
    }
    
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
    WCHAR MyCName[16];
    WCHAR OrigName[16];
    WCHAR wszWorkGroup[MAX_PATH];
    BOOL fAllowAll = FALSE;
    
    TRACEMSG(ZONE_INIT,(TEXT("+SMB_Init\n")));
    UINT i;
    Share *pIPCShare;
   
   
    //
    // Print a RETAIL message if passwords are disabled
    if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
        fAllowAll = RegAllowAll.ValueDW(L"NoSecurity", FALSE);
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
    dwLen = sizeof(SMB_Globals::g_ServerGUID);
    if(ERROR_SUCCESS != RegQueryValueEx(CRSettings, 
                                         L"GUID", 
                                         NULL,
                                         &dwType, 
                                         SMB_Globals::g_ServerGUID, 
                                         &dwLen) || (16 != dwLen) || (REG_BINARY != dwType)) {
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
    if(FALSE == CRSettings.ValueSZ(L"workgroup", wszWorkGroup, sizeof(wszWorkGroup))) {
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
    
#ifdef DEBUG
    SMB_Globals::g_PacketID = 0;    
#endif



    //
    // Load the CName from the registry
    if(FALSE == CRCName.Open(HKEY_LOCAL_MACHINE, L"Ident")) {        
        RETAILMSG(1, (L"No registy key under HKLM\\Ident"));
        dwRet = SMB_CONTEXT;
        goto Done;
    }
    if(FALSE == CRCName.ValueSZ(L"Name", MyCName, sizeof(MyCName))) {
        RETAILMSG(1, (L"Name value under HKLM\\Ident is either too large (15 chars) or isnt present -- stopping server"));
        dwRet = SMB_CONTEXT;
        goto Done;
    }
    if(FALSE == CRCName.ValueSZ(L"OrigName", OrigName, sizeof(OrigName))) {
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
    // Make a global object for FID creation
    SMB_Globals::g_pUniqueFID = new UniqueID();    
    
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
    // Add IPC Share
    pIPCShare = new Share();
    pIPCShare->SetDirectoryName(L"IPC$");
    pIPCShare->SetShareName(L"IPC$");
    pIPCShare->SetServiceName(L"IPC");
    pIPCShare->SetRequireAuth(FALSE); //let anyone in
    pIPCShare->SetRemark("Remote Inter Process Communication");
    pIPCShare->SetShareType(STYPE_IPC);
    pIPCShare->SetPrintQueue(NULL);
    
    if(FAILED(SMB_Globals::g_pShareManager->AddShare(pIPCShare))) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer failed -- cant add IPC$ share!")));
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
    if(FAILED(StartNetbiosTransport())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer on NETBsdIOS failed")));
        ASSERT(FALSE);
        dwRet = 0;
        goto Done;
    }
    if(FAILED(StartTCPTransport())) {
        TRACEMSG(ZONE_ERROR, (TEXT("-SMB_Init - StartServer on TCP failed")));
        ASSERT(FALSE);
        dwRet = 0;
        goto Done;
    }

    dwRet = SMB_CONTEXT;
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
                if(NULL != SMB_Globals::g_pUniqueFID) {
                    delete SMB_Globals::g_pUniqueFID;
                    SMB_Globals::g_pUniqueFID = NULL;
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
    // Delete unique id generator
    if(SMB_Globals::g_pUniqueFID) {
        delete SMB_Globals::g_pUniqueFID;
        SMB_Globals::g_pUniqueFID = NULL;
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
    DWORD  dwError = ERROR_SUCCESS;
    CCritSection csLock(&g_csDriverLock);
    csLock.Lock();
    
    switch(dwIoControlCode) {         
        case IOCTL_SERVICE_STOP:
            TRACEMSG(ZONE_INIT,(L"+SMB_IOControl, STOP IOCTL"));
            SMB_Deinit(0);
            break;
        case IOCTL_SERVICE_START:
            TRACEMSG(ZONE_INIT,(L"+SMB_IOControl, START IOCTL"));
            SMB_Init(0);
            break;  
        case IOCTL_SERVICE_REFRESH:
            TRACEMSG(ZONE_INIT,(L"+SMB_IOControl, RESTART IOCTL"));
            SMB_Deinit(0);
            SMB_Init(0);
            break;
        default:
            TRACEMSG(ZONE_INIT,(TEXT("+SMB_IOControl, unknown code 0x%X"),dwIoControlCode));
            break;
    }
    return TRUE;
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
            return TRUE;

        case DLL_PROCESS_DETACH:
            TRACEMSG(ZONE_INIT,(TEXT("SMBSRV: DLL_PROCESS_DETACH")));
            DeleteCriticalSection(&g_csDriverLock);
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


