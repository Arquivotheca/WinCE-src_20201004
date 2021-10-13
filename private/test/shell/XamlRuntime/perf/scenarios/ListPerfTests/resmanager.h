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


typedef struct _tagBitmapInfo {
    int nResID;
    LPCTSTR pFileName;
} BITMAP_INFO;

BITMAP_INFO BmpInfo[] = {
        //AppWide
        { ID_IMG_ONE         , L"Pix01.png" },
        { ID_IMG_TWO         , L"Pix02.png" },
        { ID_IMG_THREE       , L"Pix03.png" },
        { ID_IMG_FOUR        , L"Pix04.png" },
        { ID_IMG_05          , L"Pix05.png" },
        { ID_IMG_06          , L"Pix06.png" },
        { ID_IMG_07          , L"Pix07.png" },
        { ID_IMG_08          , L"Pix08.png" },
        { ID_IMG_09          , L"Pix09.png" },
        { ID_IMG_10          , L"Pix10.png" },
        { ID_IMG_11          , L"Pix11.png" },
        { ID_IMG_12          , L"Pix12.png" },
        { ID_IMG_13          , L"Pix13.png" },
        { ID_IMG_14          , L"Pix14.png" },
        { ID_IMG_15          , L"Pix15.png" },
        { ID_IMG_16          , L"Pix16.png" },
        { ID_IMG_17          , L"Pix17.png" },
        { ID_IMG_18          , L"Pix18.png" },
        { ID_IMG_19          , L"Pix19.png" },
        { ID_IMG_20          , L"Pix20.png" }
};

// Helper object which resolves resource strings to objects
//
class ResourceHandler :
    public IXRResourceManager
{
    bool m_bLoadImagesFromFiles;

public:
    __override ULONG   STDMETHODCALLTYPE AddRef () { return 2; }
    __override ULONG   STDMETHODCALLTYPE Release() { return 1; }

    __override HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void**)
    {
        return E_NOINTERFACE;
    }

    ResourceHandler()
    {
        m_bLoadImagesFromFiles = false;
    }

    void SetLoadImagesFromFiles( bool bLoadImagesFromFiles )
    {
        m_bLoadImagesFromFiles = bLoadImagesFromFiles;
    }

    __override BOOL STDMETHODCALLTYPE ResolveImageResource(
        const WCHAR* pUri,
        IWICBitmap** ppRetrievedImage
        )
    {
        HRESULT hr = S_OK;
        int i;
        WCHAR AppNameBuffer[MAX_PATH] = {0};

        if (!pUri)
        {
            g_dwImageRet = TPR_FAIL;
            g_pKato->Log(LOG_COMMENT, L"ResolveResource was called with NULL");
            return FALSE;
        }

        if( m_bLoadImagesFromFiles )
        {
            StringCchPrintfW( AppNameBuffer, MAX_PATH, XR_XAMLRUNTIMETESTPERFCOPIES_PATH L"\\%s", pUri );
            hr = g_pTestApplication->LoadImageFromFile(AppNameBuffer, ppRetrievedImage);
            if( FAILED(hr) )
            {
                g_dwImageRet = TPR_FAIL;
                g_pKato->Log(LOG_FAIL, L"Error loading image from file: %s", AppNameBuffer );
            }
        }
        else if( g_UseBaml )
        {
            for (i = 0; i < countof(BmpInfo); i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfo[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource((HMODULE)g_hInstance, MAKEINTRESOURCE(BmpInfo[i].nResID + ID_IMG_ONE_BAML), 
                        L"XAML_RESOURCE", 
                        ppRetrievedImage);
                    break;
                }
            }
            if (i == countof(BmpInfo))
            {
                hr = E_FAIL;            //Returns FALSE down below
            }

            if( hr != S_OK)
            {   //not in the resources
                g_dwImageRet = TPR_FAIL;
                g_pKato->Log(LOG_FAIL, L"Error loading image as baml resource: %s", pUri );
            }
        }
        else    //not using BAML
        {
            for (i = 0; i < countof(BmpInfo); i++)
            {
                if (0 == wcsnicmp(pUri, BmpInfo[i].pFileName, MAX_PATH))
                {
                    hr = g_pTestApplication->LoadImageFromResource((HMODULE)g_hInstance, MAKEINTRESOURCE(BmpInfo[i].nResID), 
                        L"PNG", 
                        ppRetrievedImage);
                    break;
                }
            }
            if (i == countof(BmpInfo))
            {
                hr = E_FAIL;            //Returns FALSE down below
            }

            if( hr != S_OK)
            {   //not in the resources
                g_dwImageRet = TPR_FAIL;
                g_pKato->Log(LOG_FAIL, L"Error loading image as resource: %s", pUri );
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
