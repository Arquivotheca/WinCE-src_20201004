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
#include "resource.h" 
#include "globals.h"

#define NUM_BITMAP          17
#define IMAGE_PERF_APP_NAME L"XRPerfBackgroundAnimationTests.dll"

typedef struct _tagBitmapInfo {
    int nResID;
    LPCTSTR pFileName;
} BITMAP_INFO;

BITMAP_INFO BmpInfo[NUM_BITMAP] = {
        //AppWide
        { ID_LARGE_BG         ,L"BackgroundAnimation/Large_BG.png"},
        { ID_STREAKS_01       ,L"BackgroundAnimation/Streaks01.png"},
        { ID_STREAKS_03       ,L"BackgroundAnimation/Streaks03.png"},
        { ID_STREAKS_04       ,L"BackgroundAnimation/Streaks04.png"},
        { ID_STREAKS_05       ,L"BackgroundAnimation/Streaks05.png"},
        { ID_BTTNS_BCKGRND    ,L"BackgroundAnimation/wmpPBMediaTitleBG.png"},
        { ID_BTTNS_NEXT       ,L"BackgroundAnimation/wmpPBNextFFPressed.png"},
        { ID_BTTNS_PAUSE      ,L"BackgroundAnimation/wmpPBPausePressed.png"},
        { ID_BTTNS_PLAY       ,L"BackgroundAnimation/wmpPBPlayPressed.png"},
        { ID_BTTNS_PREV       ,L"BackgroundAnimation/wmpPBPrevFRPressed.png"},
        { ID_BTTNS_REPEAT     ,L"BackgroundAnimation/wmpPBRepeatAllSelected.png"},
        { ID_BTTNS_SUFFLE     ,L"BackgroundAnimation/wmpPBShuffleVFramePressed.png"},
        { ID_KBRD_BACKSPACE   ,L"BackgroundAnimation/BackSpace.png"},
        { ID_KBRD_ENGLISH     ,L"BackgroundAnimation/EnglishKeyboard.png"},
        { ID_KBRD_ENTER       ,L"BackgroundAnimation/Enter.png"},
        { ID_KBRD_SHIFT_NRML  ,L"BackgroundAnimation/ShiftNormal.png"},
        { ID_KBRD_SHIFT_PRSSD ,L"BackgroundAnimation/ShiftPressed.png"}
};

BITMAP_INFO BmpInfoBaml[NUM_BITMAP] = {
        //AppWide
        { ID_LARGE_BG_BAML         ,L"#501"},
        { ID_STREAKS_01_BAML       ,L"#502"},
        { ID_STREAKS_03_BAML       ,L"#503"},
        { ID_STREAKS_04_BAML       ,L"#504"},
        { ID_STREAKS_05_BAML       ,L"#505"},
        { ID_BTTNS_BCKGRND_BAML    ,L"#506"},
        { ID_BTTNS_NEXT_BAML       ,L"#507"},
        { ID_BTTNS_PAUSE_BAML      ,L"#508"},
        { ID_BTTNS_PLAY_BAML       ,L"#509"},
        { ID_BTTNS_PREV_BAML       ,L"#510"},
        { ID_BTTNS_REPEAT_BAML     ,L"#511"},
        { ID_BTTNS_SUFFLE_BAML     ,L"#512"},
        { ID_KBRD_BACKSPACE_BAML   ,L"#513"},
        { ID_KBRD_ENGLISH_BAML     ,L"#514"},
        { ID_KBRD_ENTER_BAML       ,L"#515"},
        { ID_KBRD_SHIFT_NRML_BAML  ,L"#516"},
        { ID_KBRD_SHIFT_PRSSD_BAML ,L"#517"}
};
// Helper object which resolves resource strings to objects
//
class ResourceHandler : public IXRResourceManager
{
public:
    __override ULONG   STDMETHODCALLTYPE AddRef () { return 2; }
    __override ULONG   STDMETHODCALLTYPE Release() { return 1; }

    __override HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void**)
    {
        return E_NOINTERFACE;
    }

    ResourceHandler()
    {}

    __override BOOL STDMETHODCALLTYPE ResolveImageResource(
        const WCHAR* pUri,
        IWICBitmap** ppRetrievedImage
        )
    {
        HRESULT hr = E_FAIL;

        if (! pUri)
        {
            g_pKato->Log(LOG_COMMENT, L"ResolveResource was called with NULL");
            return FALSE;
        }

        HMODULE hMod = GetModuleHandle( IMAGE_PERF_APP_NAME );
        if(!hMod)
        {
            g_dwImageLoadRet = TPR_FAIL;
            g_pKato->Log(LOG_COMMENT, L"GetModuleHandle failed in ResManager");
            return FALSE;
        }

        if( g_bUseBaml )
        {
            for (int i = 0; i < NUM_BITMAP; i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfoBaml[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource((HMODULE)g_hInstance, MAKEINTRESOURCE(BmpInfoBaml[i].nResID), L"XAML_RESOURCE", ppRetrievedImage);
                    if( FAILED(hr) )
                    {
                        g_dwImageLoadRet = TPR_FAIL;
                        g_pKato->Log(LOG_COMMENT, L"Error loading image as baml resource: %i error code: %d" , BmpInfoBaml[i].nResID, hr );
                    }
                    break;
                }
            }
            if (i == NUM_BITMAP)
            {   //not in the resources
                g_dwImageLoadRet = TPR_FAIL;
                g_pKato->Log(LOG_COMMENT, L"Error loading image as resource: %s", pUri );
            }
        }
        else
        {
            for (int i = 0; i < NUM_BITMAP; i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfo[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource(hMod, MAKEINTRESOURCE(BmpInfo[i].nResID), L"PNG", ppRetrievedImage);
                    if( FAILED(hr) )
                    {
                        g_dwImageLoadRet = TPR_FAIL;
                        g_pKato->Log(LOG_COMMENT, L"Error loading image as resource: %i error code: %d" , BmpInfo[i].nResID, hr );
                    }
                    break;
                }
            }
            if (i == NUM_BITMAP)
            {   //not in the resources
                g_dwImageLoadRet = TPR_FAIL;
                g_pKato->Log(LOG_COMMENT, L"Error loading image as resource: %s", pUri );
            }
        }

        return SUCCEEDED(hr);
    }

    __override BOOL STDMETHODCALLTYPE ResolveFontResource(
        const WCHAR* pUri,
        HANDLE* pHandle
        )
    {
        return FALSE;
    }

    __override BOOL STDMETHODCALLTYPE ResolveStringResource(
        UINT ResourceID,
        BSTR* pbstrString
        )
    {
        return FALSE;
    }
};
