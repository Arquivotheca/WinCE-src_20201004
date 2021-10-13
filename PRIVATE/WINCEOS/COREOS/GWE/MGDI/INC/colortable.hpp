//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/

#ifndef __COLOR_TABLE_HPP_INCLUDED__
#define __COLOR_TABLE_HPP_INCLUDED__

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
	InitStockColorTables(
		void
		);

	ColorTable_t(
		unsigned int	PaletteType,
		USHORT			Mutable,
		USHORT			CntPaletteEntries,
		PALETTEENTRY*	pPaletteEntries
		);


    static
	ColorTable_t*
	Stock(
		unsigned long	BitmapFormat
		);


    //	Refcounts (if possible) or allocs (if necessary) a ColorTable_t
    //	matching the given palette entries, palette type, etc.
    static
	BOOL
	ColorTableFromBitmapinfo(
		const BITMAPINFO*	pbitmapinfo,
		UINT				iUsage,
		BOOL				Mutable,
		ColorTable_t**		ppColorTable
		);

    static
	ColorTable_t*
	ColorTableFromPalentries(
		int				CntPaletteEntries,
		PALETTEENTRY*	pPaletteEntries,
		unsigned int	PaletteType,
		BOOL			Mutable
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

	bool
	Indexed(
		void
		);


private:

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

	unsigned int	m_PaletteType;		//	PAL_INDEXED, PAL_BITFIELDS, PAL_RGB, PAL_BGR
	IUNIQ			m_iuniq;			//	Unique ID for this colortable
	USHORT			m_Mutable;			//	TRUE iff palette entries can be changed. (Really BOOL, USHORT for packing)
	USHORT			m_RefCnt;
	USHORT			m_CntPaletteEntries;
	PALETTEENTRY*	m_pPaletteEntries;
	ColorTable_t*	m_pNext;

};

#endif  // !__COLORTBL_H__
