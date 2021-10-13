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
#include "engine.h"

class DC;

// Some useful structures for statically initializing FLOAT_LONG and
// LINEATTRS structures.  The regular definitions can only be initialized
// with floats because C only allows union initialization using the first
// union field:

union LONG_FLOAT
{
	LONG		l;
	FLOAT_LONG	el;
};


//	This is the same as LINEATTRS except
//	that LONG_FLOAT is used so that it can
//	be initialized using LONG's.

union LineAttributes_t
{
	struct
	{
				FLONG		fl;
				ULONG		iJoin;
				ULONG		iEndCap;
				LONG_FLOAT	leWidth;
				FLOAT		eMiterLimit;
				ULONG		cstyle;
		const	LONG_FLOAT*	pstyle;
				LONG_FLOAT	leStyleState;
	};
	LINEATTRS		la;
};


/***************************************************************************
* The macro below makes sure that the next PATHRECORD lies on a good
* boundary.  This is only needed on some machines.  For the x86 and MIPS
* DWORD alignment of the structure suffices, so the macro below would only
* waste RAM.  (Save it for later reference.)
*
* #define NEXTPATHREC(ppr) (ppr + (offsetof(PATHRECORD, aptfx) +       \
*               sizeof(POINTFIX) * ppr->count +    \
*               sizeof(PATHRECORD) - 1) / sizeof(PATHRECORD))
***************************************************************************/

#define NEXTPATHREC(ppr)  ((PATHRECORD *) ((BYTE *) ppr +          \
                       offsetof(PATHRECORD,aptfx) +    \
                       sizeof(POINTFIX) * ppr->count))


struct _PATHRECORD {
    struct _PATHRECORD *pprnext; // ptr to next pathrec in path
    struct _PATHRECORD *pprprev; // ptr to previous pathrec in path
    FLONG    flags;              // flags describing content of record
    ULONG    count;              // number of control points in record
    ULONG    cAllocated;         // Room for this many control points in record
    POINTFIX aptfx[2];           // variable length array of points
                                 //   (we make it size 2 because we'll actually
                                 //   be declaring this structure on the
                                 //   stack to handle a LineTo, which needs
                                 //   two points)
};

typedef struct _PATHRECORD PATHRECORD;
typedef struct _PATHRECORD *PPATHREC;

/*********************************Struct***********************************\
* struct PATHDATAL
*
* Used like a PATHDATA but describes POINTLs not POINTFIXs
*
* History:
*  08-Nov-1990 -by- Paul Butzi [paulb]
* Wrote it.
\**************************************************************************/

struct _PATHDATAL {
    FLONG   flags;
    ULONG   count;
    PPOINTL pptl;
};

typedef struct _PATHDATAL PATHDATAL;
typedef struct _PATHDATAL *PPATHDATAL;



/*********************************Class************************************\
* class XPATHOBJ
*
\**************************************************************************/

class XPATHOBJ : public PATHOBJ 
{
public:
    PATHRECORD  *pprfirst;          // ptr to first record in path
    PATHRECORD  *pprlast;           // ptr to last record in path
    RECTFX       rcfxBoundBox;      // bounding box for path
    POINTFIX     ptfxSubPathStart;  // start of next sub-path
    FLONG        flags;             // flags describing state of path
    PATHRECORD  *pprEnum;           // pointer for Enumeration
    BOOL         bValid;

    XPATHOBJ();
    ~XPATHOBJ() { vFreeBlocks(); }

// Path maintenance:

    VOID vFreeBlocks();

    BOOL        bMoveTo(POINTL *pptl);
    BOOL        bMoveTo(POINTFIX *pptfx);
    BOOL        bPolyLineTo(POINTL* pptl, ULONG cPts);
    BOOL        bPolyBezierTo(POINTFIX *pptfx, ULONG cPts);
    BOOL        bAddPoint(POINTFIX ptfx);
    BOOL        newpathrec(PATHRECORD **pppr, ULONG cNeeded);
    VOID        vAddRecToEnd(PATHRECORD *ppr);

// Methods to transform or manipulate a path:

    VOID        vOffset(EPOINTL &eptl);
    VOID        vMirror(FIX fxDcWidth);
    VOID        vReverseSubpath();
    BOOL        bCloseFigure();
    VOID        vCloseAllFigures();
    BOOL        bFlatten();
    PPATHREC    pprFlattenRec(PATHRECORD *ppr);

// Methods to enumerate a path

    VOID        vEnumStart() { pprEnum = pprfirst; }
    BOOL        bEnum(PATHDATA *);

    ULONG       cTotalPts();
    ULONG       cTotalCurves();
    
// Drawing a path on a DC:

    BOOL        bStrokePath(DC *pdc);

// Compute bounding box for a set or points:
    void        vSetPathBounds(const POINT* lpPoints, int nCount);

};

