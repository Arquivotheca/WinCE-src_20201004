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
// 
//
#ifdef UNDER_CE
#include "logging.h"
#include "BaseCustomAP.h"
#include "mmimaging.h"
#include "vmrp.h"
#include "CaptureBMP.h"

CBaseCustomAP::CBaseCustomAP( HRESULT *phr )
:CUnknown(NAME("Custom Allocator Presenter"), NULL)
{
    m_bActive = FALSE;
    m_bImageSavingMode = FALSE;
    m_bMediaTypeChanged = FALSE;
    m_bCaptureMode = FALSE;
    m_bUsingOverlay = TRUE;
    m_bOverlaySupported = FALSE;
    m_CustomWindowless.AddRef();
    m_pfnCallback = NULL;
    m_pUserData = NULL;

    m_dwThreadID = 0;
    m_dwMatchCount = 0;
    m_dwFrameNo = 0;
    m_dwRotation = 0;
    m_iStride = 0;
    m_hThread = NULL;
    m_hQuitEvent = CreateEvent( NULL, FALSE, FALSE, TEXT("CCustomeAP_Quit"));
    if( !m_hQuitEvent )
    {
        LOG( TEXT("Failed to create quit event\n") );
        *phr = E_FAIL;
    }

    memset(&m_ddsd, 0, sizeof(m_ddsd));
    m_ddsd.dwSize = sizeof(m_ddsd);

    //ZeroMemory( m_StreamData, 3 * sizeof(VMRStreamData) );
    m_lpDDrawDevice = 0x0;
    m_hMonitor        = NULL;
    m_hwndApp   = NULL;
    // We only need to have a few images being saved
    m_dwVerificationRate = 100;    
    m_dwCompWidth = 0;
    m_dwCompHeight = 0;
    m_dwBufferSize = 0;
    m_pMediaType = NULL;

    m_pStream1 = NULL;
    m_pStream2 = NULL;
    m_pCompStream = NULL;
    m_pLoadedBitmap = NULL;
    m_bVerifyStream = FALSE;
    // At most 20 units tolerance in R,G,B component and sqrt(1200)/24
    m_fDiffTolerance = 1.4434f;

    ZeroMemory( &m_mt, sizeof(AM_MEDIA_TYPE) );
}


CBaseCustomAP::~CBaseCustomAP()
{
    if ( m_hQuitEvent ) 
    {       
        if ( m_hThread ) 
        {            
            SetEvent( m_hQuitEvent );
            WaitForSingleObject( m_hThread, INFINITE );
            CloseHandle( m_hThread );
        }
        
        CloseHandle( m_hQuitEvent );
    }

    // wait for thread to exit
    Sleep( 500 );

    Deactivate();

    FreeMediaType(m_mt);

    if ( m_pStream1 )
        delete [] m_pStream1;
    if ( m_pStream2 )
        delete [] m_pStream2;
    if ( m_pCompStream )
        delete [] m_pCompStream;
    if ( m_pLoadedBitmap)
        delete [] m_pLoadedBitmap;

    if(m_pMediaType)
    {
        ValidateReadWritePtr(m_pMediaType, sizeof(AM_MEDIA_TYPE));
        FreeMediaType(*m_pMediaType);
        CoTaskMemFree(m_pMediaType);
    }

    m_pStream1 = NULL;
    m_pStream2 = NULL;
    m_pCompStream = NULL;
}


HRESULT CBaseCustomAP::Activate( CComPtr<IBaseFilter> pVMR, CustomAPCallback fnCallback, VMRStreamData *pData )
{
    HRESULT hr;
    TCHAR tmp[256] = {0};
    CComPtr<IVMRFilterConfig> pVMRFilterConfig;

    hr = pVMR.QueryInterface(&pVMRFilterConfig);
    StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IFilterConfig interface ..." ), 
                        __FUNCTION__, __LINE__, __FILE__ );
    CHECKHR( hr, tmp );

    // Custom allocator presenters can just be used in renderless mode
    hr = pVMRFilterConfig->SetRenderingMode(VMRMode_Renderless);
    StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - SetRenderingMode w/ renderless ..." ), 
                        __FUNCTION__, __LINE__, __FILE__ );
    CHECKHR( hr, tmp );

    if ( DW_COMPOSITED_ID == pData->vmrId )
    {
        hr = pVMR.QueryInterface( &m_pDefaultSurfaceAllocatorNotify[0] );
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRSurfaceAllocatorNotify interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        // Create our own copy of the VMR's surface allocator so that we
        // can redirect all our calls to the original one...
        hr = m_pDefaultSurfaceAllocator[0].CoCreateInstance(CLSID_AllocPresenter, NULL, CLSCTX_INPROC_SERVER);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Create the allocator presenter ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        /*
        CComPtr<IVMRImagePresenterConfig>    pImageConfig;
        hr = m_pDefaultSurfaceAllocator[0].QueryInterface(&pImageConfig);
        CHECKHR(hr);
        hr = pImageConfig->SetRenderingPrefs( RenderPrefs_PreferAGPMemWhenMixing );
        CHECKHR(hr);
        */

        hr = m_pDefaultSurfaceAllocator[0].QueryInterface(&m_pDefaultImagePresenter);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRImagePresenter interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        hr = m_pDefaultSurfaceAllocator[0].QueryInterface(&m_pDefaultImagePresenterConfig);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRImagePresenterConfig interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        hr = m_pDefaultSurfaceAllocator[0].QueryInterface(&m_pDefaultWindowlessControl);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRWindowlessControl interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        hr = m_pDefaultSurfaceAllocator[0]->AdviseNotify(this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseNotify ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        m_CustomWindowless.SetWindowless(m_pDefaultWindowlessControl);

        // Now replace the VMR with our own allocator presenter. All calls will
        // pass through here now.
        hr = m_pDefaultSurfaceAllocatorNotify[0]->AdviseSurfaceAllocator(DW_COMPOSITED_ID, this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseSurfaceAllocator ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        m_StreamData[0] = *pData;
        
        if(m_bImageSavingMode || m_bCaptureMode)
            InitializeMediaType(1.0, 0);
        
        if ( m_bVerifyStream )
        {
            InitializeMediaType(1.0 , 0);
            m_hThread = CreateThread( NULL, 0, VerifyThreadProc, this, 0, &m_dwThreadID );
            if ( !m_hThread ) 
            {
                DWORD dwErr = GetLastError();
                hr = HRESULT_FROM_WIN32(dwErr);
            }
        }
    }
    else if ( DW_STREAM1_ID == pData->vmrId )
    {
        hr = pVMR.QueryInterface( &m_pDefaultSurfaceAllocatorNotify[1] );
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRSurfaceAllocatorNotify interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        // Create our own copy of the VMR's surface allocator so that we
        // can redirect all our calls to the original one...
        hr = m_pDefaultSurfaceAllocator[1].CoCreateInstance(CLSID_AllocPresenter, NULL, CLSCTX_INPROC_SERVER);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Create the allocator presenter ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        hr = m_pDefaultSurfaceAllocator[1]->AdviseNotify(this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseNotify ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );


        // Now replace the VMR with our own allocator presenter. All calls will
        // pass through here now.
        hr = m_pDefaultSurfaceAllocatorNotify[1]->AdviseSurfaceAllocator(DW_STREAM1_ID, this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseSurfaceAllocator ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        m_StreamData[1] = *pData;
    }
    else if ( DW_STREAM2_ID == pData->vmrId )
    {
        hr = pVMR.QueryInterface( &m_pDefaultSurfaceAllocatorNotify[2] );
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Query IVMRSurfaceAllocatorNotify interface ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );

        // Create our own copy of the VMR's surface allocator so that we
        // can redirect all our calls to the original one...
        hr = m_pDefaultSurfaceAllocator[2].CoCreateInstance(CLSID_AllocPresenter, NULL, CLSCTX_INPROC_SERVER);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Create the allocator presenter ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );


        hr = m_pDefaultSurfaceAllocator[2]->AdviseNotify(this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseNotify ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );


        // Now replace the VMR with our own allocator presenter. All calls will
        // pass through here now.
        hr = m_pDefaultSurfaceAllocatorNotify[2]->AdviseSurfaceAllocator(DW_STREAM2_ID, this);
        StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - AdviseSurfaceAllocator ..." ), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CHECKHR( hr, tmp );


        m_StreamData[2] = *pData;
    }

    m_bActive = TRUE;
    m_pfnCallback = fnCallback;

    //Check if HW supports overlay
    if(IsOverlayHardwareSupported())
    {
        m_bOverlaySupported = TRUE;
        LOG( TEXT("Overlay is Supported!!") );
    }else{
            LOG( TEXT("Overlay is NOT Supported!!") );
    }

cleanup:
    return hr;
}

HRESULT
CBaseCustomAP::InitializeMediaType(float scale, DWORD angle)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE *pMediaType = &m_mt;

    // These are standard for the YUV and RGB that we deal with
    pMediaType->bFixedSizeSamples = true;
    pMediaType->bTemporalCompression = false;
    pMediaType->pUnk = NULL;
    pMediaType->formattype = FORMAT_VideoInfo;
    pMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
    pMediaType->pbFormat = new BYTE[pMediaType->cbFormat];
    VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)pMediaType->pbFormat;
    pVIH->dwBitErrorRate = 0;
    pVIH->dwBitRate = 0;
    RECT rect0 = {0,0,0,0};
    pVIH->rcSource = rect0;
    pVIH->rcTarget = rect0;
    pVIH->bmiHeader.biClrImportant = 0;
    pVIH->bmiHeader.biClrUsed = 0;
    pVIH->bmiHeader.biPlanes = 1;
    pVIH->bmiHeader.biXPelsPerMeter = 0;
    pVIH->bmiHeader.biYPelsPerMeter = 0;
    pVIH->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    
    // Type specific
    pMediaType->majortype = MEDIATYPE_Video;
    pMediaType->subtype = MEDIASUBTYPE_RGB32;
    pVIH->bmiHeader.biBitCount = 32;
    pVIH->bmiHeader.biCompression = BI_RGB;
    pVIH->AvgTimePerFrame = 330000;

    // Set 320x240 as default supported
    pVIH->bmiHeader.biWidth = 160;
    pVIH->bmiHeader.biHeight = 120;

    // What size buffer?
    pVIH->bmiHeader.biSizeImage = AllocateBitmapBuffer(pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, pVIH->bmiHeader.biBitCount, pVIH->bmiHeader.biCompression, NULL);
    pMediaType->lSampleSize = pVIH->bmiHeader.biSizeImage;

    hr = m_Recognizer.Init( &m_mt, tszDefaultOCRTemplate, scale, angle );
    TCHAR tmp[256] = {0};
    StringCchPrintf( tmp, 256, TEXT("%S: %d@%S - Initialize the media type for the video stamper ..." ), 
                        __FUNCTION__, __LINE__, __FILE__ );
    CHECKHR( hr, tmp );

cleanup:

    return hr;
}


VOID 
CBaseCustomAP::UpdateStreamData( DWORD_PTR vmrId, VMRStreamData *pData )
{
    if ( DW_COMPOSITED_ID == pData->vmrId )
        m_StreamData[0] = *pData;
    else if ( DW_STREAM1_ID == pData->vmrId )
        m_StreamData[1] = *pData;
    else if ( DW_STREAM2_ID == pData->vmrId )
        m_StreamData[2] = *pData;
}

VOID 
CBaseCustomAP::SetCompositedSize( DWORD dwWidth, DWORD dwHeight )
{
    m_dwCompWidth = dwWidth;
    m_dwCompHeight = dwHeight;
    InitCompositedDC();
}

HRESULT 
CBaseCustomAP::CompareSavedImageWithStream()
{
    HRESULT hr = S_OK;
    TCHAR   tszSrcFileBuffer[MAX_PATH] = {0};
    HANDLE hSrcFile = NULL;
    WIN32_FIND_DATA srcFindFileData;
    BITMAPINFO imageBmi;
    GUID imgSubtype;
    BYTE* pSrcImgBuffer = NULL;
    BITMAPINFO srcImageBmi;

    VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)m_pMediaType->pbFormat;

    ZeroMemory( &imageBmi, sizeof(BITMAPINFO) );
    imageBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageBmi.bmiHeader.biCompression = pVIH->bmiHeader.biCompression;
    imageBmi.bmiHeader.biPlanes = pVIH->bmiHeader.biPlanes;

    imgSubtype = m_pMediaType->subtype;
   
#if 0
    DWORD dwFrameID;

    // Take the lock before touching m_pStream
    CAutoLock Lock(&m_csLock);

    hr = GetFrameID(&dwFrameID);
    if(SUCCEEDED(hr))
        LOG( TEXT("Successfuly get frmae ID %d!!"), dwFrameID);
    else
        dwFrameID = dwFrameCnt;
#endif

    //construct a search string with wildcard
    hr = StringCchPrintf(tszSrcFileBuffer, MAX_PATH, TEXT("%s\\VMR%d.bmp"), m_szPath, m_dwFrameNo);
    CHECKHR(hr,TEXT("Construct source images search string ..." ));

    //grab the first file handle matching our search string
    hSrcFile = FindFirstFile(tszSrcFileBuffer, &srcFindFileData);
    if(hSrcFile == INVALID_HANDLE_VALUE)
    {
        LOG( TEXT("%S: ERROR %d@%S - Fail to find file matching this pattern: %s."), __FUNCTION__, __LINE__, __FILE__, tszSrcFileBuffer);
        return E_FAIL;
    }

    hr = LoadSavedImage(tszSrcFileBuffer, &pSrcImgBuffer, &srcImageBmi, imgSubtype);
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - Load source image %s failed error code: 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, tszSrcFileBuffer, hr);
        goto cleanup;
    }

    if(srcImageBmi.bmiHeader.biWidth != imageBmi.bmiHeader.biWidth ||
        srcImageBmi.bmiHeader.biHeight != imageBmi.bmiHeader.biHeight ||
        srcImageBmi.bmiHeader.biCompression != imageBmi.bmiHeader.biCompression)
    {
        LOG( TEXT("%S: ERROR %d@%S - source image has different size from composite stream image. Something seriously wrong!!"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr =  E_FAIL;
        goto cleanup;
    }

    float pixelDiff = DiffImage( (BYTE *)pSrcImgBuffer, m_pCompStream, &imageBmi, imgSubtype );

    if ( pixelDiff < 0.0f )
    {
        LOG( TEXT("%S: ERROR %d@%S - diff for the current media sample format not implemented."), __FUNCTION__, __LINE__, __FILE__ );
        hr =  E_NOTIMPL;
        goto cleanup;
    }

    //Find a match
    if ( pixelDiff <= m_fDiffTolerance )
    {
        LOG( TEXT("%S: pixelDiff = image %s, %f meets tolerance %f."), __FUNCTION__, tszSrcFileBuffer, pixelDiff, m_fDiffTolerance );
        m_dwMatchCount++;
        hr = S_OK;
    }
    else 
    {
        LOG( TEXT("%S: ERROR %d@%S - image %s, pixelDiff = %f exceeds tolerance %f."), 
            __FUNCTION__, __LINE__, __FILE__, tszSrcFileBuffer, pixelDiff, m_fDiffTolerance );
    }

cleanup:
    if ( pSrcImgBuffer )
        delete [] pSrcImgBuffer;

    return hr;
}

DWORD
CBaseCustomAP::CompareSavedImages(TCHAR *tszSrcImagesPath)
{
    TCHAR   tszDestFileBuffer[MAX_PATH] = {0};
    TCHAR   tszSrcImageName[MAX_PATH] = {0};
    TCHAR   tszDstImageName[MAX_PATH] = {0};
    WIN32_FIND_DATA srcFindFileData;
    WIN32_FIND_DATA dstFindFileData;
    BOOL    bMoreFiles = TRUE;
    HRESULT hr = S_OK;
    HRESULT retval = E_FAIL;
    HANDLE  hSrcFile = NULL;
    HANDLE  hDstFile = NULL;
    BYTE* pSrcImgBuffer = NULL;
    BYTE* pDestImgBuffer = NULL;
    BITMAPINFO srcImageBmi;
    BITMAPINFO dstImageBmi;
    GUID imgSubtype;
    DWORD dwPassCount = 0;
    float pixelDiff = 0.0;

    //construct a search string with wildcard
    hr = StringCchPrintf(tszDestFileBuffer, MAX_PATH, TEXT("%s\\VMR*.bmp"), m_szPath);
    CHECKHR(hr,TEXT("Construct image files search string ..." ));

    //grab the first file handle matching our search string
    hDstFile = FindFirstFile(tszDestFileBuffer, &dstFindFileData);
    if(hDstFile == INVALID_HANDLE_VALUE)
    {
        LOG( TEXT("%S: ERROR %d@%S - Fail to find file matching this pattern: %s."), __FUNCTION__, __LINE__, __FILE__, tszDestFileBuffer);
        return E_FAIL;
    }
    // we always want the template to be top down. therefore
    // we set the height negative for the bitmapinfo
    memset(&srcImageBmi, 0, sizeof(BITMAPINFO));
    memset(&dstImageBmi, 0, sizeof(BITMAPINFO));
    srcImageBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    srcImageBmi.bmiHeader.biBitCount = 32;
    srcImageBmi.bmiHeader.biPlanes = 1;
    srcImageBmi.bmiHeader.biCompression = BI_RGB;

    dstImageBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dstImageBmi.bmiHeader.biBitCount = 32;
    dstImageBmi.bmiHeader.biPlanes = 1;
    dstImageBmi.bmiHeader.biCompression = BI_RGB;

    imgSubtype = MEDIASUBTYPE_RGB32;
  
    while(bMoreFiles)
    {
        hr = StringCchPrintf(tszDstImageName, MAX_PATH, TEXT("%s\\%s"), m_szPath, dstFindFileData.cFileName);
        CHECKHR(hr,TEXT("Construct destination image file full name ..." ));

        hr = StringCchPrintf(tszSrcImageName, MAX_PATH, TEXT("%s\\%s"), tszSrcImagesPath, dstFindFileData.cFileName);
        CHECKHR(hr,TEXT("Construct source image file full name ..." ));

        hSrcFile = FindFirstFile(tszSrcImageName, &srcFindFileData);
        if(hSrcFile == INVALID_HANDLE_VALUE)
            LOG( TEXT("%S: ERROR %d@%S - Can not find source image file %s."), 
                __FUNCTION__, __LINE__, __FILE__, tszSrcImageName);
        else
        {
            hr = LoadSavedImage(tszSrcImageName, &pSrcImgBuffer, &srcImageBmi, imgSubtype);
            if(FAILED(hr))
            {
                LOG( TEXT("%S: ERROR %d@%S - Load source image %s failed error code: 0x%08x."), 
                    __FUNCTION__, __LINE__, __FILE__, tszSrcImageName, hr);
                if ( pSrcImgBuffer )
                    delete [] pSrcImgBuffer;
                bMoreFiles = FindNextFile(hSrcFile, &srcFindFileData);
                continue;
            }

            hr = LoadSavedImage(tszDstImageName, &pDestImgBuffer, &dstImageBmi, imgSubtype);
            if(FAILED(hr))
            {
                LOG( TEXT("%S: ERROR %d@%S - Load destination image %s failed error code: 0x%08x."), 
                    __FUNCTION__, __LINE__, __FILE__, tszDstImageName, hr);
                if ( pDestImgBuffer )
                    delete [] pDestImgBuffer;
                bMoreFiles = FindNextFile(hSrcFile, &srcFindFileData);
                continue;
            }

            if(srcImageBmi.bmiHeader.biWidth != dstImageBmi.bmiHeader.biWidth ||
                srcImageBmi.bmiHeader.biHeight != dstImageBmi.bmiHeader.biHeight ||
                srcImageBmi.bmiHeader.biCompression != dstImageBmi.bmiHeader.biCompression)
            {
                LOG( TEXT("%S: ERROR %d@%S - source image: width:height = %d:%d has different size from destination image: width:height = %d:%d. Something seriously wrong!!"), 
                            __FUNCTION__, __LINE__, __FILE__, srcImageBmi.bmiHeader.biWidth, srcImageBmi.bmiHeader.biHeight, dstImageBmi.bmiHeader.biWidth, dstImageBmi.bmiHeader.biHeight);
                hr =  E_FAIL;
                goto cleanup;
            }
            
            pixelDiff = DiffImage( pSrcImgBuffer, pDestImgBuffer, &dstImageBmi, imgSubtype );

            //free memory first
            if ( pSrcImgBuffer )
                delete [] pSrcImgBuffer;

            if ( pDestImgBuffer )
                delete [] pDestImgBuffer;

            if ( pixelDiff < 0.0f )
            {
                LOG( TEXT("%S: ERROR %d@%S - diff for the current media sample format not implemented."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                hr =  E_NOTIMPL;
                goto cleanup;
            }

            if ( pixelDiff <= m_fDiffTolerance )
            {
                LOG( TEXT("%S: %s and %s, pixelDiff = %f meets tolerance %f."), 
                            __FUNCTION__, tszSrcImageName, tszDstImageName, pixelDiff, m_fDiffTolerance );
                dwPassCount++;
            }
            else 
            {
                LOG( TEXT("%S: ERROR %d@%S - %s and %s, pixelDiff = %f exceeds tolerance %f."), 
                            __FUNCTION__, __LINE__, __FILE__, tszSrcImageName, tszDstImageName, pixelDiff, m_fDiffTolerance );
            }

        }

        //grab the next file handle matching our search string
        bMoreFiles = FindNextFile(hDstFile, &dstFindFileData);
    }

cleanup:    

        return dwPassCount;
}

HRESULT CBaseCustomAP::ProcessMediaSample(VMRPRESENTATIONINFO* lpPresInfo, DWORD_PTR dwUserID)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE *pMediaType = NULL;
    AM_MEDIA_TYPE mediaTemplate;
    BOOL bWasLocked = FALSE;
    BOOL bLocked= FALSE;
    DDSURFACEDESC ddsd;
    BYTE *pFormatPtr = NULL;
    BYTE *pTmp = NULL;
    CSurfaceInfo dstInfo;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    while ((hr = lpPresInfo->lpSurf->Lock(NULL, &ddsd,
#ifdef UNDER_CE
                0,
#else
                DDLOCK_NOSYSLOCK | DDLOCK_DONOTWAIT,
#endif
                (HANDLE)NULL)) == DDERR_WASSTILLDRAWING)
    {
        Sleep(1);
    }

    if(hr == DDERR_SURFACEBUSY)
    {
        bWasLocked = TRUE;
        lpPresInfo->lpSurf->Unlock(NULL);
        while ((hr = lpPresInfo->lpSurf->Lock(NULL, &ddsd,
#ifdef UNDER_CE
                0,
#else
                DDLOCK_NOSYSLOCK | DDLOCK_DONOTWAIT,
#endif
                (HANDLE)NULL)) == DDERR_WASSTILLDRAWING)
        {
            Sleep(1);
        }
    }
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - lock ddraw surface failed: 0x%08x"), __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }
    bLocked = TRUE;

    if ( DW_STREAM1_ID == dwUserID )
    {
        pTmp = m_pStream1;
        m_StreamData[1].dwWidth = ddsd.dwWidth;
        m_StreamData[1].dwHeight = ddsd.dwHeight;
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        pTmp = m_pStream2;
        m_StreamData[2].dwWidth = ddsd.dwWidth;
        m_StreamData[2].dwHeight = ddsd.dwHeight;
    }
    else if ( DW_COMPOSITED_ID == dwUserID )
    {
        pTmp = m_pCompStream;
        m_StreamData[0].dwWidth = ddsd.dwWidth;
        m_StreamData[0].dwHeight = ddsd.dwHeight;
    }

    //Use surface info to process ddsd first
    dstInfo.Init(&ddsd);
    dstInfo.Rotate(AM_ROTATION_ANGLE_0);
    LPBYTE pDst = (LPBYTE)dstInfo.GetSurfacePointer();

    //If media type changed, create new media type
    if(m_ddsd.dwHeight != ddsd.dwHeight || m_ddsd.dwWidth != ddsd.dwWidth || m_ddsd.lPitch != ddsd.lPitch)
    {
        m_ddsd = ddsd;
        mediaTemplate.formattype = FORMAT_VideoInfo;
        mediaTemplate.cbFormat = sizeof(VIDEOINFOHEADER);
        pFormatPtr = (BYTE *)CoTaskMemAlloc(mediaTemplate.cbFormat);
        if (!pFormatPtr)
        {
            LOG( TEXT("%S: ERROR %d@%S - Out of memory!!"), __FUNCTION__, __LINE__, __FILE__);
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        
        mediaTemplate.pbFormat = pFormatPtr;
        hr = ConvertSurfaceDescToMediaType(&ddsd, &mediaTemplate, &pMediaType);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to convert ddraw surface to media type: 0x%08x."), 
                __FUNCTION__, __LINE__, __FILE__, hr);
            goto cleanup;
        }
        pMediaType->pUnk = NULL;
    
        LOG( TEXT("Media type changed!!"));
        //Before assing new media type to m_pMediaType, free old media type
        if(m_pMediaType)
        {
            ValidateReadWritePtr(m_pMediaType, sizeof(AM_MEDIA_TYPE));
            FreeMediaType(*m_pMediaType);
            CoTaskMemFree(m_pMediaType);
        }
        //Get new media type
        m_pMediaType = pMediaType;
        m_bMediaTypeChanged = TRUE;
    }

    if ( m_bMediaTypeChanged )
    {
        //free old buffer
        if(!pTmp)
            delete [] pTmp;
        VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)m_pMediaType->pbFormat;
        
        if(m_dwRotation == 90 || m_dwRotation == 270)
        {
            pVIH->bmiHeader.biWidth = dstInfo.Width();
            pVIH->bmiHeader.biHeight = -1 *dstInfo.Height();;
        }

        //Get the stride of this sample
        m_iStride = dstInfo.YPitch();

        //Allocate the buffer based on the media type
        m_dwBufferSize= AllocateBitmapBuffer(pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, m_iStride, m_pMediaType->subtype, &pTmp);
        if(m_dwBufferSize == 0 || !pTmp)
        {
            hr = E_OUTOFMEMORY;
            CHECKHR(hr, TEXT("SaveImageBuffer: OOM") );
        }

        if ( DW_STREAM1_ID == dwUserID )
            m_pStream1 = pTmp;
        else if ( DW_STREAM2_ID == dwUserID )
            m_pStream2 = pTmp;
        else if ( DW_COMPOSITED_ID == dwUserID )
            m_pCompStream = pTmp;

        m_bMediaTypeChanged = FALSE;
    }
    {
                // take the lock
        CAutoLock Lock(&m_csLock);
        memcpy( pTmp, pDst, m_dwBufferSize );
    }

cleanup:

    if(lpPresInfo->lpSurf){
        if(!bWasLocked && bLocked)
        {
            hr = lpPresInfo->lpSurf->Unlock(NULL);
        }
    }

    return hr;
}

HRESULT CBaseCustomAP::CheckOverlay(VMRPRESENTATIONINFO* lpPresInfo)
{
    HRESULT hr = S_OK;
    DDSURFACEDESC ddsd;
    DWORD dwPrefs;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);          

    //Surface could lost, therefore, we need to handle possible crash here
    __try 
    {

        if(lpPresInfo != NULL && lpPresInfo->lpSurf != NULL)
        {
            IDirectDrawSurface *pSurface = lpPresInfo->lpSurf;

            hr = pSurface->GetSurfaceDesc(&ddsd);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Failed to GetSurfaceDesc: 0x%08x."), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
                __leave;
            }

            hr = GetRenderingPrefs(&dwPrefs);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Failed to GetRenderingPrefs: 0x%08x."), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
                __leave;
            }

            if ((dwPrefs & RenderPrefs_ForceOverlays) ||(dwPrefs & RenderPrefs_AllowOverlays))
            {
                if (!(ddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                {
                    hr = E_FAIL;
                    LOG( TEXT("%S: ERROR %d@%S - Failed to use overlay: 0x%08x!!!"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
                }
            }
        }else
        {
            LOG( TEXT("lpPresInfo or lpPresInfo->lpSurf  is NULL pointer!!"));
            hr = E_FAIL;
        }
    }
    __finally 
    {
        LOG( TEXT("Exception, surface lost!!"));
        hr = DDERR_SURFACELOST;
    }

    return hr;
}


HRESULT 
CBaseCustomAP::SaveImage( DWORD dwFrameID)
{
    HRESULT hr = S_OK;
    BITMAPINFO imageBmi;
    GUID imgSubtype;
    
    ZeroMemory( &imageBmi, sizeof(BITMAPINFO) );
    VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)m_pMediaType->pbFormat;

    imageBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageBmi.bmiHeader.biWidth = pVIH->bmiHeader.biWidth;
    imageBmi.bmiHeader.biHeight = pVIH->bmiHeader.biHeight;
    imageBmi.bmiHeader.biCompression = pVIH->bmiHeader.biCompression;
    imageBmi.bmiHeader.biXPelsPerMeter  = pVIH->bmiHeader.biXPelsPerMeter;
    imageBmi.bmiHeader.biYPelsPerMeter  = pVIH->bmiHeader.biYPelsPerMeter;
    imgSubtype = m_pMediaType->subtype;

    // Take the lock before touching m_pStream
    CAutoLock Lock(&m_csLock);

    TCHAR fname[MAX_PATH] = {0};
    StringCchPrintf( fname, MAX_PATH, TEXT("%s\\VMR%d.bmp"), m_szPath, dwFrameID);
    
    hr = SaveBitmap( fname, &imageBmi, imgSubtype, (BYTE *)m_pCompStream, m_iStride);
    if(SUCCEEDED(hr))
        LOG( TEXT("Save bitmap %s succeed!!."), fname);
    else
        LOG( TEXT("%S: ERROR %d@%S - Save bitmap %s failed!! error code: 0x%08x."), __FUNCTION__, __LINE__, __FILE__, fname, hr );

    return hr;
}

HRESULT 
CBaseCustomAP::GetFrameID(DWORD *pdwFrameID)
{
    HRESULT hr = S_OK;

    hr = m_Recognizer.SetFormat( m_pMediaType);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to set stamp reader format."), __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = m_Recognizer.ReadStamp( m_pCompStream);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to read the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = m_Recognizer.GetFrameID( (int*)pdwFrameID );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    
    return hr;
}

HRESULT 
CBaseCustomAP::LoadSavedImage( TCHAR *tszBitmapName, BYTE** ppImageBuffer,  BITMAPINFO *pImageBmi, GUID subtype)
{
    HRESULT hr = S_OK;
//    int iFrameID;
    BITMAP imageInfo;

    int Allocated = 0;
    memset(&imageInfo, 0, sizeof(BITMAP));

    if(!tszBitmapName || !pImageBmi)
    {
        hr = E_INVALIDARG;
        return hr;
    }

    hr = GetBitmapFileInfo(tszBitmapName, &imageInfo);
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to get Bitmap file %s information !! error code: 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, tszBitmapName, hr );
        return hr;
    }

    // we set the height negative for the bitmapinfo
    pImageBmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pImageBmi->bmiHeader.biWidth = imageInfo.bmWidth;
    pImageBmi->bmiHeader.biHeight = imageInfo.bmHeight;

    Allocated = AllocateBitmapBuffer(pImageBmi->bmiHeader.biWidth, pImageBmi->bmiHeader.biHeight, subtype, ppImageBuffer);
    if(Allocated == 0 || !*ppImageBuffer)
        return E_OUTOFMEMORY;

    // load up the same bitmap to buffer
    hr = LoadImage(*ppImageBuffer, pImageBmi, subtype, tszBitmapName);
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to load image file %s !! error code: 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, tszBitmapName, hr );
    }

    return hr;
}


DWORD WINAPI
CBaseCustomAP::VerifyThreadProc( LPVOID lpParameter )
{
    CBaseCustomAP* lp = (CBaseCustomAP*)lpParameter;
    return lp->VerifyThread();
}

DWORD
CBaseCustomAP::VerifyThread()
{
    DWORD rc;
    
    while ( TRUE )
    {
        //Compare composite stream with saved image
        CompareSavedImageWithStream();
        rc = WaitForSingleObject( m_hQuitEvent, 100 );
        if ( WAIT_OBJECT_0 == rc ) 
        break;
    }
    return 0;
}



void CBaseCustomAP::Deactivate(void)
{
    if (m_bActive)
    {
        for ( int i = 0; i < 3; i++ )
        {
            if (m_pDefaultSurfaceAllocator[i])
            {
                if ( 0 == i )
                {
                    if ( m_pDefaultSurfaceAllocatorNotify[i] )
                        m_pDefaultSurfaceAllocatorNotify[i]->AdviseSurfaceAllocator(DW_COMPOSITED_ID, NULL);
                    m_pDefaultSurfaceAllocator[i]->FreeSurface(DW_COMPOSITED_ID);
                }
                else if ( 1 == i )
                {
                    if ( m_pDefaultSurfaceAllocatorNotify[i] )
                        m_pDefaultSurfaceAllocatorNotify[i]->AdviseSurfaceAllocator(DW_STREAM1_ID, NULL);
                    m_pDefaultSurfaceAllocator[i]->FreeSurface(DW_STREAM1_ID);
                }
                else if ( 2 == i )
                {
                    if ( m_pDefaultSurfaceAllocatorNotify[i] )
                        m_pDefaultSurfaceAllocatorNotify[i]->AdviseSurfaceAllocator(DW_STREAM2_ID, NULL);
                    m_pDefaultSurfaceAllocator[i]->FreeSurface(DW_STREAM2_ID);                    
                }
            }
            m_pDefaultSurfaceAllocator[i] = NULL;
            m_pDefaultSurfaceAllocatorNotify[i] = NULL;
        }
        
        m_CustomWindowless.Deactivate();
        m_pDefaultImagePresenter = NULL;
        m_pDefaultImagePresenterConfig = NULL;
        m_pDefaultWindowlessControl = NULL;
        m_bActive = FALSE;
    }
}


STDMETHODIMP CBaseCustomAP::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IVMRSurfaceAllocator)
    {
        return GetInterface((IVMRSurfaceAllocator*)this, ppv);
    }
    else if (riid == IID_IVMRImagePresenter)
    {
        return GetInterface((IVMRImagePresenter*)this, ppv);
    }
    else if (riid == IID_IVMRImagePresenterConfig)
    {
        return GetInterface((IVMRImagePresenterConfig*)this, ppv);
    }
    else if (riid == IID_IVMRWindowlessControl)
    {
        return m_CustomWindowless.NonDelegatingQueryInterface(riid, ppv);
    }

    return CUnknown::NonDelegatingQueryInterface(riid,ppv);
}


//////////////////////////////////////////////////////////////////////////////
// IVMRSurfaceAllocator
//
// Pass-through back to the VMR default surface allocator
//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CBaseCustomAP::AllocateSurface(DWORD_PTR dwUserID,
                                       VMRALLOCATIONINFO *lpAllocInfo,
                                       DWORD *lpdwBuffer,
                                       LPDIRECTDRAWSURFACE *lplpSurface)
{
    if ( DW_COMPOSITED_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[0])
        {
            return m_pDefaultSurfaceAllocator[0]->AllocateSurface(dwUserID, lpAllocInfo,lpdwBuffer, lplpSurface);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM1_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[1])
        {
            return m_pDefaultSurfaceAllocator[1]->AllocateSurface(dwUserID, lpAllocInfo,lpdwBuffer, lplpSurface);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[2])
        {
            return m_pDefaultSurfaceAllocator[2]->AllocateSurface(dwUserID, lpAllocInfo,lpdwBuffer, lplpSurface);
        }
        else
        {
            return E_POINTER;
        }
    } 
    return S_OK;
}


STDMETHODIMP CBaseCustomAP::FreeSurface(DWORD_PTR dwUserID)
{
    if ( DW_COMPOSITED_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[0])
        {
            return m_pDefaultSurfaceAllocator[0]->FreeSurface(dwUserID);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM1_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[1])
        {
            return m_pDefaultSurfaceAllocator[1]->FreeSurface(dwUserID);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[2])
        {
            return m_pDefaultSurfaceAllocator[2]->FreeSurface(dwUserID);
        }
        else
        {
            return E_POINTER;
        }
    }

    return S_OK;
}


STDMETHODIMP CBaseCustomAP::PrepareSurface(DWORD_PTR dwUserID, LPDIRECTDRAWSURFACE lplpSurface, DWORD dwSurfaceFlags)
{
    if ( DW_COMPOSITED_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[0])
        {
            return m_pDefaultSurfaceAllocator[0]->PrepareSurface(dwUserID, lplpSurface, dwSurfaceFlags);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM1_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[1])
        {
            return m_pDefaultSurfaceAllocator[1]->PrepareSurface(dwUserID, lplpSurface, dwSurfaceFlags);
        }
        else
        {
            return E_POINTER;
        }
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocator[2])
        {
            return m_pDefaultSurfaceAllocator[2]->PrepareSurface(dwUserID, lplpSurface, dwSurfaceFlags);
        }
        else
        {
            return E_POINTER;
        }
    } 

    return S_OK;
}


STDMETHODIMP CBaseCustomAP::AdviseNotify(IVMRSurfaceAllocatorNotify *lpIVMRSurfAllocNotify)
{
    HRESULT hr = S_OK;
    if (m_pDefaultSurfaceAllocator[0])
        hr = m_pDefaultSurfaceAllocator[0]->AdviseNotify(lpIVMRSurfAllocNotify);

    if (m_pDefaultSurfaceAllocator[1])
        hr = m_pDefaultSurfaceAllocator[1]->AdviseNotify(lpIVMRSurfAllocNotify);

    if (m_pDefaultSurfaceAllocator[2])
        hr = m_pDefaultSurfaceAllocator[2]->AdviseNotify(lpIVMRSurfAllocNotify);

    return hr;
}
//////////////////////////////////////////////////////////////////////////////
// IVMRImagePresenterConfig
//
// Just a pass-through back to the VMR.
//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CBaseCustomAP::SetRenderingPrefs(DWORD dwPrefs)
{
    if (m_pDefaultImagePresenterConfig)
    {
        return m_pDefaultImagePresenterConfig->SetRenderingPrefs(dwPrefs);
    }
    else
    {
        return E_POINTER;
    }
}

STDMETHODIMP CBaseCustomAP::GetRenderingPrefs(DWORD *pdwPrefs)
{
    if (m_pDefaultImagePresenterConfig)
    {
        return m_pDefaultImagePresenterConfig->GetRenderingPrefs(pdwPrefs);
    }
    else
    {
        return E_POINTER;
    }
}

//////////////////////////////////////////////////////////////////////////////
// IVMRSurfaceAllocatorNotify
//
// Pass-through back to the VMR default surface allocator
//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CBaseCustomAP::AdviseSurfaceAllocator(DWORD_PTR dwUserID, IVMRSurfaceAllocator* lpIVRMSurfaceAllocator)
{
    HRESULT hr = S_OK;
    if ( DW_COMPOSITED_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocatorNotify[0])
            hr = m_pDefaultSurfaceAllocatorNotify[0]->AdviseSurfaceAllocator(dwUserID, lpIVRMSurfaceAllocator);
    }
    else if ( DW_STREAM1_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocatorNotify[1])
            hr = m_pDefaultSurfaceAllocatorNotify[1]->AdviseSurfaceAllocator(dwUserID, lpIVRMSurfaceAllocator);
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        if (m_pDefaultSurfaceAllocatorNotify[2])
            hr = m_pDefaultSurfaceAllocatorNotify[2]->AdviseSurfaceAllocator(dwUserID, lpIVRMSurfaceAllocator);
    }
    return hr;
}


STDMETHODIMP CBaseCustomAP::SetDDrawDevice(LPDIRECTDRAW lpDDrawDevice,HMONITOR hMonitor)
{
    HRESULT hr = S_OK;

    if (m_pDefaultSurfaceAllocatorNotify[0])
        hr = m_pDefaultSurfaceAllocatorNotify[0]->SetDDrawDevice(lpDDrawDevice, hMonitor);

    if (m_pDefaultSurfaceAllocatorNotify[1])
        hr = m_pDefaultSurfaceAllocatorNotify[1]->SetDDrawDevice(lpDDrawDevice, hMonitor);

    if (m_pDefaultSurfaceAllocatorNotify[2])
        hr = m_pDefaultSurfaceAllocatorNotify[2]->SetDDrawDevice(lpDDrawDevice, hMonitor);

    if ( SUCCEEDED(hr) )
    {
        m_lpDDrawDevice = lpDDrawDevice;
        m_hMonitor        = hMonitor;
    }
    return hr;
}


STDMETHODIMP CBaseCustomAP::ChangeDDrawDevice(LPDIRECTDRAW lpDDrawDevice,HMONITOR hMonitor)
{
    HRESULT hr = S_OK;
    
    if (m_pDefaultSurfaceAllocatorNotify[0])
        hr =  m_pDefaultSurfaceAllocatorNotify[0]->ChangeDDrawDevice(lpDDrawDevice, hMonitor);
    if (m_pDefaultSurfaceAllocatorNotify[1])
        hr = m_pDefaultSurfaceAllocatorNotify[1]->ChangeDDrawDevice(lpDDrawDevice, hMonitor);
    if (m_pDefaultSurfaceAllocatorNotify[2])
        hr = m_pDefaultSurfaceAllocatorNotify[2]->ChangeDDrawDevice(lpDDrawDevice, hMonitor);

    return hr;
}


STDMETHODIMP CBaseCustomAP::RestoreDDrawSurfaces()
{
    HRESULT hr = S_OK;

    if (m_pDefaultSurfaceAllocatorNotify[0])
        hr = m_pDefaultSurfaceAllocatorNotify[0]->RestoreDDrawSurfaces();

    if (m_pDefaultSurfaceAllocatorNotify[1])
        hr = m_pDefaultSurfaceAllocatorNotify[1]->RestoreDDrawSurfaces();

    if (m_pDefaultSurfaceAllocatorNotify[2])
        hr = m_pDefaultSurfaceAllocatorNotify[2]->RestoreDDrawSurfaces();

    return hr;
}


STDMETHODIMP CBaseCustomAP::NotifyEvent(LONG EventCode, LONG_PTR lp1, LONG_PTR lp2)
{
    HRESULT hr = S_OK;

    if (m_pDefaultSurfaceAllocatorNotify[0])
        hr = m_pDefaultSurfaceAllocatorNotify[0]->NotifyEvent(EventCode, lp1, lp2);

    if (m_pDefaultSurfaceAllocatorNotify[1])
        hr = m_pDefaultSurfaceAllocatorNotify[1]->NotifyEvent(EventCode, lp1, lp2);

    if (m_pDefaultSurfaceAllocatorNotify[2])
        hr = m_pDefaultSurfaceAllocatorNotify[2]->NotifyEvent(EventCode, lp1, lp2);

    return hr;
}


STDMETHODIMP CBaseCustomAP::SetBorderColor(COLORREF Clr)
{
    HRESULT hr = S_OK;

    if (m_pDefaultSurfaceAllocatorNotify[0])
        hr = m_pDefaultSurfaceAllocatorNotify[0]->SetBorderColor(Clr);

    if (m_pDefaultSurfaceAllocatorNotify[1])
        hr = m_pDefaultSurfaceAllocatorNotify[1]->SetBorderColor(Clr);

    if (m_pDefaultSurfaceAllocatorNotify[2])
        hr = m_pDefaultSurfaceAllocatorNotify[2]->SetBorderColor(Clr);

    return hr;
}


//////////////////////////////////////////////////////////////////////////////
// IVMRImagePresenter
//
// Just a pass-through back to the VMR. We check the images before sending
// them back.
//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CBaseCustomAP::StartPresenting(DWORD_PTR dwUserID)
{
    if ( DW_COMPOSITED_ID != dwUserID )
        return NOERROR;

    if (m_pDefaultImagePresenter)
    {
        return m_pDefaultImagePresenter->StartPresenting(dwUserID);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP 
CBaseCustomAP::StopPresenting(DWORD_PTR dwUserID)
{
    if ( DW_COMPOSITED_ID != dwUserID )
        return NOERROR;

    if (m_pDefaultImagePresenter)
    {
        return m_pDefaultImagePresenter->StopPresenting(dwUserID);
    }
    else
    {
        return E_POINTER;
    }
}

HRESULT 
CBaseCustomAP::PreProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo )
{
    HRESULT hr = S_OK;
    
    //Check if HW support OVerlay
    if(m_bOverlaySupported && m_bUsingOverlay && (lpPresInfo->dwFrameNo % 5 == 0))
    {
        //If so, check if we are using overlay properly
        hr = CheckOverlay(lpPresInfo);
        if(FAILED(hr))
        {
            m_bUsingOverlay = FALSE;
            hr = S_OK;
        }
    }
    
    //If we are in image save mode, no need to present the image
    if(m_bImageSavingMode || m_bVerifyStream )
    {
        //Get the comp stream media sample propertities
        hr = ProcessMediaSample( lpPresInfo, dwUserID);
        if(FAILED(hr))
        {
            LOG( TEXT("%S: ERROR %d@%S - Process Composite stream Media Sample failed: 0x%08x"), __FUNCTION__, __LINE__, __FILE__, hr);
            goto cleanup;
        }

        m_dwFrameNo = lpPresInfo->dwFrameNo;
        
        if ( m_bImageSavingMode  && (lpPresInfo->dwFrameNo % m_dwVerificationRate == 0))
        {
        
#if 0            
            hr = GetFrameID(&dwFrameID);
            if(SUCCEEDED(hr))
                LOG( TEXT("Successfuly get frmae ID %d!!"), dwFrameID);
            else
                dwFrameID = dwFrameCnt;
#endif            

            //Save image
            SaveImage(lpPresInfo->dwFrameNo);
        }
    }

cleanup:
    return hr;
}


HRESULT 
CBaseCustomAP::PostProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo )
{
    HRESULT hr = S_OK;
    //If we are in capture mode, we need to do capture after image being presented
    if( m_bCaptureMode )
    {
#if 0            
        hr = GetFrameID(&dwFrameID);
        if(SUCCEEDED(hr))
            LOG( TEXT("Successfuly get frmae ID %d!!"), dwFrameID);
        else
            dwFrameID = dwFrameCnt;
#endif            

        TCHAR fname[MAX_PATH] = {0};
        StringCchPrintf( fname, MAX_PATH, TEXT("%s\\VMR%d.bmp"), m_szPath, lpPresInfo->dwFrameNo);

        hr = SaveSurfaceToBMP(m_hwndApp, fname, NULL);

        if(FAILED(hr))
            LOG( TEXT("Failed to save surface to BMP for frame %d (0x%x)!!"), lpPresInfo->dwFrameNo, hr);
        else
            LOG( TEXT("Successfully save surface to BMP for frame %d!!"), lpPresInfo->dwFrameNo);
    }
    
    return hr;
}


STDMETHODIMP 
CBaseCustomAP::PresentImage( DWORD_PTR dwUserID, 
                         VMRPRESENTATIONINFO* lpPresInfo )
{
    HRESULT hr = S_OK;
    DWORD dwFrameID = 0;

    // LOG( TEXT("Frame number %d!!"), lpPresInfo->dwFrameNo);
    // Just render the composited images or don't render any images at all.
    if ( DW_COMPOSITED_ID != dwUserID )
    {
        LOG( TEXT("%S: INFO %d@%S - the stream is not composite stream!!"), __FUNCTION__, __LINE__, __FILE__ );
        goto cleanup;
    }
    
    hr = PreProcessImage( dwUserID, lpPresInfo );
    if ( FAILED(hr) )
        goto cleanup;
    
    //If we are in image save mode, no need to present the image
    if( !(m_bImageSavingMode || m_bVerifyStream) )
    {
        // we need to present image        
        if (m_pDefaultImagePresenter)
        {
            if (m_pfnCallback)
            {
                m_pfnCallback(lpPresInfo->lpSurf, m_pUserData);
            }

            hr = m_pDefaultImagePresenter->PresentImage(dwUserID, lpPresInfo);
            if ( FAILED(hr) )
                goto cleanup;
        }
        else
        {
            return E_POINTER;
        }
    }
    
    hr = PostProcessImage( dwUserID, lpPresInfo );
cleanup:
     return NOERROR;
}

HRESULT 
CBaseCustomAP::SaveImageBuffer( DWORD_PTR dwUserID, IDirectDrawSurface *pSurface )
{    
    DDSURFACEDESC ddsd;
    HRESULT hr = S_OK;
    BYTE *pTmp = NULL;
    BOOL bWasLocked = FALSE;
    BOOL bLocked= FALSE;

    CheckPointer( pSurface, E_POINTER );

    if ( DW_STREAM1_ID != dwUserID && DW_STREAM2_ID != dwUserID && DW_COMPOSITED_ID != dwUserID )
        return E_FAIL;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    while ((hr = pSurface->Lock(NULL, &ddsd,
#ifdef UNDER_CE
                0,
#else
                DDLOCK_NOSYSLOCK | DDLOCK_DONOTWAIT,
#endif
                (HANDLE)NULL)) == DDERR_WASSTILLDRAWING)
    {
        Sleep(1);
    }

    if(hr == DDERR_SURFACEBUSY)
    {
        bWasLocked = TRUE;
        pSurface->Unlock(NULL);
        while ((hr = pSurface->Lock(NULL, &ddsd,
#ifdef UNDER_CE
                0,
#else
                DDLOCK_NOSYSLOCK | DDLOCK_DONOTWAIT,
#endif
                (HANDLE)NULL)) == DDERR_WASSTILLDRAWING)
        {
            Sleep(1);
        }
    }
    CHECKHR(hr, TEXT("lock ddraw surface") );

    bLocked = TRUE;
    VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)m_pMediaType->pbFormat;

    if ( DW_STREAM1_ID == dwUserID )
    {
        pTmp = m_pStream1;
        m_StreamData[1].dwWidth = ddsd.dwWidth;
        m_StreamData[1].dwHeight = ddsd.dwHeight;
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        pTmp = m_pStream2;
        m_StreamData[2].dwWidth = ddsd.dwWidth;
        m_StreamData[2].dwHeight = ddsd.dwHeight;
    }
    else if ( DW_COMPOSITED_ID == dwUserID )
    {
        pTmp = m_pCompStream;
        m_StreamData[0].dwWidth = ddsd.dwWidth;
        m_StreamData[0].dwHeight = ddsd.dwHeight;
    }

    if ( m_bMediaTypeChanged )
    {
        //free old buffer
        if(!pTmp)
            delete [] pTmp;
        
        //Allocate the buffer based on the media type
        m_dwBufferSize= AllocateBitmapBuffer(pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, m_pMediaType->subtype, &pTmp);
        if(m_dwBufferSize == 0 || !pTmp)
        {
            hr = E_OUTOFMEMORY;
            CHECKHR(hr, TEXT("SaveImageBuffer: OOM") );
        }

        if ( DW_STREAM1_ID == dwUserID )
            m_pStream1 = pTmp;
        else if ( DW_STREAM2_ID == dwUserID )
            m_pStream2 = pTmp;
        else if ( DW_COMPOSITED_ID == dwUserID )
            m_pCompStream = pTmp;

        m_bMediaTypeChanged = FALSE;
    }
    {
        // take the lock
        CAutoLock Lock(&m_csLock);
        memcpy( pTmp, ddsd.lpSurface, m_dwBufferSize );
    }

cleanup:

    if(pSurface){
        if(!bWasLocked && bLocked)
        {
            hr = pSurface->Unlock(NULL);
        }
    }

    return hr;
}



//////////////////////////////////////////////////////////////////////////////
// IVMRWindowlessControl
//
// Just a pass-through back to the VMR.
//////////////////////////////////////////////////////////////////////////////

CCustomWindowless::CCustomWindowless()
:CUnknown(NAME("Custom Windowless Control"), NULL)
{

}


CCustomWindowless::~CCustomWindowless()
{
}


void CCustomWindowless::Deactivate(void)
{
    m_pDefaultWindowlessControl = NULL;
}


STDMETHODIMP CCustomWindowless::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IVMRWindowlessControl)
    {
        return GetInterface((IVMRWindowlessControl*)this, ppv);
    }

    return CUnknown::NonDelegatingQueryInterface(riid,ppv);
}


STDMETHODIMP CCustomWindowless::GetNativeVideoSize(LONG* lWidth, LONG* lHeight,
                                LONG* lARWidth, LONG* lARHeight)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetNativeVideoSize(lWidth, lHeight, lARWidth, lARHeight);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetMinIdealVideoSize(LONG* lWidth, LONG* lHeight)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetMinIdealVideoSize(lWidth, lHeight);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetMaxIdealVideoSize(LONG* lWidth, LONG* lHeight)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetMaxIdealVideoSize(lWidth, lHeight);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::SetVideoPosition(const LPRECT lpSRCRect, const LPRECT lpDSTRect)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->SetVideoPosition(lpSRCRect, lpDSTRect);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetVideoPosition(LPRECT lpSRCRect,LPRECT lpDSTRect)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetVideoPosition(lpSRCRect, lpDSTRect);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetAspectRatioMode(DWORD* lpAspectRatioMode)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetAspectRatioMode(lpAspectRatioMode);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::SetAspectRatioMode(DWORD AspectRatioMode)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->SetAspectRatioMode(AspectRatioMode);
    }
    else
    {
        return E_POINTER;
    }
}



STDMETHODIMP CCustomWindowless::SetVideoClippingWindow(HWND hwnd)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->SetVideoClippingWindow(hwnd);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::RepaintVideo(HWND hwnd, HDC hdc)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->RepaintVideo(hwnd, hdc);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::DisplayModeChanged()
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->DisplayModeChanged();
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetCurrentImage(BYTE** lpDib)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetCurrentImage(lpDib);
    }
    else
    {
        return E_POINTER;
    }
}



STDMETHODIMP CCustomWindowless::SetBorderColor(COLORREF Clr)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->SetBorderColor(Clr);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetBorderColor(COLORREF* lpClr)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetBorderColor(lpClr);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::SetColorKey(COLORREF Clr)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->SetColorKey(Clr);
    }
    else
    {
        return E_POINTER;
    }
}


STDMETHODIMP CCustomWindowless::GetColorKey(COLORREF* lpClr)
{
    if (m_pDefaultWindowlessControl)
    {
        return m_pDefaultWindowlessControl->GetColorKey(lpClr);
    }
    else
    {
        return E_POINTER;
    }
}


void CCustomWindowless::SetWindowless(CComPtr<IVMRWindowlessControl> pWindowlessControl)
{
    m_pDefaultWindowlessControl = pWindowlessControl;
}

//
// ctor/dtor for VMR RS custom ap
//
CVMRRSCustomAP::CVMRRSCustomAP( HRESULT *phr )
: CBaseCustomAP( phr )
{
}

CVMRRSCustomAP::~CVMRRSCustomAP()
{
}

//
// ctor/dtor for VMR E2E custom ap
//
CVMRE2ECustomAP::CVMRE2ECustomAP( HRESULT *phr )
: CBaseCustomAP( phr )
{
    m_bRuntimeVerify = FALSE;

    m_hDIB = NULL;
    m_hOldDIB = NULL;
    m_hdcComp = NULL;
    m_pDIBBits = NULL;
}


CVMRE2ECustomAP::~CVMRE2ECustomAP()
{
    // release the handles
    SelectObject( m_hdcComp, m_hOldDIB );
    if ( m_hDIB )
        DeleteObject( m_hDIB );

    if ( m_hdcComp )
        DeleteDC( m_hdcComp );

    m_hDIB = NULL;
    m_hOldDIB = NULL;
    m_hdcComp = NULL;
}

BOOL 
CVMRE2ECustomAP::InitCompositedDC()
{
    if ( !m_dwCompWidth || !m_dwCompHeight )
        return FALSE;

    HDC hdc = GetDC( NULL );
    if ( !hdc ) 
        goto cleanup;

    HDC m_hdcComp = CreateCompatibleDC( hdc );
    if ( !m_hdcComp )
        goto cleanup;

    BITMAPINFO bmiComp;
    LPVOID lpvCompBits;
 
    bmiComp.bmiHeader.biSize = sizeof( bmiComp.bmiHeader );
    bmiComp.bmiHeader.biWidth = m_dwCompWidth;
    bmiComp.bmiHeader.biHeight = -1 * m_dwCompHeight;
    bmiComp.bmiHeader.biPlanes = 1;
    bmiComp.bmiHeader.biBitCount = 32;
    bmiComp.bmiHeader.biCompression = BI_RGB;

    HBITMAP m_hDIB = CreateDIBSection( m_hdcComp, &bmiComp, DIB_RGB_COLORS,
                                         &lpvCompBits, NULL, 0 );
    if ( !m_hDIB )
        goto cleanup;

    // paint the composite space black.
    ZeroMemory( lpvCompBits, m_dwCompWidth * m_dwCompHeight * 4 );

    m_hOldDIB = (HBITMAP) SelectObject( m_hdcComp, m_hDIB );
    if ( !m_hOldDIB )
        goto cleanup;

    m_pDIBBits = (BYTE *)lpvCompBits;

cleanup:

    if ( hdc )
        ReleaseDC( NULL, hdc );
    return TRUE;
}

BOOL 
CVMRE2ECustomAP::RepaintCompositedMemDC()
{
    if ( !m_pDIBBits )
        return FALSE;

    ZeroMemory( m_pDIBBits, 4 * m_dwCompWidth * m_dwCompHeight );
    return TRUE;
}

HRESULT
CVMRE2ECustomAP::BlendStreamToCompImage( DWORD_PTR dwUserID )
{
    HRESULT hr = S_OK;
    
    HDC hdc = GetDC(NULL);
    if ( !hdc ) 
        goto cleanup;

    HDC hdcStream = CreateCompatibleDC(hdc);
    if ( !hdcStream )
        goto cleanup;

    
    BITMAPINFO bmi;
    LPVOID lpvBits;

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    if ( DW_STREAM1_ID == dwUserID )
    {
        bmi.bmiHeader.biWidth = m_StreamData[1].dwWidth;
        bmi.bmiHeader.biHeight = m_StreamData[1].dwHeight;
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        bmi.bmiHeader.biWidth = m_StreamData[2].dwWidth;
        bmi.bmiHeader.biHeight = m_StreamData[2].dwHeight;
    }

    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HBITMAP hBm = CreateDIBSection( hdcStream, &bmi, DIB_RGB_COLORS,
                                         &lpvBits, NULL, 0 );

    if ( !hBm )
        goto cleanup;

    {
        // Take the lock before touching m_pStream
        CAutoLock Lock(&m_csLock);

        if ( DW_STREAM1_ID == dwUserID )
            memcpy( lpvBits, m_pStream1, m_StreamData[1].dwWidth * m_StreamData[1].dwHeight * 4 );
        else if ( DW_STREAM2_ID == dwUserID )
            memcpy( lpvBits, m_pStream2, m_StreamData[2].dwWidth * m_StreamData[2].dwHeight * 4 );
    }

    HBITMAP hOld = (HBITMAP) SelectObject( hdcStream, hBm );
    if ( !hOld )
        goto cleanup;

    BLENDFUNCTION blendFunc;
    blendFunc.BlendOp = AC_SRC_OVER;
    blendFunc.BlendFlags = 0;
    blendFunc.AlphaFormat = 0; // AC_SRC_ALPHA
    DWORD dwDstWidth = 0, dwDstHeight = 0;
    if ( DW_STREAM1_ID == dwUserID )
    {
        blendFunc.SourceConstantAlpha = (BYTE)( m_StreamData[1].fAlpha * 255 );
        dwDstWidth = (DWORD)( m_dwCompWidth * ( m_StreamData[1].nrRect.right - m_StreamData[1].nrRect.left ) );
        dwDstHeight = (DWORD)( m_dwCompHeight * ( m_StreamData[1].nrRect.bottom - m_StreamData[1].nrRect.top ) );
    }
    else if ( DW_STREAM2_ID == dwUserID )
    {
        blendFunc.SourceConstantAlpha = (BYTE)( m_StreamData[2].fAlpha * 255 );
        dwDstWidth = (DWORD)( m_dwCompWidth * ( m_StreamData[2].nrRect.right - m_StreamData[2].nrRect.left ) );
        dwDstHeight = (DWORD)( m_dwCompHeight * ( m_StreamData[2].nrRect.bottom - m_StreamData[2].nrRect.top ) );
    }

    // blend the larger one to the composited space first
    BOOL bAlphablend =  AlphaBlend( m_hdcComp, 0, 0, dwDstWidth, dwDstHeight,
                                    hdcStream, 0, 0, bmi.bmiHeader.biWidth, abs(bmi.bmiHeader.biHeight),
                                    blendFunc );

    if ( !bAlphablend )
        goto cleanup;

cleanup:

    DeleteObject( SelectObject( hdcStream, hOld ) );
    if ( hdcStream )
        DeleteDC( hdcStream );

    if ( hdc )
        ReleaseDC( NULL, hdc );

    return hr;
}

BOOL
CVMRE2ECustomAP::IsFrameNumberMatched( DWORD_PTR dwUserID )
{
    HRESULT hr = S_OK;
    CAutoLock Lock(&m_csLock);
    if ( !m_pStream1 || !m_pStream2 || !m_pCompStream )
        return FALSE;

    // Set the current media type
    // Assumption: all streams have the same media types
    hr = m_Recognizer.SetFormat( m_pMediaType );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to set stamp reader format. (0x%08x)"), __FUNCTION__, __LINE__, __FILE__, hr );
        return FALSE;
    }

    int iStreamFrameID, iCompFrameID;
    if ( DW_STREAM1_ID == dwUserID )
        hr = m_Recognizer.ReadStamp( m_pStream1 );
    else if ( DW_STREAM2_ID == dwUserID )
        hr = m_Recognizer.ReadStamp( m_pStream2 );
    else
        hr = E_INVALIDARG;

    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to read the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return FALSE;
    }

    hr = m_Recognizer.GetFrameID( &iStreamFrameID );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return FALSE;
    }

    hr = m_Recognizer.ReadStamp( m_pCompStream );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to read the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return FALSE;
    }

    hr = m_Recognizer.GetFrameID( &iCompFrameID );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get the frame id of the stream."), __FUNCTION__, __LINE__, __FILE__ );
        return FALSE;
    }

    LOG( TEXT("Stream id: %d  -- Composited Stream id: %d."), iStreamFrameID, iCompFrameID );

    if ( iStreamFrameID != iCompFrameID )
        return FALSE;
    else
        return TRUE;
}

HRESULT 
CVMRE2ECustomAP::CompareImages()
{
    HRESULT hr = S_OK;

    BITMAPINFO imageBmi;
    GUID imgSubtype;

    ZeroMemory( &imageBmi, sizeof(BITMAPINFO) );
    imageBmi.bmiHeader.biWidth = m_dwCompWidth;
    imageBmi.bmiHeader.biHeight = m_dwCompHeight;
    imageBmi.bmiHeader.biCompression = BI_RGB;
    imgSubtype = MEDIASUBTYPE_RGB32;

    // Take the lock before touching m_pStream
    CAutoLock Lock(&m_csLock);

    float pixelDiff = DiffImage( (BYTE *)m_pDIBBits, 4 * m_dwCompWidth, m_pCompStream, 4 * m_dwCompWidth, &imageBmi, imgSubtype );

    if ( pixelDiff < 0.0f )
    {
        LOG( TEXT("%S: ERROR %d@%S - diff for the current media sample format not implemented."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr =  E_NOTIMPL;
        goto cleanup;
    }

    if ( pixelDiff <= m_fDiffTolerance )
    {
        LOG( TEXT("%S: pixelDiff = %f meets tolerance %f."), 
                    __FUNCTION__, pixelDiff, m_fDiffTolerance );
        hr = S_OK;
    }
    else 
    {
        LOG( TEXT("%S: ERROR %d@%S - pixelDiff = %f exceeds tolerance %f."), 
                    __FUNCTION__, __LINE__, __FILE__, pixelDiff, m_fDiffTolerance );
        // BUGBUG: save the bitmaps and do a visual insepct.
    }

cleanup:
    return hr;
}

HRESULT
CVMRE2ECustomAP::PreProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo )
{
    HRESULT hr = S_OK;
    if ( m_bRuntimeVerify )
    {
        //Get the comp stream media sample propertities
        hr = ProcessMediaSample( lpPresInfo, dwUserID);
        if ( FAILED(hr) ) return hr;

	    // sample it every second.
	    if ( lpPresInfo->dwFrameNo % m_dwVerificationRate == 0 )
		    hr = SaveImageBuffer( dwUserID, lpPresInfo->lpSurf );
    }
    else
        hr = CBaseCustomAP::PreProcessImage( dwUserID, lpPresInfo );

    return hr;
}

HRESULT
CVMRE2ECustomAP::PostProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo )
{
    HRESULT hr = S_OK;
    return hr;
}

DWORD
CVMRE2ECustomAP::VerifyThread()
{
    DWORD rc;
    while ( TRUE )
    {
        // dynamically compare
        // BUGBUG: stream 1 is hard coded as the instrumented stream
        if ( m_bRuntimeVerify )
        {
            if ( IsFrameNumberMatched(DW_STREAM1_ID) )
            {
                BlendStreamToCompImage( DW_STREAM1_ID );
                BlendStreamToCompImage( DW_STREAM2_ID );
                CompareImages();
            }
        }
        else
        {
            //Compare composite stream with saved image
            CompareSavedImageWithStream();
        }

        rc = WaitForSingleObject( m_hQuitEvent, 100 );
        if ( WAIT_OBJECT_0 == rc ) 
            break;
    }

    return 0;
}

#endif
