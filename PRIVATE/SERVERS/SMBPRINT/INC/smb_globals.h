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

struct SMB_PACKET;


template <size_t InitCount, typename T> class ThreadSafePool;
template <size_t InitCount, typename T> class ClassPoolAllocator;
template <size_t InitCount, typename T> class pool_allocator;


#define SEC_READ   1
#define SEC_WRITE  2

namespace SMB_Globals 
{    
    //get the CName
    HRESULT GetCName(BYTE **pName, UINT *pLen);   

    //maximum packet size
    const UINT MAX_PACKET_SIZE = 4096 * 2;//0xFFFF;//4096;
    const USHORT TCP_TRANSPORT = 1;
    const USHORT NB_TRANSPORT  = 2;

	extern UINT								      g_uiMaxConnections;
    extern ShareManager                          *g_pShareManager;    
    extern UniqueID                              *g_pUniqueFID;
    extern ConnectionManager                     *g_pConnectionManager;
    extern UINT                                   g_uiMaxJobsInPrintQueue;
    extern AbstractFileSystem                    *g_pAbstractFileSystem;
    extern char                                   CName[16];
    extern MemMappedBuffer                        g_PrinterMemMapBuffers;
    extern BYTE                                   g_ServerGUID[4 * 4];
    extern CHAR                                   g_szWorkGroup[MAX_PATH];   
    extern ThreadSafePool<10, SMB_PACKET>         g_SMB_Pool;
    extern pool_allocator<10, FileStream>         g_FileStreamAllocator;
    extern pool_allocator<10, PrintStream>        g_PrintStreamAllocator;
    extern pool_allocator<10, PrintJob>           g_PrintJobAllocator;
    extern pool_allocator<10, TIDState>           g_TIDStateAllocator;
    extern pool_allocator<10, CONNECTION_HOLDER>  g_ConnectionHolderAllocator;
    extern pool_allocator<10, ncb>                g_NCBAllocator;
    
#ifdef DEBUG
    extern LONG       g_PacketID; //just for marking packets as they go through (debug only)
#endif
};
 


 #pragma pack(1)
struct DosError{
    USHORT ErrorClass;
    USHORT Error;             // Error code
} ;

 
struct Status {
    union {
        struct DosError error;
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
//#define ZONE_BUFFER     DEBUGZONE(7)
#define ZONE_DETAIL     DEBUGZONE(8)
#define ZONE_STATS      DEBUGZONE(9)
#define ZONE_SECURITY   DEBUGZONE(10)
#define ZONE_PRINTQUEUE DEBUGZONE(11)
//#define ZONE_NOTHING    DEBUGZONE(11)

#define ZONE_QUIT   DEBUGZONE(15)
#ifdef DEBUG
#define TRACEMSG(x,y) ((void)((x)?(_TRACEMSG y),1:0))
#define RETAILTRACE(x) (_TRACEMSG x)
#else
#define TRACEMSG(x,y)
#define RETAILTRACE(x) 
#endif


#define SMB_ERROR_FACILITY 30
#define E_FILE_ALREADY_EXISTS    HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)
#define E_FILE_SHARING_VIOLATION HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)
#define E_FILE_NOT_FOUND         HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
#define E_PATH_NOT_FOUUND        HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)
#define E_LOCK_VIOLATION         HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION)


#define E_DOS_ERROR_ACCESS_DENIED MAKE_HRESULT(3, SMB_ERROR_FACILITY, 0)
#define E_INVALID_FID             MAKE_HRESULT(3, SMB_ERROR_FACILITY, 1)

void GenerateGUID (GUID *pGuid);
void _TRACEMSG (WCHAR *lpszFormat, ...);
#endif
