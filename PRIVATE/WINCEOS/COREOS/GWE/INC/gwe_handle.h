//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once

#pragma warning(push)
#pragma warning(disable:4201) // nameless unions are part of C++

#include <windows.h>
#include <guts.h>
#include <ehm.h>

/*
GWE Handles:

Overview:
    GWE Handles are handles to any system object within Windows CE. The handles (GWEHandle) are stored in a 
    table (HandleTable) and are managed using 3 operations: CreateHandle, RemoveHandle, and LookupHandle.

Usage:
    A single handle table (HandleTable) can be used to hold any type of system object pointer. A wrapper is 
    used to provide functionality  (as per the current useage for HWND, please see usage in gwe_s.cpp). 
    Basically, wrapper functions for the create, lookup and remove are made to handle external handles 
    (such as HWND) and then converts it to a internal GWEHandle.

Design:
    The table is implemented as a 2 level dynamically allocated table. The top level is a fixed size, but the 
    secondary level tables are allocated/deallocated as needed.
    The primary table holds metadata elements for the secondary tables (including a ptr, and usage information)
    Each secondary table holds metadata for each handle (including a object ptr and reuse cnt)
    
    The GWEHandle used to reference the table is composed of several parts:
        prim_idx    -- the value used to index the primary table
        sec_idx     -- the value used to index the secondary table
        reuse       -- the value used to prevent stale handle occurances
                       (increments each time that table entry is used)
        chk         -- a single bit at the LSB set to 1, to discourage casting 
                       of HWND to CWindow() or vice versa
    Overall the size of the GWEHandle is 4 bytes, which is equal to external handles to provide compatibility.
    
*/ 

#define __REUSE_BITS 14
#define __RESERVED_BITS 1
#define __CHK_BITS   1
#define __HIDX_BITS  16

#define __PRIM_IDX_BITS 7
#define __SEC_IDX_BITS  9

#define __PRIM_HTABLE_SIZE  ( 1 << __PRIM_IDX_BITS )
#define __SEC_HTABLE_SIZE   ( 1 << __SEC_IDX_BITS )

// Handle structure
//   composed of an index, a reuse count, a chk bit = 1
//   total size is 4 bytes (the same size as a HWND)
struct GWEHandle
{
    union
    {
        unsigned int hid;
        struct
        {
            unsigned short int reserved : __RESERVED_BITS;
            // The reserved bit is used for Activation(Restore) purposes            
            //      - this is needed for the HWND|0x1 SetForegroundWindow trick
            //        and is ultimately used in MsgQueue_SetActiveWindowInternal
            unsigned short int chk      : __CHK_BITS;
            // The check bit (set to 1) is used to protect against HWND dereferencing
            unsigned short int reuse    : __REUSE_BITS;
            // the reuse bits are for maintaining the reuse count for this handle
            union
            {
                // Both primary and secondary index
                unsigned short int idx : __HIDX_BITS;
                struct
                {
                    // Secondary table index
                    unsigned short int sec_idx : __SEC_IDX_BITS; 
                    // Primary table index
                    unsigned short int prim_idx : __PRIM_IDX_BITS;                     
                };
            };
        };
    };   
};
CASSERT(sizeof(GWEHandle)==sizeof(HWND));

// Handle meta data
struct HandleMetaData 
{
    // A handle will be either of 2 states:
    //  (1) IN USE
    //      pvObj is used to point to the referenced data
    //      fHandleFree equals FALSE
    //  (2) FREE
    //      idxNextFreeHandle points to the next free handle (Linked List)
    //      fHandleFree equals TRUE
    union
    {
        void *pvObj;
        unsigned int idxNextFreeHandle;
    };
    unsigned short nReuseCnt  :__REUSE_BITS;
    unsigned short fHandleFree: 1;
};

// A block of handle meta data objects
struct SecHTable 
{
    HandleMetaData  rgHandles [ __SEC_HTABLE_SIZE ];
};

// Book keeping information for a block of handles
struct SecHTableMetaData 
{
    SecHTable      *pSecHTable;
    struct
    {
        unsigned int    fSecHTableFree          : 1;
        unsigned int    idxNextFreeSecHTable    : __PRIM_IDX_BITS;
        unsigned int    idxLastFreeSecHTable    : __PRIM_IDX_BITS;
        unsigned int    nUnused                 : (32 - (__PRIM_IDX_BITS + __PRIM_IDX_BITS + 1));
    };
    unsigned int    nFreeHandles;
    unsigned int    idxFreeHandleHead;
    unsigned int    idxFreeHandleTail;
};

// Handle Table
//   contains an array of handle blocks that are allocated/deallocated as needed
//   each handle block contains a block of handle meta data object that reference a system object
class HandleTable 
{
public:
    static const UINT32 PRIM_IDX_BITS       = __PRIM_IDX_BITS;
    static const UINT32 SEC_IDX_BITS        = __SEC_IDX_BITS;
    static const UINT32 HIDX_BITS           = __HIDX_BITS;
    static const UINT32 CHK_BITS            = __CHK_BITS;
    static const UINT32 REUSE_BITS          = __REUSE_BITS;
   
    static const UINT32 PRIM_HTABLE_SIZE    = __PRIM_HTABLE_SIZE;
    static const UINT32 SEC_HTABLE_SIZE     = __SEC_HTABLE_SIZE;

    static const UINT32 GWEHANDLE_XOR_KEY   = 0x50425042;

protected:
    CRITICAL_SECTION    csHTable;
    unsigned int        nFreePriHTable;
    unsigned int        idxFreePriHTableHead;
    unsigned int        idxFreePriHTableTail;
    SecHTableMetaData   rgSecHTables [ __PRIM_HTABLE_SIZE ];

    // used to encode/decode the hnd to make the handles seem more 'random'
    inline GWEHandle ConvertToInternal(GWEHandle hnd);
    inline GWEHandle ConvertToExternal(GWEHandle hnd);

    /// <summary>
    /// Allocates and creates a secondary level handle table and links it to the provided metadata element
    /// </summary>
    /// <param name="idxSecHTable">A index to a SecHTableMetaData element where the SecHTable should be created</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT CreateSecondaryTable(unsigned int idxSecHTable);

    /// <summary>
    /// Removes and deallocates a secondary level handle table provided in the metadata element
    /// </summary>
    /// <param name="idxSecHTable">A index to a SecHTableMetaData element where the SecHTable should be removed</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT RemoveSecondaryTable(unsigned int idxSecHTable);

    /// <summary>
    /// Checks if the given handle is valid, and returns the object pointer if a ppvObj is provided
    /// </summary>
    /// <param name="hnd">The GWEHandle to be validated</param>
    /// <param name="ppvObj">A pointer to a voidpointer where the object represented by the handle should be returned to (optional)</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT CheckValidHandle(GWEHandle hnd, __out_opt void **ppvObj = NULL);

    /// <summary>
    /// Allocates/reserves handles with specific handle ids that are not to be used [ used in Initialize() ]
    /// </summary>
    /// <param name="hnd">The GWEHandle to be added</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT CreateNULLHandle(GWEHandle hnd);

public:
    /// <summary>
    /// new operator overload for HandleTable class
    /// </summary>
    void* operator new (size_t cb);

    /// <summary>
    /// delete operator overload for HandleTable class
    /// </summary>
    void operator delete (void*);

    /// <summary>
    /// Initializes the instance of the HandleTable class
    /// </summary>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    virtual HRESULT Initialize();

    /// <summary>
    /// Releases the resources and handles of the HandleTable
    /// </summary>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    virtual HRESULT Release();

    /// <summary>
    /// Creates a handle given a pointer to return the handle to and a system object to create the handle for
    /// </summary>
    /// <param name="pHnd">An output pointer to a Handle for the resulting Handle to be returned at</param>
    /// <param name="pvObj">A pointer to the system object the handle is being created for</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT CreateHandle(__out GWEHandle *pHnd, __in void *pvObj);

    /// <summary>
    /// Removes a handle entry from the table given a handle
    /// </summary>
    /// <param name="hnd">The GWEHandle to be removed from the table</param>
    /// <returns>
    /// HRESULT indicating success of the operation
    /// </returns>
    HRESULT RemoveHandle(GWEHandle hnd);

    /// <summary>
    /// Does a lookup on a pointer to a system object given the handle to the object
    /// </summary>
    /// <param name="hnd">The handle of the object</param>
    /// <returns>
    /// A void* is returned holding a pointer to the object. 
    /// A return value of NULL represents that an invalid handle was provided.
    /// </returns>
    void*   LookupHandle(GWEHandle hnd);  

    UINT32  DEBUG_GetNumberHandles();
    UINT32  DEBUG_GetNumberBlocks();
    HRESULT DEBUG_SanityCheck();
};


#undef  __REUSE_BITS
#undef  __CHK_BITS
#undef  __HIDX_BITS
#undef  __PRIM_IDX_BITS
#undef  __SEC_IDX_BITS
#undef  __SEC_HTABLE_SIZE
#undef  __PRIM_HTABLE_SIZE

#pragma warning(pop)


#ifdef SHIP_BUILD

#define DBGCHK_RETAIL(exp) ((void)0)
#define ASSERT_RETAIL(exp) ((void)0)

#else // SHIP_BUILD

#define DBGCHK_RETAIL(exp) \
   ((void)((exp)?1:(          \
       NKDbgPrintfW ( TEXT("DBGCHK_RETAIL failed in file %s at line %d \r\n"), \
                 TEXT(__FILE__) ,__LINE__ ),    \
       DebugBreak(), \
       0  \
   )))


#define ASSERT_RETAIL(exp) DBGCHK_RETAIL(exp)

#endif // SHIP_BUILD
