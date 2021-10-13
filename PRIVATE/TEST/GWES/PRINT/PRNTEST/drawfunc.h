//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define IDS_String_FIRST       100
#define IDS_String_MS_LONG     100
#define IDS_String_DF_LONG     101
#define IDS_String_2           102
#define IDS_String_3           103
#define IDS_String_4           104
#define IDS_String_5           105
#define IDS_String_FIRST_JPN   108
#define IDS_String_MS_LONG_JPN 108
#define IDS_String_DF_LONG_JPN 109
#define IDS_String_2_JPN       110
#define IDS_String_3_JPN       111
#define IDS_String_4_JPN       112
#define IDS_String_5_JPN       113

#define IDC_EDIT_NETPATH   400  // for NetPath Dialog

#define MAX_TEST_BITS   7       // TransparentImage() and  DrawPagePens()

#define MAX_PENS   5
typedef struct tagPENINFO
{
    int nStyle;
    int nWidth;
    COLORREF color;
} PENINFO;

PENINFO rgPenInfo[MAX_PENS] = {
    {PS_SOLID, 36, RGB (0x55, 0xAA, 0xDD)},
    {PS_SOLID, 29, RGB (0xAA, 0xFF, 0x88)},

    {PS_DASH, 1, RGB (0, 0, 0)},
    {PS_DASH, 11, RGB (0xFF, 0xAA, 0x55)}, //

    {PS_NULL, 11, RGB (0, 0, 0)}, // expect ignore the color
};

#define MAX_BITMAPS   9
#define BITMAP_16_BITS   4      // position 4
#define BITMAP_24_BITS   5      // position 5
#define BITMAP_8_BITS    6      // position 6

TCHAR *szBitmap[MAX_BITMAPS] = {
    {TEXT ("house")},
    {TEXT ("cat")},
    {TEXT ("winnt4")},
    {TEXT ("globe8")},

    {TEXT ("16biti4")},         // index =4:  BITMAP_16_BITS == 4
    {TEXT ("bently")},

    // all 8 bits
    {TEXT ("cmdbar2")},         // 483 * 243
    {TEXT ("web8")},            // 400 * 400
    {TEXT ("globe8")},
};


TCHAR *szMaskBitmap[2] = {
    {TEXT ("bitmapma")},
    {TEXT ("white320")},
};


typedef struct tagROPINFO
{
    DWORD dwRop;
    TCHAR ropName[20];
}
ROPINFO;


#define MAX_ROPS  15
ROPINFO grgdwRop[MAX_ROPS] = {

    SRCCOPY, TEXT ("SRCCOPY    "), //(DWORD)0x00CC0020 /* dest = source                   */
    SRCINVERT, TEXT ("SRCINVERT  "), //(DWORD)0x00660046 /* dest = source XOR dest          */
    PATINVERT, TEXT ("PATINVERT  "), //(DWORD)0x005A0049 /* dest = pattern XOR dest         */

    NOTSRCCOPY, TEXT ("NOTSRCCOPY "), //(DWORD)0x00330008 /* dest = (NOT source)             */
    MERGECOPY, TEXT ("MERGECOPY  "), //(DWORD)0x00C000CA /* dest = (source AND pattern)     */
    DSTINVERT, TEXT ("DSTINVERT  "), //(DWORD)0x00550009 /* dest = (NOT dest)               */

    SRCPAINT, TEXT ("SRCPAINT   "), //(DWORD)0x00EE0086 /* dest = source OR dest           */
    SRCAND, TEXT ("SRCAND     "), //(DWORD)0x008800C6 /* dest = source AND dest          */
    NOTSRCERASE, TEXT ("NOTSRCERASE"), //(DWORD)0x001100A6 /* dest = (NOT src) AND (NOT dest) */

    SRCERASE, TEXT ("SRCERASE   "), //(DWORD)0x00440328 /* dest = source AND (NOT dest )   */
    BLACKNESS, TEXT ("BLACKNESS  "), //(DWORD)0x00000042 /* dest = BLACK                    */
    WHITENESS, TEXT ("WHITENESS  "), //(DWORD)0x00FF0062 /* dest = WHITE                    */

    PATPAINT, TEXT ("PATPAINT   "), //(DWORD)0x00FB0A09 /* dest = DPSnoo                   */
    PATCOPY, TEXT ("PATCOPY    "), //(DWORD)0x00F00021 /* dest = pattern                  */

    MERGEPAINT, TEXT ("MERGEPAINT "), //(DWORD)0x00BB0226 /* dest = (NOT source) OR dest     */
};


#define MAX_ROPS2  16
ROPINFO grgdwRop2[MAX_ROPS2] = {
//=====================================

    R2_COPYPEN, TEXT ("R2_COPYPEN    "), //13  P
    R2_NOTCOPYPEN, TEXT ("R2_NOTCOPYPEN "), //4   PN

    R2_XORPEN, TEXT ("R2_XORPEN     "), //7   DPx
    R2_NOTXORPEN, TEXT ("R2_NOTXORPEN  "), //10  DPxn

    R2_MASKPEN, TEXT ("R2_MASKPEN    "), //9   DPa
    R2_NOTMASKPEN, TEXT ("R2_NOTMASKPEN "), //8   DPan

    R2_MASKNOTPEN, TEXT ("R2_MASKNOTPEN "), //3   DPna
    R2_MASKPENNOT, TEXT ("R2_MASKPENNOT "), //5   PDna

    R2_BLACK, TEXT ("R2_BLACK      "), //1    0
    R2_NOT, TEXT ("R2_NOT        "), //6   Dn
    R2_NOP, TEXT ("R2_NOP        "), //11  D
    R2_MERGENOTPEN, TEXT ("R2_MERGENOTPEN"), //12  DPno
    R2_MERGEPENNOT, TEXT ("R2_MERGEPENNOT"), //14  PDno

    R2_WHITE, TEXT ("R2_WHITE      "), //16   1

    R2_MERGEPEN, TEXT ("R2_MERGEPEN   "), //15  DPo
    R2_NOTMERGEPEN, TEXT ("R2_NOTMERGEPEN"), //2   DPon
};


#define MAX_SZTEST   14
typedef struct tagSZTESTINFO
{
    UINT uFormat;
    TCHAR *sz;
}
SZTESTINFO;

SZTESTINFO rgszTestInfo[MAX_SZTEST + 2] = {

    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("FirtLine: abcdefghijklmnopqrstuvwxyz1234567890: -=!@#$%^&*()_:;<>?/~`{}[]")},
    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("&TabRight\tTabY\tTabZ")},
    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("Tab1\tTab2\tTabLeft")},

    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("Right K\r\nRight L\r\nRight M")},
    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("Lefe K\r\nLeft L\r\nLeft M")},
    {DT_CENTER | DT_SINGLELINE, TEXT ("At the Center!\r\n40+I+U+S!")},

    {DT_SINGLELINE, TEXT ("360:Arail 85+B+I+U")}, // index 6
    {DT_SINGLELINE, TEXT ("270:Times 40+B+I+U")}, // index 7
    {DT_SINGLELINE, TEXT ("90:Cour 40+B+I+U")}, // index 8
    {DT_SINGLELINE, TEXT ("180:Impact 40+B+UL")}, // index 9
    
    {DT_WORDBREAK | DT_TOP | DT_EXPANDTABS | DT_NOPREFIX, TEXT ("&Foo Part\n&Test Line2")}, // index 10
    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("&Foo Tab1\t&Test Part2:Right")}, // index 11

    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("ë¿¡¬√ƒ≈∆¢£§¶ß®©™´¨≠ÆØ∞±≤≥¥µ∂∑∏π∫ªºΩæø «»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁ ‡·‚„‰ÂÊÁËÈÓÔ˙˚")},

    // index 13: bottom string + EURO symbol: use Tahoma font
    {DT_RIGHT | DT_WORDBREAK | DT_BOTTOM | DT_EXPANDTABS | DT_SINGLELINE,
        TEXT ("&Bottom R: Tahoma:Ä€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘")},


    // for DrawPageFullText use:   last chars:<ß•FÂØﬁ>  from symbol.ttf.
    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("AabCdef¶ß®©™´¨≠ÆØ∞±≤≥¥lMnopqrstuvwxYzﬁﬂ‡·‚„‰ÂÊ12345\t67890:-=!@#$%^&*()_:;<>?/~`{}[]ÒÚÛÙıˆ˜¯ß•FÂØﬁ")},

    // for DrawPageFullText use:   last chars:<ß•FÂØﬁ>  from symbol.ttf.
    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
            TEXT
            ("ß•FÂØﬁ ABCDEF¶ß®©™´¨≠ÆØ∞±≤≥¥LMNOPQRSTUVWXYZµ‹”?????????[?????????4??] abc ???????] p??????????1234 times ???????")},

};

#ifdef UNICODE
SZTESTINFO rgszTestInfo_Jpn[MAX_SZTEST] = {
    // "ç≈èâÇÃçs: ±≤≥¥µ∂∑∏π∫ªºΩæø¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷‹¶›abcdefghijklmnopqrstuvwxyz1234567890: -=!@#$%^&*()_:;<>?/~`{}[]"

    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
            TEXT
            ("\x6700\x521d\x306e\x884c: \xff66\xff67\xff68\xff69\xff6a\xff6b\xff6c\xff6d\xff6e\xff6f\xff70\xff71\xff72\xff73\xff74\xff75\xff76\xff77\xff78\xff79\xff7a\xff7b\xff7c\xff7d\xff7e\xff7f\xff80\xff81\xff82\xff83\xff84\xff85\xff86\xff87\xff88\xff89\xff8a")},
    // "&âEÉ^Éu\tÉ^ÉuÇx\tÉ^ÉuÇy"

    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("&\x53f3\x30bf\x30d6\t\x30bf\x30d6\xff39\t\x30bf\x30d6\xff3a")},
    // "É^ÉuÇP\tÉ^ÉuÇQ\tç∂É^Éu"
    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS, TEXT ("\x30bf\x30d6\xff11\t\x30bf\x30d6\xff12\t\x5de6\x30bf\x30d6")},

    // "âEí[ Çj\r\nÇ›Ç¨ÇÕÇµ Çk\r\nÉ~ÉMÉnÉV Çl"

    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("\x53f3\x7aef \xff2b\r\n\x307f\x304e\x306f\x3057 \xff2c\r\n\x30df\x30ae\x30cf\x30b7 \xff2d")},
    // "ç∂í[ Çj\r\nÇ–ÇæÇËÇÕÇµ Çk\r\nÉqÉ_ÉäÉnÉV Çl"

    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("\x5de6\x7aef \xff2b\r\n\x3072\x3060\x308a\x306f\x3057 \xff2c\r\n\x30d2\x30c0\x30ea\x30cf\x30b7 \xff2d")},
    // "Ç±Ç±Ç™íÜâõÅI\r\n40+I+U+S!"
    {DT_CENTER | DT_SINGLELINE, TEXT ("\x3053\x3053\x304c\x4e2d\x592e\xff01\r\n40+I+U+S!")},

    // "360:MS ∫ﬁºØ∏ 85+B+I+U"
    {DT_SINGLELINE, TEXT ("360:MS \xff7a\xff9e\xff7c\xff6f\xff78 85+B+I+U")}, // index 6
    // "270:MS P∫ﬁºØ∏ 52+B+I+U"
    {DT_SINGLELINE, TEXT ("270:MS P\xff7a\xff9e\xff7c\xff6f\xff78 52+B+I+U")}, // index 7
    // "90:MS ∫ﬁºØ∏ 35+B+I+UL"
    {DT_SINGLELINE, TEXT ("90:MS \xff7a\xff9e\xff7c\xff6f\xff78 35+B+I+UL")}, // index 8
    // "180:MS P∫ﬁºØ∏ 40+B+UL"
    {DT_SINGLELINE, TEXT ("180:MS P\xff7a\xff9e\xff7c\xff6f\xff78 40+B+UL")}, // index 9

    {DT_WORDBREAK | DT_TOP | DT_EXPANDTABS | DT_NOPREFIX,
        TEXT ("&\xFF8C\xFF70\xFF70 \xFF8A\xFF9F\xFF70\xFF84\x31\n&\xFF83\xFF7D\xFF84 \xFF97\xFF72\xFF9D\x31")},
    // "&Ã∞∞ ¿Ãﬁ1\t&√Ωƒ  ﬂ∞ƒ2:Ç›Ç¨"

    {DT_RIGHT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
        TEXT ("&\xFF8C\xFF70\xFF70 \xFF80\xFF8C\xFF9E\x31\t&\xFF83\xFF7D\xFF84 \xFF8A\xFF9F\xFF70\xFF84\x32:\x307f\x304e")}, // index 11

    // "ÇüÇ†Ç°Ç¢Ç£Ç§Ç•Ç¶ÇßÇ®Ç©Ç™Ç´Ç¨Ç≠ÇÆÇØÇ∞Ç±Ç≤Ç≥Ç¥Çµ Ç∂Ç∑Ç∏ÇπÇ∫ÇªÇºÇΩÇæÇøÇ¿Ç¡Ç¬Ç√"


    {DT_LEFT | DT_WORDBREAK | DT_TOP | DT_EXPANDTABS,
            TEXT
            ("\x3041\x3042\x3043\x3044\x3045\x3046\x3047\x3048\x3049\x304a\x304b\x304c\x304d\x304e\x304f\x3050\x3051\x3052\x3053\x3054\x3055\x3056\x3057\x20\x3058\x3059\x305a\x305b\x305c\x305d\x305e\x305f\x3060\x3061\x3062\x3063\x3064\x3065")},

    // index 13:       // "ç≈â∫çs  âE: ÉiÉjÉkÉlÉmÉnÉoÉpÉqÉrÉsÉtÉuÉvÉwÉxÉyÉz"


    {DT_RIGHT | DT_WORDBREAK | DT_BOTTOM | DT_EXPANDTABS | DT_SINGLELINE,
            TEXT
            ("\x6700\x4e0b\x884c  \x53f3: \x30ca\x30cb\x30cc\x30cd\x30ce\x30cf\x30d0\x30d1\x30d2\x30d3\x30d4\x30d5\x30d6\x30d7\x30d8\x30d9\x30da\x30db")}
};
#endif

typedef struct tagFONTINFO
{
    int Height;
    int Weight;
    BYTE Italic;
    BYTE Underline;
    BYTE StrikeOut;

    LONG Escapement;
    LONG Orientation;
    TCHAR szFontName[32];
} FONTINFO;

FONTINFO rgFontInfo[14] = {
// height  weight italic underline strike escapement orientation  fontname
//================================================================================
    {8, 400, TRUE, TRUE, 0, 0, 0, TEXT ("Tahoma")},
    {9, 400, TRUE, TRUE, 0, 0, 0, TEXT ("Courier New")},
    {15, 400, TRUE, TRUE, TRUE, 0, 0, TEXT ("Times New Roman")},

    {28, 700, TRUE, TRUE, TRUE, 0, 0, TEXT ("Times New Roman Bold")},
    {32, 700, TRUE, TRUE, 0, 0, 0, TEXT ("Arial Bold Italic")},
    {40, 400, TRUE, TRUE, TRUE, 0, 0, TEXT ("Courier New")},

    // For string index 6:
    {85, 700, TRUE, TRUE, TRUE, 3600, 3600, TEXT ("Arail Bold Italic")},
    {40, 700, TRUE, TRUE, TRUE, 2700, 2700, TEXT ("Times New Roman")},
    {40, 700, TRUE, TRUE, FALSE, 900, 900, TEXT ("Courier New")},
    {40, 700, FALSE, TRUE, FALSE, 1800, 1800, TEXT ("Impact")},
    //
    //  for String &&Line: compare above bold font: index 10-12
    {85, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("Arail")},
    {40, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("Times New Roman")},
    {40, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("Impact")},
    // index 13: bottom string
    {32, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("Tahoma")},
};

FONTINFO rgFontInfo_Jpn[14] = {
// height  weight italic underline strike escapement orientation  fontname
//================================================================================
    {8, 400, TRUE, TRUE, 0, 0, 0, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
    {9, 400, TRUE, TRUE, 0, 0, 0, TEXT ("MS Gothic")},
    {15, 400, TRUE, TRUE, TRUE, 0, 0, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},

    {28, 700, TRUE, TRUE, TRUE, 0, 0, TEXT ("MS Gothic")},
    {32, 700, TRUE, TRUE, 0, 0, 0, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
    {40, 400, TRUE, TRUE, TRUE, 0, 0, TEXT ("MS Gothic")},

    // For string index 6:
    {85, 700, TRUE, TRUE, TRUE, 3600, 3600, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
    {40, 700, TRUE, TRUE, TRUE, 2700, 2700, TEXT ("MS Gothic")},
    {40, 700, TRUE, TRUE, FALSE, 900, 900, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
    {40, 700, FALSE, TRUE, FALSE, 1800, 1800, TEXT ("MS Gothic")},
    //
    //  for String &&Line: compare above bold font: index 10-12
    {85, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("MS Gothic")},
    {40, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
    {40, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("MS Gothic")},
    {32, 400, TRUE, FALSE, FALSE, 0, 0, TEXT ("ÇlÇr ÇoÉSÉVÉbÉN")},
};

#define MAX_FONTS   6

#ifdef USE_RASTER_FONTS
TCHAR *szFontName[MAX_FONTS] =
    { {TEXT ("arial.usa")}, {TEXT ("cour.usa")}, {TEXT ("sserife.usa")}, {TEXT ("times.usa")}, {TEXT ("terminal.usa")},
{TEXT ("system.usa")}
};
TCHAR *szFontFace[MAX_FONTS] =
    { {TEXT ("Arial")}, {TEXT ("Courier")}, {TEXT ("MS Sans Serif")}, {TEXT ("Times New Roman")}, {TEXT ("Terminal")},
{TEXT ("System")}
};
#else
TCHAR *szFontName[MAX_FONTS] =
    { {TEXT ("arialbi.ttf")}, {TEXT ("arial.ttf")}, {TEXT ("impact.ttf")}, {TEXT ("timesbd.ttf")}, {TEXT ("times.ttf")},
{TEXT ("gothic.ttf")}
};
TCHAR *szFontFace[MAX_FONTS] =
    { {TEXT ("Arial Bold Italic")}, {TEXT ("Arial")}, {TEXT ("Impact")}, {TEXT ("Times New Roman Bold")},
{TEXT ("Times New Roman")}, {TEXT ("Century Gothic")}
};
#endif // USE TTF


POINT grgTriangleSave[5] = { 100, 40, 40, 140, 160, 140,
    100, 40, 40, 140
};                              //  triangle: but use 4 points for polygon

POINT grgConcaveSave[5] = { 600, 40, 200, 40, 600, 300, 200, 300, 600, 40 };


BOOL PASCAL MyStretchBlt (HDC hdcPrint, LPRECT lpRect, DWORD dwRop, float ratio);



// Add for COLOR test
typedef struct tagCOLORTEXTINFO
{
    COLORREF clr;
    TCHAR *clrName;
}
COLORTEXTINFO;
COLORTEXTINFO rgColor[8] = {
    {RGB (127, 127, 127), TEXT ("GRAY")},
    {RGB (255, 0, 0), TEXT ("RED")},
    {RGB (0, 255, 0), TEXT ("GREEN")},
    {RGB (0, 0, 255), TEXT ("BLUE")},
    {RGB (0, 255, 255), TEXT ("CYAN")},
    {RGB (255, 255, 0), TEXT ("YELLOW")},
    {RGB (255, 0, 255), TEXT ("MAGENTA")},
    {RGB (127, 127, 127), TEXT ("GRAY")},
};
