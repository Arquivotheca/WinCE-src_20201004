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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsmain.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udf.h"

#define REGSTR_PATH_UDFS     L"System\\StorageManager\\UDFS"

PUDF_DRIVER_LIST g_pHeadFSD = NULL;          
CRITICAL_SECTION g_csMain;
HANDLE           g_hHeap = NULL;

// /////////////////////////////////////////////////////////////////////////////
// operator new
//
// Enable allocation from the volume heap using a tag to identify the contents.
//
void *operator new(size_t stAllocateBlock, POOL_TYPE Tag) 
{ 
    PVOID pReturn = NULL;

    // NOTENOTE: Tag is currently ignored.

    //
    // TODO::Check if we are over a presribed amount of memory and attempt to
    // cleanup the file manager if we are.
    //

#ifdef _PREFAST_
#pragma prefast( disable : 5448 )
#endif
    if( g_hHeap )
    {
        pReturn = HeapAlloc( g_hHeap, 
                             0, 
                             stAllocateBlock );
    }

#ifdef _PREFAST_
#pragma prefast( enable : 5448 )
#endif
    //
    // TODO::If this failed and we are returning zero, then try to cleanup the
    // volumes and retry the allocation.
    //

    if( pReturn )
    {
        ZeroMemory( pReturn, stAllocateBlock );
        pReturn = (BYTE*)pReturn;
    }

    return pReturn;
}

// /////////////////////////////////////////////////////////////////////////////
// operator new
//
// Allocate an array from the volume heap.
//
void *operator new[](size_t stAllocateBlock, POOL_TYPE Tag) 
{ 
    return operator new(stAllocateBlock, Tag);
}

// /////////////////////////////////////////////////////////////////////////////
// operator delete
//
// Delete memory from the volume heap, accounting for the pool type tag.
//
void operator delete(void *pvMem) 
{
    PVOID pDelete = pvMem;

    pDelete = (BYTE*)pDelete;

    PREFAST_DEBUGCHK(NULL != g_hHeap);
    HeapFree(g_hHeap, 0, pDelete);
}

// /////////////////////////////////////////////////////////////////////////////
// operator delete[]
//
// Delete an array from the volume heap.
//
void operator delete[](void *pvMem) 
{
    return operator delete(pvMem);
}

// /////////////////////////////////////////////////////////////////////////////
// DllMain
//
BOOL WINAPI DllMain( HANDLE DllInstance, DWORD Reason, LPVOID Reserved )
{
    switch( Reason ) 
    {
    case DLL_PROCESS_ATTACH:

        DisableThreadLibraryCalls( (HMODULE)DllInstance );
        DEBUGREGISTER((HINSTANCE) DllInstance);

        InitializeCriticalSection( &g_csMain );

#ifdef DEBUG
        RegisterDbgZones((HMODULE)DllInstance, &dpCurSettings);
#endif

        //
        // TODO::Read in the initial and maximum volume heaps from the registry.
        //

        //
        // We have to create the heap a quickly as possible for any future 
        // allocation.
        //
        g_hHeap = HeapCreate( 0, DEFAULT_INITIAL_HEAP_SIZE, DEFAULT_MAX_HEAP_SIZE );
        if( !g_hHeap )
        {
            DEBUGMSG(ZONE_INIT,(TEXT("UDFS!CVolume::Initialize - Unable to allocate heap.\r\n")));
            DeleteCriticalSection( &g_csMain );
            return FALSE;
        }

        DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_ATTACH\n")));
        return TRUE;

    case DLL_PROCESS_DETACH:
        //
        // TODO::Delete all volumes.
        //

        HeapDestroy( g_hHeap );
        
        DeleteCriticalSection( &g_csMain );

        DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_DETACH\n")));
        return TRUE;

    default:
        DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d ignored\n"), Reason));
        return TRUE;
    }

    DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d failed\n"), Reason));
    
    return FALSE;
}


// /////////////////////////////////////////////////////////////////////////////
// FSD_MountDisk
//
// Arguments:  
//    hdsk == FSDMGR disk handle, or NULL to deinit all
//    frozen volumes on *any* disk that no longer has any open
//    files or dirty buffers.
//
//
extern "C" BOOL FSD_MountDisk( HDSK hDsk )
{
    CVolume* pVolume;
    BOOL fRet;

    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!MountDisk hDsk=%08X\r\n"), hDsk));

    // Sleep(  );

    //
    //  Create the volume class
    //
    pVolume = new (UDF_TAG_VOLUME ) CVolume();

    if( pVolume == NULL ) 
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }
    
    fRet = pVolume->Initialize( hDsk );

    if( fRet ) 
    {
        //
        // TODO::Register the volume with the FSD manager.
        //
        
        EnterCriticalSection( &g_csMain );
        
        UDF_DRIVER_LIST *pUDFEntry = new (UDF_TAG_DRIVER) UDF_DRIVER_LIST;
        
        if( pUDFEntry ) 
        {
            pUDFEntry->hDsk = hDsk;
            pUDFEntry->pVolume = pVolume;
            pUDFEntry->pUDFSNext  = g_pHeadFSD;
            g_pHeadFSD = pUDFEntry;
        } 
        else 
        {
            delete pVolume;
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            fRet = FALSE;
        }
        
        LeaveCriticalSection( &g_csMain );
    } 
    else 
    {
        delete pVolume;
    }
    
    return fRet;
}


// /////////////////////////////////////////////////////////////////////////////
// FSD_UnmountDisk
//
// Arguments:  
//    hdsk == FSDMGR disk handle, or NULL to deinit all
//    frozen volumes on *any* disk that no longer has any open
//    files or dirty buffers.
//
//
extern "C" BOOL FSD_UnmountDisk( HDSK hDsk )
{
    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!UnmountDisk hDsk=%08X\r\n"), hDsk));
    
    EnterCriticalSection( &g_csMain );
    
    PUDF_DRIVER_LIST pUDFEntry = g_pHeadFSD;
    PUDF_DRIVER_LIST pUDFPrev = NULL;
    
    while( pUDFEntry ) 
    {
        if( pUDFEntry->hDsk == hDsk ) 
        {
            if( pUDFPrev ) 
            {
                pUDFPrev->pUDFSNext = pUDFEntry->pUDFSNext;
            } 
            else 
            {
                g_pHeadFSD = pUDFEntry->pUDFSNext;
            }
            break;
        }
        
        pUDFPrev = pUDFEntry;
        pUDFEntry = pUDFEntry->pUDFSNext;
    }
    
    LeaveCriticalSection( &g_csMain );
    
    if( pUDFEntry ) 
    {
        if( pUDFEntry->pVolume ) 
        {
            pUDFEntry->pVolume->Deregister();
            
            delete pUDFEntry->pVolume;
            pUDFEntry->pVolume = NULL;
        }
        
        delete pUDFEntry;
        return TRUE;
    }

    return FALSE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetChecksum
//
Uint8 GetChecksum( const DESTAG& Tag )
{
    const Uint8* pData = (Uint8*)&Tag;
    Uint8  nSum = 0;
    
    for( int i = 0; i < 16; i++ )
    {
        nSum += ( i != 4 ) ? pData[i] : 0;
    }

    return nSum;
}

// /////////////////////////////////////////////////////////////////////////////
// VerifyDescriptor
//
BOOL VerifyDescriptor( Uint32 dwSector, BYTE* pBuffer, Uint32 dwBufferSize )
{
    ASSERT(pBuffer != NULL);
    ASSERT(dwBufferSize > sizeof(DESTAG));

    PDESTAG pDestag = (PDESTAG)pBuffer;
    
    //
    // First validate the checksum.
    //
    if( pDestag->Checksum != GetChecksum( *pDestag ) )
    {
        DEBUGMSG(ZONE_INIT, (TEXT("UDFS!VerifyDescriptor Destag checksum is incorrect")));
        return FALSE;
    }

    if( pDestag->CRCLen > dwBufferSize - sizeof(DESTAG) )
    {
        DEBUGMSG(ZONE_INIT,(TEXT("UDFS!VerifyDescriptor The CRC length is too long.\n")));
        return FALSE;
    }

    //
    // TODO::How should we use the CRCLength here?  Do we use the buffer length
    // passed in, or the CRC length, or both?
    // Check the descriptor CRC.
    //
    if( pDestag->CRC != osta_crc( pBuffer + sizeof(DESTAG), 
                                  pDestag->CRCLen ) )
    {
        DEBUGMSG(ZONE_INIT, (TEXT("UDFS!VerifyDescriptor CRC is not correct")));
        return FALSE;
    }
        
    //
    // Check the descriptor tag location.
    //
    if( pDestag->Lbn != dwSector )
    {
        //
        // The Longhorn driver is currently writing an invalid value for this
        // on file identifier descriptors.
        //
        // I've reported the issue to them, but haven't heard back yet.  I'll 
        // add this hack for now so that we can read their images.
        //
        if( pDestag->Ident != DESTAG_ID_NSR_FID )
        {
            DEBUGMSG(ZONE_WARNING, (TEXT("UDFS!VerifyDescriptor Destag Location is incorrect: Actual(0x%x), Expected(0x%x)"), pDestag->Lbn, dwSector));
            return FALSE;
        }
    }
    
    return TRUE;
}
