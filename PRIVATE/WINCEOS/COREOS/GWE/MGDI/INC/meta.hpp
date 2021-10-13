//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "gdiobj.h"
#include "engine.h"

//
// Misc replacements from random NT headers.  These items may not
// live through the porting process.
//

//
// Semi-private helper function for SelectClipRgn.  Also 
// called by metafile code.
//

//int
//SelectClipRgnInternal(
//    HDC  hdc,
//    HRGN hrgn,
//    BOOL bDoMetafile    // Metafile this operation?
//    );

extern const RECT g_rectEmpty;

//DECLARE_HANDLE(HOBJ);


//
// From \NT\private\ntos\w32\ntgdi\inc\hmgshare.h
//

//
// status flags used by metafile in user and kernel
//
#define MRI_ERROR       0
#define MRI_NULLBOX     1
#define MRI_OK          2


//
// From \NT\private\ntos\w32\ntgdi\client\local.h
//
VOID    CopyCoreToInfoHeader(LPBITMAPINFOHEADER pbmih,LPBITMAPCOREHEADER pbmch);

#define NEG_INFINITY   0x80000000
#define POS_INFINITY   0x7fffffff

//
// Check if a source is needed in a 3-way bitblt operation.
// This works on both rop and rop3.  We assume that a rop contains zero
// in the high byte.
//
// This is tested by comparing the rop result bits with source (column A
// below) vs. those without source (column B).  If the two cases are
// identical, then the effect of the rop does not depend on the source
// and we don't need a source device.  Recall the rop construction from
// input (pattern, source, target --> result):
//
//      P S T | R   A B         mask for A = 0CCh
//      ------+--------         mask for B =  33h
//      0 0 0 | x   0 x
//      0 0 1 | x   0 x
//      0 1 0 | x   x 0
//      0 1 1 | x   x 0
//      1 0 0 | x   0 x
//      1 0 1 | x   0 x
//      1 1 0 | x   x 0
//      1 1 1 | x   x 0

#define ISSOURCEINROP3(rop3)    \
        (((rop3) & 0xCCCC0000) != (((rop3) << 2) & 0xCCCC0000))

#define HdcFromIhdc(i)      ((HDC)GdiHandleTable_t::HANDLE_FROM_INDEX_ONLY(i))
#define PmdcFromIhdc(i)     PmdcFromHdc(HdcFromIhdc(i))

/**************************************************************************\
 *
 * LINK stuff
 *
\**************************************************************************/

#define INVALID_INDEX      0xffffffff
#define LINK_HASH_SIZE     128
// H_INDEX was ((USHORT)(h)), but SH-3 compiler generates bad code!
#define H_INDEX(h)         ((USHORT)(((DWORD)(h)) & 0xffff))    
#define LINK_HASH_INDEX(h) (H_INDEX(h) & (LINK_HASH_SIZE-1))

typedef struct tagLINK
{
    DWORD           metalink;
    struct tagLINK *plinkNext;
    HANDLE          hobj;
} LINK, *PLINK;

extern PLINK aplHash[LINK_HASH_SIZE];

PLINK   plinkGet(HANDLE h);
PLINK   plinkCreate(HANDLE h,ULONG ulSize);
BOOL    bDeleteLink(HANDLE h);

void CleanEnhMetaFileRecords(PBYTE);

//
// Get RECTL et al.
//
//#include "engine.h"



/**************************************************************************\
 *
 *                <----------------------------------------------\
 *       hash                                                     \
 *                                                                 \
 *       +-----+                                                   |
 *      0| I16 |       metalink16                                  |
 *      1|     |                                                   |
 *      2|     |       +--------+        +--------+                |
 *      3|     |------>|idc/iobj|     /->|metalink|                |
 *      4|     |       |hobj    |    /   |hobj    |                |
 *      5|     |       |pmlNext |---/    |pmlNext |--/             |
 *       |     |       |16bit mf|        |16bit mf|                |
 *      .|     |       +--------+        +--------+                |
 *      .|     |         |                                         |
 *      .|     |         |                                         |
 *       |     |      /--/                                         |
 *    n-1|     |     /                                             |
 *       +-----+     |                                             |
 *                   |  LDC(idc)          MDC                      |
 *                   |                                             |
 *                   \->+--------+       +--------+    MHE[iobj]   |
 *                      |        |       |        |                |
 *                      |        |    /->|        |   +--------+   |
 *                      |        |   /   |        |   |hobj    |---/
 *                      |pmdc    |--/    |pmhe    |-->|idc/iobj|
 *                      +--------+       |        |   +--------+
 *                                       +--------+
 *
 *
 *
\**************************************************************************/

typedef struct tagMETALINK16
{
    DWORD       metalink;
    struct tagMETALINK16 *pmetalink16Next;
    HANDLE      hobj;

    //
    // WARNING: fields before this must match the LINK structure.
    //
    DWORD       cMetaDC16;
    HDC         ahMetaDC16[1];
} METALINK16, *PMETALINK16;

#define pmetalink16Get(h)    ((PMETALINK16) plinkGet(h))
#define pmetalink16Create(h) ((PMETALINK16)plinkCreate(h,sizeof(METALINK16)))
#define bDeleteMetalink16(h) bDeleteLink(h)

/*************************************************************************\
* Module Name: metapriv.h
*
* This file contains metafile-related definitions that are public
* in NT GDI, but are private in MGDI.  
*
* These definitions came from wingdi.h, and may have been **modified**.
* Do **not** assume that structs which have the same name (in NT and MGDI) 
* have the same members!
\*************************************************************************/

//
// Stock object flag used in the object handle index in the enhanced
// metafile records.
// E.g. The object handle index (META_STOCK_OBJECT | BLACK_BRUSH)
// represents the stock object BLACK_BRUSH.
//
#define ENHMETA_STOCK_OBJECT    0x80000000


//
// EMR is the base record type for enhanced metafiles.
//
typedef struct tagEMR
{
    DWORD   iType;              // Enhanced metafile record type
    DWORD   nSize;              // Length of the record in bytes.
                                // This must be a multiple of 4.
    int     yTop;               // Clipping info
    int     yBottom;            // Clipping info
} EMR, *PEMR;

//
// Record structures for the enhanced metafile.
//
typedef struct tagEMRRESTOREDC
{
    EMR     emr;
    LONG    iRelative;          // Specifies a relative instance
} EMRRESTOREDC, *PEMRRESTOREDC;


/* Enhanced Metafile structures */
typedef struct tagENHMETARECORD
{
    DWORD   iType;              // Record type EMR_XXX
    DWORD   nSize;              // Record size in bytes
    int     yTop;               // Clipping info
    int     yBottom;            // Clipping info
    DWORD   dParm[1];           // Parameters
} ENHMETARECORD, *PENHMETARECORD, *LPENHMETARECORD;

typedef struct tagENHMETAHEADER
{
    DWORD   iType;              // Record type EMR_HEADER
    DWORD   nSize;              // Record size in bytes.  This may be greater
                                // than the sizeof(ENHMETAHEADER).
    int     yTop;               // Clipping info
    int     yBottom;            // Clipping info
    DWORD   nBytes;             // Size of the metafile in bytes
    DWORD   nRecords;           // Number of records in the metafile
    WORD    nHandles;           // Number of handles in the handle table
                                // Handle index zero is reserved.
} ENHMETAHEADER, *PENHMETAHEADER;



#define MDC_IDENTIFIER       0x0043444D  /* 'MDC' */
#define MF_IDENTIFIER        0x0000464D  /* 'MF'  */

BOOL bMetaResetDC(HDC hdc);

/*********************************Class************************************\
* class METALINK
*
* Define a link for metafile friends.
*
* A metafile link begins with the metalink field of the LHE entry of an
* object.  If there is no link, this field is zero.  Otherwise, it begins
* with a 16-bit metafile object-link, whose first element points to the
* begining of the METALINK link list.
*
* The 16-bit metafile object-link should be created as necessary when the
* first METALINK is created.  It should be removed as necessary when the
* last METALINK is removed.
*
* We assume that the imhe object index cannot be zero.  Otherwise, bValid()
* will not work.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class METALINK
{
public:
    USHORT imhe;        // MHE index of object of next friend.
    USHORT ihdc;        // Local index of metafile DC of next friend.

public:
    //
    // Constructor -- This one is to allow METALINKs inside other classes.
    //
    METALINK()                      {}

    //
    // Constructor -- Fills the METALINK.
    //
    METALINK(USHORT imhe_, USHORT ihdc_)
    {
        imhe = imhe_;
        ihdc = ihdc_;
    }

    METALINK(PMETALINK16 pmetalink16)
    {
        if (*(PULONG) this = (ULONG)pmetalink16)
            *(PULONG) this = pmetalink16->metalink;
    }

    //
    // Destructor -- Does nothing.
    //
   ~METALINK()                      {}

    //
    // Initializer -- Initialize the METALINK.
    //
    VOID vInit(USHORT imhe_, USHORT ihdc_)
    {
        imhe = imhe_;
        ihdc = ihdc_;
    }

    VOID vInit(ULONG metalink)
    {
        imhe = ((METALINK *) &metalink)->imhe;
        ihdc = ((METALINK *) &metalink)->ihdc;
    }

    VOID vInit(PMETALINK16 pmetalink16)
    {
        if (*(PULONG) this = (ULONG)pmetalink16)
            *(PULONG) this = pmetalink16->metalink;
    }

    //
    // operator ULONG -- Return the long equivalent value.
    //
    operator ULONG()    { return(*(PULONG) this); }

    //
    // bValid -- Is this a valid metalink?
    // imhe object index cannot be zero.
    //
    BOOL bValid()
    {
        ASSERT(*(PULONG) this == 0L || imhe != 0);
        return(*(PULONG) this != 0L);
    }

    //
    // bEqual -- does this metalink refer to imhe and hdc?
    //
    BOOL bEqual(USHORT imhe_, USHORT ihdc_)
    {
        return(imhe == imhe_ && ihdc == ihdc_);
    }

    //
    // vNext -- Update *this to the next metalink.
    //
    VOID vNext();

    //
    // pmetalinkNext -- Return the pointer to the next metalink.
    //
    METALINK * pmetalinkNext();
};

typedef METALINK *PMETALINK;

/*********************************Class************************************\
* class MHE
*
* Define a Metafile Handle Entry.  Our Metafile Handle Table is an array
* of these.  The MHT is used to keep track of object handles at record time.
*
* Note that the first entry (index zero) is reserved.  See METALINK comment.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MHE
{
public:
    HANDLE   lhObject;  // Handle to the GDI object.
    METALINK metalink;  // Links to the next "metafile friend".
                        // Also links the free list.
};

typedef MHE *PMHE;

//
// Metafile Handle Table size
//
#define MHT_HANDLE_SIZE         1024
#define MHT_MAX_HANDLE_SIZE     ((unsigned) 0xFFFF) // ENHMETAHEADER.nHandles is a USHORT.

//
// Metafile palette initial size and increment
//
#define MF_PALETTE_SIZE 256

//
// Metafile memory buffer size
//
#define MF_BUFSIZE_INIT (16*1024)
#define MF_BUFSIZE_INC  (16*1024)

//
// Metafile DC flags
//
#define MDC_FATALERROR          0x0002  // Fatal error in recording.

/*********************************Class************************************\
* class MDC
*
* Metafile DC structure.
*
* There is no constructor or destructor for this object.  We do the
* initialization in pmdcAllocMDC and cleanup in vFreeMDC.
*
* History:
*  Wed Jul 17 17:10:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

//
// Enhanced metafile record types.
//
// Since we don't need to be compatible with the standard Win32, this list
// has been renumbered to yield a contiguous set of values.  This means
// that these EMR_xxx values are different than on the desktop!
//
#define EMR_HEADER                      1
#define EMR_POLYGON                     2
#define EMR_POLYLINE                    3
#define EMR_SETBRUSHORGEX               4
#define EMR_EOF                         5
#define EMR_SETPIXELV                   6
#define EMR_SETBKMODE                   7
#define EMR_SETROP2                     8
#define EMR_SETTEXTCOLOR                9
#define EMR_SETBKCOLOR                  10
#define EMR_EXCLUDECLIPRECT             11
#define EMR_INTERSECTCLIPRECT           12
#define EMR_SAVEDC                      13
#define EMR_RESTOREDC                   14
#define EMR_SELECTOBJECT                15
#define EMR_CREATEPEN                   16
#define EMR_DELETEOBJECT                17
#define EMR_FOCUSRECT                   18
#define EMR_ELLIPSE                     19
#define EMR_RECTANGLE                   20
#define EMR_ROUNDRECT                   21
#define EMR_DRAWEDGE                    22
#define EMR_SELECTPALETTE               23
#define EMR_CREATEPALETTE               24
#define EMR_SETPALETTEENTRIES           25
#define EMR_REALIZEPALETTE              26
#define EMR_FILLRGN                     27
#define EMR_EXTSELECTCLIPRGN            28
#define EMR_BITBLT                      29
#define EMR_STRETCHBLT                  30
#define EMR_MASKBLT                     31
#define EMR_EXTCREATEFONTINDIRECTW      32
#define EMR_EXTTEXTOUTW                 33
#define EMR_CREATEDIBPATTERNBRUSHPT     34
#define EMR_SMALLTEXTOUT                35
#define EMR_SETVIEWPORTORGEX            36
#define EMR_TRANSPARENTIMAGE            37
#define EMR_CREATEBRUSHINDIRECT         38
#define EMR_SETTEXTALIGN                39
#define EMR_MOVETOEX                    40
#define EMR_STRETCHDIBITS               41
#define EMR_SETDIBITSTODEVICE           42

#define EMR_MIN                         1
#define EMR_MAX                         43


class MDC
{
friend class METALINK;
friend HENHMETAFILE WINAPI CloseEnhMetaFile_I(HDC hdc);
friend MDC * pmdcAllocMDC(HDC hdcRef);
friend VOID  vFreeMDC(MDC *pmdc);
friend ULONG imheAllocMHE(HDC hdc, HANDLE lhObject);
friend VOID  vFreeMHE(HDC hdc, ULONG imhe);
friend BOOL  MF_DeleteObject(HANDLE h);
friend BOOL  MF_RealizePalette(HPALETTE hpal);
friend BOOL  MF_SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries,
                                  CONST PALETTEENTRY *pPalEntries);
private:

#ifdef DEBUG
    ULONG         ident;        // Identifier 'MDC'
#endif
    HANDLE        hMem;         // Handle to the memory metafile/buffer.
    ULONG         nMem;         // Size of the memory buffer.
    ULONG         iMem;         // Current memory index pointer.
    BOOL          fFatalError;  // Fatal error occurred?
    ENHMETAHEADER mrmf;         // MRMETAFILE record.
    ULONG         cmhe;         // Size of the metafile handle table.
    ULONG         imheFree;     // Identifies a free MHE index.
    PMHE          pmhe;         // Metafile handle table.

public:
    HDC           hdcRef;       // Info DC associated with metafile

public:
    //
    // pvNewRecord -- Allocate a metafile record from memory buffer.
    // Also set the size field in the metafile record.  If a fatal error
    // has occurred, do not allocate new record.
    //
    void * pvNewRecord(DWORD nSize);

    //
    // vUpdateNHandles -- Update number of handles in the metafile
    // header record.
    //
    VOID vUpdateNHandles(ULONG imhe)
    {
        ASSERT(imhe < (ULONG) MHT_MAX_HANDLE_SIZE && HIWORD(imhe) == 0);

        if (mrmf.nHandles < (WORD) (imhe + 1))
            mrmf.nHandles = (WORD) (imhe + 1);    // imhe is zero based.
    }

    //
    // bFatalError -- Have we encountered a fatal error?
    //
    BOOL bFatalError()  { return fFatalError; }

    //
    // SetFatalError -- Sets the fFatalError member
    //
    void SetFatalError()  { fFatalError = TRUE; }

    //
    // vCommit -- Commit a metafile record to metafile.
    //
    VOID vCommit(ENHMETARECORD &mr)
    {
        ASSERT(mr.iType >= EMR_MIN && mr.iType <= EMR_MAX);
        ASSERT(mr.nSize % 4 == 0);
        ASSERT(!fFatalError);

#ifdef DEBUG
        //
        // Make sure no one wrote pass end of record.  See MDC::pvNewRecord.
        //
        for (ULONG ii = iMem + mr.nSize;
             ii < iMem + mr.nSize + 4 && ii < nMem;
             ii++)
        {
            ASSERT(*((PBYTE) hMem + ii) == 0xCC);
        }
#endif

        iMem        += mr.nSize;
        mrmf.nBytes += mr.nSize;
        mrmf.nRecords++;
    }
};

typedef MDC *PMDC;

PMDC
PmdcFromHdc( 
	HDC	hdc
	);




/*********************************Class************************************\
*
* There is no constructor or destructor for this object.  We do the
* initialization in pmfAllocMF and cleanup in vFreeMF.
*
\**************************************************************************/

class Metafile_t : public GDIOBJ
{
private:


	Metafile_t(
		GdiObjectHandle_t
		);

	friend
	Metafile_t*
	pmfAllocMF(
		ULONG	fl,
		CONST UNALIGNED DWORD *pb,
		LPCWSTR pwszFilename
		);


	friend
	VOID
	vFreeMF(
		Metafile_t*	pmf
		);


	friend
	BOOL
	bInternalPlayEMF(
		HDC	hdc,
		HENHMETAFILE hemf,
		CONST RECT *prectClip
		);


	friend
	HENHMETAFILE
	APIENTRY
	SetEnhMetaFileBitsAlt(
		HLOCAL
		);

public:

    union
		{
		HANDLE			hMem;          // Pointer to the metafile bits.
		PENHMETAHEADER	pmrmf;       // Pointer to MRMETAFILE record.  hMem alias.
		};
	PHANDLETABLE		pht;           // Pointer to the object handle table.
	ULONG				cLevel;        // Saved level.  Init to 1 before play or enum.


	static
	Metafile_t*
	GET_pMF(
		HGDIOBJ
		);

	HENHMETAFILE	
	GetHENHMETAFILE(
		void
		);



private:
	ULONG	iMem;          // Current memory index pointer.


public:
    virtual
	int
	GetObjectW(
		int,
		void*
		);

	virtual
	GDIOBJ*
	SelectObject(
		DC*
		);


#ifdef DEBUG
    virtual int     Dump(void);
#endif
};




//	pmfAllocMF flags
#define ALLOCMF_TRANSFER_BUFFER 0x1

extern const RECTL rclNull;

#define DIB_PAL_INDICES 2

/*********************************Class************************************\
* class MR
*
* Header for metafile records.
*
\**************************************************************************/

class MR        /* mr */
{
protected:
    DWORD   iType;      // Record type EMR_.
    DWORD   nSize;      // Record size in bytes.  Set in MDC::pvNewRecord.

    //
    // The following two fields define the top and bottom of the bounding
    // rectangle for object represented by this record.  This is used
    // by the print banding code to do trivial clipping on metafile records.
    //
    // Note that we don't need a full bounding rectangle for each MR,
    // as print bands always cover the full X extent of the page.
    //
    int yTop;
    int yBottom;

public:

    //
    // vInit -- Initializer.
    //
    VOID vInit(DWORD iType1)    { iType = iType1; }

    //
    // vCommit -- Commit the record to metafile.
    //
    VOID vCommit(PMDC pmdc)
    {
        pmdc->vCommit(*(PENHMETARECORD) this);
    }

    //
    // SetBounds is how we record the Y-only bounds information
    // about this record.  If this method is never called, then the
    // metafile record is assumed to have an infinitely large
    // bounding rectangle.
    //
    void SetBounds(int yTopArg, int yBottomArg)
    {
        ASSERT(yTopArg <= yBottomArg);
        yTop = yTopArg;
        yBottom = yBottomArg;
    }

    //
    // FInRect returns TRUE iff the current MR might intersect
    // the given rectangle at playback time.  The DC's current
    // viewport origin is taken into account.
    //
    BOOL FInRect(HDC hdc, const RECT *prect);

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht)
    {
        DEBUGMSG(
            TRUE,
            (TEXT("MR::bPlay: Unknown record (type: %d, size: %d)\r\n"),
                (int)iType,
                (int)nSize));
        ASSERT(0);
        return(FALSE);
    };
};

typedef MR *PMR;
#define SIZEOF_MR       (sizeof(MR))

/*********************************Class************************************\
* class MRD : public MR
*
* Metafile record with one DWORD.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRD : public MR           /* mrd */
{
protected:
    DWORD       d1;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    VOID vInit(DWORD iType_, DWORD d1_)
    {
        MR::vInit(iType_);
        d1 = d1_;
    }
};

typedef MRD *PMRD;
#define SIZEOF_MRD      (sizeof(MRD))

/*********************************Class************************************\
* class MRDD : public MR
*
* Metafile record with two DWORDs.
*
* History:
*  Wed Jul 17 17:10:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRDD : public MR          /* mrdd */
{
protected:
    DWORD       d1;
    DWORD       d2;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    VOID vInit(DWORD iType_, DWORD d1_, DWORD d2_)
    {
        MR::vInit(iType_);
        d1 = d1_;
        d2 = d2_;
    }
};

typedef MRDD *PMRDD;
#define SIZEOF_MRDD     (sizeof(MRDD))

/*********************************Class************************************\
* class MRDDDD : public MR
*
* Metafile record with four DWORDs.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRDDDD : public MR        /* mrdddd */
{
protected:
    DWORD       d1;
    DWORD       d2;
    DWORD       d3;
    DWORD       d4;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    VOID vInit(DWORD iType_, DWORD d1_, DWORD d2_, DWORD d3_, DWORD d4_)
    {
        MR::vInit(iType_);
        d1 = d1_;
        d2 = d2_;
        d3 = d3_;
        d4 = d4_;
    }
};

typedef MRDDDD *PMRDDDD;
#define SIZEOF_MRDDDD   (sizeof(MRDDDD))


/*********************************Class************************************\
* class MRBP : public MR
*
* Metafile record with Polys.
*
* History:
*  Wed Jul 17 17:10:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRBP : public MR          /* mrbp */
{
protected:
    DWORD       cptl;           // Number of points in the array.
    POINTL      aptl[1];        // Array of POINTL structures.

public:
    //
    // Initializer -- Initialize the metafile Poly(To) record.
    //
    VOID vInit(DWORD iType1, DWORD cptl1, CONST POINTL *aptl1);
};

typedef MRBP *PMRBP;
#define SIZEOF_MRBP(cptl)  (sizeof(MRBP)-sizeof(POINTL)+(cptl)*sizeof(POINTL))


/*********************************Class************************************\
* class MRE : public MR
*
* Metafile record for ellipses and rectangles.
*
* History:
*  Fri Sep 27 15:55:57 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRE : public MR   /* mre */
{
protected:
    ERECTL      erclBox;        // Inclusive-inclusive box in world units

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    LONG iInit(DWORD iType, HDC hdc, LONG x1, LONG y1, LONG x2, LONG y2);
};

typedef MRE *PMRE;
#define SIZEOF_MRE      (sizeof(MRE))


/*********************************Class************************************\
* class MRBR : public MR
*
* Metafile record with a region.  The region data starts at offRgnData
* from the beginning of the record.
*
* History:
*  Tue Oct 29 10:43:25 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRBR : public MR          /* mrbr */
{
protected:
    DWORD       cRgnData;       // Size of region data in bytes.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	BOOL
	bInit(
		DWORD   iType,
		PMDC    pmdc,
		HRGN    hrgn,
		DWORD   cRgnData_,
		DWORD   offRgnData_
		);
//    {
//        ASSERT(cRgnData_);
//        ASSERT(offRgnData_ % 4 == 0);   // Should be DWORD aligned
//
//        MR::vInit(iType);
//        cRgnData   = cRgnData_;
//        return
//        (
//            Gdi::GetRegionData_I (
//                hrgn,
//                cRgnData_,
//                (LPRGNDATA) &((PBYTE)this)[offRgnData_]
//            ) == cRgnData_
//        );
//    }
};

typedef MRBR *PMRBR;
#define SIZEOF_MRBR(cRgnData)   (sizeof(MRBR) + ((cRgnData) + 3) / 4 * 4)

/*********************************Class************************************\
* class MRFILLRGN : public MRBR
*
* FILLRGN record.
*
* History:
*  Tue Oct 29 10:43:25 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRFILLRGN : public MRBR           /* mrfr */
{
protected:
    DWORD       imheBrush;      // Brush index in Metafile Handle Table.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	BOOL
	bInit(
		PMDC	pmdc,
		HRGN	hrgn,
		DWORD	cRgnData,
		DWORD	imheBrush_
		);
//    {
//        ASSERT(imheBrush_);
//
//        //
//        // Store bounds info if possible
//        RECT rect;
//        if (Gdi::GetRgnBox_I(hrgn, &rect) != 0)
//            SetBounds(rect.top, rect.bottom);
//
//        imheBrush = imheBrush_;
//        return(MRBR::bInit(EMR_FILLRGN, pmdc, hrgn, cRgnData, sizeof(MRFILLRGN)));
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRFILLRGN *PMRFILLRGN;
#define SIZEOF_MRFILLRGN(cRgnData)      \
        (SIZEOF_MRBR(cRgnData) + sizeof(MRFILLRGN) - sizeof(MRBR))

/*********************************Class************************************\
* class MREXTSELECTCLIPRGN : public MR
*
* EXTSELECTCLIPRGN record.  The region data follows iMode immediately.
*
* History:
*  Tue Oct 29 10:43:25 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MREXTSELECTCLIPRGN : public MR    /* mrescr */
{
protected:
    DWORD       cRgnData;       // Size of region data in bytes.
    DWORD       iMode;          // Region combine mode.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	BOOL
	bInit(
		HRGN	hrgn,
		DWORD	cRgnData_,
		DWORD	iMode_
		);
//    {
//        MR::vInit(EMR_EXTSELECTCLIPRGN);
//        cRgnData   = cRgnData_;
//        iMode = iMode_;
//
//        //
//        // There is no region data if hrgn == 0 && iMode == RGNCOPY.
//        //
//        if (!cRgnData_)
//        {
//            ASSERT(iMode_ == RGN_COPY);
//            return(TRUE);
//        }
//        else
//        {
//            return cRgnData_ == Gdi::GetRegionData_I(
//                    hrgn,
//                    cRgnData_,
//                    (LPRGNDATA) &((PBYTE) this)[sizeof(MREXTSELECTCLIPRGN)]);
//        }
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MREXTSELECTCLIPRGN *PMREXTSELECTCLIPRGN;
#define SIZEOF_MREXTSELECTCLIPRGN(cRgnData)     \
        (sizeof(MREXTSELECTCLIPRGN) + ((cRgnData) + 3) / 4 * 4)

/*********************************Class************************************\
* class MRMETAFILE : public MR
*
* METAFILE record.  This is the first record in any metafile.
*
* History:
*  Wed Jul 17 17:10:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRMETAFILE : public MR            /* mrmf */
{
protected:
    DWORD       nBytes;         // Size of the metafile in bytes
    DWORD       nRecords;       // Number of records in the metafile
    WORD        nHandles;       // Number of handles in the handle table
                                // Handle index zero is reserved.
public:
    //
    // Initializer -- Initialize the metafile record.
    //
	VOID
	vInit(
		HDC	hdcRef
		);
//    {
//        MR::vInit(EMR_HEADER);
//        nBytes     = 0;         // Update in recording.
//        nRecords   = 0;         // Update in recording.
//        nHandles   = 1;         // Update in recording. (Entry zero reserved.)
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRMETAFILE *PMRMETAFILE;

/*********************************Class************************************\
* class MRSETPIXELV : public MR
*
* SETPIXELV record.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRSETPIXELV : public MR   /* mrspv */
{
protected:
    EPOINTL     eptl;
    COLORREF    crColor;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	VOID
	vInit(
		DWORD		x1,
		DWORD		y1,
		COLORREF	crColor1
		);
//    {
//        MR::vInit(EMR_SETPIXELV);
//        eptl.vInit(x1,y1);
//        crColor = crColor1;
//        SetBounds(y1, y1);  // Clipping info
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETPIXELV *PMRSETPIXELV;
#define SIZEOF_MRSETPIXELV      (sizeof(MRSETPIXELV))

class MRFOCUSRECT : public MRE    /* mre */
{
public:

// bPlay -- Play the record.

    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRFOCUSRECT *PMRFOCUSRECT;

class MRELLIPSE : public MRE    /* mre */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRELLIPSE *PMRELLIPSE;

class MRRECTANGLE : public MRE  /* mrr */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRRECTANGLE *PMRRECTANGLE;

/*********************************Class************************************\
* class MRRECTDWDW : public MRE
*
* Handles RoundRect and DrawEdge records.
*
* Nonstandard extension invented for MGDI.
\**************************************************************************/

class MRRECTDWDW : public MRE  /* mrrr */
{
protected:
    DWORD       dw1;
    DWORD       dw2;

public:
    //
    // Initializer -- Initialize the metafile record.
    // Returns MRI_OK if successful, MRI_ERROR if error and MRI_NULLBOX if
    // the box is empty in device space.  We don't record if there is an
    // error or the box is empty in device space.
    //
    LONG iInit(DWORD iType, HDC   hdc,
               LONG  x1,    LONG  y1,
               LONG  x2,    LONG  y2,
               DWORD dw1_,  DWORD dw2_)
    {
        dw1 = dw1_;
        dw2 = dw2_;

        return(MRE::iInit(iType, hdc, x1, y1, x2, y2));
    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRRECTDWDW *PMRRECTDWDW;
#define SIZEOF_MRRECTDWDW      (sizeof(MRRECTDWDW))


/*********************************Class************************************\
* class MRCREATEPEN: public MR
*
* CREATEPEN record.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRCREATEPEN : public MR   /* mrcp */
{
protected:
    DWORD       imhe;        // Pen index in Metafile Handle Table.
    LOGPEN      logpen;      // Logical Pen.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	BOOL
	bInit(
		HANDLE	hpen_,
		ULONG	imhe_
		);
//    {
//        MR::vInit(EMR_CREATEPEN);
//        imhe = imhe_;
//        return GetObject(hpen_, sizeof(LOGPEN), (LPVOID)&logpen) ==
//                    sizeof(LOGPEN);
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRCREATEPEN *PMRCREATEPEN;
#define SIZEOF_MRCREATEPEN      (sizeof(MRCREATEPEN))

/*********************************Class************************************\
* class MRCREATEPALETTE: public MR
*
* CREATEPALETTE record.
*
* History:
*  Sun Sep 22 14:40:40 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRCREATEPALETTE : public MR       /* mrcp */
{
protected:
    DWORD       imhe;           // LogPalette index in Metafile Handle Table.
    LOGPALETTE  logpal;         // Logical Palette.

public:
    //
    // Initializer -- Initialize the metafile MRCREATEPALETTE record.
    // It sets the peFlags in the palette entries to zeroes.
    //
    BOOL bInit(HPALETTE hpal_, ULONG imhe_, USHORT cEntries_);  // MFREC.CPP

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRCREATEPALETTE *PMRCREATEPALETTE;
#define SIZEOF_MRCREATEPALETTE(cEntries)        \
        (sizeof(MRCREATEPALETTE)-sizeof(PALETTEENTRY)+(cEntries)*sizeof(PALETTEENTRY))

/*********************************Class************************************\
* class MRCREATEBRUSHINDIRECT: public MR
*
* CREATEBRUSHINDIRECT record.
*
* History:
*  Wed Jul 31 21:09:28 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRCREATEBRUSHINDIRECT : public MR /* mrcbi */
{
protected:
    DWORD       imhe;                   // Brush index in Metafile Handle Table.
    LOGBRUSH    lb;                     // Logical brush.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
	VOID
	vInit(
		ULONG		imhe_,
		LOGBRUSH&	lb_
		);
//    {
//        MR::vInit(EMR_CREATEBRUSHINDIRECT);
//        imhe = imhe_;
//        lb   = lb_;
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRCREATEBRUSHINDIRECT *PMRCREATEBRUSHINDIRECT;
#define SIZEOF_MRCREATEBRUSHINDIRECT    (sizeof(MRCREATEBRUSHINDIRECT))

/*********************************Class************************************\
* class MRBRUSH: public MR
*
* Metafile record for mono and dib pattern brushes.
*
* History:
*  Thu Mar 12 16:20:15 1992     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRBRUSH : public MR       /* mrbr */
{
protected:
    DWORD       imhe;               // Brush index in Metafile Handle Table
    HBITMAP     m_hbitmapPattern;   // Bitmap for pattern brush

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit(
        DWORD    iType1,
        DWORD    imhe1,
        HBITMAP  hbitmap1  // Brush's pattern
    );
//    {
//        MR::vInit(iType1);
//        imhe = imhe1;
//        m_hbitmapPattern = hbitmap1;
//        return TRUE;
//    }

    void
	Cleanup(
		void
		);
//    {
//        if (m_hbitmapPattern)
//            VERIFY(Gdi::DeleteObject_I(m_hbitmapPattern));
//    }

};

typedef MRBRUSH *PMRBRUSH;
#define SIZEOF_MRBRUSH    sizeof(MRBRUSH)


class MRCREATEDIBPATTERNBRUSHPT : public MRBRUSH        /* mrcdpb */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);

    void Cleanup() { MRBRUSH::Cleanup(); }
};

typedef MRCREATEDIBPATTERNBRUSHPT *PMRCREATEDIBPATTERNBRUSHPT;

/*********************************Class************************************\
* class MREXTCREATEFONTINDIRECTW: public MR
*
* EXTCREATEFONTINDIRECTW record.
*
* History:
*  Tue Jan 14 13:52:35 1992     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MREXTCREATEFONTINDIRECTW : public MR      /* mrecfiw */
{
protected:
    DWORD       imhe;           // Font index in Metafile Handle Table.
    LOGFONTW    logfont;        // Logical font.  NT: was EXTLOGFONTW.

public:
    //
    // Initializer -- Initialize the metafile MREXTCREATEFONTINDIRECTW
    // record.
    //
	BOOL
	bInit(
		HANDLE	hfont_,
		ULONG	imhe_
		);
//    {
//        MR::vInit(EMR_EXTCREATEFONTINDIRECTW);
//        imhe = imhe_;
//
//        return GetObjectW(hfont_, sizeof(LOGFONTW), (LPVOID) &logfont) ==
//                    sizeof(LOGFONTW);
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MREXTCREATEFONTINDIRECTW *PMREXTCREATEFONTINDIRECTW;
#define SIZEOF_MREXTCREATEFONTINDIRECTW (sizeof(MREXTCREATEFONTINDIRECTW))

/*********************************Class************************************\
* class MRSETPALETTEENTRIES: public MR
*
* SETPALETTEENTRIES record.
*
* History:
*  Sun Sep 22 14:40:40 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRSETPALETTEENTRIES : public MR   /* mrspe */
{
protected:
    DWORD        imhe;          // LogPalette index in Metafile Handle Table.
    DWORD        iStart;        // First entry to be set.
    DWORD        cEntries;      // Number of entries to be set.
    PALETTEENTRY aPalEntry[1];  // Palette entries.

public:
    //
    // Initializer -- Initialize the metafile MRSETPALETTEENTRIES record.
    // It sets the peFlags in the palette entries to zeroes.
    //
    VOID vInit
    (
        ULONG imhe_,
        UINT  iStart_,
        UINT  cEntries_,
        CONST PALETTEENTRY *pPalEntries_
    );                                  // MFREC.CPP

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETPALETTEENTRIES *PMRSETPALETTEENTRIES;
#define SIZEOF_MRSETPALETTEENTRIES(cEntries)    \
        (sizeof(MRSETPALETTEENTRIES)-sizeof(PALETTEENTRY)+(cEntries)*sizeof(PALETTEENTRY))

/*********************************Class************************************\
* class MREOF: public MR
*
* EOF record.
*
* History:
*  Fri Mar 20 09:37:47 1992     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MREOF : public MR         /* mreof */
{
public:
    //
    // Initializer -- Initialize the metafile MREOF record.
    //
	VOID
	vInit(
		void
		);
//    {
//        MR::vInit(EMR_EOF);
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MREOF *PMREOF;
#define SIZEOF_MREOF    sizeof(MREOF)

/*********************************Class************************************\
* class MRBB : public MR
*
* Metafile record with BitBlts.
*
* History:
*  Fri Nov 22 17:17:02 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRBB : public MR          /* mrbb */
{
protected:
    LONG        xDst;           // destination x origin
    LONG        yDst;           // destination y origin
    LONG        cxDst;          // width
    LONG        cyDst;          // height
    DWORD       rop;            // raster operation code (rop3 for MaskBlt)
    COLORREF    clrBkSrc;       // source DC BkColor.  Must be a RGB value.
    HBITMAP     m_hbitmap;      // Recorded bitmap

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit
    (
        DWORD    iType1,
        PMDC     pmdc1,
        LONG     xDst1,
        LONG     yDst1,
        LONG     cxDst1,
        LONG     cyDst1,
        DWORD    rop1,
        COLORREF clrBkSrc1,
        HBITMAP  hbitmap1
    );

    void
	Cleanup(
		void
		);
//    {
//        if (m_hbitmap)
//            VERIFY(Gdi::DeleteObject_I(m_hbitmap));
//    }
};

typedef MRBB *PMRBB;
#define SIZEOF_MRBB      sizeof(MRBB)

class MRBITBLT : public MRBB            /* mrbb */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);

    void Cleanup() { MRBB::Cleanup(); };
};

typedef MRBITBLT *PMRBITBLT;

/*********************************Class************************************\
* class MRSTRETCHBLT : public MRBB
*
* STRETCHBLT record.
*
* Also used for TRANSPARENTIMAGE.  For this, we store a COLORREF (the
* transparent color) instead of a ROP.
*
* History:
*  Wed Nov 27 12:00:33 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRSTRETCHBLT : public MRBB        /* mrsb */
{
protected:
    LONG        cxSrc;          // source width
    LONG        cySrc;          // source height

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit
    (
        DWORD    emr,
        PMDC     pmdc1,
        LONG     xDst1,
        LONG     yDst1,
        LONG     cxDst1,
        LONG     cyDst1,
        DWORD    rop1,      // COLORREF if storing TRANSPARENTIMAGE record
        LONG     cxSrc1,
        LONG     cySrc1,
        COLORREF clrBkSrc1,
        HBITMAP  hbitmap1
    )
    {
        cxSrc = cxSrc1;
        cySrc = cySrc1;

        return MRBB::bInit(
                emr,    // EMR_STRETCHBLT or EMR_TRANSPARENTIMAGE
                pmdc1,
                xDst1,
                yDst1,
                cxDst1,
                cyDst1,
                rop1,
                clrBkSrc1,
                hbitmap1);
    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);

    void Cleanup() { MRBB::Cleanup(); };
};

typedef MRSTRETCHBLT *PMRSTRETCHBLT;
#define SIZEOF_MRSTRETCHBLT   sizeof(MRSTRETCHBLT)

/*********************************Class************************************\
* class MRMASKBLT : public MRBB
*
* MASKBLT record.
*
* History:
*  Wed Nov 27 12:00:33 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRMASKBLT : public MRBB   /* mrsb */
{
protected:
    LONG        xMask;          // mask x origin
    LONG        yMask;          // mask y origin
    DWORD       iUsageMask;     // color table usage in mask's bitmap info.
                                // This contains DIB_PAL_INDICES.
    HBITMAP     m_hbitmapMask;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit(
        PMDC     pmdc1,
        LONG     xDst1,
        LONG     yDst1,
        LONG     cxDst1,
        LONG     cyDst1,
        DWORD    rop1,          // Really a rop3
        COLORREF clrBkSrc1,
        HBITMAP  hbitmapMask1,  // Optional
        LONG     xMask1,
        LONG     yMask1,
        HBITMAP  hbitmapSrc1    // Optional
    );
//    {
//        xMask           = xMask1;
//        yMask           = yMask1;
//        iUsageMask      = DIB_PAL_INDICES;  // REVIEW AnthonyL: Use DIB_RGB_INDICES?
//        m_hbitmapMask   = hbitmapMask1;
//
//        return MRBB::bInit(
//                EMR_MASKBLT,
//                pmdc1,
//                xDst1,
//                yDst1,
//                cxDst1,
//                cyDst1,
//                rop1,
//                clrBkSrc1,
//                hbitmapSrc1);
//    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);

    void
	Cleanup(
		void
		);
//    {
//        if (m_hbitmapMask)
//            VERIFY(Gdi::DeleteObject_I(m_hbitmapMask));
//        MRBB::Cleanup();
//    }

};

typedef MRMASKBLT *PMRMASKBLT;
#define SIZEOF_MRMASKBLT     sizeof(MRMASKBLT)


/*********************************Class************************************\
* class MTEXT
*
* Base record for all textout metafile records.
*
* History:
*  Thu Aug 24 15:20:33 1992     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MTEXT
{
public:
    EPOINTL eptlRef;    // Logical coordinates of the reference point
    DWORD   cchString;  // Number of chars in the string.
    DWORD   offString;  // (Dword-aligned) offset to the string.
    DWORD   fOptions;   // Flags for rectangle usage.
    ERECTL  ercl;       // Opaque of clip rectangle if exists.
    DWORD   offaDx;     // (Dword-aligned) offset to the distance array.
                        // If the distance array does not exist, it
                        // will be queried and recorded!
public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit
    (
        HDC        hdc1,
        int        x1,
        int        y1,
        UINT       fl1,
        CONST RECT *prc1,
        LPCSTR     pString1,
        int        cchString1,
        CONST INT *pdx1,
        PMR        pMR1,
        DWORD      offString1,          // dword-aligned aDx follows the string
        int        cjCh1                // size of a character in bytes
    );
};

/*********************************Class************************************\
* class MREXTTEXTOUT
*
* Metafile record for TextOutW and ExtTextOutW.
*
* History:
*  Thu Aug 24 15:20:33 1992     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MREXTTEXTOUT : public MR          /* mreto */
{
protected:
    MTEXT       mtext;          // Base record for textout.

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    BOOL bInit
    (
        DWORD      iType1,
        PMDC       pmdc1,
        HDC        hdc1,
        int        x1,
        int        y1,
        UINT       fl1,
        CONST RECT *prc1,
        LPCSTR     pString1,
        int        cchString1,
        CONST INT *pdx1,
        int        cjCh1                // size of a character in bytes
    )
    {
        ASSERT(iType1 == EMR_EXTTEXTOUTW);

        MR::vInit(iType1);

        return
        (
            mtext.bInit
            (
                hdc1,
                x1,
                y1,
                fl1,
                prc1,
                pString1,
                cchString1,
                pdx1,
                this,                           // pMR
                sizeof(MREXTTEXTOUT),           // offString
                cjCh1
            )
        );
    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MREXTTEXTOUT *PMREXTTEXTOUT;

#define SIZEOF_MREXTTEXTOUT(cchString, cjCh)                    \
        ((sizeof(MREXTTEXTOUT)                                  \
         +(cchString)*((cjCh)+sizeof(LONG))                     \
         +3) / 4 * 4)

class MRPOLYGON : public MRBP           /* mrpg */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRPOLYGON *PMRPOLYGON;

class MRPOLYLINE : public MRBP          /* mrpl */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRPOLYLINE *PMRPOLYLINE;

class MRSETVIEWPORTORGEX : public MRDD  /* mrsvoe */
{
public:

// bPlay -- Play the record.

    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETVIEWPORTORGEX *PMRSETVIEWPORTORGEX;

class MRSETBRUSHORGEX : public MRDD     /* mrsboe */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETBRUSHORGEX *PMRSETBRUSHORGEX;

class MRSETBKMODE : public MRD          /* mrsbm */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETBKMODE *PMRSETBKMODE;

class MRSETROP2 : public MRD            /* mrsr2 */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETROP2 *PMRSETROP2;

class MRSETTEXTCOLOR : public MRD       /* mrstc */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETTEXTCOLOR *PMRSETTEXTCOLOR;

class MRSETBKCOLOR : public MRD         /* mrsbc */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETBKCOLOR *PMRSETBKCOLOR;

class MRSETTEXTALIGN : public MRD
{
public:
	BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETTEXTALIGN *PMRSETTEXTALIGN;

class MREXCLUDECLIPRECT : public MRDDDD /* mrecr */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MREXCLUDECLIPRECT *PMREXCLUDECLIPRECT;

class MRINTERSECTCLIPRECT : public MRDDDD       /* mricr */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRINTERSECTCLIPRECT *PMRINTERSECTCLIPRECT;

class MRSAVEDC : public MR              /* mrsdc */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSAVEDC *PMRSAVEDC;

class MRRESTOREDC : public MRD          /* mrrdc */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRRESTOREDC *PMRRESTOREDC;


class MRSELECTPALETTE : public MRD      /* mrsp */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSELECTPALETTE *PMRSELECTPALETTE;

class MRREALIZEPALETTE : public MR      /* mrrp */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRREALIZEPALETTE *PMRREALIZEPALETTE;

class MRSELECTOBJECT : public MRD       /* mrso */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSELECTOBJECT *PMRSELECTOBJECT;

class MRDELETEOBJECT : public MRD       /* mrdo */
{
public:
    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRDELETEOBJECT *PMRDELETEOBJECT;

class MRRESETDC : public MR         /* mrescape */
{
private:
   DWORD cjDevMode;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    VOID vInit
    (
        DWORD        iType1,
        DEVMODEW     *pDevMode
    )
    {
        MR::vInit(iType1);
        cjDevMode = pDevMode->dmSize + pDevMode->dmDriverExtra;
        memcpy((PBYTE)this + sizeof(MRRESETDC), (PBYTE) pDevMode, cjDevMode);
    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRRESETDC *PMRRESETDC;


/*********************************Class************************************\
* class MRBDIB : public MRB
*
* Metafile record with Bounds and Dib.
*
* History:
*  Wed Nov 27 12:00:33 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRBDIB : public MR        /* mrsdb */
{
protected:
    LONG        xDst;           // destination x origin
    LONG        yDst;           // destination y origin
    LONG        xDib;           // dib x origin
    LONG        yDib;           // dib y origin
    LONG        cxDib;          // dib width
    LONG        cyDib;          // dib height
    DWORD       offBitsInfoDib; // offset to dib info, we don't store core info.
    DWORD       cbBitsInfoDib;  // size of dib info
    DWORD       offBitsDib;     // offset to dib bits
    DWORD       cbBitsDib;      // size of dib bits buffer
    DWORD       iUsageDib;      // color table usage in bitmap info.

public:
// Initializer -- Initialize the metafile record.

    VOID vInit
    (
        DWORD iType1,
        PMDC  pmdc1,
        LONG  xDst1,
        LONG  yDst1,
        LONG  xDib1,
        LONG  yDib1,
        LONG  cxDib1,
        LONG  cyDib1,
        DWORD offBitsInfoDib1,
        DWORD cbBitsInfoDib1,
        CONST BITMAPINFO *pBitsInfoDib1,
        DWORD offBitsDib1,
        DWORD cbBitsDib1,
        CONST VOID * pBitsDib1,
        DWORD iUsageDib1,
        DWORD cbProfData1 = 0,         // Only used with BITMAPV5
        CONST VOID * pProfData1 = NULL // Only used with BITMAPV5
    )
    {
      
        // cbBitsInfoDib1 and cbBitsDib1 may not be dword sized!

        ASSERT(offBitsDib1 % 4 == 0);
        ASSERT(offBitsInfoDib1 % 4 == 0);

        // +---------------------+ <--- offBitsInfoDib1
        // | Bitmap Info header  |   |
        // +- - - - - - - - - - -+   +- cbBitsInfoDib1
        // | Color Table         |   |
        // +- - - - - - - - - - -+  ---
        // |                     |   |
        // | Color Profile Data  |   +- cbProfData1
        // |   (BITMAPV5 only)   |   |
        // +---------------------+ <--- offBitsDib1
        // |                     |   |
        // | Bitmap Bits         |   +- cbBitsDib1
        // |                     |   |
        // +---------------------+  ---

        MR::vInit(iType1);
        xDst            = xDst1;
        yDst            = yDst1;
        xDib            = xDib1;
        yDib            = yDib1;
        cxDib           = cxDib1;
        cyDib           = cyDib1;
        offBitsInfoDib  = offBitsInfoDib1;
        cbBitsInfoDib   = cbBitsInfoDib1 + cbProfData1;
        offBitsDib      = offBitsDib1;
        cbBitsDib       = cbBitsDib1;
        iUsageDib       = iUsageDib1;

        // Copy dib info if given.

        if (cbBitsInfoDib1)
        {
        // Copy BitmapInfoHeader and color table
        memcpy((PBYTE) this + offBitsInfoDib1,(PBYTE) pBitsInfoDib1,cbBitsInfoDib1);       
        memcpy((PBYTE) this + offBitsDib1, pBitsDib1, cbBitsDib1);
        }
    }
};

/*********************************Class************************************\
* class MRSETDIBITSTODEVICE : public MRBDIB
*
* SETDIBITSTODEVICE record.
*
* History:
*  Wed Nov 27 12:00:33 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRSETDIBITSTODEVICE : public MRBDIB       /* mrsdb */
{
protected:
    DWORD       iStartScan;     // start scan
    DWORD       cScans;         // number of scans

public:
// Initializer -- Initialize the metafile record.

    VOID vInit
    (
        PMDC  pmdc1,
        LONG  xDst1,
        LONG  yDst1,
        LONG  xDib1,
        LONG  yDib1,
        DWORD cxDib1,
        DWORD cyDib1,
        DWORD iStartScan1,
        DWORD cScans1,
        DWORD cbBitsDib1,
        CONST VOID * pBitsDib1,
        DWORD cbBitsInfoDib1,
        CONST BITMAPINFO *pBitsInfoDib1,
        DWORD iUsageDib1
    )
    {
        iStartScan = iStartScan1;
        cScans     = cScans1;

        // cbBitsInfoDib1 and cbBitsDib1 may not be dword sized!

        MRBDIB::vInit
        (
            EMR_SETDIBITSTODEVICE,
            pmdc1,
            xDst1,
            yDst1,
            xDib1,
            yDib1,
            (LONG) cxDib1,
            (LONG) cyDib1,
            sizeof(MRSETDIBITSTODEVICE),
            cbBitsInfoDib1,
            pBitsInfoDib1,
            sizeof(MRSETDIBITSTODEVICE)            // Bitmap Bits will be located at
                + ((cbBitsInfoDib1 + 3) / 4 * 4),   // after bitmap info header and
            cbBitsDib1,
            pBitsDib1,
            iUsageDib1
        );
    }

// bPlay -- Play the record.

    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSETDIBITSTODEVICE *PMRSETDIBITSTODEVICE;
#define SIZEOF_MRSETDIBITSTODEVICE(cbBitsInfoDib,cbBitsDib)             \
        (sizeof(MRSETDIBITSTODEVICE)                                   \
      + (((cbBitsInfoDib) + 3) / 4 * 4)                                \
      + (((cbBitsDib) + 3) / 4 * 4))

/*********************************Class************************************\
* class MRSTRETCHDIBITS : public MRBDIB
*
* STRETCHDIBITS record.
*
* History:
*  Wed Nov 27 12:00:33 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

class MRSTRETCHDIBITS : public MRBDIB   /* mrstrdb */
{
protected:
    DWORD       rop;            // raster operation code.
    LONG        cxDst;          // destination width
    LONG        cyDst;          // destination height

public:
// Initializer -- Initialize the metafile record.

    VOID vInit
    (
        PMDC  pmdc1,
        LONG  xDst1,
        LONG  yDst1,
        LONG  cxDst1,
        LONG  cyDst1,
        LONG  xDib1,
        LONG  yDib1,
        LONG  cxDib1,
        LONG  cyDib1,
        DWORD cScans1,
        DWORD cbBitsDib1,
        CONST VOID * pBitsDib1,
        DWORD cbBitsInfoDib1,
        CONST BITMAPINFO *pBitsInfoDib1,
        DWORD iUsageDib1,
        DWORD rop1
    )
    {
        rop   = rop1;
        cxDst = cxDst1;
        cyDst = cyDst1;

        // cbBitsInfoDib1 and cbBitsDib1 may not be dword sized!

        MRBDIB::vInit
        (
            EMR_STRETCHDIBITS,
            pmdc1,
            xDst1,
            yDst1,
            xDib1,
            yDib1,
            cxDib1,
            cyDib1,
            sizeof(MRSTRETCHDIBITS),
            cbBitsInfoDib1,
            pBitsInfoDib1,
            sizeof(MRSTRETCHDIBITS)                // Bitmap bits will be located at
                + ((cbBitsInfoDib1 + 3) / 4 * 4),   // after bitmap info header and        
            cbBitsDib1,
            pBitsDib1,
            iUsageDib1
        );

        // set cScans1 in
        if (cScans1 && cbBitsInfoDib1)
        {
            PBITMAPINFO pBitInfoTemp = (PBITMAPINFO)((PBYTE)this + sizeof(MRSTRETCHDIBITS));

            pBitInfoTemp->bmiHeader.biHeight    = cScans1;
            pBitInfoTemp->bmiHeader.biSizeImage = cbBitsDib1;
        }
    }

// bPlay -- Play the record.

    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSTRETCHDIBITS *PMRSTRETCHDIBITS;
#define SIZEOF_MRSTRETCHDIBITS(cbBitsInfoDib,cbBitsDib)         \
        (sizeof(MRSTRETCHDIBITS)                                \
      + (((cbBitsInfoDib) + 3) / 4 * 4)                         \
      + (((cbBitsDib) + 3) / 4 * 4))



#define ETO_NO_RECT     0x0100
#define ETO_SMALL_CHARS 0x0200

class MRSMALLTEXTOUT : public MR         /* mrescape */
{
private:
   INT          x;
   INT          y;
   UINT         cChars;
   UINT         fuOptions;

public:
    //
    // Initializer -- Initialize the metafile record.
    //
    VOID vInit
    (
        HDC          hdc1,
        PMDC         pmdc1,
        DWORD        iType1,
        int          x1,
        int          y1,
        UINT         fuOptions1,
        RECT         *pRect,
        UINT         cChars1,
        const WCHAR        *pwc,
        BOOL         bSmallChars
    )
    {
        MR::vInit(iType1);

#ifdef DEBUG
        if (fuOptions1 & (ETO_NO_RECT | ETO_SMALL_CHARS))
        {
//           WARNING(TEXT("MRSMALLTEXTOUT:vInit: warning fuOptions conflict\n"));
        }
#endif

        fuOptions = fuOptions1 & ~(ETO_NO_RECT | ETO_SMALL_CHARS);
        fuOptions |= bSmallChars ? ETO_SMALL_CHARS : 0;
        fuOptions |= (pRect == NULL) ? ETO_NO_RECT : 0;

        x = x1;
        y = y1;
        cChars = cChars1;

        BYTE *pjThis = (PBYTE)this + sizeof(MRSMALLTEXTOUT);

        if (pRect)
        {
           memcpy(pjThis, (PBYTE) pRect, sizeof(RECT));
           pjThis += sizeof(RECT);
        }

        if (bSmallChars)
        {
           while (cChars1--)
           {
              *pjThis++ = (BYTE) *pwc++;
           }
        }
        else
        {
           memcpy(pjThis, pwc, cChars * sizeof(WCHAR));
        }
    }

    //
    // bPlay -- Play the record.
    //
    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRSMALLTEXTOUT *PMRSMALLTEXTOUT;


class MRMOVETOEX : public MRDD          
{
public:

// bPlay -- Play the record.

    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
};

typedef MRMOVETOEX *PMRMOVETOEX;

//class MRLINETO : public MRDD            
//{
//public:

// bPlay -- Play the record.

//    BOOL bPlay(HDC hdc, PHANDLETABLE pht, UINT cht);
//};

//typedef MRLINETO *PMRLINETO;


//
// afnbMRPlay is the metafile playback dispatch table.
//
typedef BOOL (MR::*MRPFN)(HDC, PHANDLETABLE, UINT);
extern const MRPFN afnbMRPlay[EMR_MAX-EMR_MIN+1];
//
// SaveDC
//
BOOL MF_Record(HDC hdc,DWORD mrType);

//
// Polygon
// Polyline
//
BOOL MF_Poly(HDC hdc, CONST POINT *pptl, DWORD cptl, DWORD mrType);

//
// SetBkMode
// SetTextColor
// SetBkColor
// RestoreDC
//
BOOL MF_SetD(HDC hdc,DWORD d1,DWORD mrType);

//
// SetBrushOrgEx
// SetViewportOrgEx
//
BOOL MF_SetDD(HDC hdc,DWORD d1,DWORD d2,DWORD mrType);

//
// ExcludeClipRect
// IntersectClipRect
//
BOOL MF_AnyClipRect(HDC hdc,int x1,int y1,int x2,int y2,DWORD mrType);

//
// SetPixel
//
BOOL MF_SetPixelV(HDC hdc,int x,int y,COLORREF color);

//
// CloseEnhMetaFile
//
BOOL MF_EOF(HDC hdc);

//
// SelectObject
// SelectPalette
//
BOOL MF_SelectAnyObject(HDC hdc,HANDLE h,DWORD mrType);

//
// CreatePen
// CreatePenIndirect
// CreateBrushIndirect
// CreateDIBPatternBrush
// CreateDIBPatternBrushPt
// CreateHatchBrush
// CreatePatternBrush
// CreatePalette
// CreateFont
// CreateFontIndirect
//
DWORD MF_InternalCreateObject(HDC hdc,HANDLE h);

//
// Ellipse
// Rectangle
//
BOOL MF_EllipseRect(HDC hdc,int x1,int y1,int x2,int y2,DWORD mrType);

//
// SelectClipRgn
// ExtSelectClipRgn
// SelectObject(hdc,hrgn)
//
BOOL MF_ExtSelectClipRgn(HDC hdc,HRGN hrgn,int iMode);

//
// BitBlt
// PatBlt
// StretchBlt
// MaskBlt
//
BOOL MF_AnyBitBlt(HDC hdcDst,int xDst,int yDst,int cxDst,int cyDst,
    CONST POINT *pptDst, HDC hdcSrc,int xSrc,int ySrc,int cxSrc,int cySrc,
    HBITMAP hbmMask,int xMask,int yMask,DWORD rop,DWORD mrType);


// SetDIBitsToDevice
// StretchDIBits
BOOL MF_AnyDIBits(HDC hdcDst,int xDst,int yDst,int cxDst,int cyDst,
    int xDib,int yDib,int cxDib,int cyDib,DWORD iStartScan,DWORD cScans,
    CONST VOID * pBitsDib, CONST BITMAPINFO *pBitsInfoDib,DWORD iUsageDib,DWORD rop,DWORD mrType);

BOOL
MF_ExtTextOut(
			HDC		hdc,
			int		x,
			int		y,
			UINT 	fl,
	const	RECT*	prcl,
	const	WCHAR*	psz,
			int		c,
	const	INT*	pdx,
			DWORD mrType
			);

//
// RoundRect
// DrawEdge
//
BOOL MF_RectDwDw(HDC hdc,DWORD iType,int x1,int y1,int x2,int y2,DWORD x3,DWORD y3);

//
// Obviously-named functions -- MF_XXX implements API XXX.
//
BOOL MF_RestoreDC(HDC hdc,int iLevel);
BOOL MF_DeleteObject(HANDLE h);
BOOL MF_RealizePalette(HPALETTE hpal);
BOOL MF_SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, CONST PALETTEENTRY *pPalEntries);
BOOL MF_FillRgn(HDC hdc,HRGN hrgn,HBRUSH hbrush);

//
// DO_METAFILE_THEN_CONTINUE is exactly like 
// DO_METAFILE_THEN_RETURN, except that the THEN_CONTINUE flavor
// will exit the function only if a metafile recording error occurs.
//
// The THEN_RETURN flavor will return the specified value even in success.
//
// The THEN_RETURN flavor is used when the execution of the API's "normal 
// code" isn't desired.  For example, once Polyline has recorded the 
// polyline arguments in the metafile, it wouldn't make sense to ask the
// driver to actually draw a polyline!
//
#define DO_METAFILE_THEN_CONTINUE(pdc, MF_record)                   \
    do                                                              \
    {                                                               \
        /* If we now have a metafile DC, record the API call */     \
        if ((pdc)->IsMeta() && !(MF_record))                        \
        {                                                           \
            /* Exit if record failed */                             \
            goto errRtn;                                            \
        }                                                           \
                                                                    \
    } while (0)

#define DO_METAFILE_THEN_RETURN(pdc, MF_record, retval)             \
    do                                                              \
    {                                                               \
        DO_METAFILE_THEN_CONTINUE(pdc, MF_record);                  \
                                                                    \
        if ((pdc)->IsMeta())                                        \
        {                                                           \
            /* If we get here, we should return success */          \
            return retval;                                          \
        }                                                           \
    } while (0)



