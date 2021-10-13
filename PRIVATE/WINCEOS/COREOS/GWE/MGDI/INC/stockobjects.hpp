//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __STOCK_OBJECTS_HPP_INCLUDED__
#define __STOCK_OBJECTS_HPP_INCLUDED__

#include "..\inc\gdiobj.h"



class StockObjects
{
private:

	friend	class	StockObjectHandles;

	static	StockObjects*	s_pTheStockObjects;

	Brush_t*	m_pWhiteBrush;
	Brush_t*	m_pLtGrayBrush;
	Brush_t*	m_pGrayBrush;
	Brush_t*	m_pDkGrayBrush;
	Brush_t*	m_pBlackBrush;
	Brush_t*	m_pNullBrush;

	HBRUSH	m_hWhiteBrush;
	HBRUSH	m_hLtGrayBrush;
	HBRUSH	m_hGrayBrush;
	HBRUSH	m_hDkGrayBrush;
	HBRUSH	m_hBlackBrush;
	HBRUSH	m_hNullBrush;
	
	Pen_t*	m_pWhitePen;
	Pen_t*	m_pBlackPen;
	Pen_t*	m_pNullPen;

	HPEN	m_hWhitePen;
	HPEN	m_hBlackPen;
	HPEN	m_hNullPen;

	
	Font_t*			m_pSystemFont;
	HFONT			m_hSystemFont;
	
	MBITMAP*		m_pDefaultBitmap;
	HBITMAP			m_hDefaultBitmap;
	
	Palette_t*		m_pDefaultPalette;
	HPALETTE		m_hDefaultPalette;



public:
    StockObjects();
    ~StockObjects() {};
    
    static
    bool
    Initialize(
        void
    );


    static
	Brush_t*
    WhiteBrush(
        void
    );


    static
	Brush_t*
    GrayBrush(
        void
    );



    static
	Pen_t*
    BlackPen(
        void
    );



    static
    MBITMAP*
    DefaultBitmap(
        void
    );



    static
	Palette_t*
    DefaultPalette(
        void
    );


    static
    void
    DefaultPalette(
		Palette_t*    pPalette
    );


    static
	Font_t*
    SystemFont(
        void
    );


    static
    bool
    IsIdInValidRange(
        int Id
    );


};


#endif

