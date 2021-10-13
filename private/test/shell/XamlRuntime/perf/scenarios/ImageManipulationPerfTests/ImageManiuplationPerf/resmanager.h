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
#include <ImagePerfResourceIDs.h>
#include "globals.h"

#define NUM_BITMAP          2
#define IMAGE_PERF_APP_NAME L"XRPerfImageManipulationTests.dll"

typedef struct _tagBitmapInfo {
    int nResID;
    LPCTSTR pFileName;
} BITMAP_INFO;

BITMAP_INFO BmpInfo[NUM_BITMAP] = {
        { ID_GRAPH_LARGE_PNG      ,L"ImageManipulation/LargeGraphic.png" },
        { ID_PHOTO_LARGE_PNG      ,L"ImageManipulation/LargePhoto.png" },
};


BITMAP_INFO BmpInfoBaml[NUM_BITMAP] = {
        { ID_GRAPH_LARGE_PNG_BAML      ,L"#511"},
        { ID_PHOTO_LARGE_PNG_BAML      ,L"#512"},
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

    __override BOOL STDMETHODCALLTYPE ResolveImageResource(const WCHAR* pUri, IWICBitmap** ppRetrievedImage)
    {
        HRESULT hr = E_FAIL;

        if (! pUri)
        {
            g_dwImageLoadRet = TPR_FAIL;
            g_pKato->Log(LOG_COMMENT, L"ResolveResource was called with NULL");
            return FALSE;
        }

        if( g_bUseBaml )
        {
            int i = 0;

            for (i; i < NUM_BITMAP; i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfoBaml[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource(g_ResourceModule, MAKEINTRESOURCE(BmpInfoBaml[i].nResID), L"XAML_RESOURCE", ppRetrievedImage);
                    if( FAILED(hr) )
                    {
                        g_dwImageLoadRet = TPR_FAIL;
                        g_pKato->Log(LOG_COMMENT, L"Error loading image as baml resource: %i error code: 0x%x" , BmpInfoBaml[i].nResID, hr );
                    }
                    break;
                }
            }
            if (i == NUM_BITMAP)
            {   //not in the resources
                g_dwImageLoadRet = TPR_FAIL;
                g_pKato->Log(LOG_COMMENT, L"Error loading image as baml resource: %s", pUri );
            }
        }
        else
        {
            int i = 0;
            for (i; i < NUM_BITMAP; i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfo[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource(g_ResourceModule, MAKEINTRESOURCE(BmpInfo[i].nResID), L"PNG", ppRetrievedImage);
                    if( FAILED(hr) )
                    {
                        g_dwImageLoadRet = TPR_FAIL;
                        g_pKato->Log(LOG_COMMENT, L"Error loading image as resource: %i error code: 0x%x" , BmpInfo[i].nResID, hr );
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
