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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <tchar.h>
#include "ycalls.h"
#include "harness.h"
#include <winuser.h>

#ifndef NO_YCALLS

TCHAR *szFailed = TEXT("Failed");
TCHAR *szPassed = TEXT("Passed");
TCHAR *szTrue   = TEXT("TRUE");
TCHAR *szFalse  = TEXT("FALSE");
TCHAR *szNULL   = TEXT("NULL");

APICALLS VMAllocationType[] = {
    MAKEAPI( MEM_COMMIT),
    MAKEAPI( MEM_RESERVE),
    MAKEAPI( MEM_TOP_DOWN),
};

#define NUM_VMAllocationType COUNT(VMAllocationType)

APICALLS VMProtect[] = {
    MAKEAPI( PAGE_READONLY),        
    MAKEAPI( PAGE_READWRITE),
    MAKEAPI( PAGE_EXECUTE),
    MAKEAPI( PAGE_EXECUTE_READ),
    MAKEAPI( PAGE_GUARD),
    MAKEAPI( PAGE_NOACCESS),
    MAKEAPI( PAGE_NOCACHE),
};

#define NUM_VMProtect COUNT(VMProtect)

APICALLS VMFreeType[] = {
    MAKEAPI( MEM_DECOMMIT), 
    MAKEAPI( MEM_RELEASE),  
};

#define NUM_VMFreeType COUNT(VMFreeType)

APICALLS HPFlags[] = {
    MAKEAPI( HEAP_GENERATE_EXCEPTIONS), 
    MAKEAPI( HEAP_NO_SERIALIZE),        
    MAKEAPI( HEAP_ZERO_MEMORY),         
};

#define NUM_HPFlags COUNT(HPFlags)

APICALLS LAFlags[] = {
    MAKEAPI( LMEM_FIXED ),       // Allocates fixed memory. This flag cannot be 
                                // combined with the LMEM_MOVEABLE or LMEM_DISCARDABLE 
                                // flag. The return value is a pointer to the memory 
                                // block. To access the memory, the calling process 
                                // simply casts the return value to a pointer.
    MAKEAPI( LMEM_MOVEABLE),    // Allocates movable memory. This flag cannot be combined 
                                // with the LMEM_FIXED flag. The return value is the 
                                // handle of the memory object. The handle is a 32-
                                // bit quantity that is private to the calling process. 
                                // To translate the handle into a pointer, use the LocalLock 
                                // function.
    MAKEAPI( LPTR),             // Combines the LMEM_FIXED and LMEM_ZEROINIT flags.
    MAKEAPI( LHND),             // Combines the LMEM_MOVEABLE and LMEM_ZEROINIT flags.
    MAKEAPI( NONZEROLHND),      // Same as the LMEM_MOVEABLE flag.
    MAKEAPI( NONZEROLPTR),      // Same as the LMEM_FIXED flag.
    MAKEAPI( LMEM_DISCARDABLE), // Allocates discardable memory. This flag cannot be combined 
                                // with the LMEM_FIXED flag. Some Win32-based applications 
                                // may ignore this flag.
    MAKEAPI( LMEM_NOCOMPACT),   // Does not compact or discard memory to satisfy the 
                                // allocation request.
    MAKEAPI( LMEM_NODISCARD),   // Does not discard memory to satisfy the allocation request.
    MAKEAPI( LMEM_ZEROINIT),    // Initializes memory contents to zero.
};

APICALLS WSOWaitRet[] = {
    MAKEAPI( WAIT_FAILED),
    MAKEAPI( WAIT_ABANDONED),
    MAKEAPI( WAIT_OBJECT_0),
    MAKEAPI( WAIT_TIMEOUT),
};



#define NUM_WSOWaitRet COUNT(WSOWaitRet)

#define NUM_LAFlags COUNT(LAFlags)

APICALLS CFDesiredAccess[] = {
    MAKEAPI( GENERIC_READ),         // Specifies read access to the object. Data can be read from the file and the
                                    // file pointer can be moved. Combine with GENERIC_WRITE for read-write access.
    MAKEAPI( GENERIC_WRITE),        // Specifies write access to the object. Data can be written to the file and the 
                                    // file pointer can be moved. Combine with GENERIC_READ for read-write access. 
};

#define NUM_CFDesiredAccess COUNT(CFDesiredAccess)

APICALLS CFShareMode[] = {
    MAKEAPI( FILE_SHARE_READ),      // Subsequent open operations on the object will succeed only if read 
                                    // access is requested. 
    MAKEAPI( FILE_SHARE_WRITE),     // Subsequent open operations on the object will succeed only if write 
                                                            // access is requested. 
};

#define NUM_CFShareMode COUNT(CFDesiredAccess)

APICALLS CFCreationDistribution[] = {
    MAKEAPI( CREATE_NEW),           // Creates a new file. The function fails if the specified file already exists.
    MAKEAPI( CREATE_ALWAYS),        // Creates a new file. The function overwrites the file if it exists.
    MAKEAPI( OPEN_EXISTING),        // Opens the file. The function fails if the file does not exist.
                                    // See the Remarks section for a discussion of why you should use the OPEN_EXISTING 
                                    // flag if you are using the CreateFile function for devices)), including the console.
    MAKEAPI( OPEN_ALWAYS),          // Opens the file, if it exists. If the file does not exist, the function creates the 
                                    // file as if dwCreationDistribution were CREATE_NEW.
    MAKEAPI( TRUNCATE_EXISTING),    // Opens the file. Once opened, the file is truncated so that its size is zero bytes. 
                                    // The calling process must open the file with at least GENERIC_WRITE access. 
                                    // The function fails if the file does not exist.
};

#define NUM_CFCreationDistribution COUNT(CFCreationDistribution)

APICALLS CFFlagsAndAttributes[] = {
    MAKEAPI( FILE_ATTRIBUTE_ARCHIVE),       // The file should be archived. Applications use this attribute to mark files 
                                            // for backup or removal.
    MAKEAPI( FILE_ATTRIBUTE_COMPRESSED),    // The file or directory is compressed. For a file), this means that all of the 
                                            // data in the file is compressed. For a directory, this means that compression is 
                                            // the default for newly created files and subdirectories.
    MAKEAPI( FILE_ATTRIBUTE_HIDDEN),        // The file is hidden. It is not to be included in an ordinary directory listing.
    MAKEAPI( FILE_ATTRIBUTE_NORMAL),        // The file has no other attributes set. This attribute is valid only if used alone.
    MAKEAPI( FILE_ATTRIBUTE_DIRECTORY),

    MAKEAPI( FILE_ATTRIBUTE_READONLY),      // The file is read only. Applications can read the file but cannot write to it or delete it.
    MAKEAPI( FILE_ATTRIBUTE_SYSTEM),        // The file is part of or is used exclusively by the operating system.
    MAKEAPI( FILE_ATTRIBUTE_TEMPORARY),     // The file is being used for temporary storage. File systems attempt to keep all of the 
                                            // data in memory for quicker access rather than flushing the data back to mass storage. 
                                            // A temporary file should be deleted by the application as soon as it is no longer needed.
    MAKEAPI( FILE_FLAG_WRITE_THROUGH),
    MAKEAPI( FILE_FLAG_OVERLAPPED),
    MAKEAPI( FILE_FLAG_NO_BUFFERING),
    MAKEAPI( FILE_FLAG_RANDOM_ACCESS),
    MAKEAPI( FILE_FLAG_SEQUENTIAL_SCAN),
    MAKEAPI( FILE_FLAG_DELETE_ON_CLOSE),
    MAKEAPI( FILE_FLAG_BACKUP_SEMANTICS),
    MAKEAPI( FILE_FLAG_POSIX_SEMANTICS),
};

#define  NUM_CFFlagsAndAttributes COUNT(CFFlagsAndAttributes)



APICALLS SFPMoveMethod[] = {
    MAKEAPI( FILE_BEGIN),       // The starting point is zero or the beginning of the file. If FILE_BEGIN is specified, 
                                // DistanceToMove is interpreted as an unsigned location for the new file pointer.
    MAKEAPI( FILE_CURRENT),     // The current value of the file pointer is the starting point.
    MAKEAPI( FILE_END),         // The current end-of-file position is the starting point.
};

#define NUM_SFPMoveMethod COUNT(SFPMoveMethod)


APICALLS CPCreationFlags[] = {
    MAKEAPI( CREATE_SUSPENDED),             //  The primary thread of the new process is created in a suspended state, and does not run until the ResumeThread function is called.
};

#define NUM_CPCreationFlags COUNT(CPCreationFlags)


APICALLS CMProtect[] = {
    MAKEAPI( PAGE_READONLY),  // Gives read-only access to the committed 
                              // region of pages. An attempt to write 
                              // to or execute the committed region results 
                              // in an access violation. The file specified 
                              // by the hFile parameter must have been 
                              // created with GENERIC_READ access.
    MAKEAPI( PAGE_READWRITE), // Gives read-write access to the committed 
                              // region of pages. The file specified by 
                              // hFile must have been created with 
                              // GENERIC_READ and GENERIC_WRITE access.
    MAKEAPI( PAGE_WRITECOPY), // Gives copy on write access to the committed 
                              // region of pages. The files specified by 
                              // the hFile parameter must have been created 
                              // with GENERIC_READ and GENERIC_WRITE access.
    MAKEAPI( SEC_COMMIT),     // Allocates physical storage in memory or in 
                              // the paging file on disk for all pages of a 
                              // section. This is the default setting.
    MAKEAPI( SEC_IMAGE),      // The file specified for a section's file mapping 
                              // is an executable image file. Because the 
                              // mapping information and file protection are 
                              // taken from the image file, no other attributes 
                              // are valid with SEC_IMAGE.
    MAKEAPI( SEC_NOCACHE),    // All pages of a section are to be set as 
                              // non-cacheable. This attribute is intended for 
                              // architectures requiring various locking 
                              // structures to be in memory that is never 
                              // fetched into the processor's. On 80x86 and 
                              // MIPS machines, using the cache for these 
                              // structures only slows down the performance 
                              // as the hardware keeps the caches coherent. 
                              // Some device drivers require noncached data 
                              // so that programs can write through to the 
                              /// physical memory. SEC_NOCACHE requires either 
                              // the SEC_RESERVE or SEC_COMMIT to also be set.
    MAKEAPI( SEC_RESERVE),    // Reserves all pages of a section without 
                              // allocating physical storage. The reserved range 
                              // of pages cannot be used by any other allocation 
                              // operations until it is released. Reserved pages 
                              // can be committed in subsequent calls to the 
                              // VirtualAlloc function. This attribute is valid 
                              // only if the hFile parameter is (HANDLE)0xFFFFFFFF; 
                              // that is, a file mapping object backed by the 
                              // operating sytem paging file.
};

#define NUM_CMProtect COUNT(CMProtect)

APICALLS NewVMProtect[] = {
    MAKEAPI(PAGE_READONLY),     //Enables read access to the committed region of pages.
                                //An attempt to write to the committed region results 
                                //in an access violation. If the system differentiates 
                                //between read-only access and execute access, an attempt 
                                //to execute code in the committed region results in an 
                                //access violation. 
    MAKEAPI(PAGE_READWRITE),    //Enables both read and write access to the committed 
                                //region of pages.
    MAKEAPI(PAGE_WRITECOPY),    //Gives copy-on-write access to the committed region of 
                                //pages. 
    
    MAKEAPI(PAGE_EXECUTE),      //Enables execute access to the committed region of pages. 
                                //An attempt to read or write to the committed region 
                                //results in an access violation. 
    MAKEAPI(PAGE_EXECUTE_READWRITE), 
                                //Enables execute, read, and write access to the committed 
                                //region of pages.
    MAKEAPI(PAGE_EXECUTE_WRITECOPY),
                                //Enables execute, read, and write access to the committed 
                                //region of pages. The pages are shared read-on-write and 
                                //copy-on-write.
    MAKEAPI(PAGE_GUARD),        //Pages in the region become guard pages. Any attempt to 
                                //access a guard page causes the operating system to raise 
                                //a STATUS_GUARD_PAGE exception and turn off the guard page
                                //status. Guard pages thus act as a one-shot access alarm. 
                                //The PAGE_GUARD flag is a page protection modifier. An application 
                                //uses it with one of the other page protection flags, with one exception: 
                                //it cannot be used with PAGE_NOACCESS. When an access attempt 
                                //leads the operating system to turn off guard page status, the underlying page protection takes over. 
                                //If a guard page exception occurs during a system service, the service typically returns a failure status indicator. 
    MAKEAPI(PAGE_NOACCESS),     //Disables all access to the committed region of pages. 
                                //An attempt to read from, write to, or execute in the 
                                //committed region results in an access violation exception, called a general protection (GP) fault

    MAKEAPI(PAGE_NOCACHE),      //Allows no caching of the committed regions of pages. 
                                //The hardware attributes for the physical memory should 
                                //be specified as "no cache." This is not recommended for 
                                //general usage. It is useful for device drivers; 
                                //for example, mapping a video frame buffer with no caching. 
                                //This flag is a page protection modifier, only valid when 
                                //used with one of the page protections other than 
                                //PAGE_NOACCESS. 
} ;

#define NUM_NewVMProtect COUNT(NewVMProtect)

APICALLS MFDesiredAccess[] = {
    MAKEAPI( FILE_MAP_WRITE),       // Read-write access. The hFileMappingObject 
                                    // parameter must have been created with 
                                    // PAGE_READWRITE protection. A read-write view 
                                    // of the file is mapped.
    MAKEAPI( FILE_MAP_READ),        // Read-only access. The hFileMappingObject 
                                    // parameter must have been created with 
                                    // PAGE_READWRITE or PAGE_READONLY protection. 
                                    // A read-only view of the file is mapped.
    MAKEAPI( FILE_MAP_ALL_ACCESS),  // Same as FILE_MAP_WRITE.

};

#define NUM_MFDesiredAccess COUNT(MFDesiredAccess)


APICALLS TPriority[] = {
    MAKEAPI( THREAD_PRIORITY_ABOVE_NORMAL),     // Indicates 1 point above normal priority for the priority class.
    MAKEAPI( THREAD_PRIORITY_BELOW_NORMAL),     // Indicates 1 point below normal priority for the priority class.
    MAKEAPI( THREAD_PRIORITY_HIGHEST),          // Indicates 2 points above normal priority for the priority class.
    MAKEAPI( THREAD_PRIORITY_IDLE),             // Indicates a base priority level of 1 for IDLE_PRIORITY_CLASS, 
                                                // NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS processes, and a 
                                                // base priority level of 16 for REALTIME_PRIORITY_CLASS processes.
    MAKEAPI( THREAD_PRIORITY_LOWEST),           // Indicates 2 points below normal priority for the priority class.
    MAKEAPI( THREAD_PRIORITY_NORMAL),           // Indicates normal priority for the priority class.
    MAKEAPI( THREAD_PRIORITY_TIME_CRITICAL),    // Indicates a base priority level of 15 for IDLE_PRIORITY_CLASS, 
                                                // NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS processes, and a 
                                                // base priority level of 31 for REALTIME_PRIORITY_CLASS processes.
}; 

#define NUM_TPriority COUNT(TPriority)

APICALLS MLLFlags[] = {
    MAKEAPI( DONT_RESOLVE_DLL_REFERENCES ),
    MAKEAPI( LOAD_LIBRARY_AS_DATAFILE ),
    MAKEAPI( LOAD_WITH_ALTERED_SEARCH_PATH )
};

#define NUM_MLLFlags COUNT( MLLFlags )

APICALLS CGMITypes[] = {
    MAKEAPI( MINFO_FUNCTION_TABLE ),
    MAKEAPI( MINFO_FILE_ATTRIBUTES ),
    MAKEAPI( MINFO_FULL_PATH ),
    MAKEAPI( MINFO_MODULE_INFO ),
    MAKEAPI( MINFO_FILE_HANDLE )
};

#define NUM_CGMITypes COUNT( CGMITypes )

/*
 ** YVirtualAlloc
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
LPVOID YVirtualAlloc( 
    LPVOID lpAddress,       // address of region to reserve or commit  
    DWORD dwSize,           // size of region 
    DWORD flAllocationType, // type of allocation 
    DWORD flProtect         // type of access protection    
   ) 
{
    TCHAR szBuffer[256] = { 0 };
    DWORD i=0;
    LPVOID lpAddr=NULL;

    
    szBuffer[0]=0;
    for (i=0; i < NUM_VMAllocationType; i++) {
        if ( VMAllocationType[i].dwValue & flAllocationType) {
            _tcscat( szBuffer, VMAllocationType[i].szText);
        }
    }
    _tcscat( szBuffer, TEXT(", "));
    for (i=0; i < NUM_VMProtect; i++) {
        if ( VMProtect[i].dwValue & flProtect) {
            _tcscat( szBuffer, VMProtect[i].szText);
        }
    }
    LogDetail( TEXT("VirtualAlloc( %08X, %ld, %s)"), lpAddress, dwSize, szBuffer);
    if (lpAddr = VirtualAlloc( lpAddress, dwSize, flAllocationType, flProtect)) {
        LogDetail( TEXT("VirtualAlloc: %s - returned %08X"), szPassed, lpAddr);
    } else {        
        LogError( TEXT("VirtualAlloc"));
    }
    return lpAddr;
}

/*
 ** YVirtualFree
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */

BOOL YVirtualFree( 
    LPVOID lpAddress,   // address of region of committed pages  
    DWORD dwSize,       // size of region 
    DWORD dwFreeType    // type of free operation    
   )
{
    BOOL bSuccess=FALSE;
    TCHAR szBuffer[128] = { 0 };
    int i = 0;


    szBuffer[0] = 0;
    for (i=0; i < NUM_VMFreeType; i++) {
        if ( VMFreeType[i].dwValue & dwFreeType) {
            _tcscat( szBuffer, VMFreeType[i].szText);
        }
    }
    LogDetail( TEXT("VirtualFree(%08X, %d, %s)"), lpAddress, dwSize, szBuffer);

    if (bSuccess = VirtualFree(lpAddress, dwSize, dwFreeType))  {
        LogDetail( TEXT("VirtualFree: %s"), szPassed);
    } else  {
        LogError( TEXT("VirtualFree")); 
    };

    return bSuccess;
}


/*
 ** YHeapCreate
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */

HANDLE YHeapCreate(
    DWORD flOptions,    // heap allocation flag 
    DWORD dwInitialSize,    // initial heap size 
    DWORD dwMaximumSize     // maximum heap size 
   )
{
    HANDLE hHeap=NULL;
    TCHAR szBuffer[128] = { 0 };
    DWORD i = 0;

    szBuffer[0]=0;
    if (flOptions) {
        for (i=0; i < NUM_HPFlags; i++) {
            if ( HPFlags[i].dwValue & flOptions) {
                _tcscat( szBuffer, HPFlags[i].szText);
            }
        }
    } else {
        _tcscat( szBuffer, TEXT("0"));
    }
    LogDetail( TEXT("HeapCreate( %s, %ld, %ld)"), szBuffer, dwInitialSize, dwMaximumSize);
    if (hHeap = HeapCreate( flOptions, dwInitialSize, dwMaximumSize)) {
        LogDetail( TEXT("HeapCreate: %s - returned %08X"), szPassed, hHeap);
    } else {
        LogError( TEXT("HeapCreate"));
    }
    return hHeap;
}

BOOL YHeapDestroy(
    HANDLE hHeap    // handle to the heap 
   )
{
    BOOL bSuccess=FALSE;

    LogDetail( TEXT("HeapDestroy( %08X)"), hHeap);

    if (bSuccess = HeapDestroy( hHeap))  {
         LogDetail( TEXT("HeapDestroy: %s"), szPassed);     
    } else  {
         LogError(TEXT("HeapDestroy"));
    };

    return bSuccess;
}
 
HANDLE YGetProcessHeap(VOID)
{
    HANDLE hHeap = NULL;

    LogDetail( TEXT("GetProcessHeap()"));

    if (hHeap = GetProcessHeap()) {
        LogDetail( TEXT("GetProcessHeap: %s - returned %08X"), szPassed, hHeap);
    } else {
        LogError( TEXT("GetProcessHeap"));
    }

    return hHeap;
}


LPVOID YHeapAlloc(
    HANDLE hHeap,   // handle to the private heap block 
    DWORD dwFlags,  // heap allocation control flags 
    DWORD dwBytes   // number of bytes to allocate    
   )
{
    TCHAR szBuffer[128] = { 0 };
    DWORD i = 0;
    LPVOID lpAddr=NULL;

    szBuffer[0]=0;
    if (dwFlags) {
        for (i=0; i < NUM_HPFlags; i++) {
            if ( HPFlags[i].dwValue & dwFlags) {
                _tcscat( szBuffer, HPFlags[i].szText);
            }
        }
    } else {
        _tcscat( szBuffer, TEXT("0"));
    }
    LogDetail( TEXT("HeapAlloc( %08X, %s, %ld)"), hHeap, szBuffer, dwBytes);
    if (lpAddr = HeapAlloc( hHeap, dwFlags, dwBytes)) {
        LogDetail( TEXT("HeapAlloc: %s - returned %08X"), szPassed, lpAddr);
    } else {        
        LogError( TEXT("HeapAlloc"));
    }
    return lpAddr;
}


BOOL YHeapFree(
    HANDLE hHeap,   // handle to the heap 
    DWORD dwFlags,  // heap freeing flags 
    LPVOID lpMem    // pointer to the memory to free 
   )
{
    TCHAR szBuffer[128] = { 0 };
    DWORD i = 0;
    BOOL bSuccess=FALSE;

    szBuffer[0]=0;
    if (dwFlags) {
        for (i=0; i < NUM_HPFlags; i++) {
            if ( HPFlags[i].dwValue & dwFlags) {
                _tcscat( szBuffer, HPFlags[i].szText);
            }
        }
     } else {
        _tcscat( szBuffer, TEXT("0"));
    }
    LogDetail( TEXT( "HeapFree( %08X, %s, %08X ) "), hHeap, szBuffer, lpMem);

    if (bSuccess = HeapFree( hHeap, dwFlags, lpMem))  {
        LogDetail( TEXT( "HeapFree: %s"), szPassed);        
    } else {
        LogError(TEXT( "HeapFree"));
    }
    return bSuccess;
}

// We don't really implement MOVEABLE on WinCE but the text for is in their just
// in case.
HLOCAL YLocalAlloc(
    UINT uFlags,    // allocation attributes 
    UINT uBytes     // number of bytes to allocate  
   )
{
    TCHAR szBuffer[128] = { 0 };
    HLOCAL hLocal = NULL;
    int i = 0;
    TCHAR *szText = TEXT("LocalAlloc");
    szBuffer[0] = 0;

    // special case LMEM_FIXED=0
    if( !uFlags && !LMEM_FIXED ) {
        _tcscat( szBuffer, TEXT("LMEM_FIXED "));
    }

    for (i=0;i < NUM_LAFlags ; i++){
        if (uFlags & LAFlags[i].dwValue) {
            _tcscat( szBuffer, LAFlags[i].szText);
        }
    }
    LogDetail( TEXT( "%s( %s, %ld)"), szText, szBuffer, uBytes);
    if (hLocal = LocalAlloc( uFlags, uBytes)) {
        LogDetail( TEXT("%s: returned %08X"), szText, hLocal);
    } else{
        LogError( szText);
    }
    return( hLocal);
}    

HLOCAL YLocalFree(
    HLOCAL hMem     // handle of local memory object 
   )    
{
    HLOCAL hLocal = NULL;
    TCHAR *szText = TEXT( "LocalFree");
    
    LogDetail( TEXT( "%s( %08X)"), szText, hMem);
    if (hLocal = LocalFree( hMem)) {
        LogError( szText);
    } else{
        LogDetail( TEXT("%s: Passed"), szText);
    }
    return( hLocal);
        
}   


HANDLE YCreateFile(
    LPCTSTR lpFileName,                         // pointer to name of the file 
    DWORD dwDesiredAccess,                      // access (read-write) mode 
    DWORD dwShareMode,                          // share mode 
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, // pointer to security attributes 
    DWORD dwCreationDistribution,               // how to create 
    DWORD dwFlagsAndAttributes,                 // file attributes 
    HANDLE hTemplateFile                        // handle to file with attributes to copy  
   )
{
    TCHAR szBuffer1[128] = { 0 }, szBuffer2[128] = { 0 };
    DWORD i = 0;
    HANDLE hFile = NULL;


    szBuffer1[0] = 0; 
    if (dwDesiredAccess) {
        for ( i=0; i < NUM_CFDesiredAccess; i++) {
            if ( CFDesiredAccess[i].dwValue & dwDesiredAccess) {
                _tcscat( szBuffer1, CFDesiredAccess[i].szText);
            }
        }
    } else {
        _tcscat( szBuffer1, TEXT("0"));
    }
    _tcscat( szBuffer1, TEXT(", "));
    if (dwShareMode) {
        for ( i=0; i < NUM_CFShareMode; i++) {
            if ( CFShareMode[i].dwValue & dwShareMode) {
                _tcscat( szBuffer1, CFShareMode[i].szText);
            }
        }
    } else {
        _tcscat( szBuffer1, TEXT("0"));
    }
    szBuffer2[0] = 0;
    for ( i=0; i < NUM_CFCreationDistribution; i++) {
        if ( CFCreationDistribution[i].dwValue == dwCreationDistribution) {
            _tcscat( szBuffer2, CFCreationDistribution[i].szText);
            break;
        }
    }
    _tcscat( szBuffer2, TEXT(", "));
    for ( i=0; i < NUM_CFFlagsAndAttributes; i++) {
        if ( CFFlagsAndAttributes[i].dwValue & dwFlagsAndAttributes) {
            _tcscat( szBuffer2, CFFlagsAndAttributes[i].szText);
        }
    }
    LogDetail( TEXT( "CreateFile( %s, %s, %08X, %s, %08X"), 
               lpFileName ? lpFileName : TEXT("NULL"), 
               szBuffer1, 
               lpSecurityAttributes, 
               szBuffer2, 
               hTemplateFile);
    
    hFile = CreateFile( lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDistribution, dwFlagsAndAttributes, hTemplateFile);
    if (hFile != INVALID_HANDLE_VALUE) {
        LogDetail( TEXT("CreateFile: %s - returned %08X"), szPassed, hFile);
    } else {
        LogError( TEXT("CreateFile"));
    }
    return hFile;
}


BOOL YDeleteFile(
    LPCTSTR lpFileName  // pointer to name of file to delete  
   )
{
    BOOL bSuccess=FALSE;

    LogDetail( TEXT( "DeleteFile( %s)"), lpFileName);
    
    if (bSuccess = DeleteFile( lpFileName))  {
        LogDetail( TEXT( "DeleteFile: %s"), szPassed);
    } else {
        LogError(  TEXT( "DeleteFile"));
    }

    return bSuccess;
}

BOOL YCloseHandle(
    HANDLE hObject  // handle to object to close  
   )
{
    BOOL bSuccess=FALSE;
    TCHAR *szText = TEXT("CloseHandle");

    LogDetail( TEXT( "%s( %08X)"), szText, hObject);
    
    if (bSuccess = CloseHandle(hObject))  {
        LogDetail(  TEXT( "%s: %s"), szText, szPassed);
    } else {
        LogError(  szText);
    }

    return bSuccess;
}


DWORD YSetFilePointer(
    HANDLE hFile,                   // handle of file 
    LONG lDistanceToMove,           // number of bytes to move file pointer 
    PLONG lpDistanceToMoveHigh,     // address of high-order word of distance to move  
    DWORD dwMoveMethod              // how to move 
   )
{
    TCHAR szBuffer[128] = { 0 };
    DWORD i = 0;
    DWORD dwRetCode = 0;

    szBuffer[0]=0;
    for (i=0; i < NUM_SFPMoveMethod; i++) {
        if ( SFPMoveMethod[i].dwValue & dwMoveMethod) {
            _tcscat( szBuffer, SFPMoveMethod[i].szText);
        }
    }
    LogDetail( TEXT("SetFilePointer( %08X, %ld, %08X, %s"), hFile, lDistanceToMove, lpDistanceToMoveHigh, szBuffer);

    dwRetCode = SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);

    if (dwRetCode != 0xFFFFFFFF) {
        LogDetail( TEXT( "SetFilePointer: %s - returned %08X"), szPassed, dwRetCode);
    } else {
        LogError( TEXT( "SetFilePointer"));
    }
    return dwRetCode;   
}

BOOL YWriteFile(
    HANDLE hFile,                   // handle to file to write to 
    LPCVOID lpBuffer,               // pointer to data to write to file 
    DWORD nNumberOfBytesToWrite,    // number of bytes to write 
    LPDWORD lpNumberOfBytesWritten, // pointer to number of bytes written 
    LPOVERLAPPED lpOverlapped       // pointer to structure needed for overlapped I/O
   )
{
    BOOL bSuccess=FALSE;

    LogDetail( TEXT( "WriteFile( %08X, %08X, %ld, %08X %08X"), hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);

    if (bSuccess = WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped))  {
        LogDetail( TEXT( "WriteFile: %s"), szPassed);
    } else {
        LogError( TEXT( "WriteFile"));
    }

    return bSuccess;    
}

BOOL YReadFile(
    HANDLE hFile,                   // handle of file to read 
    LPVOID lpBuffer,                // address of buffer that receives data  
    DWORD nNumberOfBytesToRead,     // number of bytes to read 
    LPDWORD lpNumberOfBytesRead,    // address of number of bytes read 
    LPOVERLAPPED lpOverlapped       // address of structure for data 
   )
{
    BOOL bSuccess=FALSE;

    LogDetail( TEXT( "ReadFile( %08X, %08X, %ld, %08X %08X"), 
               hFile, 
               lpBuffer, 
               nNumberOfBytesToRead, 
               lpNumberOfBytesRead, 
               lpOverlapped);   
    if (bSuccess = ReadFile( hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped))  {
        LogDetail( TEXT("ReadFile: %s"), szPassed);
    } else {
        LogError( TEXT("ReadFile"));
    }

    return bSuccess;    
}


HANDLE YCreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // pointer to thread security attributes  
    DWORD dwStackSize,                          // initial thread stack size, in bytes 
    LPTHREAD_START_ROUTINE lpStartAddress,      // pointer to thread function 
    LPVOID lpParameter,                         // argument for new thread 
    DWORD dwCreationFlags,                      // creation flags 
    LPDWORD lpThreadId                          // pointer to returned thread identifier 
   )
{
    HANDLE hThread = NULL;
    TCHAR *szText = TEXT("CreateThread");

    LogDetail( TEXT("%s( %08X, %ld, %08X, %08X, %s, %08X"), 
                szText,
                lpThreadAttributes, 
                dwStackSize,        
                lpStartAddress,     
                lpParameter,        
                dwCreationFlags ? TEXT("CREATE_SUSPENDED") : TEXT("0"), 
                lpThreadId);

    hThread = CreateThread( lpThreadAttributes, 
                            dwStackSize,        
                            lpStartAddress,     
                            lpParameter,        
                            dwCreationFlags,    
                            lpThreadId);

    if (hThread) {
        LogDetail( TEXT( "%s: Passed - returned hThread = %08X dwThreadId = %08X"), szText, hThread, lpThreadId?*lpThreadId:0);
    } else {
        LogError( szText);
    }
    return hThread;
}



DWORD YWaitForSingleObject(
    HANDLE hHandle,                 // handle of object to wait for 
    DWORD dwMilliseconds            // time-out interval in milliseconds  
   )
{
    DWORD dwRetCode = 0;
    TCHAR szBuffer[32] = { 0 };
    int i = 0;
    TCHAR *szText = TEXT("WaitForSingleObject");

    if (dwMilliseconds == INFINITE) {
        LogDetail( TEXT("%s( %08X, INFINITE)"), szText, hHandle);
    } else {
        LogDetail( TEXT("%s( %08X, %ld)"), szText, hHandle, dwMilliseconds);
    }

    dwRetCode = WaitForSingleObject( hHandle, dwMilliseconds);
    szBuffer[0] = 0;
    for (i=0; i < NUM_WSOWaitRet; i++) {
        if ( WSOWaitRet[i].dwValue == dwRetCode) {
            _tcscat( szBuffer, WSOWaitRet[i].szText);
        }
    }
    if (WAIT_FAILED != dwRetCode)  {
        LogDetail( TEXT("%s: %s - returned %s"), szText, szPassed, szBuffer);
    } else {
        LogError( szText);
    }
    return dwRetCode;
}

BOOL YCreateProcess(
    LPCTSTR lpApplicationName,                  // pointer to name of executable module 
    LPTSTR lpCommandLine,                       // pointer to command line string
    LPSECURITY_ATTRIBUTES lpProcessAttributes,  // pointer to process security attributes 
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // pointer to thread security attributes 
    BOOL bInheritHandles,                       // handle inheritance flag 
    DWORD dwCreationFlags,                      // creation flags 
    LPVOID lpEnvironment,                       // pointer to new environment block 
    LPTSTR lpCurrentDirectory,                  // pointer to current directory name 
    LPSTARTUPINFO lpStartupInfo,                // pointer to STARTUPINFO 
    LPPROCESS_INFORMATION lpProcessInformation  // pointer to PROCESS_INFORMATION  
   )
{
    BOOL bSuccess = FALSE;
    TCHAR szBuffer[128] = { 0 };
    int i = 0;

    szBuffer[0] = 0; 
    if (dwCreationFlags) {
        for ( i=0; i < NUM_CPCreationFlags; i++) {
            if ( CPCreationFlags[i].dwValue & dwCreationFlags) {
                _tcscat( szBuffer, CPCreationFlags[i].szText);
            }
        }
    } else {
        _tcscat( szBuffer, TEXT("0"));
    }
    LogDetail( TEXT("CreateProcess( %s, %s, %08X, %08X, %s, %s, %08X, %s, %08X, %08X)"),
                    lpApplicationName ? lpApplicationName : TEXT("NULL"),                   
                    lpCommandLine ? lpCommandLine : TEXT("NULL"),
                    lpProcessAttributes,
                    lpThreadAttributes,
                    bInheritHandles ? TEXT("TRUE") : TEXT("FALSE"),                  
                    szBuffer,                    
                    lpEnvironment,                   
                    lpCurrentDirectory ? lpCurrentDirectory : TEXT("NULL"),              
                    lpStartupInfo,           
                    lpProcessInformation);
                
    bSuccess = CreateProcess(
                    lpApplicationName,                  
                    lpCommandLine,
                    lpProcessAttributes,
                    lpThreadAttributes,
                    bInheritHandles,                     
                    dwCreationFlags,                     
                    lpEnvironment,                   
                    lpCurrentDirectory,              
                    lpStartupInfo,           
                    lpProcessInformation);
    if (bSuccess) {
        if (lpProcessInformation) {
            LogDetail( TEXT( "CreateProcess: %s - returned hProcess=%08X hThread=%08X ProcessId=%08X ThreadId=%08X"), 
                        szPassed,
                        lpProcessInformation->hProcess, 
                        lpProcessInformation->hThread,
                        lpProcessInformation->dwProcessId,
                        lpProcessInformation->dwThreadId);
        }
        else {
            LogDetail( TEXT( "CreateProcess: %s"), szPassed );
        }
    } else {
        LogError( TEXT("CreateProcess"));
    }

    return bSuccess;
}

HANDLE YCreateEvent(
    LPSECURITY_ATTRIBUTES lpEventAttributes,    // pointer to security attributes  
    BOOL bManualReset,                          // flag for manual-reset event 
    BOOL bInitialState,                         // flag for initial state 
    LPCTSTR lpName                              // pointer to event-object name  
   )
{
    HANDLE hEvent = NULL;

    LogDetail( TEXT("CreateEvent( %08X, %s, %s, %s)\r\n"),
                lpEventAttributes,  
                bManualReset ? TEXT("TRUE") : TEXT("FALSE"),
                bInitialState ? TEXT("TRUE") : TEXT("FALSE"),
                lpName ? lpName : TEXT("NULL")  );
    hEvent = CreateEvent( lpEventAttributes,    
                          bManualReset, 
                          bInitialState,
                          lpName); 

    if (hEvent)  {
        LogDetail( TEXT("CreateEvent: %s - returned %08X\r\n"), szPassed, hEvent);
    } else {
        LogError( TEXT("CreateEven"));
    }
    return(hEvent);
    
}


BOOL YSetEvent(
    HANDLE hEvent   // handle of event object 
   )
{
    BOOL bSuccess = FALSE;

    LogDetail( TEXT("SetEvent( %08X)"), hEvent);

    if (bSuccess = SetEvent( hEvent))  {
        LogDetail( TEXT("SetEvent: %s"), szPassed); 
    } else {
        LogError( TEXT("SetEvent"));
    }

    return(bSuccess);
} 


BOOL YCreateDirectory(
    LPCTSTR lpPathName,                         // pointer to a directory path string
    LPSECURITY_ATTRIBUTES lpSecurityAttributes  // pointer to a security descriptor 
   )
{
    BOOL bSuccess = FALSE;
    TCHAR *szText = TEXT("CreateDirectory");

    LogDetail( TEXT("%s( %s, %08X)"), szText, lpPathName, lpSecurityAttributes);

    if (bSuccess = CreateDirectory( lpPathName, lpSecurityAttributes)) {
        LogDetail( TEXT("%s: %s"), szText, szPassed);   
    } else {
        LogError( szText);
    }
    return(bSuccess);
   
}       
BOOL YRemoveDirectory(
    LPCTSTR lpPathName                          // pointer to a directory path string
   )
{
    BOOL bSuccess = FALSE;
    TCHAR *szText = TEXT("RemoveDirectory");

    LogDetail( TEXT("%s( %s)"), szText, lpPathName);

    if (bSuccess = RemoveDirectory( lpPathName)) {
        LogDetail( TEXT("%s: %s"), szText, szPassed);   
    } else {
        LogError( szText);
    }
    return(bSuccess);
   
}       

HANDLE YGetCurrentProcess(VOID)
{
    HANDLE hProcess;
    TCHAR *szText = TEXT("GetCurrentProcess");

    LogDetail( TEXT("%s()"), szText);
    if (hProcess = GetCurrentProcess()) {
        LogDetail( TEXT("%s: returned %08X"), szText, hProcess);
    } else {
        LogError( szText);
    }
    return(hProcess);
    
}   
DWORD YGetCurrentProcessId(VOID)
{
    DWORD dwProcessId = 0;
    TCHAR *szText = TEXT("GetCurrentProcessId");

    LogDetail( TEXT("%s()"), szText);
    if (dwProcessId = GetCurrentProcessId()) {
        LogDetail( TEXT("%s: returned %08X"), szText, dwProcessId);
    } else {
        LogError( szText);
    }
    return(dwProcessId);
    
}   

HANDLE YGetCurrentThread(VOID)
{
    HANDLE hThread = NULL;
    TCHAR *szText = TEXT("GetCurrentThread");

    LogDetail( TEXT("%s()"), szText);
    if (hThread = GetCurrentThread()) {
        LogDetail( TEXT("%s: returned %08X"), szText, hThread);
    } else {
        LogError( szText);
    }
    return(hThread);
    
}   

DWORD  YGetCurrentThreadId(VOID)
{
    DWORD dwThreadId;
    TCHAR *szText = TEXT("GetCurrentThreadId");

    LogDetail( TEXT("%s()"), szText);
    if (dwThreadId = GetCurrentThreadId()) {
        LogDetail( TEXT("%s: returned %08X"), szText, dwThreadId);
    } else {
        LogError( szText);
    }
    return(dwThreadId);
}   

HINSTANCE YLoadLibrary(
    LPCTSTR lpLibFileName   // String name of the module to load
   )
{
    HINSTANCE hInstance = NULL;
    TCHAR *szText = TEXT("LoadLibrary");

    LogDetail( TEXT("%s( %s)"), szText, lpLibFileName); 
    if ( hInstance = LoadLibrary( lpLibFileName))  {
        LogDetail( TEXT("%s: returned %08X"), szText, hInstance);
    } else {
        LogError( szText);
    }
    return( hInstance);
}   

FARPROC YGetProcAddress(
    HMODULE hModule,    // handle to DLL module  
    LPCTSTR lpProcName  // name of function 
   )
{
    FARPROC lpProc = NULL;
    TCHAR *szText = TEXT("GetProcAddress");

    LogDetail( TEXT("%s( %08X, %s)"), szText, hModule, lpProcName); 
    if ( lpProc = GetProcAddress( hModule, lpProcName))  {
        LogDetail( TEXT("%s: returned %08X"), szText, lpProc);
    } else {
        LogError( szText);
    }
    return( lpProc);
}   

BOOL YGetExitCodeThread(
    HANDLE hThread,     // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   )
{
    BOOL bSuccess = FALSE;
    TCHAR *szText = TEXT("GetExitCodeThread");

    LogDetail( TEXT("%s( %08X, %08X)"), szText, hThread, lpExitCode);   

    if ( bSuccess = GetExitCodeThread( hThread, lpExitCode))  {
        LogDetail( TEXT("%s: returned %08X"), szText, *lpExitCode);
    } else {
        LogError( szText);
    }
    return( bSuccess);
        
}   

BOOL YGetExitCodeProcess(
    HANDLE hProcess,        // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   )
{
    BOOL bSuccess;
    TCHAR *szText = TEXT("GetExitCodeProcess");

    LogDetail( TEXT("%s( %08X, %08X)"), szText, hProcess, lpExitCode);  

    if ( bSuccess = GetExitCodeProcess( hProcess, lpExitCode))  {
        LogDetail( TEXT("%s: returned success: lpExitCode=%08X"), szText, *lpExitCode);
    } else {
        LogError( szText);
    }
    return( bSuccess);
        
}   


BOOL YFreeLibrary( HMODULE hLibModule)
{
    BOOL bSuccess = FALSE;
    TCHAR *szText = TEXT("FreeLibrary");

    LogDetail( TEXT("%s( %08X)"), szText, hLibModule);

    if ( bSuccess = FreeLibrary( hLibModule))  {
        LogDetail( TEXT("%s: Passed"), szText);
    } else {
        LogError( szText);
    }
    return( bSuccess);
}   


#endif // NO_YCALLS
