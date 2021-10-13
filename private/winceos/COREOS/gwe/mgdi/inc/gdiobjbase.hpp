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
/*
    @class GDIOBJ | Virtual base class for all GDI objects.

    @access Public Members

    @cmember INT16 | m_nCount || Reference count.

        A reference count value of -1 prevents any reference counting.
        This is used to indicate that the object is a stock object
        and should never be deleted.

    @cmember UINT16 | m_nIndex || Index to handle table.

    @cmember static HTABLE * | m_pHTable || Handle table.

    @cmember void | GDIOBJ | () | Constructor.
    @cmember void | ~GDIOBJ | () | Destructor.
    @cmember ULONG | Increment | () | Increment reference count.
    @cmember ULONG | Decrement | () | Decrement reference count.
    @cmember virtual BOOL | DeleteObject | () | Delete object.
    @cmember virtual int | GetObject | (int,PVOID) | Get object data.
    @cmember virtual DWORD | GetObjectType | () | Get type of object.
    @cmember virtual GDIOBJ * | SelectObject | (DC *) | Select object into DC.

    @comm This base class provides reference counting for GDI objects.
        When a GDI object such as a pen or brush is selected
        into a device context, it cannot be deleted.
        Regions objects are one exception to this, because they are copied
        by GDI and can therefore be deleted immediately after being selected
        into a device context.

        Stock objects have a negative m_nCount.
*/

#ifndef __GDI_OBJ_BASE_HPP_INCLUDED__
#define __GDI_OBJ_BASE_HPP_INCLUDED__

#include <windows.h>

class DC;
class Region_t;
class Pen_t;
class Brush_t;

class BitmapBase_t;
class WBITMAP;
class MBITMAP;
class Font_t;
class Palette_t;
class Metafile_t;
class GdiMonitor_t;
class Driver_t;
class Icon_t;

class DDrawDevice_t;
class DDrawSurface_t;
class DDrawClipper_t;
class DDrawColorCtrl_t;
class DDrawGammaCtrl_t;
class DDrawVideoPort_t;

enum GdiObjectHandleVerify_t
{
    DoNotVerifyAgainstHandleTable,
    VerifyAgainstHandleTable
};


enum GdiObjectType_t
{
    GdiPenType            =   OBJ_PEN,
    GdiBrushType          =   OBJ_BRUSH,
    GdiDcType             =   OBJ_DC,
    GdiPaletteType        =   OBJ_PAL,
    GdiFontType           =   OBJ_FONT,
    GdiBitmapType         =   OBJ_BITMAP,
    GdiRegionType         =   OBJ_REGION,
    GdiMemDcType          =   OBJ_MEMDC,
    GdiEnhMetaDcType      =   OBJ_ENHMETADC,
    GdiEnhMetafileType    =   OBJ_ENHMETAFILE,
    GdiDDrawDeviceType    =   OBJ_DDRAWDEVICE,
    GdiDDrawSurfaceType   =   OBJ_DDRAWSURFACE,
    GdiDDrawClipperType   =   OBJ_DDRAWCLIPPER,
    GdiDDrawColorCtrlType =   OBJ_DDRAWCOLORCTRL,
    GdiDDrawGammaCtrlType =   OBJ_DDRAWGAMMACTRL,
#ifndef BUILDING_IN_CEDEBUGX
    GdiDDrawVideoPortType =   OBJ_DDRAWVIDEOPORT,
#endif
    GdiIconType           =   OBJ_ICON,
    GdiMonitorType                           // GdiMonitorType doesn't map to an OBJ_ define.
};


class GdiObjectHandle_t
{

private:

    friend    class GdiHandleTable_t;

    union
    {
        struct
        {
            unsigned int    m_TableIndex:16; // Keep this at 16bits or lower so that 
            unsigned int    m_nUnused:1;
            unsigned int    m_ReuseCount:10;
            unsigned int    m_nType:5; // Stores the GdiObjectType_t information. 
        };
        void*               m_p;
    };


public:

    GdiObjectHandle_t(
        void
        );


    GdiObjectHandle_t(
        void*,
        GdiObjectHandleVerify_t
        );


    GdiObjectHandle_t(
        unsigned int    TableIndex,
        unsigned int    ReuseCount,
        GdiObjectType_t Type
        );


    void
    SetUnverified(
        void*
        );


    HGDIOBJ
    AsHGDIOBJ(
        void
        ) const;


    void
    SetToNull(
        void
        );

    bool
    IsNull(
        void
        ) const;

    int
    TableIndex(
        void
        );

    int
    Type(
        void
        );

    void*
    AsVoidPtr(
        void
        ) const;

    friend
    bool
    operator ==(
        const    GdiObjectHandle_t&    GdiObjectHandle1,
        const    GdiObjectHandle_t&    GdiObjectHandle2
        );



};


class GDIOBJ
{
private:

protected:

    GdiObjectType_t      m_GdiObjectType;
    GdiObjectHandle_t    m_GdiObjectHandle;


public:


    INT16                m_nCount;           // reference count

    GDIOBJ(
        GdiObjectHandle_t,
        GdiObjectType_t
        );

    virtual
    ~GDIOBJ(
        void
        );

    GdiObjectHandle_t
    GdiObjectHandle(
        void
        ) const;


    void
    RemoveFromHandleTable(
        void
        );


    void
    SetHandleToNull(
        void
        );


    DC*
    DowncastToDC(
        void
        );


    Region_t*
    DowncastToRGN(
        void
        );


    BitmapBase_t*
    DowncastToVBITMAP(
        void
        );

    MBITMAP*
    DowncastToMBITMAP(
        void
        );

    BITMAP*
    DowncastToBITMAP(
        void
        );


    Brush_t*
    DowncastToBRUSH(
        void
        );

    Palette_t*
    DowncastToPALETTE(
        void
        );

    Font_t*
    DowncastToFONT(
        void
        );

    Pen_t*
    DowncastToPEN(
        void
        );

    Metafile_t*
    DowncastToMetafile(
        void
        );

    GdiMonitor_t*
    DowncastToMonitor(
        void
        );

    DDrawDevice_t*
    DowncastToDDrawDevice(
        void
        );

    DDrawSurface_t*
    DowncastToDDrawSurface(
        void
        );

    DDrawClipper_t*
    DowncastToDDrawClipper(
        void
        );

    DDrawColorCtrl_t*
    DowncastToDDrawColorCtrl(
        void
        );

    DDrawGammaCtrl_t*
    DowncastToDDrawGammaCtrl(
        void
        );

    DDrawVideoPort_t*
    DowncastToDDrawVideoPort(
        void
        );

    Icon_t*
    DowncastToIcon(
        void
        );


    ULONG
    Increment(
        void
        );

    ULONG
    Decrement(
        void
        );



    BOOL
    IsStockObject(
        void
        );


    virtual
    BOOL
    DeleteObject(
        void
        );

    virtual
    int
    GetObject(
        int      CntBytesBuffer,
        void*    pObject
        ) = 0;


    GdiObjectType_t
    GetObjectType_I(
        void
        );

    virtual
    GDIOBJ*
    SelectObject(
        DC*
        ) = 0;

#if DEBUG
virtual int     Dump (void) = 0;
#endif

};



class GdiHandleTableEntry_t;

class GdiHandleTable_t
{
#ifdef BUILDING_IN_CEDEBUGX
public:
#else
private:
#endif

    friend  class GDIOBJ;
    friend  class GdiHandleTableEnumerator_t;

    static const UINT MIN_GDI_OBJECT_TYPE = GdiPenType;     // Lowest occupied index in s_pHandleTables
    static const UINT MAX_GDI_OBJECT_TYPE = GdiMonitorType; // Highest occupied index in s_pHandleTables

    static GdiHandleTable_t* s_pHandleTables[MAX_GDI_OBJECT_TYPE+1]; // Array of GDI handle tables for all object types.

    const UINT              m_nGrowRate; // Rate at which the table grows upon resizing.

    GdiHandleTableEntry_t*  m_pHandleTableEntries; // Array to hold handle entries.
    UINT                    m_nNumberOfEntries; // Number of allocated entries in the array.

    UINT                    m_nFirstFreeIndex;
    UINT                    m_nLastFreeIndex;

    
    static
    void
    SetGdiObject(
        GdiObjectHandle_t,
        GDIOBJ*
        );

    static
    GdiHandleTable_t*
    GetTable(
        const UINT nType
        );

    static
    void
    CleanDC(
        const DWORD dwDyingProcess,
        const GdiObjectType_t objType
        );

    GdiHandleTable_t(
        const UINT nInitialSize, 
        const UINT nGrowRate
        );

    void
    RemoveFromHandleTable(
        GdiObjectHandle_t    GdiObjectHandle
        );

    void
    Grow(
        const UINT nGrowSize
        );

    UINT
    IndexInUse(
        GdiObjectHandle_t    GdiObjectHandle
        ) const;

    GDIOBJ*
    GET_pGDIOBJ(
        const UINT nIndex
        ) const;


    GdiHandleTable_t (const GdiHandleTable_t&);
    GdiHandleTable_t& operator=(const GdiHandleTable_t&);

public:

    static
    void
    Initialize(
        void
        );


    static
    GdiObjectHandle_t
    ReserveHandle(
        const GdiObjectType_t objType
        );


    static
    void
    ReleaseUnsetHandle(
        GdiObjectHandle_t    GdiObjectHandle
        );
    

    static
    void
    ProcessCleanup(
        HPROCESS    hprc
        );


    static
    BOOL
    DeleteObject_I(
        HGDIOBJ hgdiobj
        );


    static
    GDIOBJ*
    GET_pGDIOBJ(
        HANDLE
        );

    static
    GDIOBJ*
    GET_pGDIOBJ(
        GdiObjectHandle_t
        );


    static
    GDIOBJ*
    GET_pGDIOBJ(
                HANDLE,
        const    WCHAR*    pWarningText
        );


    static
    int
    GetObjectW_I(
        HGDIOBJ    hgdiobj,
        int        cbBuffer,
        LPVOID     lpvObject
        );


    static
    DWORD
    GetObjectType_I(
        HGDIOBJ hgdiobj
        );



    // In very extreme circumstances (basically, the 16-bit metafile code
    // ported from NT and the process cleanup code), we need to regenerate
    // a handle given only an index.
    //
    // *******************************************************************
    // ****  This is dangerous and should be avoided where possible!  ****
    // *******************************************************************
    static
    HANDLE
    HANDLE_FROM_INDEX_ONLY(
        GdiObjectType_t objType,
        UINT nTableIndex
        );

    static
    void
    SetHandleProcessToGweProcess(
        HANDLE h
        );

    static
    BOOL
    SetObjectOwnerProcess(
        HGDIOBJ     hgdiobj,
        HPROCESS    hprc
        );


    static
    void
    GdiClipDCForSettingChange(
        void
        );


    static
    void
    GdiFontSmoothingChange(
        void
        );

    static
    bool
    CanCallerDelete(
        HGDIOBJ    hgdiobj
        );

    static
    void
    CoopLvl_DisableAllPrimaryDCs(
        Driver_t * pDriver,
        BOOL       NewState
        );

    static
    void
    CoopLvl_DestroyActivePrimaryDCs(
        Driver_t * pDriver
        );

    static
    DWORD
    GetOwnerProcess(
        GdiObjectHandle_t    GdiObjectHandle
        );

#if DEBUG
    static
    void
    DumpHandleTable(
        HGDIOBJ hgdiobj
        );
#endif

};


class GdiHandleTableEnumerator_t
{
public:
    GdiHandleTableEnumerator_t(const GdiObjectType_t Type);

    BOOL
    Next(
        GDIOBJ **ppObj
        );

    BOOL
    Reset();

private:
    const GdiHandleTable_t*      m_pTable;       // Target table to iterate over.
    const GdiHandleTableEntry_t* m_pLastEntry;   // Last valid entry in thr given table.
    GdiHandleTableEntry_t*       m_pCurrentEntry;// Current position of interation.


    friend class GdiHandleTable_t;
};

#endif

