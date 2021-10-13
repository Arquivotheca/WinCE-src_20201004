//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __RGN_HPP_INCLUDED__
#define __RGN_HPP_INCLUDED__


#include "gdiobj.h"

typedef UINT8    SORTORDER;
//
// REVIEW MartSh/Vladimir: This is a UINT16 to match the type of
// RGNDATHEADER.nCount.  If RGNDATHEADER.nCount reverts to a
// DWORD, change this, too.
//
typedef UINT16 RECTCOUNT;

struct RECTS
{
    INT16 left;
    INT16 top;
    INT16 right;
    INT16 bottom;
};


struct RGNDATHEADER {
    UINT16      nCount;     // 16-bits to save space
    RECTS       rcBound;
    SORTORDER   sortorder;
};

struct RGNDAT {
    RGNDATHEADER   rdh;
    char           Buffer[1];
    //
    // To make C programs happy
    //
    void            SortTopthenLeft(void);
    void            SortClip(BOOL, BOOL);
#ifdef DEBUG
    BOOL            FValidSortOrder();
    BOOL            FDefaultSortOrder();
#endif // DEBUG
};

void RectToRects (const RECT *pr, RECTS* prs);

// Sort order requested by driver for cliprgn's rects
const SORTORDER sortRightDown = (SORTORDER)CD_RIGHTDOWN;    //0L
const SORTORDER sortLeftDown = (SORTORDER)CD_LEFTDOWN;      //1L
const SORTORDER sortRightUp = (SORTORDER)CD_RIGHTUP;        //2L
const SORTORDER sortLeftUp = (SORTORDER)CD_LEFTUP;          //3L
//const SORTORDER sortAny = (SORTORDER)CD_ANY;                //4L
// Sort order expected by rgn handling code
const SORTORDER sortTopThenLeft = 5;
// Original sort mode
const SORTORDER sortNone = 6;

void RectsToRect (const RECTS *prs, RECT* pr);

#define LESS(a,b) ((a).top < (b).top \
                       || ((a).top == (b).top && (a).left < (b).left))



/*
    @func void | rect_intersect | Compute the intersection of two rectangles.
    @parm RECT * | prDest | Destination rectangle.
    @parm RECT * | prSrc1 | First operand rectangle.
    @parm RECT * | prSrc2 | Second operand rectangle.
*/
#define rect_intersect(D,R1,R2)                                 \
            do {                                                \
                (D)->left   = MAX((R1)->left,  (R2)->left);     \
                (D)->top    = MAX((R1)->top,   (R2)->top);      \
                (D)->right  = MIN((R1)->right, (R2)->right);    \
                (D)->bottom = MIN((R1)->bottom,(R2)->bottom);   \
            } while(0)

RGNDAT*   rgn_create_rect(const RECT *, int cRect);
RGNDAT*   rgn_create_rect_minus_rect(const RECT *, const RECT *);
BOOL       rgn_check_pt(RGNDAT *,INT32,INT32);
RGNDAT*   rgn_copy(RGNDAT *);
INT32      rgn_offset(RGNDAT *,INT32,INT32);
RGNDAT*   rgn_union(RGNDAT *,RGNDAT *);
RGNDAT*   rgn_difference(RGNDAT *,RGNDAT *);
RGNDAT*   rgn_intersect(RGNDAT *,RGNDAT *);
RGNDAT*   rgn_intersect_rect(RGNDAT *,LPCRECT);
RGNDAT*   rgn_minus_rect(RGNDAT *prSrc, const RECTS* pMinusRect);
void       rgn_free(RGNDAT *);
void       rgn_consolidate(RGNDAT *);
void       rgn_calc_bound_box(RGNDAT *);
BOOL Rects_overlap(CONST RECT *,CONST RECT *);
BOOL rect_overlap(CONST RECTS *,CONST RECTS *);

RGNDAT*
resize(
	RGNDAT*		prSrc,
	RECTCOUNT	nRects
	);




class Region_t : public GDIOBJ
{
private:

	void*
	operator new(
		size_t
		);

	Region_t(
				GdiObjectHandle_t	GdiObjectHandle,
		const	RECT*				pRects,
				int					NumberOfRects
		);

public:

	RGNDAT*			m_prgn;


	static
	Region_t*
	New(
		const	RECT*
		);

	static
	Region_t*
	New(
		const	RECT*	pRects,
				int		NumberOfRects
		);

	static
	Region_t*
	NewRgnMinusRect(
		const	Region_t*,
		const	RECT*
		);


	void
	operator delete(
		void*
		);


	~Region_t(
		void
		);

	static
	Region_t*
	GET_pRGN(
		HRGN
		);

	HRGN
	GetHRGN(
		void
		) const;




	virtual
	int
	GetObject(
		int,
		PVOID
		);



	virtual
	GDIOBJ*
	SelectObject(
		DC*
		);


#if DEBUG
	virtual
	int
	Dump(
		void
		);

#endif
};




/*    From winddi.h:

typedef struct _CLIPOBJ
{
    ULONG   iUniq;
    RECTL   rclBounds;
    BYTE    iDComplexity;
    BYTE    iFComplexity;
    BYTE    iMode;
    BYTE    fjOptions;
} CLIPOBJ;
*/

struct XCLIPOBJ : public CLIPOBJ
{
    RGNDAT *m_prgnd;
    INT32   m_ircCur;   // index into array of RECTS of the rc being enum-ed
    INT32   m_ircEnd;   // index into array of RECTS of the last rc to be enum-ed
    INT8    m_ircInc;   // increment of the index into array of RECTS used for
                        // enumeration, can be 1/-1 to allow direct and reverse
                        // order of enumeration
public:
    void    prgndClipSet(RGNDAT *prgnd)   { m_prgnd = prgnd; }
    RGNDAT *prgndClipGet(void)            { return(m_prgnd); }
    BOOL    Sort(ULONG);
    void    RestartEnum(void);
};






#endif

