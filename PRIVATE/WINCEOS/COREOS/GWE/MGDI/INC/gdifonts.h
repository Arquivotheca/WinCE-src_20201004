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

class DC;

#define FONTFILETYPE_NOT_LOADED       0     // Not yet loaded
#define FONTFILETYPE_MAPPED           1     // Standard mapped file
#define FONTFILETYPE_UNCOMPRESSED_ROM 2     // Mapped file lives in ROM
#define FONTFILETYPE_COPY_RAM         3     // File was copied into RAM


/*
    @doc BOTH INTERNAL
    @struct FONTFILE | Data for each .fon file loaded.
    @field PUINT8 | m_AddrBase | Base address where font file is loaded.
    @field FONTFILE * | m_pffNext | Pointer to next <t FONTFILE>.
    @field INT32 | m_nRefCount | Reference count of gdi font objects for this file.
    @field INT16 | m_nFonts | Number of fonts in file.
    @field INT16 | m_fontfiletype | One of FONTFILETYPE_*
    @field TCHAR * | m_pszFileName | Name of file.
    @field UINT32 | m_nLoadCount | Reference count for number of loads for this file.

    @comm The reference count refers to the number of device contexts
        that are using a font from this .fon file.
        The load count refers to the number of times applications have asked
        for this font to be loaded or unloaded by the system.  When both the reference
        count and the load count are 0 the file can be safely deleted.
        Note we don't decrement these counts past 0, so they are recounted right away.
        
        The <v m_fSystem> field is used to mark system fonts that cannot
        be deleted because they are used 
*/
struct FONTFILE {
    PUINT8          m_AddrBase;
    FONTFILE        *m_pffNext;
    INT32           m_nRefCount;
    INT32           m_nLoadCount;
    INT16           m_nFonts;
    INT16           m_fontfiletype;
    CEOID           m_oid;
    TCHAR           m_pszFileName[1];
};

/*
    @doc BOTH INTERNAL
    @struct FONTDIRENTRY | Standard structure used in Windows .fon files.
*/
#pragma pack(1)
struct FONTDIRENTRY {
    WORD fontOrdinal;
    WORD dfVersion;
    DWORD dfSize;
    char dfCopyright[60];           // do not change to Unicode
    WORD dfType;
    WORD dfPoints;
    WORD dfVertRes;
    WORD dfHorizRes;
    WORD dfAscent;
    WORD dfInternalLeading;
    WORD dfExternalLeading;
    BYTE dfItalic;
    BYTE dfUnderline;
    BYTE dfStrikeOut;
    WORD dfWeight;
    BYTE dfCharSet;
    WORD dfPixWidth;
    WORD dfPixHeight;
    BYTE dfPitchAndFamily;
    WORD dfAvgWidth;
    WORD dfMaxWidth;
    BYTE dfFirstChar;
    BYTE dfLastChar;
    BYTE dfDefaultChar;
    BYTE dfBreakChar;
    WORD dfWidthBytes;
    DWORD dfDevice;
    DWORD dfFace;
    DWORD dfReserved;
};
/*
    @doc BOTH INTERNAL
    @struct FONTFILEINFO | Standard structure used in Windows .fon files.
*/
struct FONTFILEINFO {
    WORD dfVersion;
    DWORD dfSize;
    char dfCopyright[60];           // do not change to Unicode
    WORD dfType;
    WORD dfPoints;
    WORD dfVertRes;
    WORD dfHorizRes;
    WORD dfAscent;
    WORD dfInternalLeading;
    WORD dfExternalLeading;
    BYTE dfItalic;
    BYTE dfUnderline;
    BYTE dfStrikeOut;
    WORD dfWeight;
    BYTE dfCharSet;
    WORD dfPixWidth;
    WORD dfPixHeight;
    BYTE dfPitchAndFamily;
    WORD dfAvgWidth;
    WORD dfMaxWidth;
    BYTE dfFirstChar;
    BYTE dfLastChar;
    BYTE dfDefaultChar;
    BYTE dfBreakChar;
    WORD dfWidthBytes;
    DWORD dfDevice;
    DWORD dfFace;
    DWORD dfBitsPointer;
    DWORD dfBitsOffset;
    BYTE dfReserved;
};
#pragma pack()


/* This is the only information we need from the FONTFILEINFO in order to map a 
   logical font to a physical font.  We don't use the FONTFILEINFO directly so
   we do not have to map in an entire page of the font just to see if it is 
   a good match for the logical font.
*/
typedef struct ABBREVFONTINFO 
{
    WORD dfInternalLeading;
    WORD dfPixHeight;
    WORD dfWeight;
    WORD dfAvgWidth;
    BYTE dfCharSet;
    BYTE dfItalic;
    BYTE dfUnderline;
    BYTE dfStrikeOut;
    BYTE dfPitchAndFamily;
    // Constructor
    ABBREVFONTINFO(FONTFILEINFO *pfi);
} ABBREVFONTINFO;

/*
    Internal structure. This is for EUDC font only.
 */
struct EUDCFONTINFO
{
    FONTFILEINFO   *m_pFontInfo;
    WORD            m_nStart;       //  describe the character range in this
    WORD            m_nEnd;         //  EUDC font.
};

/*
    @doc BOTH INTERNAL
    @struct FONTRESOURCE | data for each font loaded
    @field TCHAR | m_pszFaceName[LF_FACESIZE] | Typeface name.
    @field FONTFILEINFO * | m_pFontInfo | Pointer to <t FONTFILEINFO>.
    @field ABBREVFONTINFO * | m_pAbbrevFontInfo | Pointer to <t ABBREVFONTINFO>.
    @field FONTFILE * | m_pFontFile | Pointer to <t FONTFILE>.
    @field FONTRESOURCE * | m_pfrNext | Pointer to next font.
*/
class FONTRESOURCE {
public:
    ATOM            m_aFaceName;
    FONTFILEINFO    *m_pFontInfo;
    ABBREVFONTINFO  *m_pAbbrevFontInfo;
    FONTFILE        *m_pFontFile;
    EUDCFONTINFO    m_EUDCFontInfo;
    FONTRESOURCE    *m_pfrNext;
};


#ifdef UNICODE
__inline BYTE TCHConvert( TCHAR tc )
{
	char c;


	WideCharToMultiByte(
		1252,
		0,
		&tc,
		1,
		&c,
		1,
		"_",
		NULL );	

	return c;
}

#define TCHARTOCHAR(uch) (HIBYTE(uch)?(TCHConvert(uch)):LOBYTE(uch))
#else
#define TCHARTOCHAR(ch)  (ch)
#endif

/*
    @doc BOTH INTERNAL
    @struct GLYPHENTRY | Standard structure used in Windows .fon files.
*/
struct GLYPHENTRY {
    UINT16  geWidth;
    UINT16  geOffset;
};


#ifdef JAPANESE_FONT_FORMAT
#define IsJapaneseFont(pfi) (((pfi)->dfVersion & 0x401) == 0x401)
extern BOOL v_fJapaneseFontsSupported;
#endif  // JAPANESE_FONT_FORMAT

void
JapaneseFont_add_fon_data (
    FONTFILE *pff,
    FONTRESOURCE *pfr,
    INT16 *pnFontsAdded,
    FONTRESOURCE ** ppFontResourceList
    );

void
JapaneseFont_get_extents(
    FONTRESOURCE *pfr,
    CONST TCHAR *pStr,
    UINT32 nLen,
    UINT32 *pnExtent,
    UINT32 *pnAscent,
    UINT32 *pnDescent
    );

UINT32
JapaneseFont_draw_text(
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

UINT32
JapaneseFont_draw_text_italic(
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

UINT32
draw_text( 
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

UINT32
draw_text_italic( 
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

//
// Given a raster font glyph, the Italicizor class creates a bitmap with an italicized version
// of that glyph.  The italicization process consists of shifting the bottom two rows of 
// pixels right by zero pixels, the next two rows by one pixel, and so on.  When complete,
// the Italicizor structure can be passed directly to the driver as a SURFOBJ containing
// a glyph bitmap.
//
// This class should only be instantiated statically, since it has no constructor but 
// assumes that many of its elements will be initialized to zero.
//
struct Italicizor : public SURFOBJ
{
    int             iOverhang;
    int             cBitmapBytes;
    
public:
    void            vSetFontHeight(int cHeight);
    BOOL            bMakeItalic(BYTE *pGlyph, int cWidth);
};

extern Italicizor   g_Ital;
extern SURFOBJ      g_soGlyph;

#define  FC_SIM_NONE               0L
#define  FC_SIM_EMBOLDEN           1L
#define  FC_SIM_ITALICIZE          2L
#define  FC_SIM_BOLDITALICIZE      3L
#define  FC_SIM_UNDERLINE          4L
#define  FC_SIM_STRIKEOUT          8L

#if 0  // Not used
BOOL 
JapaneseFont_LineBreakHere (
    TCHAR chFirst, 
    TCHAR chSecond
    );
    
#endif

#ifndef NOEUDCFONT

extern BOOL v_fEUDCFontSupported;

#define IsEUDCFont(pfi) ((pfi)->dfVersion == 0x402)
#define IsGFont(pfi) ((pfi)->dfVersion == 0x404)

BOOL
EUDCFont_addEUDCFont(
    FONTFILEINFO *pfi,
    FONTFILE     *pff,
    FONTRESOURCE *pfrList
    );

void EUDCFont_removeEUDCFont(
    FONTFILE     *pff,
    FONTRESOURCE *pfrList
    );

void
GFont_get_extents(
    FONTRESOURCE *pfr,
    CONST TCHAR  *pStr,
    UINT32        nLen,
    UINT32       *pnExtent,
    UINT32       *pnAscent,
    UINT32       *pnDescent
    );

UINT32
GFont_draw_text(
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

UINT32
GFont_draw_text_italic(
    DC          *pDC,
    UINT32       nX,
    UINT32       nY,
    CONST TCHAR *pStr,
    UINT32       nLen,
    CONST INT32 *pnWidths,
    CONST INT32  simFlags
    );

void GFont_GetTMChars(  TEXTMETRIC   *ptm,
                        FONTFILEINFO *pfi );
#endif
