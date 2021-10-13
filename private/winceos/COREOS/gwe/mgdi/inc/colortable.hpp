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


*/

#ifndef __COLOR_TABLE_HPP_INCLUDED__
#define __COLOR_TABLE_HPP_INCLUDED__

// Used to signify if the color values in the surface have NOT been
// multiplied in advance by the alpha value. (We use this flag as a
// negative becuase GDI AlphaBlend defaults to premultiplied.) This
// is NOT exposed to display drivers at the moment: it is only used
// by DirectDraw.

#define PAL_ALPHANOTMULT  0x00000020

// The bits that are sent to the display driver are masked with this define.

#define PAL_PUBLICMASK 0x0000001F

// IUNIQ is the type of the m_iUniq member of ColorTable_t.  It needs to
// be a USHORT to match the published DDI.
typedef USHORT IUNIQ;


/*


    ColorTable_t:

    A color table stores the color information associated with either a
    palette or a bitmap's colortable.

    Since palettes' entries are settable and bitmaps' colortables
    are not, each color table says whether or not it is mutable.
    (Note that the stock palettes' color tables are also immutable.)

    Immutable color tables can be shared by many clients; we implement
    a refcounting scheme to make this safe.

    Eventually, two color tables are glued together to make an XLATEOBJ,
    which is the data structure passed to the driver to allow for
    color conversion blts.

    Each unique color table has a corresponding iUniq member.  The
    iUniq field can be used as a very efficient equality test
    between two color tables.

    Color tables are used by two kinds of devices -- palettized and
    non-palettized.  This differentiated by the PALTYPE field.

-------------------------------------------------------------------*/

class ColorTable_t
{
public:

    static
    void
    Initialize(
        void
        );


    ColorTable_t(
        unsigned int    PaletteType,
        USHORT          Mutable,
        USHORT          CntPaletteEntries,
        PALETTEENTRY*   pPaletteEntries
        );


    static
    ColorTable_t*
    Stock(
        unsigned long BitmapFormat
        );


    //    Refcounts (if possible) or allocs (if necessary) a ColorTable_t
    //    matching the given palette entries, palette type, etc.
    static
    BOOL
    ColorTableFromBitmapinfo(
        const BITMAPINFO*   pbitmapinfo,
        UINT                iUsage,
        BOOL                Mutable,
        BOOL                AllowRotation,
        ColorTable_t**      ppColorTable
        );

    static
    ColorTable_t*
    ColorTableFromPalentries(
        int             CntPaletteEntries,
        PALETTEENTRY*   pPaletteEntries,
        unsigned int    PaletteType,
        BOOL            Mutable
        );

    void
    IncRefcnt(
        void
        );

    void
    DecRefcnt(
        void
        );


    //REVIEW the color table should do this itself if its contents are changed.
    // When a mutable ColorTable_t's contents are changed, the changer
    // is required to call UpdateIuniq.  If this isn't done, the
    // driver won't know that this color table's entries have changed
    // and might use a stale color conversion table.
    void
    UpdateIuniq(
        void
        );

    USHORT
    CEntries(
        void
        ) const;


    PALETTEENTRY*
    PPalentries(
        void
        ) const;

    IUNIQ
    Iuniq(
        void
        ) const;

    unsigned int
    Paltype(
        void
        ) const;

    BOOL
    Mutable(
        void
        ) const;

    BOOL
    Indexed(
        void
        ) const;

    bool
    IsSameFormat(
        const ColorTable_t* pColorTable
        ) const;

    BOOL
    Premultiplied(
        void
        ) const;

#ifdef BUILDING_IN_CEDEBUGX
public:
#else
private:
#endif

    static
    void
    InitStockColorTables(
        void
        );


    ~ColorTable_t(
        void
        );

    void
    AddToPool(
        void
        );
    void
    RemoveFromPool(
        void
        );

    IUNIQ
    IuniqNext(
        void
        );

    unsigned int    m_PaletteType;      //    PAL_INDEXED, PAL_BITFIELDS, PAL_RGB, PAL_BGR
    UINT32          m_RefCnt;
    PALETTEENTRY*   m_pPaletteEntries;
    ColorTable_t*   m_pNext;

    IUNIQ           m_iuniq;            //    Unique ID for this colortable
    USHORT          m_Mutable;          //    TRUE iff palette entries can be changed. (Really BOOL, USHORT for packing)
    USHORT          m_CntPaletteEntries;

};

#endif  // !__COLORTBL_H__
