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

/**************************************************************************


Module Name:

   font.cpp

Abstract:

   Gdi Tests:  Font APIs

***************************************************************************/

#include "global.h"
#include "fontdata.h"


/***********************************************************************************
***
***   Enum Fonts
***
************************************************************************************/
int     iFontsEnumerated = 0;
TDC     g_hdc;

#define  pass1 0x01010101
#define  pass2 0x10101010

//***********************************************************************************
// This callback procedure will get called once for each font that
// is currently installed in the GDI server.
int     CALLBACK
EnumerateFace(const ENUMLOGFONT * lpelf, const NEWTEXTMETRIC * lpntm, DWORD FontType, LPARAM lParam)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("inside EnumerateFace"));

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    // Check the value of lParam.
    if (lParam != pass2)
    {
        info(FAIL, TEXT("lParam2 has the wrong value."));
        return 0;
    }

    info(DETAIL, TEXT("<%s> <%s>  h:%d w:%d"), lpelf->elfFullName, lpelf->elfStyle, lpelf->elfLogFont.lfHeight,
         lpelf->elfLogFont.lfWidth);
    return 1;
}

//***********************************************************************************
int     CALLBACK
EnumerateFont(const ENUMLOGFONT * lpelf, const NEWTEXTMETRIC * lpntm, DWORD FontType, LPARAM lParam)
{

    int EnumRVal;

    if (gSanityChecks)
        info(DETAIL, TEXT("inside EnumerateFont"));

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    // This callback procedure will get called once for each font family
    // that is currently installed in the GDI server.

    // Check the value of lParam.
    if (lParam != pass1)
    {
        info(FAIL, TEXT("lParam1 has the wrong value."));
        return 0;
    }

    // Increment the count of how many total fonts have been enumerated.
    iFontsEnumerated++;

    // do this for each Font
    CheckNotRESULT(0, EnumRVal = EnumFontFamilies(g_hdc, lpelf->elfLogFont.lfFaceName, (FONTENUMPROC) EnumerateFace, pass2));

    if(!EnumRVal)
    {
        info(FAIL, TEXT("EnumFontFamilies failed"));
        return 0;
    }
    return 1;
}

//***********************************************************************************
int     CALLBACK
EnumerateFont2(LOGFONT * lpelf, const NEWTEXTMETRIC * lpntm, DWORD FontType, LPARAM lParam)
{
    int EnumRVal;

    if (gSanityChecks)
        info(DETAIL, TEXT("inside EnumerateFont2"));

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    // This callback procedure will get called once for each font family
    // that is currently installed in the GDI server.

    // Check the value of lParam.
    if (lParam != pass1)
    {
        info(FAIL, TEXT("lParam1 has the wrong value."));
        return 0;
    }

    // Increment the count of how many total fonts have been enumerated.
    iFontsEnumerated++;

    // do this for each Font
    CheckNotRESULT(0, EnumRVal = EnumFontFamilies(g_hdc, lpelf->lfFaceName, (FONTENUMPROC) EnumerateFace, pass2));

    if(!EnumRVal)
    {
        info(FAIL, TEXT("EnumFontFamilies failed"));
        return 0;
    }
    return 1;
}

// We're tracking just the first 25 fonts enumerated.
// In general, this covers all fonts on Windows CE, however
// the desktop normally enumerates hundreds.
#ifdef UNDER_CE
#define MAXFACESINEFC 25
#else
#define MAXFACESINEFC 130
#endif

#define CHARSETINDEXCOUNT 18
NameValPair gnvCharSets[] = {
                                                NAMEVALENTRY(ANSI_CHARSET),
                                                NAMEVALENTRY(BALTIC_CHARSET),
                                                NAMEVALENTRY(CHINESEBIG5_CHARSET),
                                                NAMEVALENTRY(EASTEUROPE_CHARSET),
                                                NAMEVALENTRY(GB2312_CHARSET),
                                                NAMEVALENTRY(GREEK_CHARSET),
                                                NAMEVALENTRY(HANGUL_CHARSET),
                                                NAMEVALENTRY(MAC_CHARSET),
                                                NAMEVALENTRY(OEM_CHARSET),
                                                NAMEVALENTRY(RUSSIAN_CHARSET),
                                                NAMEVALENTRY(SHIFTJIS_CHARSET),
                                                NAMEVALENTRY(SYMBOL_CHARSET),
                                                NAMEVALENTRY(TURKISH_CHARSET),
                                                NAMEVALENTRY(VIETNAMESE_CHARSET),
                                                NAMEVALENTRY(JOHAB_CHARSET),
                                                NAMEVALENTRY(ARABIC_CHARSET),
                                                NAMEVALENTRY(HEBREW_CHARSET),
                                                NAMEVALENTRY(THAI_CHARSET)
                                                };

int GetCharsetIndex(int nFaceName)
{
        switch(nFaceName)
        {
            case ANSI_CHARSET:        return 1;
            case BALTIC_CHARSET:      return 2;
            case CHINESEBIG5_CHARSET: return 3;
            case EASTEUROPE_CHARSET:  return 4;
            case GB2312_CHARSET:      return 5;
            case GREEK_CHARSET:       return 6;
            case HANGUL_CHARSET:      return 7;
            case MAC_CHARSET:         return 8;
            case OEM_CHARSET:         return 9;
            case RUSSIAN_CHARSET:     return 10;
            case SHIFTJIS_CHARSET:    return 11;
            case SYMBOL_CHARSET:      return 12;
            case TURKISH_CHARSET:     return 13;
            case VIETNAMESE_CHARSET:  return 14;
            case JOHAB_CHARSET:       return 15;
            case ARABIC_CHARSET:      return 16;
            case HEBREW_CHARSET:      return 17;
            case THAI_CHARSET:        return 18;
            // default is 0, the unknown entry.
            default: return 0;
        }
}

#define FAMILYINDEXCOUNT 7
NameValPair gnvFamilyNames[] = {
                                                NAMEVALENTRY(FF_DECORATIVE),
                                                NAMEVALENTRY(FF_DONTCARE),
                                                NAMEVALENTRY(FF_MODERN),
                                                NAMEVALENTRY(FF_ROMAN),
                                                NAMEVALENTRY(FF_SCRIPT),
                                                NAMEVALENTRY(FF_SWISS)
                                                };

int GetFamilyIndex(int nFamilyName)
{
    switch(nFamilyName)
    {
        case FF_DECORATIVE: return 1;
        case FF_DONTCARE: return 2;
        case FF_MODERN: return 3;
        case FF_ROMAN: return 4;
        case FF_SCRIPT: return 5;
        case FF_SWISS: return 6;
        // default is 0, the unknown entry.
        default: return 0;
    }
}

struct EnumFontCount {
    // Proc instrucations
    BOOL bUseFaceName;
    BOOL bUseFullName;

    //data
    int nTotalEnumerations;
    TCHAR lfFamilyRequested[LF_FACESIZE];
    int nFamilyMatchCount;

    // for tracking face names enumerated.
    TCHAR lfFaceNames[MAXFACESINEFC][LF_FACESIZE];
    int anFaceNameCount[MAXFACESINEFC];
    int nFaceNamesInUse;

    int nCharSet[CHARSETINDEXCOUNT];

    int nFontFamily[FAMILYINDEXCOUNT];
};

void
OutputEnumData(struct EnumFontCount * efc)
{
    #define OUTPUTINT(__var) \
    { \
        info(DETAIL, TEXT("%s = %d"), TEXT(#__var), __var); \
    }
    #define OUTPUTSTRING(__string) \
    { \
        info(DETAIL, TEXT("%s = %s"), TEXT(#__string), __string); \
    }
    int i;

    // commands
    OUTPUTINT(efc->bUseFaceName);
    OUTPUTINT(efc->bUseFullName);

    // data
    OUTPUTINT(efc->nTotalEnumerations);
    OUTPUTSTRING(efc->lfFamilyRequested);
    OUTPUTINT(efc->nFamilyMatchCount);

    for(i = 0; i < efc->nFaceNamesInUse; i++)
    {
        OUTPUTSTRING(efc->lfFaceNames[i]);
        OUTPUTINT(efc->anFaceNameCount[i]);
    }
    OUTPUTINT(efc->nFaceNamesInUse);

    info(DETAIL, TEXT("Unknown/Default = %d"), efc->nCharSet[0]);
    for(i = 0; i < countof(gnvCharSets); i++)
        info(DETAIL, TEXT("%s = %d"), gnvCharSets[i].szName, efc->nCharSet[GetCharsetIndex(gnvCharSets[i].dwVal)]);

    info(DETAIL, TEXT("Unknown = %d"), efc->nFontFamily[0]);
    for(i = 0; i < countof(gnvFamilyNames); i++)
        info(DETAIL, TEXT("%s = %d"), gnvFamilyNames[i].szName, efc->nFontFamily[GetFamilyIndex(gnvFamilyNames[i].dwVal)]);

#undef OUTPUTSTRING
#undef OUTPUTINT
}

// This callback updates a structure with the counts of certain types of fonts
// like how many times a particular fontface was enumerated, how many of each
// charset, and how many of each family.
int CALLBACK
EnumFontFamCountProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD dwFontType, LPARAM lParam)
{
    struct EnumFontCount *efc;
    int nFaceNameIndex;
    TCHAR *tcFontNameForComparison = NULL;

#ifdef UNDER_CE
    if(dwFontType != TRUETYPE_FONTTYPE && dwFontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), dwFontType);
#endif

    if(lParam)
    {
        efc = (struct EnumFontCount *) lParam;

        efc->nTotalEnumerations++;

        // bUseFullName or bUseFaceName needs to be set.
        if(efc->bUseFullName || efc->bUseFaceName)
        {
            if(efc->bUseFullName)
                tcFontNameForComparison = lpelfe->elfFullName;

            if(efc->bUseFaceName)
                tcFontNameForComparison = lpelfe->elfLogFont.lfFaceName;

            for(nFaceNameIndex = 0; nFaceNameIndex < efc->nFaceNamesInUse; nFaceNameIndex++)
            {
                // if the strings compare the same, and the lengths are identical, then they're the same.
                if(_tcsncmp(tcFontNameForComparison, efc->lfFaceNames[nFaceNameIndex], LF_FACESIZE) == 0 &&
                    _tcslen(tcFontNameForComparison) == _tcslen(efc->lfFaceNames[nFaceNameIndex]))
                {
                    efc->anFaceNameCount[nFaceNameIndex]++;
                    break;
                }
            }

            if(nFaceNameIndex < MAXFACESINEFC && nFaceNameIndex == efc->nFaceNamesInUse)
            {
                _tcsncpy_s(efc->lfFaceNames[efc->nFaceNamesInUse], tcFontNameForComparison, LF_FACESIZE-1);
                efc->anFaceNameCount[efc->nFaceNamesInUse]++;
                efc->nFaceNamesInUse++;
            }

            // increment the counter for this character set.
            efc->nCharSet[GetCharsetIndex(lpelfe->elfLogFont.lfCharSet)]++;

            // increment the counter for this family
            efc->nFontFamily[GetFamilyIndex(lpelfe->elfLogFont.lfPitchAndFamily & 0xF0)]++;

            if(_tcscmp(lpelfe->elfLogFont.lfFaceName, efc->lfFamilyRequested) == 0)
                efc->nFamilyMatchCount++;
        }
        else info(FAIL, TEXT("Invalid EnumFontCount structure passed."));
    }
    else info(FAIL, TEXT("Invalid lParam passed."));

    return 1;
}

//***********************************************************************************
void
TestEnumFonts(int testFunc)
{

    info(ECHO, TEXT("%s - BVT test"), funcName[testFunc]);

    iFontsEnumerated = 0;

    // Enumerate each font
    g_hdc = myGetDC();
    switch (testFunc)
    {
        case EEnumFontFamilies:
            CheckNotRESULT(0, EnumFontFamilies(g_hdc, NULL, (FONTENUMPROC) EnumerateFont, pass1));
            break;
        case EEnumFonts:
            CheckNotRESULT(0, EnumFonts(g_hdc, NULL, (FONTENUMPROC) EnumerateFont2, pass1));
            break;
    }
    info(DETAIL, TEXT("after %s"), funcName[testFunc]);

    // did we hit the right number of fonts
    if (numFonts > iFontsEnumerated)
        info(DETAIL, TEXT("%d fonts were enumerated."), iFontsEnumerated);

    myReleaseDC(g_hdc);
    g_hdc = NULL;
}

/***********************************************************************************
***
***   Simple Add & Remove Font Pairs
***
************************************************************************************/

//***********************************************************************************
void
SimpleAddRemoveFontPairs(int testFunc)
{

    info(ECHO, TEXT("%s -  SimpleAddRemoveFontPairs"), funcName[testFunc]);

    int     aResult,
            rResult;
    DWORD   start,
            end,
            subStart,
            subEnd;
    DWORD   secs,
            subSecs,
            totalSize;

    RemoveAllUserFonts();

    if(gnUserFontsEntries > 0)
    {
        for (int j = 0; j < 5; j++)
        {
            totalSize = 0;
            start = GetTickCount();
            for (int i = 0; i < gnUserFontsEntries; i++)
            {
                // only do this for fonts which were added, otherwise we could be dealing with fonts
                // which are already in the system.
                if(gfdUserFonts[i].bFontAdded)
                {
                    subStart = GetTickCount();

                    SetLastError(ERROR_SUCCESS);
                    aResult = AddFontResource(gfdUserFonts[i].tszFullPath);

                    if(!aResult)
                        CheckForLastError(ERROR_INVALID_DATA);

                    subEnd = GetTickCount();
                    rResult = RemoveFontResource(gfdUserFonts[i].tszFullPath);

                    if (!aResult && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                        info(FAIL, TEXT("AddFontResource failed on %s"), gfdUserFonts[i].tszFullPath);
                    if (!rResult && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                        info(FAIL, TEXT("RemoveFontResource failed on %s"), gfdUserFonts[i].tszFullPath);

                    if (aResult)
                    {
                        totalSize += gfdUserFonts[i].dwFileSizeHigh;
                        subSecs = subEnd - subStart;
                        if (subSecs > 0)
                            info(DETAIL, TEXT("%s    Bytes:%ld   ticks:%ld"), gfdUserFonts[i].tszFullPath, gfdUserFonts[i].dwFileSizeHigh, subSecs);
                        else
                            info(DETAIL, TEXT("%s Fraction of a second - results unreliable"), gfdUserFonts[i].tszFullPath);
                    }
                }
            }
            end = GetTickCount();
            secs = end - start;
            info(DETAIL, TEXT("* * * * * * * * Total Bytes:%ld  ticks:%ld"), totalSize, secs);
        }
    }
    else info(DETAIL, TEXT("No user fonts available to test."));
    AddAllUserFonts();

    return;
}

/***********************************************************************************
***
***   AddAdd & RemoveRemove Font Pairs
***
************************************************************************************/

//***********************************************************************************
void
AddAddRemoveRemoveFontPairs(int testFunc)
{

    info(ECHO, TEXT("%s -  AddAdd & RemoveRemove Pairs"), funcName[testFunc]);

    int     aResult0,
            rResult0,
            aResult1,
            rResult1;

    RemoveAllUserFonts();
    for (int i = 0; i < gnUserFontsEntries; i++)
    {
        // only do this for fonts which were added.
        if(gfdUserFonts[i].bFontAdded)
        {
            SetLastError(ERROR_SUCCESS);
            aResult0 = AddFontResource(gfdUserFonts[i].tszFullPath);
            if(!aResult0)
                CheckForLastError(ERROR_INVALID_DATA);

            SetLastError(ERROR_SUCCESS);
            aResult1 = AddFontResource(gfdUserFonts[i].tszFullPath);
            if(!aResult1)
                CheckForLastError(ERROR_INVALID_DATA);

            SetLastError(ERROR_SUCCESS);
            rResult0 = RemoveFontResource(gfdUserFonts[i].tszFullPath);
            if(!rResult0)
                    CheckForLastError(ERROR_FILE_NOT_FOUND);

            SetLastError(ERROR_SUCCESS);
            rResult1 = RemoveFontResource(gfdUserFonts[i].tszFullPath);
            if(!rResult1)
                    CheckForLastError(ERROR_FILE_NOT_FOUND);

            if (!aResult0 && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                info(FAIL, TEXT("AddFontResource failed on %s on #1 try"), gfdUserFonts[i].tszFullPath);

            if (!rResult0 && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                info(FAIL, TEXT("RemoveFontResource failed on %s on #1 try"), gfdUserFonts[i].tszFullPath);

            if (!aResult1 && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                info(FAIL, TEXT("AddFontResource failed on %s on #2 try"), gfdUserFonts[i].tszFullPath);

            if (!rResult1 && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                info(FAIL, TEXT("RemoveFontResource failed on %s on #2 try"), gfdUserFonts[i].tszFullPath);
        }
    }

    AddAllUserFonts();

}

/***********************************************************************************
***
***   Remove Stock Font
***
************************************************************************************/
#define numSysFonts  2

//***********************************************************************************
void
RemoveStockFont(int testFunc)
{

    info(ECHO, TEXT("%s -  Remove Stock fonts"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    TCHAR   outName[LF_FACESIZE];
    LOGFONT lFont;
    HFONT   hFont,
            stockFont;
    int     i;

    TCHAR  *sysFont[numSysFonts] = {
        TEXT("jovear.ttf"), TEXT("msitvg.ttf"), //TEXT("dovai.ttf"),
    };

    TCHAR  *sysFontFace[numSysFonts] = {
        TEXT("Jovear MS"), TEXT("MS ITV Gothic"),   //TEXT("Dovai MS"),
    };

    for (i = 0; i < numSysFonts; i++)
    {
        info(DETAIL, TEXT("using font:<%s>"), sysFont[i]);
        while (RemoveFontResource(sysFont[i]))  // clean off any trace of this font
            ;

        // set up log info
        lFont.lfHeight = 24;
        lFont.lfWidth = 0;
        lFont.lfEscapement = 0;
        lFont.lfOrientation = 0;
        lFont.lfWeight = 400;
        lFont.lfItalic = 0;
        lFont.lfUnderline = 0;
        lFont.lfStrikeOut = 0;
        lFont.lfCharSet = ANSI_CHARSET;
        lFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lFont.lfQuality = DEFAULT_QUALITY;
        lFont.lfPitchAndFamily = DEFAULT_PITCH;

        _tcscpy_s(lFont.lfFaceName, sysFontFace[i]);
        // create and select in font
        CheckNotRESULT(NULL, hFont = CreateFontIndirect(&lFont));
        CheckNotRESULT(NULL, stockFont = (HFONT) SelectObject(hdc, hFont));

        if (!hFont)
            info(FAIL, TEXT("CreateFontIndirect failed on %s #:%d"), sysFont[i], i);

        CheckNotRESULT(0, GetTextFace(hdc, LF_FACESIZE, (LPTSTR) outName));

        CheckNotRESULT(NULL, SelectObject(hdc, stockFont));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Remove "fake" Font
***
************************************************************************************/

//***********************************************************************************
void
AddRemoveFake(int testFunc)
{
    info(ECHO, TEXT("%s -  Remove Fake font"), funcName[testFunc]);

    int     aResult,
            rResult;
    TCHAR *tszFakeFonts[] = {
                                            TEXT("fake1.ttf"),
                                            TEXT("fake1.ttc"),
                                            TEXT("fake1.fon"),
                                            TEXT("fake1.fnt"),
                                            TEXT("fake1.ac3"),
                                            };
    for (int i = 0; i < countof(tszFakeFonts); i++)
    {
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, aResult = AddFontResource(tszFakeFonts[i]));
        CheckForLastError(ERROR_FILE_NOT_FOUND);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, rResult = RemoveFontResource(tszFakeFonts[i]));
        CheckForLastError(ERROR_FILE_NOT_FOUND);

        if (aResult)
            info(FAIL, TEXT("AddFontResource succeeded on bad font: %d on %s"), aResult, tszFakeFonts[i]);

        if (rResult)
             info(FAIL, TEXT("RemoveFontResource succeeded on bad font:%d on %s"), rResult, tszFakeFonts[i]);
    }
}

/***********************************************************************************
***
***   Add multiple times
***
************************************************************************************/

//***********************************************************************************
void
SuperAddRemove(int testFunc)
{

    info(ECHO, TEXT("%s - SuperAddRemove"), funcName[testFunc]);    //testCycles, testCycles);

    int     i,
            j,
            aResult,
            rResult;            // to avoid fake.ttf

    RemoveAllUserFonts();

    for (j = 0; j < 10; j++)    // testCycles eats too much mem
        for (i = 0; i < gnUserFontsEntries; i++)
        {
            // only do this for user added fonts, otherwise this could be a system font file.
            if(gfdUserFonts[i].bFontAdded)
            {
                SetLastError(ERROR_SUCCESS);
                aResult = AddFontResource(gfdUserFonts[i].tszFullPath);

                // adding a valid font file can only fail if the types in the system don't match.
                if (!aResult && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                    info(FAIL, TEXT("AddFontResource failed on %s"), gfdUserFonts[i].tszFullPath);

                // if the call failed, verify the last error was set.
                if(aResult == 0)
                    CheckForLastError(ERROR_INVALID_DATA);
            }
        }

    for (j = 0; j < 10; j++)
        for (i = 0; i < gnUserFontsEntries; i++)
        {
            if(gfdUserFonts[i].bFontAdded)
            {

                SetLastError(ERROR_SUCCESS);
                rResult = RemoveFontResource(gfdUserFonts[i].tszFullPath);

                if (!rResult && gfdUserFonts[i].dwType == fontArray[aFont].dwType && gfdUserFonts[i].dwFileType != TRUETYPEAGFA_FILETYPE)
                    info(FAIL, TEXT("RemoveFontResource failed on %s"), gfdUserFonts[i].tszFullPath);

                // if the call failed, verify the last error was set.
                if(rResult == 0)
                    CheckForLastError(ERROR_FILE_NOT_FOUND);
            }
        }

    // then try removing again
    for (i = 0; i < gnUserFontsEntries; i++)
    {
        if(gfdUserFonts[i].bFontAdded)
        {
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, rResult = RemoveFontResource(gfdUserFonts[i].tszFullPath));
            CheckForLastError(ERROR_FILE_NOT_FOUND);

            if (rResult)
                info(FAIL, TEXT("RemoveFontResource should have failed on %s"), gfdUserFonts[i].tszFullPath);
        }
    }
    return;
}

/***********************************************************************************
***
***   Arabic Check Width Functions
***
************************************************************************************/

//***********************************************************************************
// This callback procedure will get called once for each font that
// is currently installed in the GDI server.
int     CALLBACK
getNameInfo(const ENUMLOGFONT * lpelf, const NEWTEXTMETRIC * lpntm, DWORD FontType, LPARAM lParam)
{
    if (gSanityChecks)
        info(DETAIL, TEXT("inside getNameInfo"));

    int     i,
            chArray['z' - '!' + 1];
    LOGFONT lf;
    HFONT   hFont;

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    // handle the case when EnumFontFam enumerates multiple fonts of the Tahoma family, but with different metrics,
    // the FullName of the font will mismatch
    // Also, if tahomaFont isn't set, then the tahoma font file in use doesn't exactly match the expected font.
    if(_tcscmp(lpelf->elfFullName, TEXT("Tahoma")) || tahomaFont == -1)
    {
        info(DETAIL, TEXT("Unknown font %s"), lpelf->elfFullName);
        return 1;
    }

    // Create the font so we can use it.
    lf = lpelf->elfLogFont;

    // Currently, the hard coded data that this test accesses only has the info for a height 39 width 14 Tahoma.
    lf.lfHeight = 39;
    lf.lfWidth = 14;

    info(DETAIL, TEXT("Using font height: %d Width: %d Escapement: %d Orientation: %d Weight: %d"), lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation, lf.lfWeight);
    info(DETAIL, TEXT("Italic: %d Underline: %d Strikeout: %d CharSet: %d OutPrecision: %d"), lf.lfItalic, lf.lfUnderline, lf.lfStrikeOut, lf.lfCharSet, lf.lfOutPrecision);
    info(DETAIL, TEXT("ClipPrecision: %d Quality: %d PitchAndFamily: %d FaceName: %s"), lf.lfClipPrecision, lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName);

    CheckNotRESULT(NULL, hFont = CreateFontIndirect(&lf));

    if (!hFont)
    {
        info(FAIL, TEXT("Could not create the font."));
        return 0;
    }

    // Select the font into the DC.
    CheckNotRESULT(NULL, SelectObject(g_hdc, hFont));

    switch (lParam)
    {
        case EGetCharABCWidths:
            {
            int     ABCindex = 0;
            ABC     abcArray['z' - '!' + 1];

            CheckNotRESULT(FALSE, GetCharABCWidths(g_hdc, '!', 'z', abcArray));
            for (i = 0; i <= 'z' - '!'; i++)
                if(fontArray[tahomaFont].nLinkedFont == -1 || !fontArray[tahomaFont].st.IsInSkipTable(i + '!'))
                {
                    if (!(NT_ABCWidths[ABCindex][i].abcA == abcArray[i].abcA &&
                           NT_ABCWidths[ABCindex][i].abcB == abcArray[i].abcB &&
                           NT_ABCWidths[ABCindex][i].abcC == abcArray[i].abcC))
                        info(FAIL, TEXT("* <%c> expected: (%d, %d, %d) found: (%d, %d, %d)"), i + '!',
                             NT_ABCWidths[ABCindex][i].abcA, NT_ABCWidths[ABCindex][i].abcB, NT_ABCWidths[ABCindex][i].abcC,
                             abcArray[i].abcA, abcArray[i].abcB, abcArray[i].abcC);
                }
                else info(DETAIL, TEXT("<%c> %d is in the skip table for the font."), i + '!', i + '!');
            }
            break;
        case EGetCharWidth32:
                CheckNotRESULT(FALSE, GetCharWidth32(g_hdc, '!', 'z', chArray));
                for (i = 0; i <= 'z' - '!'; i++)
                    if(fontArray[tahomaFont].nLinkedFont == -1 || !fontArray[tahomaFont].st.IsInSkipTable(i + '!'))
                    {
                        if (NT_32Widths[i] != chArray[i])
                            info(FAIL, TEXT("<%c> expected: (%d) found: (%d)"), i + '!', NT_32Widths[i], chArray[i]);
                    }
                    else info(DETAIL, TEXT("<%c> %d is in the skip table for the font."), i + '!', i + '!');
            break;
// not supported on CE.
#ifndef UNDER_CE
        case EGetCharWidth:
            CheckNotRESULT(FALSE, GetCharWidth(g_hdc, '!', 'z', chArray));
            for (i = 0; i <= 'z' - '!'; i++)
                if(fontArray[tahomaFont].nLinkedFont == -1 || !fontArray[tahomaFont].st.IsInSkipTable(i + '!'))
                {
                    if (NT_32Widths[i] != chArray[i])
                        info(FAIL, TEXT("<%c> expected: (%d) found: (%d)"), i + '!', NT_32Widths[i], chArray[i]);
                }
                else info(DETAIL, TEXT("<%c> %d is in the skip table for the font."), i + '!', i + '!');
            break;
#endif // UNDER_CE
        default:
            info(FAIL, TEXT("Unrecognized API %d"), lParam);
            break;
    }

    SelectObject(g_hdc, GetStockObject(SYSTEM_FONT));
    CheckNotRESULT(FALSE, DeleteObject(hFont));
    return 1;
}

/***********************************************************************************
***
***   Check Width Functions
***
************************************************************************************/

//***********************************************************************************
void
TestFontWidths(int testFunc)
{
    info(ECHO, TEXT("%s - Testing Width"), funcName[testFunc]);

                        // currently, ttf tahoma font is the only font that this test handles.
    int     fontTest[] = { tahomaFont };
    BOOL    bTestedSomething = FALSE;

    g_hdc = myGetDC();

    for (int i = 0; i < countof(fontTest); i++)
    {
        // if the font to test isn't on the system, don't run the test.
        // tahomaFont (index) is -1
        if(fontTest[i] != -1)
        {
            info(ECHO, TEXT("%s - Testing Width - Font:<%s>"), funcName[testFunc], fontArray[fontTest[i]]);
            CheckNotRESULT(0, EnumFontFamilies(g_hdc, fontArray[fontTest[i]].tszFaceName, (FONTENUMPROC) getNameInfo, testFunc));
            bTestedSomething = TRUE;
        }
    }

    if(!bTestedSomething)
        info(ECHO, TEXT("Test found no fonts to test - skipping"));

    myReleaseDC(g_hdc);
    g_hdc = NULL;
}

/***********************************************************************************
***
***   TestCreateFontIndirect
***
************************************************************************************/

//***********************************************************************************
void
TestCreateFontIndirect(int testFunc)
{

    info(ECHO, TEXT("%s - %d BVTs"), funcName[testFunc], testCycles);

    LOGFONT lFont;
    HFONT   hFont;

    for (int i = 0; i < testCycles; i++)
    {
        CheckNotRESULT(NULL, hFont = CreateFontIndirect(&lFont));
        if (!hFont)
            info(FAIL, TEXT("%s failed"), funcName[testFunc]);
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }
}

/***********************************************************************************
***
***   Test EnumFontFamProc
***
************************************************************************************/

int MyRoundedMulDiv(double nNumber, int nNumerator, int nDenominator)
{
    double MulTemp = (double) nNumber * nNumerator;
    double DivTemp = (double) MulTemp / nDenominator;

    if((DivTemp - (int) DivTemp) >= .5)
        DivTemp += 1;

    return (int) (DivTemp);
}

static int g_LogPixelsX = 0;
static int g_LogPixelsY = 0;

//***********************************************************************************
// This callback procedure will get called once for each font that
// is currently installed in the GDI server.
int     CALLBACK
TestReturns(const ENUMLOGFONT * lpelf, const TEXTMETRIC * lpnt, DWORD FontType, LPARAM n)
{
    struct subtest
    {
        TCHAR  *paramdesc;
        long    result;
        int     expected[3];    // 0 == tahomaFont, 1 == courierFont
        BOOL bVarianceToMatch;
    };

    int     whichFont;      // == aFont ? 0 : 1;

    info(DETAIL, TEXT("Checking Font %s, %s"), lpelf->elfLogFont.lfFaceName, lpelf->elfFullName );

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    if(n == tahomaFont)
        whichFont = 0;
    else if(n == courierFont)
        whichFont = 1;
    else if(n == arialRasterFont)
        whichFont = 2;
    else whichFont = -1;

    const NEWTEXTMETRIC * lpntm = (NEWTEXTMETRIC*) lpnt;

    subtest subtests[] = {
        {TEXT("lpelf->elfLogFont.lfHeight"), 0, {MyRoundedMulDiv(29, g_LogPixelsY, 72) /*39*/, MyRoundedMulDiv(27.25, g_LogPixelsY, 72) /*36*/, 16}, TRUE},
        {TEXT("lpelf->elfLogFont.lfWidth"), 0, {MyRoundedMulDiv(10.75, g_LogPixelsX, 72) /*14*/, MyRoundedMulDiv(14.4, g_LogPixelsX, 72) /*19*/, 7}, TRUE},
        {TEXT("lpelf->elfLogFont.lfEscapement"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfOrientation"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfWeight"), 0, {400, 400, 400}, FALSE},

        {TEXT("lpelf->elfLogFont.lfItalic"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfUnderline"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfStrikeOut"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfCharSet"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpelf->elfLogFont.lfOutPrecision"), 0, {0, 0, 1}, FALSE},

        {TEXT("lpelf->elfLogFont.lfClipPrecision"), 0, {2, 2, 2}, FALSE},
        {TEXT("lpelf->elfLogFont.lfQuality"), 0, {1, 1, 1}, FALSE},
        {TEXT("lpelf->elfLogFont.lfPitchAndFamily"), 0, {34, 49, 40}, FALSE},
        {TEXT("lpntm->tmHeight"), 0, {MyRoundedMulDiv(29, g_LogPixelsY, 72) /*39*/, MyRoundedMulDiv(27.25, g_LogPixelsY, 72) /*36*/, 16}, TRUE},
        {TEXT("lpntm->tmAscent"), 0, {MyRoundedMulDiv(24, g_LogPixelsY, 72) /*32*/, MyRoundedMulDiv(20.1, g_LogPixelsY, 72) /*26*/, 13}, TRUE},

        {TEXT("lpntm->tmDescent"), 0, {MyRoundedMulDiv(4.875, g_LogPixelsY, 72) /*7*/, MyRoundedMulDiv(7.3, g_LogPixelsY, 72) /*10*/, 3}, TRUE},
        {TEXT("lpntm->tmInternalLeading"), 0, {MyRoundedMulDiv(4.875, g_LogPixelsX, 72) /*7*/, MyRoundedMulDiv(3.25, g_LogPixelsX, 72) /*4*/, 3}, TRUE},
        {TEXT("lpntm->tmExternalLeading"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpntm->tmAveCharWidth"), 0, {MyRoundedMulDiv(10.75, g_LogPixelsX, 72) /*14*/, MyRoundedMulDiv(14.5, g_LogPixelsX, 72) /*19*/, 7}, TRUE},
        {TEXT("lpntm->tmMaxCharWidth"), 0, {MyRoundedMulDiv(33.375, g_LogPixelsX, 72) /*45*/, MyRoundedMulDiv(16.0, g_LogPixelsX, 72) /*21*/, 14}, TRUE},

        {TEXT("lpntm->tmWeight"), 0, {400, 400, 400}, FALSE},
        {TEXT("lpntm->tmOverhang"), 0, {0, 0, 0}, FALSE},
        {TEXT("tmDigitizedAspect ratio"), 0, {1000, 1000, 1000}, FALSE},
        {TEXT("lpntm->tmFirstChar"), 0, {32, 32, 30}, FALSE},
        {TEXT("lpntm->tmLastChar"), 0, {64258, 64258, 255}, FALSE},

        {TEXT("lpntm->tmDefaultChar"), 0, {31, 31, 31}, FALSE},
        {TEXT("lpntm->tmBreakChar"), 0, {32, 32, 32}, FALSE},
        {TEXT("lpntm->tmItalic"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpntm->tmUnderlined"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpntm->tmStruckOut"), 0, {0, 0, 0}, FALSE},

        {TEXT("lpntm->tmPitchAndFamily"), 0, {39, 54, 39}, FALSE},
        {TEXT("lpntm->tmCharSet"), 0, {0, 0, 0}, FALSE},
        {TEXT("lpntm->ntmFlags"), 0, {64, 64, 0}, FALSE},
        {TEXT("lpntm->ntmSizeEM"), 0, {2048, 2048, 0}, FALSE},

        {TEXT("lpntm->ntmCellHeight"), 0, {2472, 2320, 0}, FALSE},
        {TEXT("lpntm->ntmAvgWidth"), 0, {910, 1229, 0}, FALSE},
        {TEXT("FontType"), 0, {4, 4, 1}, FALSE},
    };
    int     numsubtests = countof(subtests);
    int     i = 0;

    subtests[i++].result = lpelf->elfLogFont.lfHeight;
    subtests[i++].result = lpelf->elfLogFont.lfWidth;
    subtests[i++].result = lpelf->elfLogFont.lfEscapement;
    subtests[i++].result = lpelf->elfLogFont.lfOrientation;
    subtests[i++].result = lpelf->elfLogFont.lfWeight;
    subtests[i++].result = lpelf->elfLogFont.lfItalic;
    subtests[i++].result = lpelf->elfLogFont.lfUnderline;
    subtests[i++].result = lpelf->elfLogFont.lfStrikeOut;
    subtests[i++].result = lpelf->elfLogFont.lfCharSet;
    subtests[i++].result = lpelf->elfLogFont.lfOutPrecision;
    subtests[i++].result = lpelf->elfLogFont.lfClipPrecision;
    subtests[i++].result = lpelf->elfLogFont.lfQuality;
    subtests[i++].result = lpelf->elfLogFont.lfPitchAndFamily;
    if(FontType == TRUETYPE_FONTTYPE)
    {
        subtests[i++].result = lpntm->tmHeight;
        subtests[i++].result = lpntm->tmAscent;
        subtests[i++].result = lpntm->tmDescent;
        subtests[i++].result = lpntm->tmInternalLeading;
        subtests[i++].result = lpntm->tmExternalLeading;
        subtests[i++].result = lpntm->tmAveCharWidth;
        subtests[i++].result = lpntm->tmMaxCharWidth;
        subtests[i++].result = lpntm->tmWeight;
        subtests[i++].result = lpntm->tmOverhang;
        subtests[i++].result = lpntm->tmDigitizedAspectX * 1000 / lpntm->tmDigitizedAspectY;
        subtests[i++].result = lpntm->tmFirstChar;
        subtests[i++].result = lpntm->tmLastChar;
        subtests[i++].result = lpntm->tmDefaultChar;
        subtests[i++].result = lpntm->tmBreakChar;
        subtests[i++].result = lpntm->tmItalic;
        subtests[i++].result = lpntm->tmUnderlined;
        subtests[i++].result = lpntm->tmStruckOut;
        subtests[i++].result = lpntm->tmPitchAndFamily;
        subtests[i++].result = lpntm->tmCharSet;
        subtests[i++].result = lpntm->ntmFlags;
        subtests[i++].result = lpntm->ntmSizeEM;
        subtests[i++].result = lpntm->ntmCellHeight;
        subtests[i++].result = lpntm->ntmAvgWidth;
        subtests[i++].result = FontType;
    }
    else
    {
        subtests[i++].result = lpnt->tmHeight;
        subtests[i++].result = lpnt->tmAscent;
        subtests[i++].result = lpnt->tmDescent;
        subtests[i++].result = lpnt->tmInternalLeading;
        subtests[i++].result = lpnt->tmExternalLeading;
        subtests[i++].result = lpnt->tmAveCharWidth;
        subtests[i++].result = lpnt->tmMaxCharWidth;
        subtests[i++].result = lpnt->tmWeight;
        subtests[i++].result = lpnt->tmOverhang;
        subtests[i++].result = lpnt->tmDigitizedAspectX * 1000 / lpntm->tmDigitizedAspectY;
        subtests[i++].result = lpnt->tmFirstChar;
        subtests[i++].result = lpnt->tmLastChar;
        subtests[i++].result = lpnt->tmDefaultChar;
        subtests[i++].result = lpnt->tmBreakChar;
        subtests[i++].result = lpnt->tmItalic;
        subtests[i++].result = lpnt->tmUnderlined;
        subtests[i++].result = lpnt->tmStruckOut;
        subtests[i++].result = lpnt->tmPitchAndFamily;
        subtests[i++].result = lpnt->tmCharSet;
        subtests[i++].result = 0;
        subtests[i++].result = 0;
        subtests[i++].result = 0;
        subtests[i++].result = 0;
        subtests[i++].result = FontType;
    }

    if(n >= 0 && n < numFonts)
    {
        if(whichFont >= 0 && !_tcscmp(lpelf->elfFullName, fontArray[n].tszFaceName))
        {
            for (i = 0; i < numsubtests; i++)
                if (subtests[i].result != subtests[i].expected[whichFont])
                {
                    if(!(subtests[i].bVarianceToMatch) || (abs((subtests[i].result) - (subtests[i].expected[whichFont])) > 1))
                        info(FAIL, TEXT("%s returned:%ld expected:%ld"), subtests[i].paramdesc, subtests[i].result,
                             subtests[i].expected[whichFont]);
                }

            if (_tcscmp(lpelf->elfLogFont.lfFaceName, fontArray[n].tszFaceName) || _tcscmp(lpelf->elfFullName, fontArray[n].tszFaceName))
                info(FAIL, TEXT("lpelf->elfLogFont.lfFaceName returned:\"%s, %s\" expected:\"%s\""), lpelf->elfLogFont.lfFaceName, lpelf->elfFullName, fontArray[n].tszFaceName);
        }
    }
    else info(FAIL, TEXT("Font index %s outside of known font range"), n);

    return 1;
}

void
TestEnumFontFamHighDPITest(int testFunc)
{

    info(ECHO, TEXT("TestEnumFontFamHighDPITest"));
// ce specific behavior.
#ifdef UNDER_CE
    HKEY hKey;
    DWORD dwOldXValue;
    DWORD dwOldYValue;
    BOOL bOldXValueValid = FALSE;
    BOOL bOldYValueValid = FALSE;
    DWORD dwDisposition;
    // interesting x & y values to test with.
    DWORD dwXValues[] = { 96, 192 };
    DWORD dwYValues[] = { 96, 192 };

    // base registry key for GPE.
    TCHAR szGPERegKey[] = TEXT("Drivers\\Display\\GPE");

    // Load the test display driver to verify it's existance.  If it doesn't load then skip the test
    // and output detailed info on why the test is skipped and give remedies on how to make it not skip.
    if(!g_hdcSecondary)
    {
        info(DETAIL, TEXT("Failed to load the test display driver, test skipped.  Please verify that ddi_test.dll is available."));
        return;
    }

    // Verify the trust level before running this test case
    if (OEM_CERTIFY_TRUST != GetDirectCallerProcessId())
    {
        info(DETAIL, TEXT("The trust level is not set to OEM_CERTIFY_TRUST, test skipped."));
        return;
    }

    // cleanup the existing fonts in the system.

    // retreive the key.
    CheckForRESULT(ERROR_SUCCESS, RegCreateKeyEx(HKEY_LOCAL_MACHINE, szGPERegKey, 0, 0, 0, NULL, 0, &hKey, &dwDisposition));

    // if the key was new, then don't save the old value.
    if(dwDisposition == REG_OPENED_EXISTING_KEY)
    {
        DWORD dwSize, dwType;
        // backup the old key.
        dwSize = sizeof(DWORD);
        if(ERROR_SUCCESS == RegQueryValueEx(hKey, TEXT("LogicalPixelsX"), NULL, &dwType, (BYTE*) &dwOldXValue, &dwSize))
        {
            // the registry key type should be DWORD
            CheckForRESULT(REG_DWORD, dwType);
            // the size should be a DWORD
            CheckForRESULT(sizeof(DWORD), dwSize);
            bOldXValueValid = TRUE;
        }

        if(ERROR_SUCCESS == RegQueryValueEx(hKey, TEXT("LogicalPixelsY"), NULL, &dwType, (BYTE*) &dwOldYValue, &dwSize))
        {
            // the registry key type should be DWORD
            CheckForRESULT(REG_DWORD, dwType);
            // the size should be a DWORD
            CheckForRESULT(sizeof(DWORD), dwSize);
            bOldYValueValid = TRUE;
        }

    }

    // these two arrays must be the same size.
    assert(countof(dwXValues) == countof(dwYValues));

    // step through the interesting values for this registry key and verify the driver returns them.
    for(int i=0; i < countof(dwXValues); i++)
    {
        // change the registry key to the entry to something interesting
        CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, TEXT("LogicalPixelsX"), NULL, REG_DWORD, (BYTE*) &dwXValues[i], sizeof(DWORD)));
        CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, TEXT("LogicalPixelsY"), NULL, REG_DWORD, (BYTE*) &dwYValues[i], sizeof(DWORD)));

        // reset the verification driver that's already loaded.
        ResetVerifyDriver();

        g_LogPixelsX = GetDeviceCaps(g_hdcSecondary, LOGPIXELSX);
        g_LogPixelsY = GetDeviceCaps(g_hdcSecondary, LOGPIXELSY);


        info(DETAIL, TEXT("Testing with LogPixelsX=%d and LogPixelsY=%d"), g_LogPixelsX, g_LogPixelsY);
        // step through the fonts and verify the values changed.
        for (int j = 0; j < numFonts; j++)
        {
            // only test good fonts, and only do it for tahoma, courier, or arial since that's all we know the metrics for.
            if((j == tahomaFont || j == courierFont || j ==  arialRasterFont))
            {
                CheckNotRESULT(0, EnumFontFamilies(g_hdcSecondary, fontArray[j].tszFaceName, (FONTENUMPROC) TestReturns, j));
            }
        }
    }

    // if the key was new, don't restore the old value
    if(dwDisposition == REG_OPENED_EXISTING_KEY)
    {
        // restore the old value if we got an old value, otherwise delete it.
        if(bOldXValueValid)
        {
            CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, TEXT("LogicalPixelsX"), NULL, REG_DWORD, (BYTE*) &dwOldXValue, sizeof(DWORD)));
        }
        else
        {
            CheckForRESULT(ERROR_SUCCESS, RegDeleteValue(hKey, TEXT("LogicalPixelsX")));
        }

        if(bOldYValueValid)
        {
            CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, TEXT("LogicalPixelsY"), NULL, REG_DWORD, (BYTE*) &dwOldXValue, sizeof(DWORD)));
        }
        else
        {
            CheckForRESULT(ERROR_SUCCESS, RegDeleteValue(hKey, TEXT("LogicalPixelsY")));
        }
    }

    // close the open reg key.
    CheckForRESULT(ERROR_SUCCESS, RegCloseKey(hKey));

    // if the key was created, then delete it.
    if(dwDisposition == REG_CREATED_NEW_KEY)
        CheckForRESULT(ERROR_SUCCESS, RegDeleteKey(HKEY_LOCAL_MACHINE, szGPERegKey));

    g_LogPixelsX = g_LogPixelsY = 0;
#else
    info(DETAIL, TEXT("This test is only valid on CE."));
#endif

}

//***********************************************************************************
void
TestEnumFontFamProc(int testFunc)
{

    info(ECHO, TEXT("%s - Testing Width"), funcName[testFunc]);

    g_hdc = myGetDC();

    g_LogPixelsX = GetDeviceCaps(g_hdc, LOGPIXELSX);
    g_LogPixelsY = GetDeviceCaps(g_hdc, LOGPIXELSY);

    for (int i = 0; i < numFonts; i++)
    {
        CheckNotRESULT(0, EnumFontFamilies(g_hdc, fontArray[i].tszFaceName, (FONTENUMPROC) TestReturns, i));
    }

    g_LogPixelsX = g_LogPixelsY = 0;

    myReleaseDC(g_hdc);
    g_hdc = NULL;

}

/***********************************************************************************
***
***   Passes Null as every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2Font(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC     aDC = myGetDC();
    LOGFONT lf;

    switch (testFunc)
    {
        case EAddFontResource:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, AddFontResource(TEXT("fake0")));
            CheckForLastError(ERROR_FILE_NOT_FOUND);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, AddFontResource(TEXT("")));
            if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
                CheckForLastError(ERROR_FILE_NOT_FOUND);
            else
                CheckForLastError(ERROR_ACCESS_DENIED);

#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, AddFontResource(NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case EEnumFontFamilies:
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamilies((TDC) NULL, NULL, NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamilies(aDC,NULL,NULL /* null enum proc */,NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamilies((TDC) NULL, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamilies(g_hdcBAD, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamilies(g_hdcBAD2, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);
#endif
            break;
        case EEnumFontFamiliesEx:
            memset((LPVOID) & lf, 0, sizeof (LOGFONT));
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamiliesEx((TDC) NULL, &lf, (FONTENUMPROC) TestReturns, aFont, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamiliesEx(g_hdcBAD, &lf, (FONTENUMPROC) TestReturns, aFont, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamiliesEx(g_hdcBAD2, &lf, (FONTENUMPROC) TestReturns, aFont, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
            // exception in test process space on desktop
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamiliesEx(aDC, &lf, NULL, NULL, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            // desktop enumerates every font on the system, CE fails.
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFontFamiliesEx(aDC, NULL, (FONTENUMPROC) TestReturns, aFont, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case EEnumFonts:
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFonts((TDC) NULL, NULL, NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFonts(aDC, NULL, NULL /* null enum proc */, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFonts((TDC) NULL, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFonts(g_hdcBAD, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, EnumFonts(g_hdcBAD2, NULL, (FONTENUMPROC) TestReturns, aFont));
            CheckForLastError(ERROR_INVALID_HANDLE);
#endif
            break;
        case ERemoveFontResource:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RemoveFontResource(TEXT("fake0")));
            CheckForLastError(ERROR_FILE_NOT_FOUND);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RemoveFontResource(TEXT("")));
            CheckForLastError(ERROR_FILE_NOT_FOUND);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RemoveFontResource(NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;

        case ETranslateCharsetInfo:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, TranslateCharsetInfo(NULL, NULL, 0x00));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
    }

    myReleaseDC(aDC);
}

/***********************************************************************************
***
***   Test Confused Font
***
************************************************************************************/
#define numberInFonts 1

//***********************************************************************************
void
ConfusedFont(int testFunc)
{

    info(ECHO, TEXT("%s - Using Confused Fonts"), funcName[testFunc]);

    LOGFONT inLf;
    TCHAR   outName[LF_FACESIZE];
    HFONT   hFont, hFontStock;
    TDC     aDC = myGetDC();
    int     index;

    for (index = 0; index < numFonts; index++)
    {
        _tcscpy_s(inLf.lfFaceName, fontArray[index].tszFaceName);
        inLf.lfItalic = 0;
        inLf.lfWeight = FW_DONTCARE;
        inLf.lfHeight = 0;
        inLf.lfWidth = 0;
        inLf.lfEscapement = 0;
        inLf.lfOrientation = 0;
        inLf.lfUnderline = 0;
        inLf.lfStrikeOut = 0;
        inLf.lfCharSet = DEFAULT_CHARSET;
        inLf.lfOutPrecision = 0;
        inLf.lfClipPrecision = 0;
        inLf.lfQuality = 0;
        inLf.lfPitchAndFamily = 0;

        CheckNotRESULT(NULL, hFont = CreateFontIndirect(&inLf));

        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(aDC, hFont));
        if(GetTextFace(aDC, LF_FACESIZE, (LPTSTR) outName))
        {
            if (_tcscmp(outName, fontArray[index].tszFaceName) != 0)
                info(FAIL, TEXT("GetTextFace: expected:<%s> returned:<%s>"), fontArray[index].tszFaceName, outName);
        }
        else info(FAIL, TEXT("failed to retrieve text face from dc"));

        CheckNotRESULT(NULL, SelectObject(aDC, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }

    myReleaseDC(aDC);

}

/***********************************************************************************
***
***   Pass odd size
***
************************************************************************************/

//***********************************************************************************
void
passOddSize(int testFunc, int size)
{
    info(ECHO, TEXT("%s - Passing %d as Height"), funcName[testFunc], size);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    TEXTMETRIC tmFont;
    int     i,
            index = (size == 0) ? 0 : 1;

    int     fontList[] = {tahomaFont, courierFont, symbolFont, timesFont, wingdingFont};


    int     expectedNTPassOdds[2][10] = {
        {19, 18, 20, 19, 17},
        {29, 27, 30, 27, 26},
    };

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));

    for (i = 0; i < countof(fontList); i++)
    {
        if(fontList[i] != -1)
        {
            info(DETAIL, TEXT("Testing font %s"), fontArray[fontList[i]].tszFullName);
            CheckNotRESULT(NULL, hFont = setupKnownFont(fontList[i], size, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
            CheckNotRESULT(FALSE, GetTextMetrics(hdc, &tmFont));
            if (tmFont.tmHeight != expectedNTPassOdds[index][i])
                info(FAIL, TEXT("Font size incorrect:%d expected:%d"), tmFont.tmHeight, expectedNTPassOdds[index][i]);
            CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
            CheckNotRESULT(FALSE, DeleteObject(hFont));

        }
    }

    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Width of Modified Font
***
************************************************************************************/
static BYTE attrib[8][3] = { {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1} };

// by definition the result of GetCharWidth32 should be identical to the a+b+c widths for every character
// in every font.

void
WidthModifiedABCvsChar32Comparison(int testFunc)
{
    info(ECHO, TEXT("%s - WidthModifiedABCvsChar32Comparison"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    // just do aFont because this can take a while when going through a lot of modifications to the font.
    int     fontList[] = {aFont};
    ABC   abcArray['z' - '!' + 1];
    int     chArray['z' - '!' + 1];
    HFONT   hFont, hFontStock;
    int     allowVariance = 0;
    int     ABCResult = 0;
    int     nFont = 0;
    long    Hei,
            Wid,
            Esc,
            Ori,
            Wei,
            Att;

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));

    for(nFont = 0; nFont < countof(fontList); nFont++)
    {
        if(fontList[nFont] != -1)
        {
            info(DETAIL, TEXT("Testing font %s"), fontArray[fontList[nFont]].tszFaceName);

            // step through an assortment of modifications to the font, including the modifications compounded together
                for (Wid = Hei = 0; Wid < 100; Hei = (Wid += 30))
                    for (Att = 0; Att < 8; Att += 1)
                        for (Esc = 0; Esc < 900; Esc += 300)
                            for (Ori = 0; Ori < 900; Ori += 300)
                                for (Wei = 0; Wei < 1000; Wei += 100)
                                {
                                    CheckNotRESULT(NULL, hFont = setupFont(fontArray[fontList[nFont]].tszFullName, Hei, Wid, Esc, Ori, Wei, attrib[Att][0], attrib[Att][1],attrib[Att][2]));
                                    CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
                                    CheckNotRESULT(FALSE, GetCharABCWidths(hdc, '!', 'z', abcArray));
                                    CheckNotRESULT(FALSE, GetCharWidth32(hdc, '!', 'z', chArray));

                                    for(int i=0; i < ('z' - '!' + 1); i++)
                                    {
                                        ABCResult = abcArray[i].abcA + abcArray[i].abcB + abcArray[i].abcC;

                                        // when Escapement values are used, allow an off-by-one variance in either direction
                                        allowVariance = (0==Esc) ? (0) : (1);
                                        if (abs(ABCResult - chArray[i]) > allowVariance)
                                        {
                                            info(FAIL, TEXT("(%c) ABC:%d != Char32:%d for Hei:%d Wid:%d Esc:%d Ori:%d Wei:%d Ital:%d Under:%d Strike:%d"),
                                                     '!'+i, ABCResult, chArray[i], Hei, Wid, Esc, Ori, Wei, attrib[Att][0], attrib[Att][1], attrib[Att][2]);
                                            goto Lexit;  // we found a mismatch, no point in continuing with the comparison
                                        }
                                    }
                                    CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
                                    CheckNotRESULT(FALSE, DeleteObject(hFont));
                                }
        }
    }

Lexit:
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Multiple Add test
***
************************************************************************************/

//***********************************************************************************
void
MultipleAddTest(int testFunc)
{
    info(ECHO, TEXT("%s -  MultipleAddTest"), funcName[testFunc]);
    int     result = 1;
    int UserFontIndex;
    int j;

    RemoveAllUserFonts();

    for (UserFontIndex = 0; UserFontIndex < gnUserFontsEntries; UserFontIndex++)
    {
        if(gfdUserFonts[UserFontIndex].bFontAdded)
            break;
    }

    if(UserFontIndex != gnUserFontsEntries)
    {
        info(DETAIL, TEXT("Using %s"), gfdUserFonts[UserFontIndex].tszFullPath);

        for (j = 0; j < 1000 && result; j++)
        {
            // my addfontresource returns the number of fonts added, which is not true/false, but
            // a count, 1&=2 is false, not true
            CheckNotRESULT(0, AddFontResource(gfdUserFonts[UserFontIndex].tszFullPath));
        }

        // now decrease the refcount appropriately.
        for (j = 0; j < 1000 && result; j++)
        {
            CheckNotRESULT(FALSE, RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath));
        }
    }
    else info(DETAIL, TEXT("No user fonts added for testing"));

    AddAllUserFonts();
}

//***********************************************************************************
void
OutOfOrderAddRemoveTest(int testFunc)
{
    info(ECHO, TEXT("%s -  OutOfOrderAddRemoveTest"), funcName[testFunc]);
    int     result = 1;
    int UserFontIndex1, UserFontIndex2;
    int nAvailableUserFonts;
    int *anAvailableUserFonts;

    RemoveAllUserFonts();

    nAvailableUserFonts = 0;
    // count up how many user fonts are available to use.
    for (UserFontIndex1 = 0; UserFontIndex1 < gnUserFontsEntries; UserFontIndex1++)
    {
        if(gfdUserFonts[UserFontIndex1].bFontAdded)
            nAvailableUserFonts++;
    }

    if(nAvailableUserFonts >= 2)
    {
        anAvailableUserFonts = new(int[nAvailableUserFonts]);

        if(anAvailableUserFonts)
        {
            nAvailableUserFonts=0;
            // setup the available user font array
            for (UserFontIndex1 = 0; UserFontIndex1 < gnUserFontsEntries; UserFontIndex1++)
            {
                if(gfdUserFonts[UserFontIndex1].bFontAdded)
                    anAvailableUserFonts[nAvailableUserFonts++] = UserFontIndex1;
            }

            for(int i = 0; i < 10; i++)
            {

                UserFontIndex1 = GenRand() % nAvailableUserFonts;
                UserFontIndex2 = GenRand() % nAvailableUserFonts;

                info(DETAIL, TEXT("Using %s and %s"), gfdUserFonts[anAvailableUserFonts[UserFontIndex1]].tszFullPath, gfdUserFonts[anAvailableUserFonts[UserFontIndex2]].tszFullPath);

                CheckNotRESULT(0, AddFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex1]].tszFullPath));
                CheckNotRESULT(0, AddFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex2]].tszFullPath));
                CheckNotRESULT(FALSE, RemoveFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex1]].tszFullPath));
                CheckNotRESULT(FALSE, RemoveFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex2]].tszFullPath));
                CheckNotRESULT(0, AddFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex1]].tszFullPath));
                CheckNotRESULT(0, AddFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex2]].tszFullPath));
                CheckNotRESULT(FALSE, RemoveFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex2]].tszFullPath));
                CheckNotRESULT(FALSE, RemoveFontResource(gfdUserFonts[anAvailableUserFonts[UserFontIndex1]].tszFullPath));

            }
            delete[] anAvailableUserFonts;

        }
        else info(FAIL, TEXT("Array allocation failed."));
    }
    else info(DETAIL, TEXT("%d available user fonts, need 2 or more"), nAvailableUserFonts);

    AddAllUserFonts();
}

/***********************************************************************************
***
***   Multiple Adds and Removes test
***
************************************************************************************/

//***********************************************************************************
void
MultipleAddRemoveTest(int testFunc)
{

    info(ECHO, TEXT("%s -  MultipleAddRemoveTest"), funcName[testFunc]);
    int     result0 = 0,
            result1,
            result2,
            result3,
            result4,
            result5;
    int UserFontIndex;

    RemoveAllUserFonts();

    for (UserFontIndex = 0; UserFontIndex < gnUserFontsEntries; UserFontIndex++)
    {
        if(gfdUserFonts[UserFontIndex].bFontAdded)
            break;
    }

    if(UserFontIndex != gnUserFontsEntries)
    {
        info(DETAIL, TEXT("Using %s"), gfdUserFonts[UserFontIndex].tszFullPath);

        for (int j = 0; j < 3; j++)
            result0 +=AddFontResource(gfdUserFonts[UserFontIndex].tszFullPath);

        if (!result0)
        {
            info(ABORT, TEXT("AddFontResource returned:%d expected:>1"), result0);
            return;
        }

        result1 = RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath);
        result2 = RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath);
        result3 = RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath);
        result4 = RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath);
        result5 = RemoveFontResource(gfdUserFonts[UserFontIndex].tszFullPath);

        if (!result1)
            info(FAIL, TEXT("RemoveFontResource#1 returned:%d expected:1"), result1);

        if (!result2)
            info(FAIL, TEXT("RemoveFontResource#2 returned:%d expected:1"), result2);

        if (!result3)
            info(FAIL, TEXT("RemoveFontResource#3 returned:%d expected:1"), result3);

        if (result4)
            info(FAIL, TEXT("RemoveFontResource#4 returned:%d expected:0"), result4);

        if (result5)
            info(FAIL, TEXT("RemoveFontResource#5 returned:%d expected:0"), result5);
    }
    else info(DETAIL, TEXT("No user fonts added for testing"));

    AddAllUserFonts();
}

/***********************************************************************************
***
***   Font EnumFont Check
***
************************************************************************************/
int     EnumFontCheckCBAdded;

//***********************************************************************************
int     CALLBACK
EnumFontCheckCB(LOGFONT * lpelf, const NEWTEXTMETRIC * lpntm, DWORD FontType, LPARAM lParam)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("inside EnumFontCheckCB"));

#ifdef UNDER_CE
    if(FontType != TRUETYPE_FONTTYPE && FontType != RASTER_FONTTYPE)
        info(FAIL, TEXT("CE only supports truetype and raster fonts, recieved a fonttype of %d"), FontType);
#endif

    // Check the value of lParam.
    if (lParam != pass1)
        info(FAIL, TEXT("lParam1 has the wrong value."));

    // Increment the count of how many total fonts have been enumerated.
    info(DETAIL, TEXT("<%s> h:%d w:%d"), lpelf->lfFaceName, lpelf->lfHeight, lpelf->lfWidth);
    EnumFontCheckCBAdded++;

    return pass2;
}

//***********************************************************************************
void
EnumFontCheck(int testFunc)
{

    info(ECHO, TEXT("%s -  EnumFontCheck"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result1;

    for (int i = 0; i < numFonts; i++)
    {
        EnumFontCheckCBAdded = 0;
        result1 = EnumFonts(hdc, fontArray[i].tszFaceName, (FONTENUMPROC) EnumFontCheckCB, pass1);
        if (result1 != pass2)
            info(FAIL, TEXT("EnumFonts returned:0x%x expected:0x%x %s %d"), result1, pass2, fontArray[i].tszFaceName, i);

        info(DETAIL, TEXT("<%s>  EFCB:%d"), fontArray[i].tszFaceName, EnumFontCheckCBAdded);
    }

    myReleaseDC(hdc);

}


/***********************************************************************************
***
***   TestCreateFontIndirectZero
***
************************************************************************************/

//***********************************************************************************
void
TestCreateFontIndirectZero(int testFunc)
{

    info(ECHO, TEXT("%s - Passing LOGFONT of Zero contents:"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    LOGFONT lFont;
    HFONT   hFont,
            hFont2,
            hFontStock;

    CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 65, 0, 450, 0, 0, 0));

    CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

    memset((LPVOID) & lFont, 0, sizeof (LOGFONT));
    CheckNotRESULT(NULL, hFont2 = CreateFontIndirect(&lFont));
    CheckNotRESULT(NULL, SelectObject(hdc, hFont2));
    CheckNotRESULT(NULL, hFont2 = (HFONT) GetCurrentObject(hdc, OBJ_FONT));
    CheckNotRESULT(0, GetObject(hFont2, sizeof (LOGFONT), &lFont));

    if (0 !=
        lFont.lfHeight + lFont.lfWidth + lFont.lfEscapement + lFont.lfOrientation + lFont.lfWeight + lFont.lfItalic +
        lFont.lfUnderline + lFont.lfStrikeOut + lFont.lfCharSet + lFont.lfOutPrecision + lFont.lfClipPrecision +
        lFont.lfQuality + lFont.lfPitchAndFamily + lFont.lfFaceName[0])
        info(FAIL, TEXT("Expecting all zeros: %d %d %d %d %d %d %d %d %d %d %d %d %d <%s>"), lFont.lfHeight, lFont.lfWidth,
             lFont.lfEscapement, lFont.lfOrientation, lFont.lfWeight, lFont.lfItalic, lFont.lfUnderline, lFont.lfStrikeOut,
             lFont.lfCharSet, lFont.lfOutPrecision, lFont.lfClipPrecision, lFont.lfQuality, lFont.lfPitchAndFamily,
             lFont.lfFaceName);

    CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
    CheckNotRESULT(FALSE, DeleteObject(hFont));
    CheckNotRESULT(FALSE, DeleteObject(hFont2));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetCharABCWidths
***
************************************************************************************/

// depth test only tests a few lowercase characters in one character set with a wide range of modifications.
// restrict to zero escapements because the bounding box needs to be angle-adjusted for non-zero values.
//***********************************************************************************
void
ABCDrawTestDepth(int testFunc)
{
    info(ECHO, TEXT("%s - ABCDrawTestDepth"), funcName[testFunc]);
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    HFONT hFont, hFontStock;
    RECT rc = { 20, 20, zx, zy };
    // fghijklm has a fairly wide assorment of abc values to test
    ABC     abcArray['m' - 'f' + 1];
    TCHAR tch[1];
    long    Hei,
            Wid,
            Esc = 0,
            Wei;
    BYTE Ital;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
            if(!g_bRTL)
                CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));
            else
                CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        info(DETAIL, TEXT("Testing font %s"), fontArray[aFont].tszFaceName);
        for (Hei = 0; Hei <= 100; Hei += 50)
            for (Wid = 0; Wid <= 100; Wid += 50)
                // TODO: adjust bounding box for non-zero escapements
                //for (Esc = 0; Esc <= 400; Esc += 200)
                for (Wei = 0; Wei < 1000; Wei += 200)
                    for (Ital = 0; Ital <= 1; Ital++)
                    {
                        // setupFont will output error message
                        CheckNotRESULT(NULL, hFont = setupFont(fontArray[aFont].tszFullName, Hei, Wid, Esc, 0, Wei, Ital, 0, 0));

                        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
                        CheckNotRESULT(FALSE, GetCharABCWidths(hdc, 'f', 'm', abcArray));

                        // calculate the height of the rect to speed up the patblt.
                        tch[0] = 'a';
                        CheckNotRESULT(0, DrawText(hdc, tch, 1, &rc, DT_LEFT | DT_TOP | DT_NOCLIP | DT_NOPREFIX | DT_CALCRECT));

                        for(int i=0; i < ('m' - 'f' + 1); i++)
                        {
                            tch[0] =(unsigned short) ('f' + i);
                            CheckNotRESULT(0, ExtTextOut(hdc, rc.left, rc.top, ETO_IGNORELANGUAGE, &rc, tch, 1, NULL));
                            // increase the box size by 1 all the way around, to compensate for possible bleeding outside of the box
                            // when doing cleartype and antialiasing.
                            if(!g_bRTL)
                                CheckNotRESULT(FALSE, PatBlt(hdc, rc.left - 1 + abcArray[i].abcA, rc.top - 1,
                                        rc.left + 1 + (abcArray[i].abcA > 0 ? abcArray[i].abcA:0) + abcArray[i].abcB + abs(abcArray[i].abcC < 0?abcArray[i].abcC:0),
                                        rc.bottom + 1, WHITENESS));
                            else
                                CheckNotRESULT(FALSE, PatBlt(hdc, rc.left - 1 + abcArray[i].abcC, rc.top - 1,
                                        rc.left + 1 + (abcArray[i].abcC > 0 ? abcArray[i].abcC:0) + abcArray[i].abcB + abs(abcArray[i].abcA < 0?abcArray[i].abcA:0),
                                        rc.bottom + 1, WHITENESS));

                             // check all white returns number of mismatches, we expect 0, so if it's > 0, we have a failure.
                             if(CheckAllWhite(hdc, FALSE, 0))
                             {
                                 info(FAIL, TEXT("ExtTextOut draws outside bounding box: Hei %d Wid %d Esc %d Wei %d Italic %d "), Hei, Wid, Esc, Wei, Ital);
                                 info(FAIL, TEXT("ExtTextOut draws outside bounding box: character %c larger than bounding box"), tch[0]);
                                 info(FAIL, TEXT("ExtTextOut draws outside bounding box: ABC is %d %d %d"), abcArray[i].abcA, abcArray[i].abcB, abcArray[i].abcC);
                                 // that character overlapped, so all following tests will fail unless the screen is reset.
                                 CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                             }
                        }
                        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
                        CheckNotRESULT(FALSE, DeleteObject(hFont));
                    }

        // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
            CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

// Breadth test tests all characters of all character sets, but with only one set of modifications.
void
ABCDrawTestBreadth(int testFunc)
{
    info(ECHO, TEXT("%s - ABCDrawTestBreadth"), funcName[testFunc]);
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    HFONT hFont, hFontStock;
    RECT rc = { 20, 20, zx, zy };
    ABC     abcArray['z' - '!' + 1];
    TCHAR tch[1];

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
            if(!g_bRTL)
                CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));
            else
                CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));

        info(DETAIL, TEXT("Using Width=50 Height=50 Weight=500 for all fonts"));

        for(int index = 0; index < numFonts; index++)
        {
            if(_tcsicmp(fontArray[index].tszFaceName, TEXT("Wingdings")) != 0)
            {
                info(DETAIL, TEXT("Testing font %s"), fontArray[index].tszFullName);

                CheckNotRESULT(NULL, hFont = setupKnownFont(index, 50, 50, 0, 0, 0, 0));
                CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
                // calculate the height of the rect to speed up the PatBlt Call
                tch[0] = 'a';
                CheckNotRESULT(0, DrawText(hdc, tch, 1, &rc, DT_LEFT | DT_TOP | DT_NOCLIP | DT_NOPREFIX | DT_CALCRECT));

                CheckNotRESULT(FALSE, GetCharABCWidths(hdc, '!', 'z', abcArray));

                for(int i=0; i < ('z' - '!' + 1); i++)
                {
                    tch[0] =(unsigned short) ('!' + i);
                    CheckNotRESULT(0, ExtTextOut(hdc, rc.left, rc.top, ETO_IGNORELANGUAGE, &rc, tch, 1, NULL));

                    // increase the box size by 1 all the way around, to compensate for possible bleeding outside of the box
                    // when doing cleartype and antialiasing.
                    if(!g_bRTL)
                        CheckNotRESULT(FALSE, PatBlt(hdc,  rc.left - 1 + abcArray[i].abcA, rc.top - 1,
                                rc.left + 1 + (abcArray[i].abcA > 0 ? abcArray[i].abcA:0) + abcArray[i].abcB + abs(abcArray[i].abcC < 0?abcArray[i].abcC:0),
                                rc.bottom + 1, WHITENESS));
                    else
                        CheckNotRESULT(FALSE, PatBlt(hdc,  rc.left - 1 + abcArray[i].abcC, rc.top - 1,
                                rc.left + 1 + (abcArray[i].abcC > 0 ? abcArray[i].abcC:0) + abcArray[i].abcB + abs(abcArray[i].abcA < 0?abcArray[i].abcA:0),
                                rc.bottom + 1, WHITENESS));

                    // check all white returns number of mismatches, we expect 0, so if it's > 0, we have a failure.
                    if(CheckAllWhite(hdc, FALSE, 0))
                    {
                        info(FAIL, TEXT("character %c larger than bounding box"), tch[0]);
                        info(FAIL, TEXT("ABC is %d %d %d"), abcArray[i].abcA, abcArray[i].abcB, abcArray[i].abcC);
                        // that character overlapped, so all following tests will fail unless the screen is reset.
                        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                    }
                }
                CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
                CheckNotRESULT(FALSE, DeleteObject(hFont));
            }
        }

            // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
            CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
CreateFontSmoothingTest(int testFunc)
{
    info(ECHO, TEXT("%s - CreateFontSmoothingTest"), funcName[testFunc]);
#ifdef SPI_GETFONTSMOOTHING

    TDC hdc = myGetDC();
    LOGFONT lf;
    int naQualities[] = {
                                 ANTIALIASED_QUALITY,
                                 NONANTIALIASED_QUALITY,
                                 CLEARTYPE_COMPAT_QUALITY,
                                 CLEARTYPE_QUALITY,
                                 DEFAULT_QUALITY,
                                 DRAFT_QUALITY
                                };
    HFONT hFontCurrentFonts[countof(naQualities)];
    TDC hdcCompat[countof(naQualities)];
    HFONT hfontStock, hfontRetrieved;
    int i;
    BOOL bOriginalFontSmoothing, bSmoothing = TRUE;

    // retrieve the current fontsmoothing
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalFontSmoothing, 0));

    for(int j =0; j < 2; j++)
    {
        // pass 0, j is 0 and we turn off smoothing before the test, but turn it on later.
        // pass 1, j is 1 and we turn on smoothing before the test, but turn it off later.
        bSmoothing = j;

        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bSmoothing, 0));

        memset(&lf, 0, sizeof(LOGFONT));

        // retrieve the stock font
        CheckNotRESULT(NULL, hfontStock = (HFONT) GetCurrentObject(hdc, OBJ_FONT));

        // create the fonts and DC's to select them into.
        for(i=0; i < countof(naQualities); i++)
        {
            CheckNotRESULT(NULL, hdcCompat[i] = CreateCompatibleDC(hdc));
            lf.lfQuality = naQualities[i];
            CheckNotRESULT(NULL, hFontCurrentFonts[i] = CreateFontIndirect(&lf));
            CheckNotRESULT(NULL, SelectObject(hdcCompat[i], hFontCurrentFonts[i]));
        }

        // pass 0, j is 0 and we turn off smoothing before the test, but turn it on later.
        // pass 1, j is 1 and we turn on smoothing before the test, but turn it off later.
        bSmoothing = !j;
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bSmoothing, 0, 0));

        for(i=0; i < countof(naQualities); i++)
        {
            // retrieve the font from the current dc.
            CheckNotRESULT(NULL, hfontRetrieved = (HFONT) GetCurrentObject(hdcCompat[i], OBJ_FONT));
            CheckNotRESULT(NULL, GetObject(hfontRetrieved, sizeof(LOGFONT), &lf));

            if(lf.lfQuality != naQualities[i])
                info(FAIL, TEXT("Font returned doesn't match expected.  %d returned, %d expected"), lf.lfQuality, naQualities[i]);
        }

        // cleanup the array's of fonts and DC's.
        for(i=0; i < countof(naQualities); i++)
        {
            CheckNotRESULT(NULL, SelectObject(hdcCompat[i], hfontStock));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat[i]));
            CheckNotRESULT(FALSE, DeleteObject(hFontCurrentFonts[i]));
        }
    }

    // restore the original smoothing.
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalFontSmoothing, 0, 0));

    myReleaseDC(hdc);
#endif
}

void
TestEnumFontFamiliesNullFontname(int testFunc)
{
    info(ECHO, TEXT("%s - TestEnumFontFamiliesNullFontname"), funcName[testFunc]);

    // this test counts enumerations of fonts based on their full name.
    // there can be isntances where a font, like "tahoma bold" can be enumerated multiple times
    // because a user font was added which matches the font name
    RemoveAllUserFonts();

    TDC hdc = myGetDC();
    struct EnumFontCount efc;
    LOGFONT lf;

    memset(&efc, 0, sizeof(struct EnumFontCount));
    memset(&lf, 0, sizeof(LOGFONT));

    switch(testFunc)
    {
        // EnumFontFamiliesEX enumerates one of each full name
        case EEnumFontFamiliesEx:
            efc.bUseFullName = TRUE;
            CheckNotRESULT(0, EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efc, 0));
            break;
        //EnumFontFamilies and EnumFonts enumerates one of each face name.
        case EEnumFontFamilies:
            efc.bUseFaceName = TRUE;
            CheckNotRESULT(0, EnumFontFamilies(hdc, NULL, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efc));
            break;
        case EEnumFonts:
            efc.bUseFaceName = TRUE;
            CheckNotRESULT(0, EnumFonts(hdc, NULL, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efc));
            break;
    }

    // verify that no font was enumerated twice (since by definition these api's are supposed to enumerate one of each.
    for(int i = 0; i < efc.nFaceNamesInUse; i++)
        if(efc.anFaceNameCount[i] > 1)
            info(FAIL, TEXT("Font %s enumerated %d times, expected to be enumerated only once."), efc.lfFaceNames[i], efc.anFaceNameCount[i]);

    myReleaseDC(hdc);

    AddAllUserFonts();
}

void
TestEnumFontFamiliesExCharsetTest(int testFunc)
{
    info(ECHO, TEXT("%s - TestEnumFontFamiliesExCharsetTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    struct EnumFontCount efc;
    LOGFONT lf;

    for(int i = 0; i < countof(gnvCharSets); i++)
    {
        memset(&efc, 0, sizeof(struct EnumFontCount));
        memset(&lf, 0, sizeof(LOGFONT));

        efc.bUseFullName = TRUE;
        lf.lfCharSet = (BYTE) gnvCharSets[i].dwVal;

        info(DETAIL, TEXT("Testing with charset = %s"), gnvCharSets[i].szName);
        // if no fonts of a character set are present, this may return 0.
        EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efc, 0);

        if(efc.nCharSet[0] != 0)
            info(FAIL, TEXT("An unknown charset was enumerated when requesting %s"), gnvCharSets[i].szName);
        for(int j=0; j < countof(gnvCharSets); j++)
        {
            if(j != i && efc.nCharSet[GetCharsetIndex(gnvCharSets[j].dwVal)])
                info(FAIL, TEXT("Requested %s charset, however %d of %s charset were enumerated"),
                                gnvCharSets[i].szName, efc.nCharSet[GetCharsetIndex(gnvCharSets[j].dwVal)], gnvCharSets[j].dwVal);
            else if(j == i && efc.nTotalEnumerations != efc.nCharSet[GetCharsetIndex(gnvCharSets[j].dwVal)])
                info(FAIL, TEXT("All %d enumerations were expected to be of %s charset, %d were"),
                                efc.nTotalEnumerations, gnvCharSets[j].szName, efc.nCharSet[GetCharsetIndex(gnvCharSets[j].dwVal)]);

        }
    }

    myReleaseDC(hdc);
}

void
TestEnumFontFaceTest(int testFunc)
{
    info(ECHO, TEXT("%s - TestEnumFontFamiliesExCharsetTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    struct EnumFontCount efc, efcEnumerated;
    LOGFONT lf;

    memset(&efc, 0, sizeof(struct EnumFontCount));
    memset(&lf, 0, sizeof(LOGFONT));

    efc.bUseFaceName = TRUE;
    lf.lfCharSet = DEFAULT_CHARSET; // enumerate everything in the default charset
    CheckNotRESULT(0, EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efc, 0));

    // now enumerate each separate face and verify the count returned matches what's expected and no other faces are enumerated.
    for(int i = 0; i < efc.nFaceNamesInUse; i++)
    {
        memset(&efcEnumerated, 0, sizeof(struct EnumFontCount));
        memset(&lf, 0, sizeof(LOGFONT));

        lf.lfCharSet = DEFAULT_CHARSET;
        efcEnumerated.bUseFaceName = TRUE;

        // copy over the face name to enumerate
        _tcsncpy_s(lf.lfFaceName, efc.lfFaceNames[i], LF_FACESIZE-1);

        switch(testFunc)
        {
            case EEnumFontFamiliesEx:
                CheckNotRESULT(0, EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efcEnumerated, 0));
                break;
            case EEnumFontFamilies:
                CheckNotRESULT(0, EnumFontFamilies(hdc, lf.lfFaceName, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efcEnumerated));
                break;
            case EEnumFonts:
                CheckNotRESULT(0, EnumFonts(hdc, lf.lfFaceName, (FONTENUMPROC) EnumFontFamCountProc, (LPARAM) &efcEnumerated));
                break;
        }

        // atleast 1 face name should have been enumerated
        // more than one face name could have been enumerated, and the face names may be enumerated more or less than the original times enumerated.
        if(efcEnumerated.nFaceNamesInUse <= 0)
            info(FAIL, TEXT("Expected atleast 1 face name to be enumerated for %s, however %d were"), lf.lfFaceName, efcEnumerated.nFaceNamesInUse);
    }

    myReleaseDC(hdc);
}

void
TestTranslateCharsetInfo(int testFunc)
{
    info(ECHO, TEXT("%s - TestTranslateCharsetInfo"), funcName[testFunc]);

    // define our supported charset parameters
    const int NCHARSETS = 16;
    const unsigned int charsets[NCHARSETS] = {
        ANSI_CHARSET,    SHIFTJIS_CHARSET,    HANGEUL_CHARSET,    JOHAB_CHARSET,
        GB2312_CHARSET,  CHINESEBIG5_CHARSET, HEBREW_CHARSET,     ARABIC_CHARSET,
        GREEK_CHARSET,   TURKISH_CHARSET,     BALTIC_CHARSET,     EASTEUROPE_CHARSET,
        RUSSIAN_CHARSET, THAI_CHARSET,        VIETNAMESE_CHARSET, SYMBOL_CHARSET };
    const unsigned int codepages[NCHARSETS] = {
        1252, 932,  949,  1361,
        936,  950,  1255, 1256,
        1253, 1254, 1257, 1250,
        1251, 874,  1258, 42 };
    const DWORD fs[NCHARSETS] = {
        FS_LATIN1,      FS_JISJAPAN,    FS_WANSUNG,    FS_JOHAB,
        FS_CHINESESIMP, FS_CHINESETRAD, FS_HEBREW,     FS_ARABIC,
        FS_GREEK,       FS_TURKISH,     FS_BALTIC,     FS_LATIN2,
        FS_CYRILLIC,    FS_THAI,        FS_VIETNAMESE, FS_SYMBOL };

    // test all flag combinations
    const int NTESTCASES = 3;
    const DWORD dwTestCase[NTESTCASES] = {
        TCI_SRCCHARSET, TCI_SRCCODEPAGE, TCI_SRCFONTSIG };

    // cycle through all supported code pages and flag combinations
    for (int nCodePage=0; nCodePage<NCHARSETS; nCodePage++)
    {
        for (int i=0; i<NTESTCASES; i++)
        {
            DWORD dwFlags = dwTestCase[i];
            DWORD FAR *lpSrc = NULL;
            CHARSETINFO myCs = {0};

            switch (dwFlags)
            {
            case TCI_SRCCHARSET:
                lpSrc = (DWORD*)charsets[nCodePage];
                break;

            case TCI_SRCCODEPAGE:
                lpSrc = (DWORD*)codepages[nCodePage];
                break;

            case TCI_SRCFONTSIG:
                myCs.fs.fsCsb[0] = fs[nCodePage];
                myCs.fs.fsCsb[1] = 0x00;
                lpSrc = myCs.fs.fsCsb;
                break;
            }

            // attempt to retrieve our charset info
            SetLastError(ERROR_SUCCESS);
            CheckNotRESULT(0, TranslateCharsetInfo(lpSrc, &myCs, dwFlags));

            // verify expected result matches our defined support
            CheckForRESULT(charsets[nCodePage], myCs.ciCharset);
            CheckForRESULT(codepages[nCodePage], myCs.ciACP);
            CheckForRESULT(fs[nCodePage], myCs.fs.fsCsb[0]);
            CheckForLastError(ERROR_SUCCESS);
        }
    }

    // verify invalid bit in OEM code page results in failure
    {
        CHARSETINFO cs = {0};
        DWORD FAR *lpSrc = NULL;

        cs.fs.fsCsb[0] = 0x00;
        cs.fs.fsCsb[1] = 0x01;
        lpSrc = cs.fs.fsCsb;

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, TranslateCharsetInfo(lpSrc, &cs, TCI_SRCFONTSIG));
        CheckForLastError(ERROR_INVALID_PARAMETER);
    }
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI AddFontResource_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(EAddFontResource);
    AddRemoveFake(EAddFontResource);
    SimpleAddRemoveFontPairs(EAddFontResource);
    SuperAddRemove(EAddFontResource);
    AddAddRemoveRemoveFontPairs(EAddFontResource);
    MultipleAddTest(EAddFontResource);
    MultipleAddRemoveTest(EAddFontResource);
    OutOfOrderAddRemoveTest(EAddFontResource);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateFontIndirect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    TestCreateFontIndirect(ECreateFontIndirect);
    TestCreateFontIndirectZero(ECreateFontIndirect);
    ConfusedFont(ECreateFontIndirect);
    passOddSize(ECreateFontIndirect, 0);
    passOddSize(ECreateFontIndirect, -24);
    CreateFontSmoothingTest(ECreateFontIndirect);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EnumFonts_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(EEnumFonts);
    TestEnumFonts(EEnumFonts);
    EnumFontCheck(EEnumFonts);
    TestEnumFontFamiliesNullFontname(EEnumFonts);
    TestEnumFontFaceTest(EEnumFonts);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EnumFontFamilies_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(EEnumFontFamilies);
    TestEnumFonts(EEnumFontFamilies);
    TestEnumFontFamiliesNullFontname(EEnumFontFamilies);
    TestEnumFontFaceTest(EEnumFontFamilies);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EnumFontFamProc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

#ifdef UNDER_CE // not a valid test under XP due to the large number of similar font's supported on XP

    // Breadth
    TestEnumFontFamProc(EEnumFontFamProc);

    // Depth
    TestEnumFontFamHighDPITest(EEnumFontFamProc);

#endif

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetCharABCWidths_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    TestFontWidths(EGetCharABCWidths);
    ABCDrawTestBreadth(EGetCharABCWidths);

    // Depth
    WidthModifiedABCvsChar32Comparison(EGetCharABCWidths);
    ABCDrawTestDepth(EGetCharABCWidths);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetCharWidth_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // Breadth
    TestFontWidths(EGetCharWidth);

    // Depth
    // None
#endif

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetCharWidth32_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    TestFontWidths(EGetCharWidth32);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI RemoveFontResource_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(ERemoveFontResource);
    AddRemoveFake(EAddFontResource);
    SimpleAddRemoveFontPairs(ERemoveFontResource);
    SuperAddRemove(ERemoveFontResource);
    RemoveStockFont(ERemoveFontResource);
    AddAddRemoveRemoveFontPairs(ERemoveFontResource);
    MultipleAddTest(ERemoveFontResource);
    MultipleAddRemoveTest(ERemoveFontResource);
    OutOfOrderAddRemoveTest(ERemoveFontResource);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EnableEUDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    // None

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI TranslateCharsetInfo_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(ETranslateCharsetInfo);
    TestTranslateCharsetInfo(ETranslateCharsetInfo);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EnumFontFamiliesEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Font(EEnumFontFamiliesEx);
    TestEnumFontFamiliesNullFontname(EEnumFontFamiliesEx);
    TestEnumFontFamiliesExCharsetTest(EEnumFontFamiliesEx);
    TestEnumFontFaceTest(EEnumFontFamiliesEx);

    // Depth
    // None

    return getCode();
}
