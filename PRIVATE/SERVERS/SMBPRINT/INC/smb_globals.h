//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef SMB_GLOBALS_H
#define SMB_GLOBALS_H

#include <windows.h>
#include <smb_debug.h>
#include <block_allocator.hxx>



const BOOL g_fFileServer = TRUE;
const BOOL g_fPrintServer = TRUE;

//
// Forward declare these classes (rather than including .h) to keep
//   compliation dependencies down
class UniqueID;
class ShareManager;
class SMBPrintQueue;
class AbstractFileSystem;
class MemMappedBuffer;
class ConnectionManager;
class FileStream;
class PrintStream;
class TIDState;
class CONNECTION_HOLDER;
class ncb;
class PrintJob;
class WakeUpOnEvent;

struct SMB_PACKET;


template <size_t InitCount, typename T> class ThreadSafePool;
template <size_t InitCount, typename T> class ClassPoolAllocator;


#define SEC_READ   1
#define SEC_WRITE  2

namespace SMB_Globals
{
    //get the CName
    HRESULT GetCName(BYTE **pName, UINT *pLen);

    //maximum packet size
    const UINT MAX_PACKET_SIZE = 0xFFF0;
    const USHORT TCP_TRANSPORT = 1;
    const USHORT NB_TRANSPORT  = 2;

    //
    // Bookeeping information
    extern CRITICAL_SECTION g_Bookeeping_CS;
    extern LARGE_INTEGER g_Bookeeping_TotalRead;
    extern LARGE_INTEGER g_Bookeeping_TotalWritten;

    //
    // ServerAdministration Information
    //
    extern CRITICAL_SECTION SrvAdminNameLock;
    extern PWSTR *SrvAdminServerNameList;
    extern PWSTR *SrvAdminIpAddressList;
    extern PWSTR *SrvAdminAllowedServerNameList;

    //
    // Constants
    extern UINT                                   g_uiMaxConnections;
    extern UINT                                   g_uiAllowBumpAfterIdle;
    extern UINT                                   g_uiMaxJobsInPrintQueue;
    extern char                                   CName[16];
    extern BYTE                                    g_ServerGUID[4 * 4];
    extern CHAR                                    g_szWorkGroup[MAX_PATH];
    extern WCHAR                                   g_AdapterAllowList[MAX_PATH];
    extern UINT                                    g_uiMaxCrackingThreads;

    //
    // Thread Safe Objects
    extern ShareManager                          *g_pShareManager;         //ShareManager is thread safe
    extern ConnectionManager                     *g_pConnectionManager;   //ConnectionManager is thread safe
    extern AbstractFileSystem                    *g_pAbstractFileSystem;  //AFS is thread safe
    extern MemMappedBuffer                         g_PrinterMemMapBuffers; //globally inited, used under CS for RingBuffer
    extern ThreadSafePool<10, SMB_PACKET>         g_SMB_Pool;              //TSP is thread safe
    extern WakeUpOnEvent                           *g_pWakeUpOnEvent;       //WUOE is thread safe

    extern ce::fixed_block_allocator<30>          g_FileStreamAllocator;
    extern ce::fixed_block_allocator<10>          g_PrintStreamAllocator;
    extern ce::fixed_block_allocator<10>          g_PrintJobAllocator;


    extern SVSThreadPool                           *g_pCrackingPool;
    extern HANDLE                                    g_CrackingSem;

#ifdef DEBUG
    extern LONG       g_PacketID; //just for marking packets as they go through (debug only)
    extern LONG       g_lMemoryCurrentlyUsed;
#endif
};



 #pragma pack(1)
struct DosError{
    USHORT ErrorClass;
    USHORT Error;             // Error code
} ;


struct Status {
    union {
        //struct DosError error;
        ULONG dwError;
        ULONG Status;                 // 32-bit error code
    }Fields;
};

 struct SMB_HEADER {
    UCHAR Protocol[4];                // Contains 0xFF,'SMB'
    UCHAR Command;                    // Command code
    struct Status StatusFields;
    UCHAR Flags;                      // Flags
    USHORT Flags2;                    // More flags
    union {
        USHORT Pad[6];                // Ensure section is 12 bytes long
        struct {
            USHORT PidHigh;           // High part of PID
            UCHAR SecuritySignature[8];// reserved for security
       } Extra;
    };
    USHORT Tid;                       // Tree identifier
    USHORT Pid;                       // Caller's process id
    USHORT Uid;                       // Unauthenticated user id
    USHORT Mid;                       // multiplex id

#ifndef DEBUG
    private:
        ~SMB_HEADER() {ASSERT(FALSE);}
#endif

};


void *operator new(size_t size);



#pragma pack()


#ifdef DEBUG
extern DBGPARAM dpCurSettings;
#define IFDBG(c) c

WCHAR *GetCMDName(BYTE opCode);

typedef struct _CMD_TABLE {
    LPTSTR  ct_name;
    BYTE    ct_code;
} CMD_TABLE, * PCMD_TABLE;
#else
#define IFDBG(c)
#endif

#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_ERROR      DEBUGZONE(1)
#define ZONE_WARNING    DEBUGZONE(2)
#define ZONE_NETBIOS    DEBUGZONE(3)
#define ZONE_TCPIP      DEBUGZONE(4)
#define ZONE_SMB        DEBUGZONE(5)
#define ZONE_FILES      DEBUGZONE(6)
#define ZONE_MEMORY     DEBUGZONE(7)
#define ZONE_DETAIL     DEBUGZONE(8)
#define ZONE_STATS      DEBUGZONE(9)
#define ZONE_SECURITY   DEBUGZONE(10)
#define ZONE_PRINTQUEUE DEBUGZONE(11)
#define ZONE_IPC        DEBUGZONE(12)
#define ZONE_PROTOCOL   DEBUGZONE(13)
#define ZONE_UNDEFINED  DEBUGZONE(14)
#define ZONE_QUIT       DEBUGZONE(15)

#ifdef DEBUG
#define TRACEMSG(x,y) ((void)((x)?(_TRACEMSG y),1:0))
#define RETAILTRACE(x) (_TRACEMSG x)
#else
#define TRACEMSG(x,y)
#define RETAILTRACE(x)
#endif


#define SMB_ERROR_FACILITY 30
/*#define E_FILE_ALREADY_EXISTS    HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)
#define E_FILE_SHARING_VIOLATION HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)
#define E_FILE_NOT_FOUND         HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
#define E_PATH_NOT_FOUND         HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)
#define E_LOCK_VIOLATION         HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION)
#define E_NOT_SUPPORTED          HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)

#define E_INVALID_FID             MAKE_HRESULT(1, SMB_ERROR_FACILITY, 1)
#define E_PIPE_ERROR              MAKE_HRESULT(1, SMB_ERROR_FACILITY, 2)
#define E_FILE_IS_DIRECTORY      MAKE_HRESULT(1, SMB_ERROR_FACILITY, 3)
//#define E_OBJECT_NAME_COLLISON  MAKE_HRESULT(1, SMB_ERROR_FACILITY, 4)*/


void GenerateGUID (GUID *pGuid);
void _TRACEMSG (WCHAR *lpszFormat, ...);
#endif
