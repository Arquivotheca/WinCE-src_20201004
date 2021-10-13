//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

class	DC;
class	Region_t;
class	Pen_t;
class	Brush_t;

class	BitmapBase_t;
class	WBITMAP;
class	MBITMAP;
class	Font_t;
class	Palette_t;
class	Metafile_t;


enum GdiObjectHandleVerify_t
{
	DoNotVerifyAgainstHandleTable,
	VerifyAgainstHandleTable
};


class GdiObjectHandle_t
{

private:

	friend	class GdiHandleTable_t;

	union
		{
		struct
			{
			unsigned int	m_TableIndex:17;
			unsigned int	m_ReuseCount:10;
			unsigned int	m_Unused:5;
			};
		void*				m_p;
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
		unsigned int	TableIndex,
		unsigned int	ReuseCount
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

	void*
	AsVoidPtr(
		void
		) const;

	friend
	bool
	operator ==(
		const	GdiObjectHandle_t&	GdiObjectHandle1,
		const	GdiObjectHandle_t&	GdiObjectHandle2
		);



};


enum GdiObjectType_t
{
	GdiPenType			=	OBJ_PEN,
	GdiBrushType		=	OBJ_BRUSH,
	GdiDcType			=	OBJ_DC,
	GdiPaletteType		=	OBJ_PAL,
	GdiFontType			=	OBJ_FONT,
	GdiBitmapType		=   OBJ_BITMAP,
	GdiRegionType		=	OBJ_REGION,
	GdiMemDcType		=   OBJ_MEMDC,
	GdiEnhMetaDcType	=	OBJ_ENHMETADC,
	GdiEnhMetafileType	=   OBJ_ENHMETAFILE

};


class GDIOBJ
{
private:

protected:

	GdiObjectType_t		m_GdiObjectType;
	GdiObjectHandle_t	m_GdiObjectHandle;


public:


	INT16				m_nCount;           // reference count

	GDIOBJ(
		GdiObjectHandle_t,
		GdiObjectType_t
		);

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
		int		CntBytesBuffer,
		void*	pObject
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
private:

	friend	class GDIOBJ;

	static  GdiHandleTableEntry_t*	s_pHandleTableEntries;
	static	int						s_NumberOfEntries;

	static	int						s_FirstFreeIndex;
	static	int						s_LastFreeIndex;

	static
	void
	Grow(
		void
		);

	static
	int
	IndexInUse(
		GdiObjectHandle_t	GdiObjectHandle
		);


	static
	void
	RemoveFromHandleTable(
		GdiObjectHandle_t	GdiObjectHandle,
		GDIOBJ*				pGdiObject
		);


	static
	void
	SetGdiObject(
		GdiObjectHandle_t,
		GDIOBJ*
		);


public:

	static
	void
	Initialize(
		void
		);


	static
	GdiObjectHandle_t
	ReserveHandle(
		void
		);


	static
	void
	ReleaseUnsetHandle(
		GdiObjectHandle_t	GdiObjectHandle
		);
	

	static
	void
	ProcessCleanup(
		HPROCESS	hprc
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
		const	WCHAR*	pWarningText
		);


	static
	int
	GetObjectW_I(
		HGDIOBJ	hgdiobj,
		int		cbBuffer,
		LPVOID	lpvObject
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
		int	i
		);

	static
	void
	SetHandleProcessToGweProcess(
		HANDLE	h
		);

	static
	BOOL
	SetObjectOwnerProcess(
		HGDIOBJ		hgdiobj,
		HPROCESS	hprc
		);


	static
	void
	GdiClipDCForSettingChange(
		void
		);



#if DEBUG
	static
	void
	DumpHandleTable(
		HGDIOBJ hgdiobj
		);
#endif






};




#endif

